/* FILECOPY.C - Copy a file
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

#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include "amuser.h"

#define CHUNKSIZE  32000   // This must not exceed 32K

/* This function copies a file from the source to the destination.
 */
BOOL WINAPI EXPORT Amfile_Copy( LPSTR szSource, LPSTR szDest )
{
   HNDFILE hFileSource;
   BOOL fSuccess;

   /* Open source file for reading
    */
   fSuccess = FALSE;
   hFileSource = Amfile_Open( szSource, AOF_READ );
   if( HNDFILE_ERROR != hFileSource )
      {
      HNDFILE hFileDest;
      WORD wDate;
      WORD wTime;

      /* Get the source file date/time.
       */
      Amfile_GetFileTime( hFileSource, &wDate, &wTime );

      /* Open destination file for writing
       */
      hFileDest = Amfile_Create( szDest, 0 );
      if( HNDFILE_ERROR != hFileDest )
         {
         LPSTR gPtr;

         /* Create a buffer to copy through
          */
         INITIALISE_PTR(gPtr);
         if( fNewMemory( &gPtr, CHUNKSIZE + 10 ) )
            {
            WORD wBytesRead;

            /* Since Amfile_Read and Amfile_Write can not handle a buffer greater
             * than 32K, we need to read the file in chunks.  Hence,
             * loop until all the chunks have been copied.  On the last
             * loop, wBytesRead will be less than CHUNKSIZE, thus the
             * loop will fall out.
             */
            do {
               wBytesRead = Amfile_Read( hFileSource, gPtr, (WORD)CHUNKSIZE );
               fSuccess = (UINT)-1 != wBytesRead;
               if( fSuccess )
                  fSuccess = Amfile_Write( hFileDest,  gPtr, wBytesRead ) == wBytesRead;
               }
            while( fSuccess && wBytesRead == CHUNKSIZE );
            FreeMemory( &gPtr );
            }
         Amfile_Close( hFileSource );
         }
      Amfile_SetFileTime( hFileDest, wDate, wTime );
      Amfile_Close( hFileDest );
      }
   return( fSuccess );
}
