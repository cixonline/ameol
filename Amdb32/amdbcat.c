/* AMDBCAT.C - The Palantir database engine
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
#include "amcntxt.h"
#include "htmlhelp.h"
#include <assert.h>
#include <string.h>
#include "amlib.h"
#include "amctrls.h"
#include "amuser.h"
#include <io.h>
#include "amdbi.h"
#include "amdbrc.h"
#include "amevent.h"

#include <sys\stat.h>

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK DatabasePassword( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL DatabasePassword_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL DatabasePassword_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL DatabasePassword_OnInitDialog( HWND, HWND, LPARAM );

BOOL FASTCALL Amdb_VerifyDatabasePassword( DBHEADER * );

extern const char NEAR szHelpFile[];

static BYTE rgEncodeKey[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };

int cTotalTopics;                         /* Total number of topics in database */
BOOL fDataChanged;                        /* TRUE if database changed */
DBHEADER dbHdr;                           /* Database header */
LPSTR lpszGlbDatabase;                    /* Database filename */

/* User folder structure 
 */
#pragma pack(1)
typedef struct tagUCLITEM {
   char szSigFile[ 9 ];                   /* Signature file */
   DWORD dwFlags;                         /* User folder flags */
   PURGEOPTIONS po;                       /* Folder purge options */
} UCLITEM;
#pragma pack()

/* Disk image user folder structure.
 */
#pragma pack(1)
typedef struct tagUDCLITEM {
   char szFolderName[ 80 ];               /* Folder name */
   WORD cTopics;                          /* Number of topics in folder */
   UCLITEM uclItem;                       /* User folder settings */
} UDCLITEM;
#pragma pack()

/* Disk image user folder structure (version 1.13)
 */
#pragma pack(1)
typedef struct tagOLD_UDCLITEM {
   char szFolderName[ 80 ];               /* Folder name */
   WORD cTopics;                          /* Number of topics in folder */
   char szSigFile[ 9 ];                   /* Signature file */
   DWORD dwFlags;                         /* User folder flags */
} OLD_UDCLITEM;
#pragma pack()

/* User topic structure
 */
#pragma pack(1)
typedef struct tagUTLITEM {
   char szSigFile[ 9 ];                   /* Signature file */
   PURGEOPTIONS po;                       /* Topic purge options */
   VIEWDATA vd;                           /* Topic display information */
   MSGCOUNT msgc;                         /* Message counts */
   DWORD dwFlags;                         /* User status flags */
} UTLITEM;
#pragma pack()

/* Disk image user topic structure.
 */
#pragma pack(1)
typedef struct tagUDTLITEM {
   char szTopicName[ 80 ];                /* Topic name */
   UTLITEM utlItem;                       /* User topic settings */
} UDTLITEM;
#pragma pack()

int FASTCALL UpgradeIndexFiles( void );

/* This function sets the data directory for subsequent data
 * access. We also get the user directory for loading the user
 * image files.
 */
void WINAPI EXPORT Amdb_SetDataDirectory( LPSTR lpszDataDir )
{
   lstrcpy( szDataDir, lpszDataDir );
   Amuser_GetActiveUserDirectory( szUserDir, sizeof(szUserDir) );
   fInitDataDirectory = TRUE;
}

/* This function reads the list of folders from the DATABASE.DAT file
 * and stores them in memory as a linked list.
 */
