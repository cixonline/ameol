/* AMIMPRT.C - Ameol Address Book importer
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
#include "palrc.h"
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "amaddr.h"
#include "amaddri.h"
#include "lbf.h"
#include "ini.h"

#define  THIS_FILE   __FILE__

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static char sz[ _MAX_PATH ];              /* Import file name */

extern LPADDRBOOK lpGlbAddrBook;          /* Global address book handle */

/* Old style Ameol address book structure.
 */
#pragma pack(1)
typedef struct tagA1ADDRINFO {
   char szCixName[ 100 ];
   char szFullName[ 81 ];
   char szComment[ 64 ];
} A1ADDRINFO;
#pragma pack()

typedef struct tagIDTABLE {
   HADDRBOOK lpAddrBook;      /* Handle of entry */
   WORD cGrpArray;            /* If entry is a group, this is the number of members in the group */
   WORD FAR * pGrpArray;      /* Pointer to the group member IDs */
} IDTABLE;

/* List of import extensions.
 */
static char strFilters[] = "Ameol Address Book (*.dat)\0*.dat\0All Files (*.*)\0*.*\0\0";
static char strCIMFilters[] = "CompuServe Address Book (*.dat)\0*.dat\0All Files (*.*)\0*.*\0\0";
static char strCSVFilters[] = "CSV Address Book (*.txt)\0*.txt\0CSV Address Book with Groups (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
static char strCSVFilters2[] = "CSV Address Book (*.txt)\0*.txt\0"
                       "CSV Address Book with Groups (*.txt)\0*.txt\0"
                       "vCard Address Book - use for CIXonline (*.vcf)\0*.vcf\0"
                       "All Files (*.*)\0*.*\0\0"; // !!SM!! 2045
static char strCSVFilters3[] = "CSV Address Book (*.txt)\0*.txt\0"
                       "CSV Address Book with Groups (*.txt)\0*.txt\0"
                       "vCard Address Book - use for CIXonline (*.vcf)\0*.vcf\0"
                       "All Files (*.*)\0*.*\0\0"; // !!SM!! 2045

/* Full list of import extensions.
 */
static char strAllFilters[] =
   "Ameol 1.x Address Book (addrbook.dat)\0addrbook.dat\0"
   "Ameol 2.x Address Book (*.aab)\0*.aab\0"
   "WinCIM/DOSCIM Address Book (addrbook.dat)\0addrbook.dat\0"
   "Virtual Access Address Book (address)\0address\0"
//    "VCard with All Contacts (*.vcf)\0*.vcf\0" 
   "CSV Address Book (*.txt)\0*.txt\0"
   "CSV Address Book with Groups (*.txt)\0*.txt\0"
   "All Files (*.*)\0*.*\0\0";

static char szImportLogFile[] = "CIMIMPRT.LOG";

#define  AB_VERSION        0xF45F

BOOL FASTCALL Amaddr_RealImport( HWND, LPSTR, BOOL );
BOOL FASTCALL ReadPascalString( HNDFILE, LPSTR, LPBYTE );
BOOL FASTCALL YieldToSystem( void );
BOOL FASTCALL CvtCIMAddrToInternetAddr( char *, char * );
void FASTCALL FillGroupArray( IDTABLE FAR *, int );
char * FASTCALL ExportAddrField( char *, char * );

BOOL EXPORT CALLBACK AddrImport( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddrImport_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AddrImport_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL AddrImport_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT WINAPI Amaddr_AmeolImport( HWND, LPSTR );
void EXPORT WINAPI Amaddr_CSVImport( HWND, LPSTR );
void EXPORT WINAPI Amaddr_CSVGroupImport( HWND, LPSTR );
void EXPORT WINAPI Amaddr_CIMImport( HWND, LPSTR );
void EXPORT WINAPI Amaddr_VAImport( HWND, LPSTR );
BOOL EXPORT WINAPI Amaddr_Ameol2Import( HWND, char * );
void EXPORT WINAPI Amaddr_AmeolExport( HWND, LPSTR ); 
void EXPORT WINAPI Amaddr_CSVGroupExport( HWND, LPSTR );
void EXPORT WINAPI Amaddr_VCardExport( HWND, LPSTR ); // !!SM!! 2045

/* Import a CSV address book
 */
void EXPORT WINAPI Amaddr_Import( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   DWORD nFilterIndex;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL != lpszFilename )
      nFilterIndex = 1;
   else
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "ADDRBOOK.DAT" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strAllFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return;
      lpszFilename = sz;
      nFilterIndex = ofn.nFilterIndex;
      }

   /* Switch based on filter index.
    */
   switch( nFilterIndex )
      {
      case 1:  Amaddr_AmeolImport( hwnd, lpszFilename ); break;
      case 2:  Amaddr_Ameol2Import( hwnd, lpszFilename ); break;
      case 3:  Amaddr_CIMImport( hwnd, lpszFilename ); break;
      case 4:  Amaddr_VAImport( hwnd, lpszFilename ); break;
      case 5:  Amaddr_CSVImport( hwnd, lpszFilename ); break;
      case 6:  Amaddr_CSVGroupImport( hwnd, lpszFilename ); break;
      case 7:  Amaddr_CSVImport( hwnd, lpszFilename ); break;
      }
}

/* Import a CSV address book
 */
