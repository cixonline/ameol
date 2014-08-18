/* PRINT.H - Print interface definitions
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

#ifndef _H_PRINT

#include <commdlg.h>

#define  PR_PORTRAIT    0
#define  PR_LANDSCAPE   1

typedef struct tagPRINTSTRUCT {
   HWND hwnd;           /* Handle of owner window */
   int dy;              /* Height of one line */
   HDC hDC;             /* DC of the printer device */
   int nPage;           /* Number of page being printed */
   int x, y;            /* Current page x/y coords in pixels */
   int x1, y1;          /* Top left of page, in pixels */
   int x2, y2;          /* Bottom right of page, in pixels */
   int cyHeader;        /* Height of header */
   int cyFooter;        /* Height of footer */
   HFONT hCurFont;      /* Font being used for printing */
   HFONT hOldFont;      /* Saved font */
} PRINTSTRUCT;

typedef struct tagPRINTSTRUCT FAR * HPRINT;

#define  PFON_DEFAULT      0
#define  PFON_WINDOW       1
#define  PFON_PRINTERFONT  2

HPRINT FASTCALL BeginPrint( HWND, char *, LOGFONT FAR * );
BOOL FASTCALL EndPrint( HPRINT );
BOOL FASTCALL PrintLine( HPRINT, LPSTR );
BOOL FASTCALL BeginPrintPage( HPRINT );
BOOL FASTCALL EndPrintPage( HPRINT );
void FASTCALL ResetPageNumber( HPRINT );
BOOL FASTCALL PrintPage( HPRINT, HPSTR );
int FASTCALL GetPrinterName( LPSTR, int );
void FASTCALL SetOrientation( void );
void FASTCALL PrintSelectFont( HPRINT, LOGFONT FAR * );

int FASTCALL DrawHeader( HDC, RECT *, PRINTSTRUCT FAR *, LPSTR, LPSTR, LPSTR, BOOL );
int FASTCALL DrawFooter( HDC, RECT *, PRINTSTRUCT FAR *, LPSTR, LPSTR, LPSTR, BOOL );

#define _H_PRINT
#else
#error print.h already included!
#endif
