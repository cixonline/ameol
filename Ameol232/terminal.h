/* TERMINAL.H - Terminal emulator definitions
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

#ifndef _H_TERMINAL

#include "intcomms.h"
#include "hxfer.h"

#define  DEF_BUFFERSIZE       100
#define  MAX_LOGS             10
#define  MAX_COMBUFSIZE       1024

#define  BLACK       0
#define  RED         1
#define  GREEN       2
#define  CYAN        3
#define  BLUE        4
#define  MAGENTA     5
#define  YELLOW      6
#define  WHITE       7

#define  UNDERLINE   0x08
#define  BOLD        0x80

#define  SetFgAttr(b,c)    ((b)=((b)&0xF8)|(c))
#define  SetBgAttr(b,c)    ((b)=((b)&0x8F)|((c)<<4))
#define  GetFgAttr(b)      (BYTE)((b) & 0x07)
#define  GetBgAttr(b)      (BYTE)(((b) >> 4)&0x07)
#define  SetBold()         (ptwnd->crAttrByte |= BOLD)
#define  SetUnderline()    (ptwnd->crAttrByte |= UNDERLINE)
#define  ClearBold()       (ptwnd->crAttrByte &= ~BOLD)
#define  ClearUnderline()  (ptwnd->crAttrByte &= ~UNDERLINE)
#define  GetBold()         (BOOL)(ptwnd->crAttrByte & BOLD)
#define  GetUnderline()    (BOOL)(ptwnd->crAttrByte & UNDERLINE)

typedef struct tagTERMINALPROPERTIES {
   BOOL fSound;                     /* TRUE if incoming ASCII 7 beeps */
   BOOL fAutoWrap;                  /* TRUE if text wraps at the right hand column */
   BOOL fNewline;                   /* TRUE if incoming CR gets implicit LF */
   BOOL fOutCRtoCRLF;               /* TRUE if we expand outgoing CR to CR/LF */
   BOOL fAnsi;                      /* TRUE if ANSI terminal mode enabled */
   BOOL fStripHigh;                 /* TRUE if bit 7 stripped on input/output */
   BOOL fLocalEcho;                 /* TRUE if chrs typed at keyboard are echoed locally */
   BOOL fShowNegotiation;           /* TRUE if we show negotiation options during connect */
   BOOL fTelnetBinary;              /* TRUE if we negotiation binary */
   HFONT hFont;                     /* Active terminal font */
   COLORREF crTermText;             /* Terminal text colour */
   COLORREF crTermBack;             /* Terminal background colour */
   COLORREF FAR * lpcrBgMapTable;   /* Background colour mapping table */
   COLORREF FAR * lpcrFgMapTable;   /* Foreground colour mapping table */
   int nBufferSize;                 /* Scroll buffer size */
   int nLineWidth;                  /* Line width - 80 or 132 characters */
   char szConnCard[LEN_CONNCARD+1]; /* Connection card name */
} TERMINALPROPERTIES;

typedef struct tagTERMINALWND {
   TERMINALPROPERTIES tp;           /* Terminal properties */
   LPCOMMDEVICE lpcdev;             /* Communications device handle */
   HXFER hxfer;                     /* Active file transfer handle */
   BOOL fAttached;                  /* TRUE if comms handle was attached */
   char szName[LEN_TERMNAME+1];     /* Name of this connection */
   HWND hwnd;                       /* Window handle */
   HWND hwndTerm;                   /* Terminal window handle */
   char crAttrByte;                 /* Current Ansi attribute byte */
   char crRealAttrByte;             /* Real foreground/background attribute */
   int nCharWidth;                  /* Character cell width, in pixels */
   int nLineHeight;                 /* Character cell height in pixels */
   LPSTR lpScrollBuf;               /* Pointer to scroll buffer lines */
   LPSTR lpColourBuf;               /* Pointer to colour buffer lines */
   int nSBQX;                       /* Cursor offset on current line */
   int nSBQY;                       /* Cursor line offset in scroll buffer */
   int nSBQYTop;                    /* Offset of top row of VT100 display */
   int nSBQYBottom;                 /* Offset of bottom row of VT100 display */
   int nSBQXShift;                  /* Horizontal scroll offset */
   int nSBQYShift;                  /* Vertical scroll offset */
   int nSBQXCaret;                  /* Absolute caret X position */
   int nSBQYCaret;                  /* Absolute caret Y position */
   int nSBQXSize;                   /* Width of visible area, in characters */
   int nSBQYSize;                   /* Height of visible area, in characters */
   int nSBQYExtent;                 /* Vertical extent of cursor in scroll buffer */
   int nSBQXExtent;                 /* Horizontal extent of cursor in scroll buffer */
   int nSBQX1Anchor;                /* Selection anchor starting column */
   int nSBQY1Anchor;                /* Selection anchor starting row */
   int nSBQX2Anchor;                /* Selection anchor ending column */
   int nSBQY2Anchor;                /* Selection anchor ending row */
   BOOL fSelection;                 /* TRUE if there is a selection */
   BOOL fHasCaret;                  /* TRUE if we have the caret */
   BOOL fWndHasCaret;               /* TRUE if window has the caret */
   BOOL fCurrentLineRendered;       /* TRUE if the current text line has been painted to the image buffer */
   BOOL fReverseOn;                 /* TRUE if terminal in reverse video mode */
   int nWriteState;                 /* State machine for ANSI output */
   int nAnsiVal;                    /* Ansi numeric parameter value */
   int nSaveCol;                    /* Saved column offset (Ansi) */
   int nSaveRow;                    /* Saved row offset (Ansi) */
   int AnsiParmVal[ 2 ];            /* Ansi parameters (max.2) */
   int nMode;                       /* Ansi terminal mode */
   BOOL fOriginMode;                /* TRUE if terminal is in origin mode */
   LPLOGFILE lpLogFile;             /* Pointer to open log file */
} TERMINALWND; 

#define  GWW_EXTRABYTES       4
#define  GetTerminalWnd(h)    ((TERMINALWND FAR *)GetWindowLong((h),0))
#define  PutTerminalWnd(h,b)     (SetWindowLong((h),0,(LONG)(b)))

BYTE FASTCALL FgRGBToB4( TERMINALWND FAR *, COLORREF );
BYTE FASTCALL BgRGBToB4( TERMINALWND FAR *, COLORREF );

void FASTCALL Terminal_ResizeBuffer( HWND );
void FASTCALL Terminal_ComputeCellDimensions( HWND );
void FASTCALL TerminalFrame_OnSize( HWND, UINT, int, int );
BOOL FASTCALL Terminal_SetAnsi( HWND, BOOL );
void FASTCALL Terminal_SetDefaultFont( TERMINALWND FAR *, char * );

#define _H_TERMINAL
#endif