void EXPORT WINAPI Amaddr_CSVImport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "ADDRBOOK.TXT" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCSVFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "CSV Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Read the CSV address book format.
    * Each line in the address book is a CSV separated entry
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) != HNDFILE_ERROR )
      {
      DWORD dwFileSize;
      DWORD dwReadSoFar;
      HWND hwndGauge;
      WORD wOldCount;
      int cItems;
      LPLBF hlbf;
      HWND hDlg;

      /* We'll use the file size as the progress meter.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwReadSoFar = 0L;
      wOldCount = 0;

      /* Open the import progress dialog if the file is
       * more than 4K long.
       */
      hDlg = NULL;
      hwndGauge = NULL;
      if( dwFileSize > 4096 )
         {
         hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRIMPORT), AddrImport, 0L );
         EnableWindow( ip.hwndFrame, FALSE );
         UpdateWindow( ip.hwndFrame );
         hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );

         /* Show filename.
          */
         SetDlgItemText( hDlg, IDD_FILENAME, lpszFilename );

         /* Set the progress bar gauge range.
          */
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         }

      /* Initialise
       */
      cItems = 0;
      if( hlbf = Amlbf_Open( fh, AOF_READ ) )
         {
         char sz[ 256 ];

         /* Read each line from the default book and add it to
          * the address book.
          */
         while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
            {
            char * pszCixName;
            char * pszFullName;
            char * pszComment;

            /* Update the progress gauge.
             */
            if( NULL != hDlg )
               {
               WORD wCount;

               wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
               if( wCount != wOldCount )
                  {
                  SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                  wOldCount = wCount;
                  }
               }

            /* Yield to the system.
             */
            if( !YieldToSystem() )
               break;

            /* Parse the line we have just read.
             */
            if( NULL != ( pszCixName = MakeCompactString( sz ) ) )
               {
               pszFullName = pszCixName + strlen( pszCixName ) + 1;
               pszComment = pszFullName + strlen( pszFullName ) + 1;
               StripLeadingTrailingQuotes( pszFullName );
               StripLeadingTrailingQuotes( pszComment );
         
               if (lstrlen(pszCixName) >= AMLEN_INTERNETNAME) 
                  pszCixName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2044
               if (lstrlen(pszFullName) >= AMLEN_FULLCIXNAME) 
                  pszFullName[ AMLEN_FULLCIXNAME ] = '\0'; // !!SM 2044
               if (lstrlen(pszComment) >= AMLEN_COMMENT) 
                  pszComment[AMLEN_COMMENT] = '\0';        // !!SM 2044

//             pszCixName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2.55
//             pszFullName[ AMLEN_FULLCIXNAME ] = '\0'; // !!SM 2.55
//             pszComment[AMLEN_COMMENT] = '\0';        // !!SM 2.55

               if( *pszCixName && *pszFullName )
                  {
                  ADDRINFO addrinfo;

                  strcpy( addrinfo.szCixName, pszCixName );
                  strcpy( addrinfo.szFullName, pszFullName );
                  strcpy( addrinfo.szComment, pszComment );
                  Amaddr_SetEntry( NULL, &addrinfo );
                  ++cItems;
                  }
               FreeMemory( &pszCixName );
               }
            }
         Amlbf_Close( hlbf );
         }

      /* End of import.
       */
      if( NULL != hDlg )
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
      Amaddr_CommitChanges();

      /* All done. Clean up.
       */
      if( NULL != hDlg )
         {
         EnableWindow( ip.hwndFrame, TRUE );
         DestroyWindow( hDlg );
         }

      /* Show how many entries were imported.
       */
      if( 0 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1174) );
      else if( 1 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1175) );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1176), cItems );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }
   else
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
}

/* Import a CSV address book with groups
 */
void EXPORT WINAPI Amaddr_CSVGroupImport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "ADDRBKG.TXT" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCSVFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "CSV Address Book with Groups";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Read the CSV address book format.
    * Each line in the address book is a CSV separated entry
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) != HNDFILE_ERROR )
      {
      DWORD dwFileSize;
      DWORD dwReadSoFar;
      HWND hwndGauge;
      WORD wOldCount;
      int cItems;
      LPLBF hlbf;
      HWND hDlg;
      char *pdest;

      /* We'll use the file size as the progress meter.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwReadSoFar = 0L;
      wOldCount = 0;

      /* Open the import progress dialog if the file is
       * more than 4K long.
       */
      hDlg = NULL;
      hwndGauge = NULL;
      if( dwFileSize > 4096 )
         {
         hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRIMPORT), AddrImport, 0L );
         EnableWindow( ip.hwndFrame, FALSE );
         UpdateWindow( ip.hwndFrame );
         hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );

         /* Show filename.
          */
         SetDlgItemText( hDlg, IDD_FILENAME, lpszFilename );

         /* Set the progress bar gauge range.
          */
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         }

      /* Initialise
       */
      cItems = 0;
      if( hlbf = Amlbf_Open( fh, AOF_READ ) )
         {
         char sz[ 256 ];

         /* Read each line from the default book and add it to
          * the address book.
          */
         while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
            {
            char * pszCixName;
            char * pszFullName;
            char * pszComment;
            BOOL fBroken = TRUE;

            /* Update the progress gauge.
             */
            if( NULL != hDlg )
               {
               WORD wCount;

               wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
               if( wCount != wOldCount )
                  {
                  SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                  wOldCount = wCount;
                  }
               }

            /* Yield to the system.
             */
            if( !YieldToSystem() )
               break;

            /* Parse the line we have just read.
             */
            pdest = strstr( sz, "#$#$Group" );
            if( NULL != pdest )
            {
            char * pszGroupName;
            char szOut[ AMLEN_INTERNETNAME + AMLEN_FULLCIXNAME + AMLEN_COMMENT ];
            int count = 0;


            pszGroupName = strchr( sz, '\'' );
            if( *pszGroupName == '\'' )
               pszGroupName++;
            while( ( pszGroupName != NULL ) && *pszGroupName && *pszGroupName != '\'' )
            {
               szOut[ count ] = *pszGroupName++;
               count++;
            }
            szOut[ count ] = '\0';
            if( strlen( szOut ) > 0 )
            {
               Amaddr_DeleteGroup( szOut );
               Amaddr_CreateGroup( szOut );
               ++cItems;
               while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
               {
                  pdest = strstr( sz, "^&^&Group" );
                  if( NULL != pdest )
                  {
                     fBroken = Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad );
                     break;
                  }
               if( NULL != ( pszCixName = MakeCompactString( sz ) ) )
                  {
                  pszFullName = pszCixName + strlen( pszCixName ) + 1;
                  pszComment = pszFullName + strlen( pszFullName ) + 1;
                  StripLeadingTrailingQuotes( pszFullName );
                  StripLeadingTrailingQuotes( pszComment );
                  if( *pszCixName && *pszFullName )
                     {
                     ADDRINFO addrinfo;

                     if (lstrlen(pszCixName) >= AMLEN_INTERNETNAME) 
                        pszCixName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2044
                     if (lstrlen(pszFullName) >= AMLEN_FULLCIXNAME) 
                        pszFullName[ AMLEN_FULLCIXNAME ] = '\0'; // !!SM 2044
                     if (lstrlen(pszComment) >= AMLEN_COMMENT) 
                        pszComment[AMLEN_COMMENT] = '\0';        // !!SM 2044

                     strcpy( addrinfo.szCixName, pszCixName );
                     strcpy( addrinfo.szFullName, pszFullName );
                     strcpy( addrinfo.szComment, pszComment );
                     Amaddr_SetEntry( NULL, &addrinfo );
                     Amaddr_AddToGroup( szOut, addrinfo.szFullName );
                     }
                  FreeMemory( &pszCixName );
                  }
               }
            }
            }
            if( fBroken )
            {
            if( NULL != ( pszCixName = MakeCompactString( sz ) ) )
               {
               pszFullName = pszCixName + strlen( pszCixName ) + 1;
               pszComment = pszFullName + strlen( pszFullName ) + 1;
               StripLeadingTrailingQuotes( pszFullName );
               StripLeadingTrailingQuotes( pszComment );
               if( *pszCixName && *pszFullName )
                  {
                  ADDRINFO addrinfo;

                  if (lstrlen(pszCixName) >= AMLEN_INTERNETNAME) 
                     pszCixName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2044
                  if (lstrlen(pszFullName) >= AMLEN_FULLCIXNAME) 
                     pszFullName[ AMLEN_FULLCIXNAME ] = '\0'; // !!SM 2044
                  if (lstrlen(pszComment) >= AMLEN_COMMENT) 
                     pszComment[AMLEN_COMMENT] = '\0';        // !!SM 2044
                  strcpy( addrinfo.szCixName, pszCixName );
                  strcpy( addrinfo.szFullName, pszFullName );
                  strcpy( addrinfo.szComment, pszComment );
                  Amaddr_SetEntry( NULL, &addrinfo );
                  ++cItems;
                  }
               FreeMemory( &pszCixName );
               }
            }
            }
         Amlbf_Close( hlbf );
         }

      /* End of import.
       */
      if( NULL != hDlg )
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
      Amaddr_CommitChanges();

      /* All done. Clean up.
       */
      if( NULL != hDlg )
         {
         EnableWindow( ip.hwndFrame, TRUE );
         DestroyWindow( hDlg );
         }

      /* Show how many entries were imported.
       */
      if( 0 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1174) );
      else if( 1 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1175) );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1176), cItems );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }
   else
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
}