int WINAPI EXPORT Amdb_LoadDatabase( LPSTR lpszDatabase )
{
   BOOL fHasCategories;
   HNDFILE fhData;
   CATITEM catItem;
   int cRead;

   /* Open the global database.
    */
   if( HNDFILE_ERROR == ( fhData = Amdb_OpenFile( lpszDatabase, AOF_READ ) ) )
      return( AMDBERR_FILENOTFOUND );

   /* Refresh interface properties.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Get the buffer upper limit for paging in
    * topics.
    */
   wBufferUpperLimit = Amuser_GetPPInt( "Settings", "PagingBufferUpperLimit", 65000U );
   if( wBufferUpperLimit > 65000 )
      wBufferUpperLimit = 65000;

   /* Check for SHARE.EXE
    * If it's loaded, fHasShare is set to TRUE. We use this to decide whether
    * to do file locking.
    */
   fHasShare = Amfile_IsSharingLoaded();

   /* Load global database header and check format.
    */
   cRead = Amfile_Read( fhData, &dbHdr, sizeof( DBHEADER ) );
   if( cRead != sizeof( DBHEADER ) )
      {
      Amfile_Close( fhData );
      return( AMDBERR_BADFORMAT );
      }
   else if( dbHdr.wID != DB_ID && dbHdr.wVersion < 0x104 )
      {
      Amfile_Close( fhData );
      return( AMDBERR_BADVERSION );
      }

   /* Is this database password protected? If so, prompt for the
    * password first.
    */
   if( !Amdb_VerifyDatabasePassword( &dbHdr ) )
      return( AMDBERR_BADPASSWORD );

   /* Prompt before upgrading.
    */
// if( dbHdr.wVersion < 0x105 )
//    if( IDNO == MessageBox( ip.hwndFrame, GS(IDS_STR534), "Warning", MB_YESNO|MB_ICONINFORMATION ) )
//       return( AMDBERR_USERABORT );

   /* Save database information.
    */
   lpszGlbDatabase = _strdup(lpszDatabase);
   fDataChanged = FALSE;

   /* Keep count of total topics read.
    */
   cTotalTopics = 0;

   /* Read the first category item. If this is a version 1.04
    * or earlier database, then create a default category as
    * none are defined in the database.
    */
   memset( &catItem, 0, sizeof(CATITEM) );
   if( dbHdr.wVersion < 0x105 )
      {
      cRead = sizeof(CATITEM);
      strcpy( catItem.szCategoryName, "Main" );
      catItem.timeCreated = Amdate_GetPackedCurrentTime();
      catItem.dateCreated = Amdate_GetPackedCurrentDate();
      catItem.cFolders = 0xFFFF;
      catItem.cMsgs = 0;
      catItem.dwFlags = DF_EXPANDED;
      catItem.wLastPurgeDate = 0;
      catItem.wLastPurgeTime = 0;
      fHasCategories = FALSE;
      }
   else
      {
      cRead = Amfile_Read( fhData, &catItem, sizeof(CATITEM) );
      fHasCategories = TRUE;
      }
   
   /* Now loop and read categories.
    */
   while( (WORD)cRead == sizeof(CATITEM) )
      {
      register UINT k;
      PCAT pcat;
      PCL pcl;

      /* Create the new category.
       */
      if( !fNewCategory( &pcat, &catItem ) )
         {
         Amfile_Close( fhData );
         return( AMDBERR_OUTOFMEMORY );
         }
   
      /* Loop for all folders in this category.
       */
      pcl = NULL;
      for( k = 0; k < catItem.cFolders; ++k )
         {
         CLITEM clItem;
         UINT c;

         /* Read and link the list of folders and topics from the database file.
          */
         memset( &clItem, 0, sizeof(CLITEM) );
         cRead = Amfile_Read( fhData, &clItem, dbHdr.cbFolder );
         if( !fHasCategories && 0 == cRead )
            break;

         /* Create the folder.
          */
         if( !fNewFolder( pcat, &pcl, NULL, &clItem ) )
            {
            Amfile_Close( fhData );
            return( AMDBERR_OUTOFMEMORY );
            }

         /* Loop and read each topic in the folder.
          */
         pcl->clItem.cTopics = 0;
         for( c = 0; c < clItem.cTopics; ++c )
            {
            TLITEM tlItem;
            PTL ptl;
      
            /* First read from the global file.
             */
            memset( &tlItem, 0, sizeof(TLITEM) );
            cRead = Amfile_Read( fhData, &tlItem, dbHdr.cbTopic );
            if( (WORD)cRead != dbHdr.cbTopic )
               break;

            /* Fix the upper message count dynamically. Also
             * clear any spurious in-use flag.
             */
            if( tlItem.dwUpperMsg < tlItem.dwMaxMsg )
               tlItem.dwUpperMsg = tlItem.dwMaxMsg;
            tlItem.dwFlags &= ~TF_TOPICINUSE;
            if( !fNewTopic( &ptl, pcl, &tlItem ) )
               {
               Amfile_Close( fhData );
               return( AMDBERR_OUTOFMEMORY );
               }
            ++cTotalTopics;

            /* Update total message counts.
             */
            if( fHasCategories )
               {
               UpdateMessageCounts( &cTotalMsgc, &ptl->tlItem.msgc );
               UpdateMessageCounts( &pcl->msgc, &ptl->tlItem.msgc );
               UpdateMessageCounts( &pcat->msgc, &ptl->tlItem.msgc );
               }
            }
         }

      /* Reset the count of folders.
       */
      pcat->catItem.cFolders = k;

      /* Read the next category.
       */
      memset( &catItem, 0, sizeof(CATITEM) );
      cRead = Amfile_Read( fhData, &catItem, sizeof(CATITEM) );
      }

   /* Now process any user file. This is only needed when
    * upgrading to v1.05 of the database format. The file
    * isn't used by this or later versions.
    */
   if( !fHasCategories )
      {
      HNDFILE fhUser;

      /* Open the user database. If not found, don't worry because
       * it'll be recreated.
       */
      fhUser = Amdb_UserOpenFile( lpszDatabase, AOF_READ );
      if( HNDFILE_ERROR != fhUser )
         {
         UDCLITEM udclItem;
         int cbUdClItem;

         /* Load user database header and check format. If they
          * differ, we will delete the user database and rebuild.
          */
         cRead = Amfile_Read( fhUser, &dbHdr, sizeof( DBHEADER ) );

         /* Disk CLITEM structure size.
          */
         cbUdClItem = ( dbHdr.wVersion < 0x104 ) ? sizeof(OLD_UDCLITEM) : sizeof(UDCLITEM);

         /* Loop for every folder.
          */
         cRead = Amfile_Read( fhUser, &udclItem, cbUdClItem );
         while( cRead == cbUdClItem )
            {
            register UINT c;
            PCL pcl;

            /* Fix udclItem structure if it has changed.
             */
            if( dbHdr.wVersion < 0x104 )
               udclItem.uclItem.po = poDefault;

            /* Locate this folder in the main database. If it
             * doesn't exist, then the folder has either been renamed
             * or deleted.
             */
            if( NULL != ( pcl = Amdb_OpenFolder( NULL, udclItem.szFolderName ) ) )
               {
               strcpy( pcl->clItem.szSigFile, udclItem.uclItem.szSigFile );
               pcl->clItem.po = udclItem.uclItem.po;
               pcl->clItem.dwFlags |= udclItem.uclItem.dwFlags;
               }

            /* Loop for each topic in the folder, even if the
             * folder was deleted.
             */
            for( c = 0; c < udclItem.cTopics; ++c )
               {
               UDTLITEM udtlItem;
               PTL ptl;

               /* Read the topic record.
                */
               cRead = Amfile_Read( fhUser, &udtlItem, sizeof( UDTLITEM ) );
               if( cRead != sizeof( UDTLITEM ) )
                  break;

               /* If the folder exists, locate the topic in the folder. If the
                * topic doesn't exist, it has either been renamed or deleted.
                */
               if( NULL != pcl )
                  if( NULL != ( ptl = Amdb_OpenTopic( pcl, udtlItem.szTopicName ) ) )
                     {
                     strcpy( ptl->tlItem.szSigFile, udtlItem.utlItem.szSigFile );
                     ptl->tlItem.po = udtlItem.utlItem.po;
                     ptl->tlItem.vd = udtlItem.utlItem.vd;
                     ptl->tlItem.dwFlags |= udtlItem.utlItem.dwFlags;
                     ptl->tlItem.msgc = udtlItem.utlItem.msgc;
                     pcl->pcat->catItem.cMsgs += ptl->tlItem.cMsgs;

                     /* Update total message counts.
                      */
                     UpdateMessageCounts( &cTotalMsgc, &ptl->tlItem.msgc );
                     UpdateMessageCounts( &pcl->msgc, &ptl->tlItem.msgc );
                     UpdateMessageCounts( &pcl->pcat->msgc, &ptl->tlItem.msgc );
                     }
               }
            cRead = Amfile_Read( fhUser, &udclItem, cbUdClItem );
            }
         Amfile_Close( fhUser );
         }
      }

   /* All done. Close it
    */
   Amfile_Close( fhData );

   /* If database format out of date, we need to upgrade all THD
    * index files.
    */
   if( dbHdr.wVersion < 0x104 )
      {
      int nRetCode;

      /* Now upgrade the index files.
       */
      nRetCode = UpgradeIndexFiles();
      if( nRetCode != AMDBERR_NOERROR )
         return( AMDBERR_USERABORT );
      }

   /* Otherwise ensure the new format is committed
    * to disk.
    */
   else if( dbHdr.wVersion < 0x105 )
      {
      fDataChanged = TRUE;
      Amdb_CommitDatabase( FALSE );
      Amdb_UserDeleteFile( lpszDatabase );
      }
   return( AMDBERR_NOERROR );
}

