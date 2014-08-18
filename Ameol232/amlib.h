/* AMLIB.H - Ameol general purpose library
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

#ifndef _INC_AMLIB

#ifndef WIN32
#define  USE_WINHEAP
#endif

#include "pdefs.h"

#define INITIALISE_PTR(npDir)    (npDir) = NULL

/* HUGE.C */
#ifndef WIN32
DWORD FASTCALL hstrlen( HPSTR );
HPSTR FASTCALL hstrcat( HPSTR, HPSTR );
HPSTR FASTCALL hmemset( HPSTR, int, DWORD );
HPSTR FASTCALL hstrcpy( HPSTR, HPSTR );
int FASTCALL hstrcmpi( HPSTR, HPSTR );
HPSTR FASTCALL hmemmove( HPSTR, HPSTR, DWORD );
#else
#define hstrlen(h)      strlen((h))
#define hstrcat(s,d)    strcat((s),(d))
#define hmemset(s,c,d)  memset((s),(c),(d))
#define hstrcpy(s,d)    strcpy((s),(d))
#define hstrcmpi(s,d)   _strcmpi((s),(d))
#define hmemmove(s,d,b) memmove((s),(d),(b))
#endif

/* GS.C */
char * FASTCALL GS( UINT );
LPCSTR FASTCALL GINTRSC( LPCSTR );

/* STRS.C */
void FASTCALL QuickReplace(LPSTR pSource, LPSTR pDest, LPSTR pSearchFor, LPSTR pReplaceWith); // 20060228 - 2.56.2049.06
BOOL FASTCALL CheckForBlinkErrors(LPSTR, LPSTR, int); // !!SM!!
int FASTCALL strcmpiwild(LPSTR, LPSTR, BOOL ); //!!SM!!
int FASTCALL Hstrcmpiwild(LPSTR, HPSTR, BOOL ); //!!SM!!
int FASTCALL FStrMatch( LPSTR, LPSTR, BOOL, BOOL );
int FASTCALL HStrMatch( HPSTR, LPSTR, BOOL, BOOL );
void FASTCALL StripLeadingTrailingChars( char *, char );
void FASTCALL StripLeadingChars(char *, char); // !!SM!! 2.56.2055
void FASTCALL StripTrailingChars( char * , char ); // !!SM!! 2.55.2032
void FASTCALL StripLeadingTrailingSpaces( char * );
void FASTCALL StripLeadingTrailingQuotes( char * );
void FASTCALL StripCharacter( char *, char );
LPSTR FASTCALL FormatThousands( DWORD, LPSTR );
void FASTCALL FitPathString( HDC, LPSTR, UINT );
LPSTR FASTCALL MakeCompactString( LPSTR );
void FASTCALL UncompactString( LPSTR );
LPSTR FASTCALL HpLpStrCopy( HPSTR );
HPSTR FASTCALL ConcatenateStrings( HPSTR, HPSTR );
char * FASTCALL stristr( char *, char * );
void FASTCALL StripBinaryCharacter( char *, int, char );
void FASTCALL StripBinaryCharacter32( char *, LONG , char ); //!!SM!! 2042
void FASTCALL StripNonPrintCharacters( char *, int );
void FASTCALL StoreTitle( LPSTR, LPSTR );
int FASTCALL RawLineLength( LPSTR );
char * FASTCALL AdvanceLine( char * );
void FASTCALL StrExpandPrefix( char *, char *, int );
void FASTCALL StrStripPrefix( char *, char *, int );
DWORD FASTCALL CountSize( HPSTR );

/* String buffer functions
 */
typedef struct tagSTRBUF {
   HPSTR hpBufText;
   DWORD dwBufLength;
   DWORD dwBufSize;
} STRBUF;

BOOL FASTCALL StrBuf_Init( STRBUF FAR * );
BOOL FASTCALL StrBuf_AppendLine( STRBUF FAR *, char * );
BOOL FASTCALL StrBuf_AppendText( STRBUF FAR *, char *, BOOL fNewline );
BOOL FASTCALL StrBuf_PrependLine( STRBUF FAR *, char * );
DWORD FASTCALL StrBuf_GetBufferSize( STRBUF FAR * );
HPSTR FASTCALL StrBuf_GetBuffer( STRBUF FAR * );

int ExtractToken(char * source, char * dest, int token, char sep);

#define _INC_AMLIB
#endif