UINT CALLBACK OFNHookProc(
  HWND hdlg,      // handle to child dialog window
  UINT uiMsg,     // message identifier
  WPARAM wParam,  // message parameter
  LPARAM lParam   // message parameter
)
{
   OFNOTIFY lpon;

   if (uiMsg == WM_NOTIFY)
   {
      memcpy((LPOFNOTIFY)&lpon, (LPOFNOTIFY)lParam, sizeof(OFNOTIFY)); 

      switch( lpon.hdr.code )
      {
         case CDN_TYPECHANGE:
            {
//             char lpszFilename[255];
               char * l;
               int i;
               LPOFNOTIFY lpon = (LPOFNOTIFY) lParam; 
               switch(lpon->lpOFN->nFilterIndex)
               {
                  case 1:
                  case 2:
                  case 4:
                  {
                     if (strstr(lpon->lpOFN->lpstrFile, ".txt") == NULL) 
                     {
                        i = 0;
                        l = lpon->lpOFN->lpstrFile;
                        while(*l && *l != '.')
                           *l++,i++;
                        lpon->lpOFN->lpstrFile[i]='\x0';
                        strcat(lpon->lpOFN->lpstrFile, ".txt");
                        CommDlg_OpenSave_SetControlText(GetParent(hdlg), 0x480, lpon->lpOFN->lpstrFile) ;
                     }
                     return (1);
                  }
                  case 3:  
                  {
                     if (strstr(lpon->lpOFN->lpstrFile, ".vcf") == NULL) 
                     {
                        l = lpon->lpOFN->lpstrFile;
                        i = 0;
                        while(*l && *l != '.')
                           *l++,i++;
                        lpon->lpOFN->lpstrFile[i]='\x0';
                        strcat(lpon->lpOFN->lpstrFile, ".vcf");
                        CommDlg_OpenSave_SetControlText(GetParent(hdlg), 0x480, lpon->lpOFN->lpstrFile) ;
                     }
                     return (1);
                  }
               } 
            }
      }
   }
   return(0);
}

/* Export a CSV address book
 */
void EXPORT WINAPI Amaddr_CSVExport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   DWORD nFilterIndex = 1;
   OPENFILENAME ofn;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */


   /* Use the current directory.
    */
   ofn.lpstrInitialDir = "";
   strcpy( sz, "Addrbook.txt" );
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwnd;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = strCSVFilters2;
   ofn.lpstrCustomFilter = NULL; 
   ofn.nMaxCustFilter = 0; 
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = sz;
   ofn.nMaxFile = sizeof( sz );
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrTitle = "CSV Address Book";
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_ENABLEHOOK|OFN_EXPLORER ;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = "txt";//NULL; // !!SM!! 2047
   ofn.lCustData = 0;
   ofn.lpfnHook = OFNHookProc;//NULL; // !!SM!! 2047
   ofn.lpTemplateName = 0;
   if( !GetSaveFileName( &ofn ) )
      return;

   lpszFilename = sz;
   nFilterIndex = ofn.nFilterIndex;

   /* Switch based on filter index.
    */
   switch( nFilterIndex )
      {
      case 1:  Amaddr_AmeolExport( hwnd, lpszFilename ); break;
      case 2:  Amaddr_CSVGroupExport( hwnd, lpszFilename ); break;
      case 3:  {
/*          if(ofn.Flags && OFN_EXTENSIONDIFFERENT)
            {
               lpszFilename[ofn.nFileExtension] = '\x0';
               strcat(lpszFilename, "vcf");
               fMessageBox( hwndFrame, 0, lpszFilename, MB_OK|MB_ICONEXCLAMATION );
            }*/
            Amaddr_VCardExport( hwnd, lpszFilename ); break; // !!SM!! 2045
            }
      case 4:  Amaddr_CSVExport( hwnd, lpszFilename ); break;
      }
}

void EXPORT WINAPI Amaddr_AmeolExport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "ADDRBKG.TXT" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCSVFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "CSV Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetSaveFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Create the CSV address book format.
    * Each line in the address book is a CSV separated entry
    */
   if( ( fh = Amfile_Create( lpszFilename, 0 ) ) != HNDFILE_ERROR )
      {
      ADDRINFO addrinfo;
      HADDRBOOK hAdr;
      UINT cWritten;

      hAdr = NULL;
      cWritten = 0;
      while( hAdr = Amaddr_GetNextEntry( hAdr, &addrinfo ) )
         {
         char * pBuf;

         pBuf = lpTmpBuf;
         pBuf = ExportAddrField( pBuf, addrinfo.szCixName );
         *pBuf++ = ',';
         pBuf = ExportAddrField( pBuf, addrinfo.szFullName );
         *pBuf++ = ',';
         pBuf = ExportAddrField( pBuf, addrinfo.szComment );
         *pBuf++ = '\r';
         *pBuf++ = '\n';
         *pBuf = '\0';
         Amfile_Write( fh, lpTmpBuf, strlen(lpTmpBuf) );
         ++cWritten;
         }
      if( 0 == cWritten )
         strcpy( lpTmpBuf, GS(IDS_STR1192) );
      else if( 1 == cWritten )
         strcpy( lpTmpBuf, GS(IDS_STR1193) );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1194), cWritten );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      Amfile_Close( fh );
      }
   else
      FileCreateError( hwnd, sz, FALSE, FALSE );
}