/* Upgrade the topic index files.
 */
int FASTCALL UpgradeIndexFiles( void )
{
   DWORD dwSpaceRequired;
   DWORD dwSpaceAvail;
   WORD wOldCount;
   HWND hwndGauge;
   int cTopics;
   PCAT pcat;
   HWND hDlg;
   PCL pcl;
   PTL ptl;

   /* Tell the user what we propose to do.
    */
   if( IDNO == MessageBox( ip.hwndFrame, GS(IDS_STR530), "Warning", MB_YESNO|MB_ICONINFORMATION ) )
      return( AMDBERR_USERABORT );

   /* First check how much disk space is needed for the
    * conversion.
    */
   hDlg = Adm_Billboard( hRscLib, ip.hwndFrame, MAKEINTRESOURCE(IDDLG_CHECKINGSPACE) );
   VERIFY( hwndGauge = GetDlgItem( hDlg, IDD_GAUGE ) );
   SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
   SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

   /* Big loop here.
    */
   dwSpaceRequired = 0L;
   cTopics = 0;
   wOldCount = 0;
   for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
      for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
         for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
            {
            char szFileName[ _MAX_PATH ];
            struct _stat st;
            WORD wCount;

            /* Count how much space taken up by this THD file.
             */
            wsprintf( szFileName, "%s\\%s.THD", szDataDir, ptl->tlItem.szFileName );
            if( _stat( szFileName, &st ) != -1 )
               dwSpaceRequired += st.st_size;
            ++cTopics;
            wCount = (WORD)( ( cTopics * 100 ) / cTotalTopics );
            if( wCount != wOldCount )
               {
               SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
               wOldCount = wCount;
               }
            }
   DestroyWindow( hDlg );

   /* Enough space?
    */
   dwSpaceAvail = Amdir_FreeDiskSpace( szDataDir );
   if( dwSpaceAvail < dwSpaceRequired )
      {
      char szSpaceAvail[ 32 ];
      char szSpaceRequired[ 32 ];
      char sz[ 512 ];

      FormatThousands( dwSpaceAvail, szSpaceAvail );
      FormatThousands( dwSpaceRequired - dwSpaceAvail, szSpaceRequired );
      wsprintf( sz, GS(IDS_STR528), szSpaceAvail, szSpaceRequired );
      MessageBox( ip.hwndFrame, sz, NULL, MB_OK|MB_ICONINFORMATION );
      return( AMDBERR_USERABORT );
      }

   /* Announcement for the benefit of the
    * viewers.
    */
   hDlg = Adm_Billboard( hRscLib, ip.hwndFrame, MAKEINTRESOURCE(IDDLG_UPGRADE) );
   VERIFY( hwndGauge = GetDlgItem( hDlg, IDD_GAUGE ) );
   SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
   SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

   /* Initalise vars.
    */
   wOldCount = 0;
   cTopics = 0;

   /* Big loop here.
    */
   for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
      {
      if( dbHdr.cbMsg != sizeof(THITEM) )
         for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
            for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
               {
               char szOldFileName[ 13 ];
               char szFileName[ 13 ];
               THITEM thItem;
               HNDFILE fh;
               WORD wCount;

               /* If there is a .~A~ file for this topic, then it has
                * already been converted, so skip it.
                */
               wsprintf( szFileName, "%s.~A~", (LPSTR)ptl->tlItem.szFileName );
               if( Amfile_QueryFile( szFileName ) )
                  {
                  ++cTopics;
                  wCount = (WORD)( ( cTopics * 100 ) / cTotalTopics );
                  if( wCount != wOldCount )
                     {
                     SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                     wOldCount = wCount;
                     }
                  continue;
                  }

               /* First open the original file.
                */
               wsprintf( szOldFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
               if( ( fh = Amdb_OpenFile( szOldFileName, AOF_READ|AOF_SEQUENTIAL_IO ) ) != HNDFILE_ERROR )
                  {
                  HNDFILE fhOut;

                  wsprintf( szFileName, "%s.WRK", (LPSTR)ptl->tlItem.szFileName );
                  if( ( fhOut = Amdb_CreateFile( szFileName, 0 ) ) != HNDFILE_ERROR )
                     {
                     DWORD dwMsgPtr;
                     int cWritten;

                     /* Loop for each index record and write out the larger/smaller
                      * version to the output file.
                      */
                     cWritten = 0;
                     dwMsgPtr = 0L;
                     memset( &thItem, 0, sizeof(THITEM) );
                     while( Amfile_Read( fh, &thItem, dbHdr.cbMsg ) == dbHdr.cbMsg )
                        {
                        if( !YieldToSystem() )
                           break;
                        thItem.dwMsgPtr = dwMsgPtr;
                        Amfile_Write( fhOut, (char *)&thItem, sizeof(THITEM) );
                        dwMsgPtr += sizeof(THITEM);
                        ++cWritten;
                        }
                     Amfile_Close( fhOut );
                     Amfile_Close( fh );

                     /* Update progress gauge.
                      */
                     YieldToSystem();
                     ++cTopics;
                     wCount = (WORD)( ( cTopics * 100 ) / cTotalTopics );
                     if( wCount != wOldCount )
                        {
                        SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                        wOldCount = wCount;
                        }

                     /* All done. Only upgrade if no errors.
                      */
                     wsprintf( szFileName, "%s.~A~", (LPSTR)ptl->tlItem.szFileName );
                     Amdb_RenameFile( szOldFileName, szFileName );
                     wsprintf( szFileName, "%s.WRK", (LPSTR)ptl->tlItem.szFileName );
                     Amdb_RenameFile( szFileName, szOldFileName );
                     }
                  else
                     {
                     /* Can't upgrade THD file. This is serious, so warn the
                      * user!
                      */
                     Amfile_Close( fh );
                     }
                  }
               }

      /* Fix this database.
       */
      dbHdr.wID = DB_ID;
      dbHdr.wVersion = DB_VERSION;
      dbHdr.cbFolder =  sizeof(CLITEM);
      dbHdr.cbTopic = sizeof(TLITEM);
      dbHdr.cbMsg = sizeof(THITEM);
      fDataChanged = TRUE;
      }
   DestroyWindow( hDlg );

   /* All done, so vape the ~A~ files.
    */
   MessageBox( ip.hwndFrame, GS(IDS_STR529), "Done", MB_OK|MB_ICONINFORMATION );
   return( AMDBERR_NOERROR );
}

/* This function updates the message count of a database from a folder, or
 * a folder from a topic.
 */
void FASTCALL UpdateMessageCounts( MSGCOUNT * pmsgcDst, MSGCOUNT * pmsgcSrc )
{
   pmsgcDst->cUnRead += pmsgcSrc->cUnRead;
   pmsgcDst->cMarked += pmsgcSrc->cMarked;
   pmsgcDst->cPriority += pmsgcSrc->cPriority;
   pmsgcDst->cUnReadPriority += pmsgcSrc->cUnReadPriority;
   pmsgcDst->cTagged += pmsgcSrc->cTagged;
}

/* This function updates the message count of a database from a folder, or
 * a folder from a topic.
 */
void FASTCALL ReduceMessageCounts( MSGCOUNT * pmsgcDst, MSGCOUNT * pmsgcSrc )
{
   pmsgcDst->cUnRead -= pmsgcSrc->cUnRead;
   pmsgcDst->cMarked -= pmsgcSrc->cMarked;
   pmsgcDst->cPriority -= pmsgcSrc->cPriority;
   pmsgcDst->cUnReadPriority -= pmsgcSrc->cUnReadPriority;
   pmsgcDst->cTagged -= pmsgcSrc->cTagged;
}

/* This function checks the database password.
 */
BOOL FASTCALL Amdb_VerifyDatabasePassword( DBHEADER * pdbHdr )
{
   char szActiveUser[ 40 ];
   char szPassword[ 40 ];

   /* Is the owner the same as the active
    * user? If so, ignore any password.
    */
   Amuser_GetActiveUser( szActiveUser, 40 );
   if( strcmp( szActiveUser, pdbHdr->szOwner ) == 0 )
      return( TRUE );

   /* Create the encrypted no-password string to
    * check for absent password.
    */
   memset( szPassword, 0, 40 );
   Amuser_Encrypt( szPassword, rgEncodeKey );
   if( memcmp( szPassword, pdbHdr->szPassword, 40 ) == 0 )
      return( TRUE );

   /* Otherwise get a password from the user.
    */
   return( Adm_Dialog( hLibInst, GetFocus(), MAKEINTRESOURCE(IDDLG_PASSWORD), DatabasePassword, (LPARAM)(LPSTR)pdbHdr ) );
}

/* Handles the Address Book Password prompt dialog.
 */
BOOL EXPORT CALLBACK DatabasePassword( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, DatabasePassword_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Edit Group dialog.
 */
LRESULT FASTCALL DatabasePassword_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, DatabasePassword_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, DatabasePassword_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPASSWORD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL DatabasePassword_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         char szPassword[ 40 ];
         DBHEADER * pdbHdr;
         HWND hwndEdit;

         /* Get the entered password, encrypt it and compare it
          * against the specified password.
          */
         pdbHdr = (DBHEADER *)GetWindowLong( hwnd, DWL_USER );
         memset( szPassword, 0, 40 );
         hwndEdit = GetDlgItem( hwnd, IDD_PASSWORD );
         Edit_GetText( hwndEdit, szPassword, 40 );
         Amuser_Encrypt( szPassword, rgEncodeKey );
         if( memcmp( pdbHdr->szPassword, szPassword, 40 ) != 0 )
            {
            /* No match, so give them another go.
             */
            MessageBox( hwnd, GS(IDS_STR523), NULL, MB_OK|MB_ICONINFORMATION );
            SetFocus( hwndEdit );
            Edit_SetSel( hwndEdit, 0, 32767 );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL DatabasePassword_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   DBHEADER * pHdr;
   HWND hwndEdit;

   /* Dereference the address book and save it
    */
   pHdr = (DBHEADER *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set the edit fields.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_NAME );
   Edit_SetText( hwndEdit, pHdr->szName );

   /* Blank the password field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_PASSWORD );
   Edit_LimitText( hwndEdit, 40 );
   return( TRUE );
}

/* This function closes any open caches.
 */
void WINAPI EXPORT Amdb_CommitCachedFiles( void )
{
   if( fhCachedThd != HNDFILE_ERROR )
      {
      Amfile_Close( fhCachedThd );
      fhCachedThd = HNDFILE_ERROR;
      ptlCachedThd = NULL;
      }
   if( fhCachedTxt != HNDFILE_ERROR )
      {
      Amfile_Close( fhCachedTxt );
      fhCachedTxt = HNDFILE_ERROR;
      ptlCachedTxt = NULL;
      }
}

/* This function returns a pointer to the file basename.
 */
LPSTR FASTCALL GetFileBasename( LPSTR npszFile )
{
   LPSTR npszFileT;

   npszFileT = npszFile;
   while( *npszFileT )
      {
      if( *npszFileT == ':' || *npszFileT == '\\' || *npszFileT == '/' )
         npszFile = npszFileT + 1;
      ++npszFileT;
      }
   return( npszFile );
}

BOOL WINAPI EXPORT Amdb_Archive( LPCSTR lpszFileName, BOOL fDeleteOriginal )
{
   char szFilePath[ _MAX_PATH ];
   /* Only archive if the file exists.
    */
   if( Amdb_QueryFileExists( lpszFileName ) )
   {
      char szBaseName[ 9 ];
      register int c;
      char sz[ 144 ];
      char * npsz;

      /* Find the file basename and the extension.
       */
      npsz = GetFileBasename( (LPSTR)lpszFileName );
      for( c = 0; c < 8 && npsz[ c ] && npsz[ c ] != '.'; ++c )
         szBaseName[ c ] = npsz[ c ];
      szBaseName[ c ] = '\0';

      /* In the archive directory, shift up the current files
       * by renumbering their extensions.
       */
      for( c = 10; c > 1; --c )
      {
         wsprintf( sz, "%s.%3.3d", szBaseName, c );
         wsprintf( szFilePath, "%s.%3.3d", szBaseName, c - 1 );
         Amdb_DeleteFile( sz );
         Amdb_RenameFile( szFilePath, sz );
      }

      /* Delete any 001 file. It will previousy have been renumbered
       * to 002.
       */
      wsprintf( sz, "%s.001", (LPSTR)szBaseName );
      Amdb_DeleteFile( sz );

      /* If we are permitted to archive, archive the file by copying
       * or moving depending on the fDeleteOriginal flag.
       */
      if( !fDeleteOriginal )
      {
         BOOL f;
   
         f = Amdb_CopyFile( (LPSTR)lpszFileName, sz );
         return( f == 0 );
      }
      if( !Amdb_RenameFile( lpszFileName, sz ) )
      {
         BOOL f;

         /* Could not rename (poss x-device rename), so
          * copy it instead.
          */
         if( 0 == ( f = Amdb_CopyFile( (LPSTR)lpszFileName, sz ) ) )
            Amdb_DeleteFile( lpszFileName );
         return( f == 0 );
      }
   }
   return( TRUE );
}

/* This function commits changes to the database to disk and
 * optionally creates a backup file.
 */
int WINAPI EXPORT Amdb_CommitDatabase( BOOL fCreateBackup )
{
   BOOL fOk = TRUE;

   /* Close any open cached file handle
    */
   Amdb_SweepAndDiscard();
   Amdb_CommitCachedFiles();

   /* Write out the data portion if it changed. Obviously only
    * the owner can change this.
    */
   if( fDataChanged )
      {
      char szFilePath[ _MAX_PATH ];
      char * pExt;
      HNDFILE fh;

      /* Create a backup of the main file first. Don't worry
       * if the Amdb_DeleteFile fails because if it does, the
       * Amdb_RenameFile will also fail and we'll catch the
       * error then.
       */
      if( fCreateBackup )
         {
         strcpy( szFilePath, lpszGlbDatabase );
         pExt = strrchr( szFilePath, '.' );
         if( NULL != pExt )
            strcpy( pExt, ".BAK" );
         Amdb_Archive( szFilePath, FALSE );
         Amdb_DeleteFile( szFilePath );
         if( Amdb_QueryFileExists( lpszGlbDatabase ) )
            if( !Amdb_RenameFile( lpszGlbDatabase, szFilePath ) )
               {
               char sz[ 400 ];

               wsprintf( sz, GS(IDS_STR525), szFilePath );
               if( IDNO == MessageBox( GetFocus(), sz, NULL, MB_YESNO|MB_ICONINFORMATION ) )
                  return( AMDBERR_CANNOTBACKUP );
               }
         }

      /* Created backup, so OK to continue.
       */
      strcpy( szFilePath, lpszGlbDatabase );
      pExt = strrchr( szFilePath, '.' );
      if( NULL != pExt )
         strcpy( pExt, ".TMP" );
      if( ( fh = Amdb_CreateFile( szFilePath, AOF_WRITE ) ) == HNDFILE_ERROR )
         fOk = FALSE;
      else
         {
         PCAT pcat;

         /* Make sure header structure sizes reflect the
          * current settings.
          */
         dbHdr.wID = DB_ID;
         dbHdr.wVersion = DB_VERSION;
         dbHdr.cbFolder =  sizeof(CLITEM);
         dbHdr.cbTopic = sizeof(TLITEM);
         dbHdr.cbMsg = sizeof(THITEM);

         /* This big loop writes out every folder and topic to the database file,
          * and, for each topic, updates the message thread file if the topic has been
          * altered.
          */
         if( fOk )
            fOk = Amfile_Write( fh, (LPCSTR)&dbHdr, sizeof( DBHEADER ) ) == sizeof( DBHEADER );
         for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
            {
            PCL pcl;

            fOk = Amfile_Write( fh, (LPCSTR)&pcat->catItem, sizeof( CATITEM ) ) == sizeof( CATITEM );
            for( pcl = pcat->pclFirst; fOk && pcl; pcl = pcl->pclNext )
               {
               PTL ptl;

               fOk = Amfile_Write( fh, (LPCSTR)&pcl->clItem, sizeof( CLITEM ) ) == sizeof( CLITEM );
               for( ptl = pcl->ptlFirst; fOk && ptl; ptl = ptl->ptlNext )
                  {
                  SaveAttachments( ptl );
                  fOk = Amfile_Write( fh, (LPCSTR)&ptl->tlItem, sizeof( TLITEM ) ) == sizeof( TLITEM );
                  }
               }
            }
         Amfile_Close( fh );

         /* If all okay, delete the original database file and
          * rename the temporary one.
          */
         if( fOk )
            {
            Amdb_DeleteFile( lpszGlbDatabase );
            Amdb_RenameFile( szFilePath, lpszGlbDatabase );
            }
         else
            Amdb_DeleteFile( szFilePath );

         /* Changes to the active database ALWAYS means changes to
          * the user file.
          */
         fDataChanged = FALSE;
         }
      }
   return( fOk ? AMDBERR_NOERROR : AMDBERR_OUTOFDISKSPACE );
}

/* This function closes the specified database without
 * removing it from disk.
 */
int WINAPI EXPORT Amdb_CloseDatabase( void )
{
   PCAT pcatNext;
   PCAT pcat = NULL;

   /* Close any cached files
    */
   Amdb_CommitCachedFiles();

   /* Inform the caller that the database is being
    * closed.
    */
   Amuser_CallRegistered( AE_CLOSEDATABASE, (LPARAM)pcat, 0 );
   for( pcat = pcatFirst; pcat; pcat = pcatNext )
      {
      PCL pclNext;
      PCL pcl;

      pcatNext = pcat->pcatNext;
      for( pcl = pcat->pclFirst; pcl; pcl = pclNext )
         {
         PTL ptlNext;
         PTL ptl;

         pclNext = pcl->pclNext;
         for( ptl = pcl->ptlFirst; ptl; ptl = ptlNext )
            {
            PAH pahNext;
            PTH pthNext;
            PTH pth;
            PAH pah;

            for( pth = ptl->pthFirst; pth; pth = pthNext )
               {
               pthNext = pth->pthNext;
               FreeMemory( &pth );
               }
            for( pah = ptl->pahFirst; pah; pah = pahNext )
               {
               pahNext = pah->pahNext;
               FreeMemory( &pah );
               }
            ptlNext = ptl->ptlNext;
            FreeMemory( &ptl );
            }
         if( pcl->lpModList )
            FreeMemory( &pcl->lpModList );
         FreeMemory( &pcl );
         }
      FreeMemory( &pcat );
      }
   free( lpszGlbDatabase );
   ptlCachedThd = NULL;
   ptlCachedTxt = NULL;
   return( AMDBERR_NOERROR );
}

/* This function enumerates all available databases. It calls the user
 * supplied function, lpEnumDbProc, with a list of database names.
 */
BOOL WINAPI EXPORT Amdb_EnumDatabases( DATABASEENUMPROC lpEnumDbProc, LPARAM lParam )
{
   char sz[ _MAX_PATH ];
   register int n;
   BOOL fContinue;
   FINDDATA ft;
   HFIND r;

   fContinue = TRUE;
   wsprintf( sz, "%s\\*.ADB", (LPSTR)szDataDir );
   for( n = r = Amuser_FindFirst( sz, _A_NORMAL, &ft ); fContinue && n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      HNDFILE fh;

      if( HNDFILE_ERROR != ( fh = Amdb_OpenFile( ft.name, AOF_READ ) ) )
         {
         DBHEADER dbHdr;
         int cRead;

         if( sizeof(DBHEADER) == ( cRead = Amfile_Read( fh, &dbHdr, sizeof( DBHEADER ) ) ) )
            if( dbHdr.wVersion == DB_VERSION && dbHdr.wID == DB_ID )
               {
               DATABASEINFO dbinfo;

               /* Construct a dummy DATABASEINFO structure for this
                * database.
                */
               strcpy( dbinfo.szFilename, ft.name );
               strcpy( dbinfo.szName, dbHdr.szName );
               strcpy( dbinfo.szOwner, dbHdr.szOwner );
               dbinfo.cCategories = dbHdr.cCategories;
               dbinfo.timeCreated = dbHdr.timeCreated;
               dbinfo.dateCreated = dbHdr.dateCreated;
               Amfile_Close( fh );
               fContinue = lpEnumDbProc( &dbinfo, lParam );
               continue;
               }
         Amfile_Close( fh );
         }
      }
   Amuser_FindClose( r );
   return( fContinue );
}

/* This function creates a new, empty, database and adds it to the
 * database list.
 */
BOOL WINAPI EXPORT Amdb_CreateDatabase( LPDBCREATESTRUCT lpdbcs )
{
   /* Fill out the dbHdr structure.
    */
   dbHdr.wID = DB_ID;
   dbHdr.wVersion = DB_VERSION;
   lstrcpy( dbHdr.szName, lpdbcs->szName );
   memset( dbHdr.szPassword, 0, sizeof(dbHdr.szPassword) );
   lstrcpy( dbHdr.szPassword, lpdbcs->szPassword );
   Amuser_Encrypt( dbHdr.szPassword, rgEncodeKey );
   Amuser_GetActiveUser( dbHdr.szOwner, sizeof(dbHdr.szOwner) );
   dbHdr.timeCreated = Amdate_GetPackedCurrentTime();
   dbHdr.dateCreated = Amdate_GetPackedCurrentDate();
   dbHdr.cCategories = 0;
   dbHdr.cbFolder = sizeof(CLITEM);
   dbHdr.cbTopic = sizeof(TLITEM);
   dbHdr.cbMsg = sizeof(THITEM);
   lpszGlbDatabase = _strdup(lpdbcs->szFilename);
   fDataChanged = TRUE;

   /* Create a default category.
    */
   Amdb_CreateCategory( "Main" );
   return( TRUE );
}

/* This function deletes a database given the database filename
 * rather than loading it into memory. It also needs to delete all
 * THD, TXT and other files associated with the database.
 */
void WINAPI EXPORT Amdb_PhysicalDeleteDatabase( char * pszFilename )
{
   HNDFILE fh;

   if( HNDFILE_ERROR != ( fh = Amdb_OpenFile( pszFilename, AOF_READ ) ) )
      {
      DBHEADER dbHdr;
      int cRead;

      if( sizeof(DBHEADER) == ( cRead = Amfile_Read( fh, &dbHdr, sizeof( DBHEADER ) ) ) )
         if( dbHdr.wVersion == DB_VERSION && dbHdr.wID == DB_ID )
            {
            CLITEM clItem;

            cRead = Amfile_Read( fh, &clItem, dbHdr.cbFolder );
            while( (WORD)cRead == dbHdr.cbFolder )
               {
               char szFileName[ _MAX_PATH ];
               register UINT c;

               /* Delete the CNF file.
                */
               wsprintf( szFileName, "%s.CNF", clItem.szFileName );
               Amdb_DeleteFile( szFileName );

               /* Delete the CXN file.
                */
               wsprintf( szFileName, "%s.CXN", clItem.szFileName );
               Amdb_DeleteFile( szFileName );

               /* Delete the CNO file.
                */
               wsprintf( szFileName, "%s.CNO", clItem.szFileName );
               Amdb_DeleteFile( szFileName );

               /* Loop for each topic and delete the files.
                */
               for( c = 0; c < clItem.cTopics; ++c )
                  {
                  TLITEM tlItem;
            
                  /* First read from the global file.
                   */
                  cRead = Amfile_Read( fh, &tlItem, dbHdr.cbTopic );
                  if( (WORD)cRead != dbHdr.cbTopic )
                     break;

                  /* Now delete the THD, TXT and ATC files for this
                   * topic.
                   */
                  wsprintf( szFileName, "%s.THD", tlItem.szFileName );
                  Amdb_DeleteFile( szFileName );
                  wsprintf( szFileName, "%s.TXT", tlItem.szFileName );
                  Amdb_DeleteFile( szFileName );
                  wsprintf( szFileName, "%s.ATC", tlItem.szFileName );
                  Amdb_DeleteFile( szFileName );
                  }
               cRead = Amfile_Read( fh, &clItem, dbHdr.cbFolder );
               }
            }
      Amfile_Close( fh );

      /* Finally commit hari-kiri.
       */
      Amdb_DeleteFile( pszFilename );
      }
}
