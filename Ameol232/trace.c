/* TRACE.C - Trace debug stuff.
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

#ifdef _DEBUG

#define  THIS_FILE   __FILE__

#include "main.h"
#include "amlib.h"
#include <stdio.h>
#include "trace.h"
#include <string.h>

void FASTCALL TraceOutputDebug( LPCSTR );
void FASTCALL TraceLogFile( LPCSTR );

/* This function writes a string to either the debug terminal or
 * the TRACE.LOG file depending on global settings.
 */
void FASTCALL TraceDebug( LPCSTR lpszOut )
{
   if( fTraceOutputDebug )
      TraceOutputDebug( lpszOut );
   if( fTraceLogFile )
      TraceLogFile( lpszOut );
}

/* This function writes a string to the debug terminal
 */
void FASTCALL TraceOutputDebug( LPCSTR lpszOut )
{
   char szOutLF[512];

   ASSERT( strlen( lpszOut ) + 2 < 256 );
   wsprintf( szOutLF, "%s\r\n", lpszOut );
   OutputDebugString( szOutLF );
}

/* This function writes a string to the TRACE.LOG file.
 */
void FASTCALL TraceLogFile( LPCSTR lpszOut )
{
   FILE * pFile = NULL;
   char szOutLF[512];

   ASSERT( strlen( lpszOut ) + 2 < 256 );

   /* Open the trace log file for appending.
    */
   pFile = fopen( "trace.log", "a+b" );

   /* I've put this code in because I'm assuming that you can
    * only open a file for appending if it already exists.
    */ 
   if( pFile == NULL )
      pFile = fopen( "trace.log", "wb" );

   /* If we've got a trace log file handle, write the string
    * to the file.
    */
   if( pFile )
      {
      wsprintf( szOutLF, "%s\r\n", lpszOut );
      if( -1 == fprintf( pFile, szOutLF ) )
         {
         wsprintf( szOutLF, "Cannot write to file trace.log: %s\r\n", lpszOut );
         OutputDebugString( szOutLF );
         }
      if( fclose( pFile ) == EOF )
         OutputDebugString( "Could not close file trace.log\r\n" );
      }
   else
      OutputDebugString( "Could not open file trace.log\r\n" );

}
#endif