typedef BOOL (FAR PASCAL * SAVEVCARDENTRY)(int, LPSTR, LPSTR, LPSTR, LPSTR);

void EXPORT WINAPI Amaddr_VCardExport( HWND hwnd, LPSTR lpszFilename ) // !!SM!! 2045
{
   INTERFACE_PROPERTIES ip;
   SAVEVCARDENTRY lpImpProc;
   ADDRINFO addrinfo;
   HADDRBOOK hAdr;
   UINT cWritten;

   if (hRegLib < (HINSTANCE)32)
      return;

   if( NULL == ( lpImpProc = (SAVEVCARDENTRY)GetProcAddress( hRegLib, "SaveVCardEntry" ) ) )
      return;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "Addrbook.vcf" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCSVFilters2;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "vCard Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetSaveFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Create the VCard address book format.
    */

   hAdr = NULL;
   cWritten = 0;
   while( hAdr = Amaddr_GetNextEntry( hAdr, &addrinfo ) )
      {
      lpImpProc(cWritten, addrinfo.szFullName, addrinfo.szCixName, addrinfo.szComment, lpszFilename);
      ++cWritten;
      }
   if( 0 == cWritten )
      strcpy( lpTmpBuf, GS(IDS_STR1192) );
   else if( 1 == cWritten )
      strcpy( lpTmpBuf, GS(IDS_STR1193) );
   else
      wsprintf( lpTmpBuf, GS(IDS_STR1194), cWritten );
   fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
}

/* Export a CSV address book with groups
 */
void EXPORT WINAPI Amaddr_CSVGroupExport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* Create the CSV address book format.
    * Each line in the address book is a CSV separated entry
    */
   if( ( fh = Amfile_Create( lpszFilename, 0 ) ) != HNDFILE_ERROR )
      {
//    ADDRINFO addrinfo;
//    HADDRBOOK hAdr;
      UINT cWritten;
      LPADDRBOOK lpAddrBook;

//    hAdr = NULL;
      cWritten = 0;
      for( lpAddrBook = lpAddrBookFirst; lpAddrBook; lpAddrBook = lpAddrBook->lpNext )
         {
         char * pBuf;

         if( !lpAddrBook->fGroup )
         {
         pBuf = lpTmpBuf;
         pBuf = ExportAddrField( pBuf, lpAddrBook->szCixName );
         *pBuf++ = ',';
         pBuf = ExportAddrField( pBuf, lpAddrBook->szFullName );
         *pBuf++ = ',';
         pBuf = ExportAddrField( pBuf, lpAddrBook->szComment );
         *pBuf++ = '\r';
         *pBuf++ = '\n';
         *pBuf = '\0';
         Amfile_Write( fh, lpTmpBuf, strlen(lpTmpBuf) );
         ++cWritten;
         }
         else
         {
            register WORD c;
            LPADDRBOOKGROUP lpGroup = (LPADDRBOOKGROUP)lpAddrBook;


            wsprintf( lpTmpBuf, "#$#$Group \'%s\' starts**\r\n", lpGroup->szGroupName );
            Amfile_Write( fh, lpTmpBuf, strlen(lpTmpBuf) );

            for( c = 0; c < lpGroup->cGroup; ++c )
            {
               pBuf = lpTmpBuf;
               pBuf = ExportAddrField( pBuf, lpGroup->lpAddrBook[ c ]->szCixName );
               *pBuf++ = ',';
               pBuf = ExportAddrField( pBuf, lpGroup->lpAddrBook[ c ]->szFullName );
               *pBuf++ = ',';
               pBuf = ExportAddrField( pBuf, lpGroup->lpAddrBook[ c ]->szComment );
               *pBuf++ = '\r';
               *pBuf++ = '\n';
               *pBuf = '\0';
               Amfile_Write( fh, lpTmpBuf, strlen(lpTmpBuf) );
            }
            wsprintf( lpTmpBuf, "^&^&Group \'%s\' ends**\r\n", lpGroup->szGroupName );
            ++cWritten;
            Amfile_Write( fh, lpTmpBuf, strlen(lpTmpBuf) );
         }
         }
      if( 0 == cWritten )
         strcpy( lpTmpBuf, GS(IDS_STR1192) );
      else if( 1 == cWritten )
         strcpy( lpTmpBuf, GS(IDS_STR1193) );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1194), cWritten );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      Amfile_Close( fh );
      }
   else
      FileCreateError( hwnd, sz, FALSE, FALSE );
}

/* This function writes a text string to an INI string field. If
 * the string contains commas, it is embedded in quotes.
 */
char * FASTCALL ExportAddrField( char * psz, char * pszText )
{
   if( strchr( pszText, ',' ) || strchr( pszText, ' ' ) )
      {
      *psz++ = '"';
      while( *pszText )
         *psz++ = *pszText++;
      *psz++ = '"';
      }
   else
      {
      while( *pszText )
         *psz++ = *pszText++;
      }
   *psz = '\0';
   return( psz );
}

/* This function is the CompuServe address-book import entry point.
 */
