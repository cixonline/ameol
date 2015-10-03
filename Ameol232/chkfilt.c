/* CHKFILT.C - Check and restores import/export filters
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
#include <direct.h>
#include <io.h>
#include <string.h>
#include <dos.h>
#include "amlib.h"

static BOOL fInstall = FALSE;

void FASTCALL CheckFilters( void )
{
   LPSTR lpszExporters;
   LPSTR lpszImporters;

   INITIALISE_PTR(lpszExporters);
   INITIALISE_PTR(lpszImporters);

   /* Fill with a list of export filters from
    * the [Export] section of AMEOL2.INI
    */
   if( fNewMemory( &lpszExporters, 16000 ) )
      {
      LPSTR lp;

      Amuser_GetLMString( szExport, NULL, "", lpszExporters, 16000 );
      lp = lpszExporters;
      if( !*( lp ) )
         fInstall = TRUE;
      FreeMemory( &lpszExporters );
      }

   /* Fill with a list of export filters from
    * the [Import] section of AMEOL2.INI
    */
   if( fNewMemory( &lpszImporters, 16000 ) )
      {
      LPSTR lp;

      Amuser_GetLMString( szImport, NULL, "", lpszImporters, 16000 );
      lp = lpszImporters;
      if( !*( lp ) )
         fInstall = TRUE;
      FreeMemory( &lpszImporters );
      }

   if( fInstall )
   {
      /* Export */
      Amuser_WriteLMString( szExport, "Address Book", "Ameol232.exe|Amaddr_CSVExport" );
      Amuser_WriteLMString( szExport, "CIX Scratchpad", "Ameol232.exe|CIXExport" );
      Amuser_WriteLMString( szExport, "Text File", "Ameol232.exe|TextFileExport" );

      /* Import */
      Amuser_WriteLMString( szImport, "Address Book", "Ameol232.exe|Amaddr_Import" );
      Amuser_WriteLMString( szImport, "Ameol Import Wizard", "Ameol232.exe|AmdbImportWizard" );
      Amuser_WriteLMString( szImport, "Ameol Scratchpad", "Ameol232.exe|AmeolImport" );
      Amuser_WriteLMString( szImport, "CIX Conferences List", "Ameol232.exe|CIXList_Import" );
      Amuser_WriteLMString( szImport, "CIX Newsgroups List", "Ameol232.exe|UsenetList_Import" );
      Amuser_WriteLMString( szImport, "CIX Scratchpad", "Ameol232.exe|CIXImport" );
      Amuser_WriteLMString( szImport, "CIX Users List", "Ameol232.exe|UserList_Import" );
   }
}