void EXPORT WINAPI Amaddr_CIMImport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Look for \CSERVE\SUPPORT\ADDRBOOK.DAT and use that
       * directory if found.
       */
      ofn.lpstrInitialDir = "";
      if( Amfile_QueryFile( "\\CSERVE\\SUPPORT\\ADDRBOOK.DAT" ) )
         ofn.lpstrInitialDir = "\\CSERVE\\SUPPORT";
      strcpy( sz, "ADDRBOOK.DAT" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCIMFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "CompuServe Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Read the CIM address book format.
    * Look for the version number at the start of the file to distinguish
    * between this and the older format.
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) != HNDFILE_ERROR )
      {
      HNDFILE fhImp;
      DWORD dwFileSize;
      DWORD dwReadSoFar;
      HWND hwndGauge;
      BYTE bVersion;
      WORD wOldCount;
      int cInvalidItems;
      int cItems;
      HWND hDlg;

      /* Create the CIMIMPRT.LOG file.
       */
      fhImp = Amfile_Create( szImportLogFile, 0 );

      /* We'll use the file size as the progress meter.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwReadSoFar = 0L;
      wOldCount = 0;

      /* Open the import progress dialog if the file is
       * more than 4K long.
       */
      hDlg = NULL;
      hwndGauge = NULL;
      if( dwFileSize > 4096 )
         {
         hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRIMPORT), AddrImport, 0L );
         EnableWindow( ip.hwndFrame, FALSE );
         UpdateWindow( ip.hwndFrame );
         hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );

         /* Show filename.
          */
         SetDlgItemText( hDlg, IDD_FILENAME, lpszFilename );

         /* Set the progress bar gauge range.
          */
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         }

      /* Read the first byte of the file for the version number. If it is 1, this
       * is a DOSCIM 1.x address book. If it is 2, it is a WinCIM address book.
       */
      cInvalidItems = 0;
      cItems = 0;
      if( Amfile_Read( fh, (LPSTR)&bVersion, sizeof(BYTE) ) == sizeof(BYTE) )
         {
         char szName[ 256 ];
         char szTrueAddr[ 256 ];
         char szAddr[ 256 ];
         char szComment[ 256 ];

         if( bVersion == 0x01 )
            {
            register int c;
            BYTE bCount;

            /* First byte after version number is the number of records. Note
             * that all strings are in Pascal format.
             */
            dwReadSoFar += Amfile_Read( fh, (LPSTR)&bCount, sizeof(BYTE) );
            for( c = 0; c < bCount; ++c )
               {
               WORD wCount;
               BYTE cRead;

               /* Update the progress gauge.
                */
               if( NULL != hDlg )
                  {
                  wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
                  if( wCount != wOldCount )
                     {
                     SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                     wOldCount = wCount;
                     }
                  }

               /* Yield to the system.
                */
               if( !YieldToSystem() )
                  break;

               /* Read the name.
                */
               if( !ReadPascalString( fh, szName, &cRead ) )
                  break;
               dwReadSoFar += cRead;

               /* Read the address.
                */
               if( !ReadPascalString( fh, szAddr, &cRead ) )
                  break;
               dwReadSoFar += cRead;

               /* Read the comment.
                */
               if( !ReadPascalString( fh, szComment, &cRead ) )
                  break;
               dwReadSoFar += cRead;

               /* The address will be in CIS format. Change it to be in internet
                * format.
                */
               if( CvtCIMAddrToInternetAddr( szAddr, szTrueAddr ) )
                  {
                  ADDRINFO addrinfo;

                  /* Add this entry to the address book.
                   */
                  strcpy( addrinfo.szCixName, szTrueAddr );
                  strcpy( addrinfo.szFullName, szName );
                  strcpy( addrinfo.szComment, szComment );
                  Amaddr_SetEntry( szName, &addrinfo );
                  ++cItems;                  
                  }
               else
                  {
                  wsprintf( sz, GS(IDS_STR1172), szName, szAddr );
                  Amfile_Write( fhImp, sz, strlen(sz) );
                  ++cInvalidItems;
                  }
               }
            }
         else if( bVersion == 0x02 )
            {
            IDTABLE FAR * lpIDTable;
            register UINT c;
            WORD nCount;

            INITIALISE_PTR(lpIDTable);

            /* First word after version number is the number of entries.
             */
            dwReadSoFar += Amfile_Read( fh, (LPSTR)&nCount, sizeof(WORD) );

            /* Allocate an array of HHDLs for each entry.
             */
            if( fNewMemory( &lpIDTable, sizeof(IDTABLE) * nCount ) )
               {
               /* Loop for each entry.
                */
               for( c = 0; c < nCount; ++c )
                  {
                  WORD wID;
                  BOOL fGroup;
                  WORD wCount;
                  BYTE cRead;
   
                  /* Update the progress gauge.
                   */
                  if( NULL != hDlg )
                     {
                     wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
                     if( wCount != wOldCount )
                        {
                        SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                        wOldCount = wCount;
                        }
                     }
   
                  /* Yield to the system.
                   */
                  if( !YieldToSystem() )
                     break;
   
                  /* Read the ID word.
                   */
                  dwReadSoFar += Amfile_Read( fh, (LPSTR)&wID, sizeof(WORD) );
                  fGroup = ( wID & 0x8000 ) ? TRUE : FALSE;
                  wID = ( wID & 0x0FFF ) - 1;

                  /* Clear ID table.
                   */
                  lpIDTable[ wID ].cGrpArray = 0;

                  /* Read the name.
                   */
                  if( !ReadPascalString( fh, szName, &cRead ) )
                     break;
                  dwReadSoFar += cRead;
   
                  /* If this is not a group, the next entry is the address. Otherwise
                   * it is an array of numbers that refer to full entries.
                   */
                  if( !fGroup )
                     {
                     /* Read the address.
                      */
                     if( !ReadPascalString( fh, szAddr, &cRead ) )
                        break;
                     dwReadSoFar += cRead;
   
                     /* Read the comment.
                      */
                     if( !ReadPascalString( fh, szComment, &cRead ) )
                        break;
                     dwReadSoFar += cRead;
   
                     /* The address will be in CIS format. Change it to be in internet
                      * format.
                      */
                     if( CvtCIMAddrToInternetAddr( szAddr, szTrueAddr ) )
                        {
                        ADDRINFO addrinfo;

                        /* Add this entry to the address book.
                         */
                        strcpy( addrinfo.szCixName, szTrueAddr );
                        strcpy( addrinfo.szFullName, szName );
                        strcpy( addrinfo.szComment, szComment );
                        Amaddr_SetEntry( szName, &addrinfo );
   
                        /* Associate this lphl with the specified ID.
                         */
                        lpIDTable[ wID ].lpAddrBook = lpGlbAddrBook;
                        lpIDTable[ wID ].cGrpArray = 0;
                        ++cItems;
                        }
                     else
                        {
                        wsprintf( sz, GS(IDS_STR1172), szName, szAddr );
                        Amfile_Write( fhImp, sz, strlen(sz) );
                        ++cInvalidItems;
                        }
                     }
                  else
                     {
                     register int n;
                     BYTE cMembers;
   
                     /* Read number of group entries.
                      */
                     dwReadSoFar += Amfile_Read( fh, (LPSTR)&cMembers, sizeof(BYTE) );
   
                     /* Create an array of IDs which will be assigned to
                      * this group.
                      */
                     INITIALISE_PTR(lpIDTable[ wID ].pGrpArray);
                     if( !fNewMemory( &lpIDTable[ wID ].pGrpArray, cMembers * sizeof(WORD) ) )
                        break;
                     lpIDTable[ wID ].cGrpArray = cMembers;
   
                     /* For each group entry, read the ID and store it in the
                      * array. We'll fill in the group entries later.
                      */
                     for( n = 0; n < cMembers; ++n )
                        {
                        WORD wMemberID;
   
                        dwReadSoFar += Amfile_Read( fh, (LPSTR)&wMemberID, sizeof(WORD) );
                        lpIDTable[ wID ].pGrpArray[ n ] = wMemberID - 1;
                        }
   
                     /* Now create the group and associate the group lphl with
                      * the specified ID.
                      */
                     Amaddr_CreateGroup( szName );
                     lpIDTable[ wID ].lpAddrBook = lpGlbAddrBook;
   
                     /* Read and discard the comment. We don't currently have comments
                      * with groups, but might do at a later date.
                      */
                     if( !ReadPascalString( fh, szComment, &cRead ) )
                        break;
                     dwReadSoFar += cRead;
                     }
                  }
   
               /* Okay. Now scan the array of IDs and for each group found,
                * create entries based on their IDs.
                */
               nCount -= cInvalidItems;
               for( c = 0; c < nCount; ++c )
                  FillGroupArray( lpIDTable, c );

               /* Free up memory.
                */
               for( c = 0; c < nCount; ++c )
                  if( lpIDTable[ c ].cGrpArray )
                     FreeMemory( &lpIDTable[ c ].pGrpArray );
               FreeMemory( &lpIDTable );
               }
            }
         else
            {
            /* Unknown CIM address book version.
             */
            if( NULL != hDlg )
               {
               EnableWindow( ip.hwndFrame, TRUE );
               DestroyWindow( hDlg );
               hDlg = NULL;
               }
            fMessageBox( hwnd, 0, GS(IDS_STR1171), MB_OK|MB_ICONEXCLAMATION );
            }
         }

      /* End of import.
       */
      if( NULL != hDlg )
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
      Amfile_Close( fhImp );
      Amfile_Close( fh );
      Amaddr_CommitChanges();

      /* All done. Clean up.
       */
      if( NULL != hDlg )
         {
         EnableWindow( ip.hwndFrame, TRUE );
         DestroyWindow( hDlg );
         }

      /* If any errors, notify user.
       */
      if( cInvalidItems )
         {
         wsprintf( sz, GS(IDS_STR1173), cInvalidItems );
         fMessageBox( hwnd, 0, sz, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         {
         char sz[ 256 ];

         /* Show how many entries were imported.
          */
         Amfile_Delete( szImportLogFile );
         if( 0 == cItems )
            strcpy( sz, GS(IDS_STR1174) );
         else if( 1 == cItems )
            strcpy( sz, GS(IDS_STR1175) );
         else
            wsprintf( sz, GS(IDS_STR1176), cItems );
         fMessageBox( hwnd, 0, sz, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   else
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
}

/* This function checks whether lpIDTable[ wID ] is a group and, if so,
 * scans the list of group member IDs and assigns their entry handles to
 * the group lphl.
 */
void FASTCALL FillGroupArray( IDTABLE FAR * lpIDTable, int wID )
{
   if( lpIDTable[ wID ].cGrpArray )
      {
      register WORD n;

      for( n = 0; n < lpIDTable[ wID ].cGrpArray; ++n )
         {
         WORD nGrpArray;
         LPADDRBOOK lpAddrBook;
         LPADDRBOOKGROUP lpGrpEntry;

         /* For each item, get the actual entry handle and
          * extract the physical details. Then create a duplicate
          * in the group.
          */
         nGrpArray = lpIDTable[ wID ].pGrpArray[ n ];
         lpAddrBook = lpIDTable[ nGrpArray ].lpAddrBook;
         lpGrpEntry = lpIDTable[ wID ].lpAddrBook;
         Amaddr_AddToGroup( lpGrpEntry->szGroupName, lpAddrBook->szFullName );
         }
      }
}

/* This function reads a pascal string from the specified file. A pascal
 * string is one where the string length precedes the string. The string
 * cannot be longer than 255 characters.
 */
BOOL FASTCALL ReadPascalString( HNDFILE fh, LPSTR lpStr, LPBYTE lpbRead )
{
   BYTE bLen;

   if( Amfile_Read( fh, (LPSTR)&bLen, sizeof(BYTE) ) == sizeof(BYTE) )
      if( Amfile_Read( fh, lpStr, bLen ) == bLen )
         {
         lpStr[ bLen ] = '\0';
         *lpbRead = bLen + 1;
         return( TRUE );
         }
   *lpbRead = 0;
   return( FALSE );
}


/* This function is the Ameol address-book import entry point
 * with no prompts or feedback.
 */
BOOL EXPORT WINAPI Amaddr_QuietImport( HWND hwnd, LPSTR lpszFilename )
{
   return( Amaddr_RealImport( hwnd, lpszFilename, FALSE ) );
}

/* This function is the Ameol address-book import entry point.
 */
BOOL EXPORT WINAPI Amaddr_AmeolImport( HWND hwnd, LPSTR lpszFilename )
{
   return( Amaddr_RealImport( hwnd, lpszFilename, TRUE ) );
}

/* This function is the Ameol address-book import entry point.
 */
BOOL FASTCALL Amaddr_RealImport( HWND hwnd, LPSTR lpszFilename, BOOL fPrompt )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;
      char szDir[ _MAX_PATH ];

      /* Look for the [Ameol] section in WIN.INI to locate
       * the current Ameol installation.
       */
      GetProfileString( "Ameol", "Setup", "", szDir, sizeof(szDir) );
      if( *szDir )
         strcat( szDir, "\\DATA\\ADDRBOOK" );
      strcpy( sz, "ADDRBOOK.DAT" );
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
      ofn.lpstrInitialDir = szDir;
      ofn.lpstrTitle = "Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return( FALSE );
      lpszFilename = sz;
      }

   /* Read the Ameol 1.21 address book format.
    * Look for the version number at the start of the file to distinguish
    * between this and the older format.
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) != HNDFILE_ERROR )
      {
      DWORD dwFileSize;
      DWORD dwReadSoFar;
      HWND hwndGauge;
      WORD wVersion;
      WORD wOldCount;
      WORD fGroup;
      BOOL fBook21;
      HWND hDlg;
      int cItems;
      char sz[ 256 ];

      /* We'll use the file size as the progress meter.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwReadSoFar = 0L;
      wOldCount = 0;

      /* Open the import progress dialog if the file is
       * more than 4K long.
       */
      hDlg = NULL;
      hwndGauge = NULL;
      if( dwFileSize > 4096 )
         {
         hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRIMPORT), AddrImport, 0L );
         EnableWindow( ip.hwndFrame, FALSE );
         UpdateWindow( ip.hwndFrame );
         hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
   
         /* Show filename.
          */
         SetDlgItemText( hDlg, IDD_FILENAME, lpszFilename );
   
         /* Set the progress bar gauge range.
          */
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         }

      /* Read the first two bytes of the file. If they are AB_VERSION, then
       * this is an Ameol 1.21 address book.
       */
      fBook21 = FALSE;
      cItems = 0;
      if( Amfile_Read( fh, (LPSTR)&wVersion, sizeof( WORD ) ) == sizeof( WORD ) )
         {
         if( wVersion == AB_VERSION )
            {
            dwReadSoFar += 2;
            fBook21 = TRUE;
            }
         else
            Amfile_SetPosition( fh, 0L, ASEEK_BEGINNING );
         }

      /* Loop for the rest of the book.
       */
      while( !fMainAbort && Amfile_Read( fh, (LPSTR)&fGroup, sizeof( WORD ) ) == sizeof( WORD ) )
         {
         WORD wCount;

         /* Update the progress gauge.
          */
         if( NULL != hwndGauge )
            {
            wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
            if( wCount != wOldCount )
               {
               SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
               wOldCount = wCount;
               }
            }

         /* Yield to the system.
          */
         if( !YieldToSystem() )
            break;

         /* Is this a group?
          */
         if( fGroup )
            {
            char szGroupName[ 100 ];
            char szFullName[ 100 ];
            register WORD c;
            int cbGroupName;
            int cbFullName;
            WORD cGroup;
            int cRead;

            /* Bump the gauge.
             */
            dwReadSoFar += sizeof(WORD);

            /* Some sizes.
             */
            cbGroupName = sizeof(szGroupName) - 1;
            cbFullName = sizeof(szFullName) - 1;

            /* We're reading a group, so create the new group.
             */
            if( cbGroupName == ( cRead = Amfile_Read( fh, szGroupName, cbGroupName ) ) )
               {
               dwReadSoFar += cRead;
               if( sizeof(WORD) == ( cRead = Amfile_Read( fh, &cGroup, sizeof( WORD ) ) ) )
                  {
                  dwReadSoFar += cRead;
                  Amaddr_CreateGroup( szGroupName );
                  for( c = 0; !fMainAbort && c < cGroup; ++c )
                     {
                     /* Create the group entry.
                      */
                     if( cbFullName != ( cRead = Amfile_Read( fh, szFullName, cbFullName ) ) )
                        break;
                     dwReadSoFar += cRead;
                     Amaddr_AddToGroup( szGroupName, szFullName );
                     ++cItems;
                     }
                  }
               }
            }
         else
            {
            A1ADDRINFO a1addrinfo;
            char szFaxNo[ 21 ];
            ADDRINFO addrinfo;

            dwReadSoFar += Amfile_Read( fh, a1addrinfo.szCixName, sizeof(a1addrinfo.szCixName)-1 );
            dwReadSoFar += Amfile_Read( fh, a1addrinfo.szFullName, sizeof(a1addrinfo.szFullName)-1 );
            if( fBook21 )
               dwReadSoFar += Amfile_Read( fh, a1addrinfo.szComment, sizeof(a1addrinfo.szComment)-1 );
            else
               {
               dwReadSoFar += Amfile_Read( fh, szFaxNo, sizeof(szFaxNo)-1 );
               addrinfo.szComment[ 0 ] = '\0';
               }
            if( *a1addrinfo.szFullName == '\0' )
               strcpy( a1addrinfo.szFullName, a1addrinfo.szCixName );

            /* Now there is a distinct possibility that an entry with this
             * user name already exists. If so, just update the entries from
             * the address book.
             */
            strcpy( addrinfo.szCixName, a1addrinfo.szCixName );
            strcpy( addrinfo.szFullName, a1addrinfo.szFullName );
            strcpy( addrinfo.szComment, a1addrinfo.szComment );
            Amaddr_SetEntry( a1addrinfo.szFullName, &addrinfo );
            ++cItems;
            }
         }

      /* End of import.
       */
      if( NULL != hDlg )
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
      Amfile_Close( fh );
      Amaddr_CommitChanges();

      /* All done. Clean up.
       */
      if( NULL != hDlg )
         {
         EnableWindow( ip.hwndFrame, TRUE );
         DestroyWindow( hDlg );
         }

      /* Show import complete information only if
       * fPrompt is FALSE.
       */
      if( 0 == cItems )
         {
         fMessageBox( hwnd, 0, GS(IDS_STR1174), MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
      if( fPrompt )
         {
         if( 1 == cItems )
            strcpy( sz, GS(IDS_STR1175) );
         else
            wsprintf( sz, GS(IDS_STR1176), cItems );
         fMessageBox( hwnd, 0, sz, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   else
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
   return( TRUE );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
BOOL EXPORT CALLBACK AddrImport( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = AddrImport_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
LRESULT FASTCALL AddrImport_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AddrImport_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, AddrImport_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddrImport_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
         fMainAbort = TRUE;
         break;
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddrImport_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   fMainAbort = FALSE;
   return( TRUE );
}

/* This function yields to the system to allow other tasks to
 * run. It returns TRUE if okay, or FALSE if a WM_QUIT was seen.
 */
BOOL FASTCALL YieldToSystem( void )
{
   MSG msg;

   if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
      if( msg.message == WM_QUIT )
         {
         PostQuitMessage( msg.wParam );
         return( FALSE );
         }
      else
         {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
         }
   return( TRUE );
}

/* This function converts a CIM address in the format YYYYY,XXX to an
 * internet address in the format YYYYY.XXX@compuserve.com
 */
BOOL FASTCALL CvtCIMAddrToInternetAddr( char * pszCIMAddr, char * pszIPAddr )
{
   /* Skip spaces.
    */
   while( isspace( *pszCIMAddr ) )
      ++pszCIMAddr;

   /* Handle >INTERNET prefix by stripping it out.
    */
   if( strlen( pszCIMAddr ) > 10 )
      if( _strnicmp( pszCIMAddr, ">INTERNET:", 10 ) == 0 || _strnicmp( pszCIMAddr, "INTERNET:", 9 ) == 0 )
         {
         while( *pszCIMAddr != ':' )
            ++pszCIMAddr;
         ++pszCIMAddr;
         while( isspace( *pszCIMAddr ) )
            ++pszCIMAddr;
         strcpy( pszIPAddr, pszCIMAddr );
         return( TRUE );
         }

   /* Any other '>' prefix cannot be handled.
    */
   if( *pszCIMAddr == '>' )
      return( FALSE );

   /* Handle pure CIM address.
    */
   while( *pszCIMAddr )
      {
      if( *pszCIMAddr == ',' )
         *pszIPAddr++ = '.';
      else if( !isdigit( *pszCIMAddr ) )
         return( FALSE );
      else
         *pszIPAddr++ = *pszCIMAddr;
      ++pszCIMAddr;
      }
   *pszIPAddr = '\0';
   strcat( pszIPAddr, "@compuserve.com" );
   return( TRUE );
}

/* Import a Virtual Access address book
 */
void EXPORT WINAPI Amaddr_VAImport( HWND hwnd, LPSTR lpszFilename )
{
   INTERFACE_PROPERTIES ip;
   HNDFILE fh;

   /* Get the frame window handle.
    */
   Amuser_GetInterfaceProperties( &ip );

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      OPENFILENAME ofn;

      /* Use the current directory.
       */
      ofn.lpstrInitialDir = "";
      strcpy( sz, "address" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strCSVFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrTitle = "Virtual Access Address Book";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return;
      lpszFilename = sz;
      }

   /* Read the CSV address book format.
    * Each line in the address book is a CSV separated entry
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) != HNDFILE_ERROR )
      {
      DWORD dwFileSize;
      DWORD dwReadSoFar;
      HWND hwndGauge;
      WORD wOldCount;
      int cItems;
      LPLBF hlbf;
      HWND hDlg;

      /* We'll use the file size as the progress meter.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwReadSoFar = 0L;
      wOldCount = 0;

      /* Open the import progress dialog if the file is
       * more than 4K long.
       */
      hDlg = NULL;
      hwndGauge = NULL;
      if( dwFileSize > 4096 )
         {
         hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRIMPORT), AddrImport, 0L );
         EnableWindow( ip.hwndFrame, FALSE );
         UpdateWindow( ip.hwndFrame );
         hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );

         /* Show filename.
          */
         SetDlgItemText( hDlg, IDD_FILENAME, lpszFilename );

         /* Set the progress bar gauge range.
          */
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         }

      /* Initialise
       */
      cItems = 0;
      if( hlbf = Amlbf_Open( fh, AOF_READ ) )
         {
         char sz[ 256 ];

         /* Read each line from the default book and add it to
          * the address book.
          */
         while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
            {
            char * pszCixName;
            char * pszFullName;
            char * pszComment;

            /* Update the progress gauge.
             */
            if( NULL != hDlg )
               {
               WORD wCount;

               wCount = (WORD)( ( dwReadSoFar * 100 ) / dwFileSize );
               if( wCount != wOldCount )
                  {
                  SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
                  wOldCount = wCount;
                  }
               }

            /* Yield to the system.
             */
            if( !YieldToSystem() )
               break;

            /* Parse the line we have just read.
             */
            pszFullName = sz;
            pszComment = strchr( pszFullName, '\t' );
            if( NULL != pszComment )
               {
               *pszComment++ = '\0';
               pszCixName = strchr( pszComment, '\t' );
               if( NULL != pszCixName )
                  {
                  char szCixName[ 64 ];
                  ADDRINFO addrinfo;

                  /* Fiddle with the e-mail address. Remove the
                   * VA style prefixes and convert them to internet
                   * addresses.
                   */
                  *pszCixName++ = '\0';
                  strcpy( szCixName, pszCixName );
                  if( _strnicmp( szCixName, "cis:", 4 ) == 0 )
                     {
                     char * pszDest;

                     pszCixName = szCixName + 4;
                     pszDest = addrinfo.szCixName;
                     while( *pszCixName && *pszCixName != ',' )
                        *pszDest++ = *pszCixName++;
                     if( *pszCixName != ',' )
                        strcpy( addrinfo.szCixName, szCixName + 4 );
                     else
                        {
                        ++pszCixName;
                        *pszDest++ = '.';
                        while( *pszCixName )
                           *pszDest++ = *pszCixName++;
                        strcpy( pszDest, "@compuserve.com" );
                        }
                     }
                  else if( _strnicmp( szCixName, "ashmount:", 9 ) == 0 )
                     {
                     pszCixName = szCixName + 9;
                     strcpy( addrinfo.szCixName, pszCixName );
                     }
                  else if( _strnicmp( szCixName, "cix:", 4 ) == 0 )
                     {
                     pszCixName = szCixName + 4;
                     strcpy( addrinfo.szCixName, pszCixName );
                     strcat( addrinfo.szCixName, "@" );
                     strcat( addrinfo.szCixName, szCixCoUk );
                     }
                  else
                     strcpy( addrinfo.szCixName, pszCixName );

                  /* Full name may be duplicated because VA sorts on
                   * e-mail address instead.
                   */
                  if( !Amaddr_Find( pszFullName, NULL ) )
                     strcpy( addrinfo.szFullName, pszFullName );
                  else
                     {
                     int cItem;

                     cItem = 1;
                     do
                        wsprintf( addrinfo.szFullName, "%s (%u)", pszFullName, cItem++ );
                     while( Amaddr_Find( addrinfo.szFullName, NULL ) );
                     }

                  /* Now create the address book entry.
                   */
                  strcpy( addrinfo.szComment, pszComment );
                  Amaddr_SetEntry( NULL, &addrinfo );
                  ++cItems;
                  }
               }
            }
         Amlbf_Close( hlbf );
         }

      /* End of import.
       */
      if( NULL != hDlg )
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
      Amaddr_CommitChanges();

      /* All done. Clean up.
       */
      if( NULL != hDlg )
         {
         EnableWindow( ip.hwndFrame, TRUE );
         DestroyWindow( hDlg );
         }

      /* Show how many entries were imported.
       */
      if( 0 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1174) );
      else if( 1 == cItems )
         strcpy( lpTmpBuf, GS(IDS_STR1175) );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1176), cItems );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }
   else
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
}
