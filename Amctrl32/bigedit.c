/* BIGEDIT.C - Handles the big edit window
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

#define  _WIN32_WINNT 0x0400

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <limits.h>
#include "amctrlrc.h"
#include "amctrls.h"
#include "amctrli.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define THIS_FILE       __FILE__

#define MAX_LINES       65535
#define DOORSTOP        0xFFFFFFFF

#define CH_BOLD         0x04
#define CH_ITALIC       0x05
#define CH_UNDERLINE    0x06
#define CH_NORMAL       0x03
#define CH_LINK         0x07

#define  IsStyleChr(c)        ((c) >= CH_NORMAL && (c) <= CH_LINK)
#define  IsURLTerminator(c)   ((c) == '.' || (c) == '?')

const char pCtlBold[] = { CH_BOLD };
const char pCtlItalic[] = { CH_ITALIC };
const char pCtlUnderline[] = { CH_UNDERLINE };
const char pCtlNormal[] = { CH_NORMAL };
const char pCtlLink[] = { CH_LINK };

typedef struct tagBIGEDIT {
   HPSTR hpText;                                /* Pointer to text buffer */
   LPSELRANGE lplIndex;                         /* Pointer to index buffer */
   HFONT hFont;                                 /* Active font */
   HFONT hCachedUFont;                          /* Cached font for hotlinks */
   HFONT hCachedBFont;                          /* Cached boldface font */
   HFONT hCachedIFont;                          /* Cached italic font */
   int * lpChrWidths;                           /* Array of character widths */
   SELRANGE lSelStart;                          /* Index of start of selection */
   SELRANGE lSelEnd;                            /* Index of end of selection */
   COLORREF crMsgText;                          /* Edit control text colour */
   COLORREF crMsgWnd;                           /* Edit control background colour */
   DWORD cbTextLength;                          /* Length of text */
   COLORREF crQuoteText;                        /* Quoted text colour */
   COLORREF crHotlink;                          /* Hotlinks colour */
   struct {
      unsigned fColourQuotes:1;                 /* TRUE if we colour quoted text */
      unsigned fUseHotlinks:1;                  /* TRUE if we highlight URLs */
      unsigned fWordWrap:1;                     /* TRUE if we word-wrap */
      };
   char * pszQuotedText;                       /* Chars that mark quoted text */
   int nYShift;                                /* Index of line at top */
   int nXShift;                                /* Index of character at left */
   int cyPage;                                 /* Height of window, in lines */
   int dyLine;                                 /* Height of line */
   int dxChar;                                 /* Width of character (average) */
   int cLines;                                 /* Number of lines in the text */
   int cLongestLine;                           /* Length, in chrs, of longest line */
   BOOL fSelection;                            /* TRUE if there is a selection */
   BOOL fWndHasCaret;                          /* TRUE if this window has the caret */
   BOOL fHasCaret;                             /* TRUE if window has created a caret */
   BOOL fOverHotspot;                          /* TRUE if we're over a hotspot */
   RECT rcHotspot;                             /* Hotspot if fOverHotspot is TRUE */
   BOOL fMouseClick;
   BOOL fHotLinks;
} BIGEDIT;

typedef struct tagBIGEDIT FAR * LPBIGEDIT;

typedef struct tagSTYLETABLE {
   LPSTR pszLine;                               /* Pointer to line */
   COLORREF crText;                             /* Text colour */
   COLORREF crBack;                             /* Background table */
   HFONT hFont;                                 /* Font */
} STYLETABLE;

#define GWW_EXTRA                               4
#define GetBBlock(h)                            ((LPBIGEDIT)GetWindowLong((h),0))
#define PutBBlock(h,b)                          (SetWindowLong((h),0,(LONG)(b)))

/* URL character matching. A '1' means that the
 * character is valid in a URL.
 *
 * A separate utility, HTTPMAP.EXE, creates this
 * table as a header file.
 */
#include "httpmap.h"

BOOL FASTCALL BigEditWnd_OnNCCreate( HWND, CREATESTRUCT FAR * );
BOOL FASTCALL BigEditWnd_OnCreate( HWND, CREATESTRUCT FAR * );
void FASTCALL BigEditWnd_OnDestroy( HWND );
void FASTCALL BigEditWnd_OnPaint( HWND );
void FASTCALL BigEditWnd_OnNCPaint( HWND, HRGN );
BOOL FASTCALL BigEditWnd_OnEraseBkGnd( HWND, HDC );
void FASTCALL BigEditWnd_OnVScroll( HWND, HWND, UINT, int );
void FASTCALL BigEditWnd_OnHScroll( HWND, HWND, UINT, int );
void FASTCALL BigEditWnd_OnSetText( HWND, LPCSTR );
void FASTCALL BigEditWnd_OnSize( HWND, UINT, int, int );
void FASTCALL BigEditWnd_OnSetFont( HWND, HFONT, BOOL );
HFONT FASTCALL BigEditWnd_OnGetFont( HWND );
void FASTCALL BigEditWnd_OnLButtonDown( HWND, BOOL, int, int, UINT );
void FASTCALL BigEditWnd_OnLButtonUp( HWND, int, int, UINT );
void FASTCALL BigEditWnd_OnSetFocus( HWND, HWND );
void FASTCALL BigEditWnd_OnKey( HWND, UINT, BOOL, int, UINT );
void FASTCALL BigEditWnd_OnKillFocus( HWND, HWND );
BOOL FASTCALL BigEditWnd_OnSetCursor( HWND, HWND, UINT, UINT );
void FASTCALL BigEditWnd_SetCursor( HWND );
void FASTCALL BigEditWnd_OnMouseMove( HWND hwnd, int x, int y, UINT );
void FASTCALL BigEditWnd_OnSetSel( HWND, UINT, LPBEC_SELECTION );
void FASTCALL BigEditWnd_OnGetSel( HWND, LPBEC_SELECTION );
void FASTCALL BigEditWnd_OnCopy( HWND );
void FASTCALL BigEditWnd_OnReplaceSel( HWND, LPSTR );
void FASTCALL BigEditWnd_HShift( HWND, int );
void FASTCALL BigEditWnd_VShift( HWND, int );
void FASTCALL BigEditWnd_OnGetRect( HWND, LPRECT );
SELRANGE FASTCALL BigEditWnd_OnGetFirstVisibleLine( HWND );
DWORD FASTCALL BigEditWnd_OnGetTextLength( HWND );
DWORD FASTCALL BigEditWnd_OnGetText( HWND, LPBEC_GETTEXT );
HGLOBAL FASTCALL BigEditWnd_CopySelection( HWND );
void FASTCALL BigEditWnd_InvalidateSelection( HWND, BOOL, SELRANGE, SELRANGE );
void FASTCALL BigEditWnd_GetSelection( LPBIGEDIT, SELRANGE *, SELRANGE * );
void FASTCALL BigEditWnd_OnScroll( HWND, LPBEC_SCROLL );
int FASTCALL BigEditWnd_OnLineFromChar( HWND, SELRANGE );
void FASTCALL ScrollDelay( void );
BOOL FASTCALL BigEditWnd_SetCaret( HWND, int, int );
COLORREF FASTCALL BigEditWnd_OnGetTextColour( HWND );
COLORREF FASTCALL BigEditWnd_OnGetBackColour( HWND );
COLORREF FASTCALL BigEditWnd_OnGetQuoteColour( HWND );
COLORREF FASTCALL BigEditWnd_OnSetTextColour( HWND, COLORREF, BOOL );
COLORREF FASTCALL BigEditWnd_OnSetBackColour( HWND, COLORREF, BOOL );
COLORREF FASTCALL BigEditWnd_OnSetQuoteColour( HWND, COLORREF, BOOL );
BOOL FASTCALL BigEditWnd_OnColourQuotes( HWND, BOOL, LPCSTR );
BOOL FASTCALL BigEditWnd_OnEnableHotlinks( HWND, BOOL );
BOOL FASTCALL BigEditWnd_OnSetWordWrap( HWND, BOOL );
BOOL FASTCALL BigEditWnd_OnGetWordWrap( HWND );
COLORREF FASTCALL BigEditWnd_OnSetHotlinkColour( HWND, COLORREF, BOOL );

UINT FASTCALL BigEditWnd_OnOldGetTextLength( HWND );
UINT FASTCALL BigEditWnd_OnOldGetFirstVisibleLine( HWND );
DWORD FASTCALL BigEditWnd_OnOldGetSel( HWND );
UINT FASTCALL BigEditWnd_OnOldLineIndex( HWND, UINT );
UINT FASTCALL BigEditWnd_OnOldGetText( HWND, UINT, LPSTR );
HFONT FASTCALL BigEdit_NeedBoldFont( LPBIGEDIT );
HFONT FASTCALL BigEdit_NeedItalicFont( LPBIGEDIT );
HFONT FASTCALL BigEdit_NeedUnderlineFont( LPBIGEDIT );
void FASTCALL BigEdit_DeleteCachedFonts( LPBIGEDIT );
void FASTCALL BigEditWnd_ShowCaret( HWND, LPBIGEDIT );
void FASTCALL BigEditWnd_HideCaret( HWND, LPBIGEDIT );
void FASTCALL BigEditWnd_SetHome( HWND );
void FASTCALL BigEditWnd_SetEnd( HWND );

static BOOL fDragging = FALSE;

#ifdef MULTILINESTYLE
BOOL fInStyle;
#endif MULTILINESTYLE

BOOL fWin95;
WORD wWinVer;                    /* Windows version number */

typedef struct tagLINEBUF {
   HPSTR hpFmtText;
   DWORD cbFmtText;
} LINEBUF, FAR * LPLINEBUF;

SELRANGE FASTCALL PointToOffset( HWND, int, int, BOOL );
void FASTCALL OffsetToPoint( HWND, SELRANGE, int *, int * );
void FASTCALL CalculateLongestLine( HWND hwnd );
void FASTCALL AdjustScrolls( HWND, BOOL );
BOOL FASTCALL TestForHotlink( HWND, int, int, char *, int, RECT * );
UINT FASTCALL LocateNextSegment( LPBIGEDIT, STYLETABLE *, BOOL, int * );
BOOL FASTCALL IsHotlinkPrefix( char *, int * );
void FASTCALL BuildLineIndexTable( HWND, LPBIGEDIT );
void FASTCALL BuildFormatText( HWND, LPBIGEDIT );
void FASTCALL AddToFormatBuffer( LPLINEBUF, const HPSTR, DWORD );
DWORD FASTCALL CopyFormattedLine( HPSTR, HPSTR, DWORD );
char * FASTCALL LocateStyleChr( char * );
void FASTCALL ComputeCharWidths( HWND, LPBIGEDIT );

LRESULT EXPORT CALLBACK BigEditWndFn( HWND, UINT, WPARAM, LPARAM );
SELRANGE FASTCALL CountFormatChars(HWND, SELRANGE );

/*DWORD FASTCALL CountFormatCharacters( HPSTR pSrc, DWORD cStream )
{
   DWORD lCount;
   HPSTR lSrc;

   lSrc = pSrc;
   lCount = 0;
   while( *pSrc && cStream-- )
   {
      if( *lSrc == CH_BOLD )
         ;
      else if( *lSrc == CH_ITALIC )
         ;
      else if( *lSrc == CH_UNDERLINE )
         ;
      else if( *lSrc == CH_LINK )
         lCount++;         
      else if( *lSrc == CH_NORMAL )
         lCount++;         
      else 
      {
         ;
      }
      ++lSrc;
   }
   return lCount;

} */

/*
 Word Break Char
 */
BOOL FASTCALL IsWBChar(int c)
{
   return (c == ' ') || (c == '\n') || (c == '\r') || (c == 9)/*|| (c == CH_BOLD) || (c == CH_ITALIC) || (c == CH_UNDERLINE) || (c == CH_LINK) || (c == CH_NORMAL)*/;
}

/*
 Line Break Char
 */
BOOL IsLBChar(int c)
{
   return (c == '\n') || (c == '\r');
}

BOOL FASTCALL Amctl_IsHotlinkPrefix( char * pszText, int * cbMatch )
{
   return IsHotlinkPrefix( pszText, cbMatch );
}

SELRANGE FASTCALL Amctl_CountFormatChars(HWND hwnd, SELRANGE lSelStart )
{
    LPBIGEDIT lpbe;
    HPSTR hpText;
    SELRANGE cnt = 0;

   if( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES )
   {
      lpbe = GetBBlock( hwnd );

      hpText = lpbe->hpText;
      if(hpText)
      {
         while(lSelStart && *hpText)
         {
            if( *hpText == CH_BOLD )
               cnt++;
            else if( *hpText == CH_ITALIC )
               cnt++;
            else if( *hpText == CH_UNDERLINE )
               cnt++;
            else if( *hpText == CH_LINK )
               cnt++;
            else if( *hpText == CH_NORMAL )
               cnt++;
            else 
            {
               ;
            }
            *hpText++;
            lSelStart--;
         }
      }
   }
    return cnt;
}

SELRANGE FASTCALL CountFormatChars(HWND hwnd, SELRANGE lSelStart )
{
        LPBIGEDIT lpbe;
        HPSTR hpText;
        SELRANGE cnt = 0;

        lpbe = GetBBlock( hwnd );

//      ASSERT(lpbe->fSelection);

        hpText = lpbe->hpText;
        if(hpText)
        {
                while(lSelStart && *hpText)
                {
//                        if( ( *hpText == CH_LINK ) || ( *hpText == CH_NORMAL ) )
                  if( IsStyleChr(*hpText) )
                     cnt++;
                        *hpText++;
                        lSelStart--;
                }
        }
        return cnt;
}

/* This function registers the big edit window class.
 */
BOOL FASTCALL RegisterBigEditClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   /* Register the big edit window class
    */
   wc.style       = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_GLOBALCLASS;
   wc.lpfnWndProc    = BigEditWndFn;
   wc.hIcon       = NULL;
   wc.hCursor        = LoadCursor( NULL, IDC_IBEAM );
   wc.lpszMenuName      = NULL;
   wc.cbWndExtra     = GWW_EXTRA;
   wc.cbClsExtra     = 0;
   wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW + 1 );
   wc.lpszClassName  = WC_BIGEDIT;
   wc.hInstance      = hInst;
   return( RegisterClass( &wc ) );
}

/* This is the window procedure for the info bar window.
 */
LRESULT EXPORT CALLBACK BigEditWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_NCCREATE, BigEditWnd_OnNCCreate );
      HANDLE_MSG( hwnd, WM_CREATE, BigEditWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_DESTROY, BigEditWnd_OnDestroy );
      HANDLE_MSG( hwnd, WM_PAINT, BigEditWnd_OnPaint );
      HANDLE_MSG( hwnd, WM_NCPAINT, BigEditWnd_OnNCPaint );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, BigEditWnd_OnEraseBkGnd );
      HANDLE_MSG( hwnd, WM_VSCROLL, BigEditWnd_OnVScroll );
      HANDLE_MSG( hwnd, WM_HSCROLL, BigEditWnd_OnHScroll );
      HANDLE_MSG( hwnd, WM_SETTEXT, BigEditWnd_OnSetText );
      HANDLE_MSG( hwnd, WM_SIZE, BigEditWnd_OnSize );
      HANDLE_MSG( hwnd, WM_SETFONT, BigEditWnd_OnSetFont );
      HANDLE_MSG( hwnd, WM_GETFONT, BigEditWnd_OnGetFont );
      HANDLE_MSG( hwnd, WM_SETCURSOR, BigEditWnd_OnSetCursor );
      HANDLE_MSG( hwnd, WM_LBUTTONDOWN, BigEditWnd_OnLButtonDown );
      HANDLE_MSG( hwnd, WM_LBUTTONDBLCLK, BigEditWnd_OnLButtonDown );
      HANDLE_MSG( hwnd, WM_LBUTTONUP, BigEditWnd_OnLButtonUp );
      HANDLE_MSG( hwnd, WM_SETFOCUS, BigEditWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_KILLFOCUS, BigEditWnd_OnKillFocus );
      HANDLE_MSG( hwnd, WM_MOUSEMOVE, BigEditWnd_OnMouseMove );
      HANDLE_MSG( hwnd, WM_KEYDOWN, BigEditWnd_OnKey );
      HANDLE_MSG( hwnd, EM_GETSEL, BigEditWnd_OnOldGetSel );
      HANDLE_MSG( hwnd, EM_GETFIRSTVISIBLELINE, BigEditWnd_OnOldGetFirstVisibleLine );
      HANDLE_MSG( hwnd, EM_LINEINDEX, BigEditWnd_OnOldLineIndex );
      HANDLE_MSG( hwnd, WM_GETTEXT, BigEditWnd_OnOldGetText );
      HANDLE_MSG( hwnd, WM_GETTEXTLENGTH, BigEditWnd_OnOldGetTextLength );
      HANDLE_MSG( hwnd, BEM_SETSEL, BigEditWnd_OnSetSel );
      HANDLE_MSG( hwnd, BEM_GETSEL, BigEditWnd_OnGetSel );
      HANDLE_MSG( hwnd, BEM_REPLACESEL, BigEditWnd_OnReplaceSel );
      HANDLE_MSG( hwnd, BEM_GETTEXTLENGTH, BigEditWnd_OnGetTextLength );
      HANDLE_MSG( hwnd, BEM_GETTEXT, BigEditWnd_OnGetText );
      HANDLE_MSG( hwnd, BEM_SCROLL, BigEditWnd_OnScroll );
      HANDLE_MSG( hwnd, BEM_GETFIRSTVISIBLELINE, BigEditWnd_OnGetFirstVisibleLine );
      HANDLE_MSG( hwnd, BEM_GETRECT, BigEditWnd_OnGetRect );
      HANDLE_MSG( hwnd, BEM_LINEFROMCHAR, BigEditWnd_OnLineFromChar );
      HANDLE_MSG( hwnd, BEM_SETTEXTCOLOUR, BigEditWnd_OnSetTextColour );
      HANDLE_MSG( hwnd, BEM_SETBACKCOLOUR, BigEditWnd_OnSetBackColour );
      HANDLE_MSG( hwnd, BEM_SETQUOTECOLOUR, BigEditWnd_OnSetQuoteColour );
      HANDLE_MSG( hwnd, BEM_SETHOTLINKCOLOUR, BigEditWnd_OnSetHotlinkColour );
      HANDLE_MSG( hwnd, BEM_GETTEXTCOLOUR, BigEditWnd_OnGetTextColour );
      HANDLE_MSG( hwnd, BEM_GETBACKCOLOUR, BigEditWnd_OnGetBackColour );
      HANDLE_MSG( hwnd, BEM_GETQUOTECOLOUR, BigEditWnd_OnGetQuoteColour );
      HANDLE_MSG( hwnd, BEM_COLOURQUOTES, BigEditWnd_OnColourQuotes );
      HANDLE_MSG( hwnd, BEM_ENABLEHOTLINKS, BigEditWnd_OnEnableHotlinks );
      HANDLE_MSG( hwnd, BEM_SETWORDWRAP, BigEditWnd_OnSetWordWrap );
      HANDLE_MSG( hwnd, BEM_GETWORDWRAP, BigEditWnd_OnGetWordWrap );
      HANDLE_MSG( hwnd, WM_COPY, BigEditWnd_OnCopy );

   #ifdef WIN32
      case WM_MOUSEWHEEL: {
         static int zDeltaAccumulate = 0;
         int zDelta;
         int nLines;

         /* Handle the Intellimouse wheel.
          */
         zDelta = (short)HIWORD( wParam );
         zDeltaAccumulate += zDelta;
         if( nLines = zDeltaAccumulate / WHEEL_DELTA )
            {
            if( WHEEL_PAGESCROLL == iScrollLines )
               {
               if( nLines > 0 )
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEUP, 0L );
               else
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L );
               }
            else
               {
               nLines *= iScrollLines;
               while( nLines > 0 )
                  {
                  SendMessage( hwnd, WM_VSCROLL, SB_LINEUP, 0L );
                  --nLines;
                  }
               while( nLines < 0 )
                  {
                  SendMessage( hwnd, WM_VSCROLL, SB_LINEDOWN, 0L );
                  ++nLines;
                  }
               }
            zDeltaAccumulate = 0;
            }
         return( 0L );
         }
   #endif
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the BEM_LINEFROMCHAR message.
 */
int FASTCALL BigEditWnd_OnLineFromChar( HWND hwnd, SELRANGE offset )
{
   LPBIGEDIT lpbe;
   LPSELRANGE lpib;
   int line;

   lpbe = GetBBlock( hwnd );
   lpib = lpbe->lplIndex;
   line = 0;
   while( *lpib != DOORSTOP )
      {
      if( offset >= lpib[ 0 ] && offset <= lpib[ 1 ] )
         break;
      ++line;
      ++lpib;
      }
   return( line );
}

/* This function handles the EM_LINEINDEX message.
 */
UINT FASTCALL BigEditWnd_OnOldLineIndex( HWND hwnd, UINT line )
{
   LPBIGEDIT lpbe;
   LPSELRANGE lpib;

   lpbe = GetBBlock( hwnd );
   lpib = lpbe->lplIndex;
   if( line < (UINT)lpbe->cLines )
      return( (UINT)min( lpib[ line ], -1 ) );
   return( (UINT)-1 );
}

/* This function handles the EM_GETFIRSTVISIBLELINE message
 */
UINT FASTCALL BigEditWnd_OnOldGetFirstVisibleLine( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( (UINT)lpbe->nYShift );
}

/* This function returns the offset of the first visible line in
 * the edit control.
 */
SELRANGE FASTCALL BigEditWnd_OnGetFirstVisibleLine( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->nYShift );
}

/* This function handles the WM_COPY message.
 */
void FASTCALL BigEditWnd_OnCopy( HWND hwnd )
{
   LPBIGEDIT lpbe;
   HGLOBAL hMem;

   lpbe = GetBBlock( hwnd );
   if( lpbe->fSelection )
      {
      hMem = BigEditWnd_CopySelection( hwnd );
      if( hMem && OpenClipboard( hwnd ) )
         {
         EmptyClipboard();
         SetClipboardData( CF_TEXT, hMem );
         CloseClipboard();
         }
      }
}

/* This function handles the BEM_GETRECT message.
 */
void FASTCALL BigEditWnd_OnGetRect( HWND hwnd, LPRECT lprc )
{
   GetClientRect( hwnd, lprc );
   InflateRect( lprc, -2, -2 );
}

/* This function handles the BEM_GETTEXTCOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnGetTextColour( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->crMsgText );
}

/* This function handles the BEM_GETBACKCOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnGetBackColour( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->crMsgWnd );
}

/* This function handles the BEM_GETQUOTECOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnGetQuoteColour( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->crQuoteText );
}

/* This function handles the BEM_SETTEXTCOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnSetTextColour( HWND hwnd, COLORREF crText, BOOL fRedraw )
{
   LPBIGEDIT lpbe;
   COLORREF crPrevText;

   lpbe = GetBBlock( hwnd );
   crPrevText = lpbe->crMsgText;
   lpbe->crMsgText = crText;
   if( fRedraw )
      {
      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
   return( crPrevText );
}

/* This function enables or disables hotlinks.
 */
BOOL FASTCALL BigEditWnd_OnEnableHotlinks( HWND hwnd, BOOL fUseHotlinks )
{
   LPBIGEDIT lpbe;
   BOOL fDidUseHotlinks;

   lpbe = GetBBlock( hwnd );
   fDidUseHotlinks = lpbe->fUseHotlinks;
   lpbe->fUseHotlinks = fUseHotlinks;
   lpbe->fHotLinks = fUseHotlinks;
   return( fDidUseHotlinks );
}

/* This function enables or disables word wrapping.
 */
BOOL FASTCALL BigEditWnd_OnSetWordWrap( HWND hwnd, BOOL fWordWrap )
{
   LPBIGEDIT lpbe;
   BOOL fDidWordWrap;

   lpbe = GetBBlock( hwnd );
   fDidWordWrap = lpbe->fWordWrap;
   lpbe->fWordWrap = fWordWrap;
   if( fDidWordWrap != fWordWrap )
      {
      BuildLineIndexTable( hwnd, lpbe );
      AdjustScrolls( hwnd, TRUE );
      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
   return( fDidWordWrap );
}

/* This function retrieves the word wrapping flag
 */
BOOL FASTCALL BigEditWnd_OnGetWordWrap( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->fWordWrap );
}

/* This function sets the quotes colour.
 */
BOOL FASTCALL BigEditWnd_OnColourQuotes( HWND hwnd, BOOL fColourQuotes, LPCSTR lpszQuoteDelims )
{
   LPBIGEDIT lpbe;
   BOOL fDidColourQuotes;

   lpbe = GetBBlock( hwnd );
   fDidColourQuotes = lpbe->fColourQuotes;
   lpbe->fColourQuotes = fColourQuotes;
   if( NULL != lpbe->pszQuotedText )
      FreeMemory( &lpbe->pszQuotedText );
   if( fNewMemory( &lpbe->pszQuotedText, lstrlen(lpszQuoteDelims)+1 ) )
      lstrcpy( lpbe->pszQuotedText, lpszQuoteDelims );
   return( fDidColourQuotes );
}

/* This function handles the BEM_SETBACKCOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnSetBackColour( HWND hwnd, COLORREF crWnd, BOOL fRedraw )
{
   LPBIGEDIT lpbe;
   COLORREF crPrevWnd;

   lpbe = GetBBlock( hwnd );
   crPrevWnd = lpbe->crMsgWnd;
   lpbe->crMsgWnd = crWnd;
   if( fRedraw )
      {
      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
   return( crPrevWnd );
}

/* This function handles the BEM_SETQUOTECOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnSetQuoteColour( HWND hwnd, COLORREF crQuote, BOOL fRedraw )
{
   LPBIGEDIT lpbe;
   COLORREF crPrevQuote;

   lpbe = GetBBlock( hwnd );
   crPrevQuote = lpbe->crQuoteText;
   lpbe->crQuoteText = crQuote;
   if( fRedraw )
      {
      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
   return( crPrevQuote );
}

/* This function handles the BEM_SETHOTLINKCOLOUR message.
 */
COLORREF FASTCALL BigEditWnd_OnSetHotlinkColour( HWND hwnd, COLORREF crHotlink, BOOL fRedraw )
{
   LPBIGEDIT lpbe;
   COLORREF crPrevHotlink;

   lpbe = GetBBlock( hwnd );
   crPrevHotlink = lpbe->crHotlink;
   lpbe->crHotlink = crHotlink;
   if( fRedraw )
      {
      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
   return( crPrevHotlink );
}

/* This function handles the EM_GETTEXT message.
 */
UINT FASTCALL BigEditWnd_OnOldGetText( HWND hwnd, UINT cchMaxText, LPSTR lpszText )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   cchMaxText = (UINT)min( cchMaxText, lpbe->cbTextLength );
   hmemcpy( lpszText, lpbe->hpText, cchMaxText );
   return( (UINT)CopyFormattedLine( lpszText, lpbe->hpText, (UINT)cchMaxText ) );
}

/* This function handles the BEM_GETTEXT message.
 */
DWORD FASTCALL BigEditWnd_OnGetText( HWND hwnd, LPBEC_GETTEXT lpgt )
{
   LPBIGEDIT lpbe;
   SELRANGE cchMaxText;

   /* Compute hpText to point to the start of the
    * current selection.
    */
   lpbe = GetBBlock( hwnd );
   
// lpgt->bsel.lo += Amctl_CountFormatChars(hwnd, lpgt->bsel.lo );
// lpgt->bsel.hi += Amctl_CountFormatChars(hwnd, lpgt->bsel.hi );

   cchMaxText = lpgt->bsel.hi ? ( lpgt->bsel.hi - lpgt->bsel.lo ) + 1 : lpgt->cchMaxText;
   cchMaxText = min( cchMaxText, lpgt->cchMaxText );
   if( 0 < cchMaxText )
   {
//    if( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES )
//       cchMaxText = CopyFormattedLine( lpgt->hpText, lpbe->hpText + lpgt->bsel.lo, cchMaxText - 1 ); /*!!SM!!*/
//    else
         cchMaxText = CopyFormattedLine( lpgt->hpText, lpbe->hpText + lpgt->bsel.lo, cchMaxText );
      lpgt->hpText[ cchMaxText ] = '\0';
   }
   return( cchMaxText );
}

/* This function handles the WM_GETTEXTLENGTH message.
 */
UINT FASTCALL BigEditWnd_OnOldGetTextLength( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( (UINT)min( lpbe->cbTextLength, -1 ) );
}

/* This function handles the BEM_GETTEXTLENGTH message.
 */
DWORD FASTCALL BigEditWnd_OnGetTextLength( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   return( lpbe->cbTextLength );
}

/* This function handles the BEM_REPLACESEL message.
 */
void FASTCALL BigEditWnd_OnReplaceSel( HWND hwnd, LPSTR lpszNewText )
{
   LPBIGEDIT lpbe;
   SELRANGE lSelStart;
   SELRANGE lSelEnd;
   HPSTR hpText;

   /* Compute hpText to point to the start of the
    * current selection.
    */
   lpbe = GetBBlock( hwnd );
   ASSERT(lpbe->fSelection);
   BigEditWnd_GetSelection( lpbe, &lSelStart, &lSelEnd );
   hpText = lpbe->hpText + lSelStart;

   /* Replace with the specified text, then invalidate
    * to force a repaint.
    * BUGBUG: Only works with in-place replacements where the new text is
    * the same length as the selection!
    */
   BigEditWnd_InvalidateSelection( hwnd, FALSE, lSelStart, lSelEnd );
   hmemcpy( hpText, lpszNewText, lstrlen(lpszNewText) );
   BigEditWnd_InvalidateSelection( hwnd, TRUE, lSelStart, lSelEnd );
}

/* This function handles the BEM_GETSEL message.
 */
void FASTCALL BigEditWnd_OnGetSel( HWND hwnd, LPBEC_SELECTION lpbsel )
{
        LPBIGEDIT lpbe;
        SELRANGE lSelStart;
        SELRANGE lSelEnd;

        lpbe = GetBBlock( hwnd );

        /* Adjust start and end so that they have values within the range
         * of the text. This fixes a bug where copy tried to copy text
         * from 23 to 0xFFFFFFFF for instance.
         * YH 08/05/96
         */
        lSelStart = min( lpbe->cbTextLength, lpbe->lSelStart );
        lSelEnd = min( lpbe->cbTextLength, lpbe->lSelEnd );

        lpbsel->lo = min( lSelStart, lSelEnd);
        lpbsel->hi = max( lSelStart, lSelEnd);

      /*
      If the following is enabled, then quoted text in conference messages is truncated.
      */

//        lpbsel->lo -= Amctl_CountFormatChars(hwnd, lpbsel->lo );
//        lpbsel->hi -= Amctl_CountFormatChars(hwnd, lpbsel->hi );
}

/* This function handles the EM_GETSEL message.
 */
DWORD FASTCALL BigEditWnd_OnOldGetSel( HWND hwnd )
{
        LPBIGEDIT lpbe;
        UINT lo;
        UINT hi;

        lpbe = GetBBlock( hwnd );
        lo = (UINT)min( min( lpbe->lSelStart, lpbe->lSelEnd ), -1 );
        hi = (UINT)min( max( lpbe->lSelStart, lpbe->lSelEnd ), -1 );

        lo -= (UINT)CountFormatChars(hwnd, lo );
        hi -= (UINT)CountFormatChars(hwnd, hi );

        return( MAKELPARAM( lo, hi ) );
}

/* This function handles the BEM_SETSEL message.
 */
void FASTCALL BigEditWnd_OnSetSel( HWND hwnd, UINT flags, LPBEC_SELECTION lpbsel )
{
   LPBIGEDIT lpbe;
   SELRANGE lo;
   SELRANGE hi;
   SELRANGE tmp;
   BOOL fShift;
   int nBase;

   /* If there's a selection, invalidate the selection first.
    */
   lpbe = GetBBlock( hwnd );
   if( lpbe->fSelection )
      {
      lpbe->fSelection = FALSE;
      BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
      }

   /* Ensure that lo < hi
    */
   lo = lpbsel->lo;
   hi = lpbsel->hi;

   if( lo > hi )
   { 
      tmp = lo; 
      lo = hi; 
      hi = tmp; 
   }

   lo += (UINT)Amctl_CountFormatChars(hwnd, lo );
   if( hi > lpbe->cbTextLength )
      hi = lpbe->cbTextLength;
   hi += (UINT)Amctl_CountFormatChars(hwnd, hi );

   /* If necessary, ensure that the selection is visible by
    * adjusting the top line. If the start of the selection is
    * visible, that's sufficient.
    */
   fShift = FALSE;
   nBase = lpbe->nYShift;
   if( lo < lpbe->lplIndex[ nBase ] )
      {
      while( lo < lpbe->lplIndex[ nBase ] )
         {
         --lpbe->nYShift;
         --nBase;
         fShift = TRUE;
         }
      }
   else
      {
      nBase = min( lpbe->nYShift + lpbe->cyPage, lpbe->cLines - 1 );
      if( hi > lpbe->lplIndex[ nBase ] )
         {
         while( hi > lpbe->lplIndex[ nBase ] )
            {
            ++lpbe->nYShift;
            ++nBase;
            fShift = TRUE;
            }
         }
      }

   /* If we had to shift the screen, redraw it.
    */
   if( fShift )
      BigEditWnd_VShift( hwnd, lpbe->nYShift );

   /* Now update the selection. If lo == hi, then the
    * selection is being removed.
    */
   lpbe->lSelStart = (SELRANGE)lo;
   lpbe->lSelEnd = (SELRANGE)hi;
   if( lo != hi )
      {
      lpbe->fSelection = TRUE;
      BigEditWnd_HideCaret( hwnd, lpbe );
      BigEditWnd_InvalidateSelection( hwnd, TRUE, lo, hi );
      }
   else
      BigEditWnd_SetCursor( hwnd );
}

/* This function computes the rectangle bounding the current
 * selection. It is assumed that lpbe->lSelStart and lpbe->lSelEnd
 * are the offsets of the start and end of the selection.
 */
void FASTCALL BigEditWnd_InvalidateSelection( HWND hwnd, BOOL fUpdate, SELRANGE lSelStart, SELRANGE lSelEnd )
{
   LPBIGEDIT lpbe;
   LPSELRANGE lpib;
   RECT rc;
   SELRANGE tmp;
   int ib;

   /* Swap lSelStart and lSelEnd if necessary.
    */
   if( lSelEnd < lSelStart )
      {
      tmp = lSelEnd;
      lSelEnd = lSelStart;
      lSelStart = tmp;
      }

   /* Start
    */
   lpbe = GetBBlock( hwnd );
   lpib = lpbe->lplIndex;
   ib = lpbe->nYShift;
   SetRectEmpty( &rc );
   if( lSelEnd > lpib[ ib ] )
      {
      int ibBase;
      int y;

      y = 2;
      ibBase = ib + lpbe->cyPage;
      if( lSelStart > lpib[ ib ] )
         while( lpib[ ib ] != DOORSTOP && ib < ibBase )
            {
            if( lSelStart >= lpib[ ib ] && lSelStart < lpib[ ib + 1 ] )
               break;
            y += lpbe->dyLine;
            ++ib;
            }
      rc.left = 0;
      rc.top = y;

      y += lpbe->dyLine;
      while( lpib[ ib ] != DOORSTOP && ib < ibBase )
         {
         if( lSelEnd >= lpib[ ib ] && lSelEnd < lpib[ ib + 1 ] )
            break;
         y += lpbe->dyLine;
         ++ib;
         }
      rc.right = 9999;
      rc.bottom = y;

      InvalidateRect( hwnd, &rc, FALSE );
      if( fUpdate )
         UpdateWindow( hwnd );
      }
}

/* This function handles the WM_SETCURSOR message
 */
BOOL FASTCALL BigEditWnd_OnSetCursor( HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   if( lpbe->fOverHotspot )
      {
      SetCursor( LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_HOTSPOT) ));
      return( TRUE );
      }
   return( FORWARD_WM_SETCURSOR( hwnd, hwndCursor, codeHitTest, msg, DefWindowProc ) );
}


/* This function handles the WM_LBUTTONDOWN message.
 */
void FASTCALL BigEditWnd_OnLButtonDown( HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags )
{
   LPBIGEDIT lpbe;

   /* Get the edit record and make sure that the
    * control has the focus.
    */
   lpbe = GetBBlock( hwnd );
   if( GetFocus() != hwnd )
      SetFocus( hwnd );

   lpbe->fMouseClick = TRUE;

   /* Handle double-clicking. Scan for the start and end of the word, and
    * set lSelStart and lSelEnd to the offsets.
    */
   if( fDoubleClick )
      {
      SELRANGE lSelCur;

      if( ( lSelCur = PointToOffset( hwnd, x, y, FALSE ) ) != DOORSTOP )
         {
         HPSTR hpsz;

         /* Destroy any existing selection first.
          */
         if( lpbe->fSelection )
            {
            lpbe->fSelection = FALSE;
            BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
            }

         /* Scan back for start of word.
          */
         lpbe->lSelStart = lSelCur;
         hpsz = lpbe->hpText + lpbe->lSelStart;
         while( lpbe->lSelStart > 0 )
            {
//          if( !isgraph( *(hpsz-1) ) ) // !!SM!!
            if( IsWBChar( *(hpsz-1) ) ) // !!SM!!
            {
/*             if (*(hpsz-1) = CH_LINK)
               {
                  --lpbe->lSelStart;
                  --hpsz;
               }*/
               break;
            }
            --lpbe->lSelStart;
            --hpsz;
            }

         /* Scan forward for end of word.
          */
         lpbe->lSelEnd = lSelCur;
         hpsz = lpbe->hpText + lpbe->lSelEnd;
         while( lpbe->lSelEnd < lpbe->cbTextLength )
            {
//          if( !isgraph( *hpsz ) ) // !!SM!!
            if( IsWBChar( *hpsz ) ) // !!SM!!
               break;
            ++lpbe->lSelEnd;
            ++hpsz;
            }
// !!SM!!
         while( lpbe->lSelEnd < lpbe->cbTextLength && IsWBChar( *hpsz ) && (*hpsz != '\n') && (*hpsz != '\r') )
            {
            ++lpbe->lSelEnd;
            ++hpsz;
            } 
// !!SM!!
         /* All done, so show result.
          */
         if( lpbe->lSelEnd != lpbe->lSelStart )
            {
            lpbe->fSelection = TRUE;
            BigEditWnd_HideCaret( hwnd, lpbe );
            BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
            }
         }
      }
   else {
      if( !( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) /* && lpbe->fSelection */) )
         {
         SELRANGE lSelNew;

         /* Destroy any existing selection first.
          */
         if( lpbe->fSelection )
            {
            lpbe->fSelection = FALSE;
            BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
            }
         if( ( lSelNew = PointToOffset( hwnd, x, y, FALSE ) ) != DOORSTOP )
            {
            lpbe->lSelStart = lSelNew;
            lpbe->lSelEnd = lpbe->lSelStart;
            }
         }
      else
         {
         SELRANGE lSelLast;

         /* Shift key is down, so extend the selection to
          * the cursor position.
          */
         lSelLast = lpbe->lSelEnd;
         lpbe->lSelEnd = PointToOffset( hwnd, x, y, FALSE );
         lpbe->fSelection = TRUE;
         BigEditWnd_InvalidateSelection( hwnd, TRUE, min( lSelLast, lpbe->lSelEnd ), max( lSelLast, lpbe->lSelEnd ) );
         }
   
      /* Don't show the caret when we're selecting.
       */
      BigEditWnd_HideCaret( hwnd, lpbe );
      SetCapture( hwnd );
      fDragging = TRUE;
      }
}

/* This function handles the WM_LBUTTONUP message.
 */
void FASTCALL BigEditWnd_OnLButtonUp( HWND hwnd, int x, int y, UINT keyFlags )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   if( lpbe->fMouseClick && !lpbe->fOverHotspot )
      if( GetFocus() != hwnd )
         SetFocus( hwnd );

   if( lpbe->fMouseClick && lpbe->fOverHotspot )
   {
   BEMN_HOTSPOT bemn;
   POINT pt;

   /* Clicked on a hotspot, so send a notification
    * to the parent window.
    */
   lpbe->fMouseClick = FALSE;
   bemn.fHotspot = TRUE;
   pt.x = x;
   pt.y = y;
   if( TestForHotlink( hwnd, x, y, bemn.szText, sizeof(bemn.szText), NULL ) )
      Amctl_SendNotify( GetParent( hwnd ), hwnd, BEMN_CLICK, (LPNMHDR)&bemn );
   else
      lpbe->fOverHotspot = FALSE;
   }


   /* Show the caret only if there is no selection.
    */
   lpbe = GetBBlock( hwnd );
   if( lpbe->fMouseClick && !lpbe->fOverHotspot )
   {
   if( !lpbe->fSelection )
      BigEditWnd_ShowCaret( hwnd, lpbe );
   BigEditWnd_SetCursor( hwnd );
   }
   ReleaseCapture();
   fDragging = FALSE;
   lpbe->fMouseClick = FALSE;

}

/* This function handles the WM_MOUSEMOVE message.
 */
void FASTCALL BigEditWnd_OnMouseMove( HWND hwnd, int x, int y, UINT keyFlags )
{
   LPBIGEDIT lpbe;
   POINT pt;

   lpbe = GetBBlock( hwnd );
   pt.x = x;
   pt.y = y;
   if( fDragging )
      {
      SELRANGE lSelPos;
      int nSavedYShift;

      do {
         nSavedYShift = lpbe->nYShift;
         lSelPos = PointToOffset( hwnd, pt.x, pt.y, TRUE );
         if( lSelPos == DOORSTOP )
            break;
         if( lSelPos != lpbe->lSelEnd )
            {
            SELRANGE lSelLast;
   
            lSelLast = lpbe->lSelEnd;
            lpbe->lSelEnd = lSelPos;
            lpbe->fSelection = TRUE;
            BigEditWnd_InvalidateSelection( hwnd, TRUE, lSelLast, lSelPos );
            }
         GetCursorPos( &pt );
         ScreenToClient( hwnd, &pt );
         }
      while( lpbe->nYShift != nSavedYShift );
      lpbe->fSelection = TRUE;
      }
   else if( lpbe->fUseHotlinks )
      {
      char szText[ 500 ];

      /* See if we're over an URL link and change the cursor
       * to the hand cursor if so.
       */
      if( lpbe->fOverHotspot )
         {
         /* If we're over a hotspot, then lpbe->rcHotspot is
          * valid so see if we've moved out of it.
          */
         if( PtInRect( &lpbe->rcHotspot, pt ) )
            return;
         lpbe->fOverHotspot = FALSE;
         }

      /* Locate the hotspot nearest to the cursor
       */
      if( TestForHotlink( hwnd, x, y, szText, sizeof(szText), &lpbe->rcHotspot ) )
         {
         SetCursor( LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_HOTSPOT) ) );
         lpbe->fOverHotspot = TRUE;
         }
      else
         lpbe->fOverHotspot = FALSE;
      }
}

/* This function checks whether the (x,y) coordinates cover a hotlink and, if so,
 * returns the hotlink in pszText and optionally the rectangle of the hotlink
 * in prc.
 */
BOOL FASTCALL TestForHotlink( HWND hwnd, int x, int y, char * pszText, int cbMax, RECT * prc )
{
   LPBIGEDIT lpbe;
   int line;

   lpbe = GetBBlock( hwnd );
   line = min( ( ( y - 2 ) / lpbe->dyLine ) + lpbe->nYShift, lpbe->cLines - 1 );
   if( NULL != prc )
      SetRectEmpty( prc );
   if( line >= lpbe->nYShift && line < lpbe->nYShift + lpbe->cyPage )
      {
      LPSELRANGE lpib;

      lpib = &lpbe->lplIndex[ line ];
      if( lpib[ 1 ] != DOORSTOP )
         {
         HPSTR hpszWordStart;
         HFONT hOldFont;
         int xWordStart;
         HPSTR hpsz;
         int cbLine;
         HDC hdc;
         int w;
         int c;

         /* Scan hpsz until GetTabbedTextExtent finds our
          * horizontal offset.
          */
         hpsz = lpbe->hpText + *lpib;
         cbLine = (int)( lpib[ 1 ] - lpib[ 0 ] );
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         if( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES )
         {
            HPSTR hpszLink;
            BOOL fInLink;
            int cxStart;
            char ch;
            int col;
            int cx;
            int c;

            /* If we use attributes, things are quite easy. Scan
             * the line looking for a link sequence and if the
             * x coordinate occurs within that link, exit now.
             */
            cx = 0;
            col = 0;
            cxStart = 0;
            fInLink = FALSE;
            hpszLink = NULL;
            x -= 2 + lpbe->nXShift * lpbe->dxChar;
            for( c = 0; c < cbLine; ++c )
            {
               if( ( ch = *hpsz++ ) == CH_LINK )
                  {
                  hpszLink = hpsz;
                  fInLink = TRUE;
                  cxStart = cx;
                  }
               else if( ch == CH_NORMAL && fInLink )
                  {
                  if( x >= cxStart && x < cx )
                     {
                     if( prc )
                        {
                        prc->left = cxStart;
                        prc->right = cx;
                        prc->top = y;
                        prc->bottom = y + lpbe->dyLine;
                        }
                     --cbMax;
                     while( cbMax-- && *hpszLink != CH_NORMAL )
                        *pszText++ = *hpszLink++;
                     *pszText = '\0';
                     return( TRUE );
                     }
                  }
               else if( !IsStyleChr( ch ) )
                  if( '\t' == ch )
                     cx += ( 8 - ( col % 8 ) ) * lpbe->dxChar;
                  else
                     {
                     cx += lpbe->lpChrWidths[ (BYTE)ch ];
                     ++col;
                     }
            }
            /* Not ideal but makes it behave the same as without styles
             */
            if (fInLink)         /*!!SM!!*/
            {
               while( cbMax-- && *hpszLink != CH_NORMAL && !IsLBChar(*hpszLink) )
                  *pszText++ = *hpszLink++;
               *pszText = '\0';
//                return ( TRUE ); 
            }
         }
         else
            {
            c = 0;
            w = 0;
            xWordStart = 0;
            hdc = GetDC( hwnd );
            hpszWordStart = hpsz;
            hOldFont = SelectFont( hdc, lpbe->hFont );
            while( c < cbLine )
               {
               int cbLength;
               int cx;

               cx = LOWORD( GetTabbedTextExtent( hdc, hpsz, c, 0, NULL ) );
               if( lpbe->fHotLinks && IsHotlinkPrefix( &hpsz[ c ], &cbLength ) )
                  if( cx < ( x - ( 2 + lpbe->nXShift * lpbe->dxChar ) ) )
                     {
                     register int k;

                     w = c;
                     xWordStart = cx;

                     /* Copy in the word.
                      */

                     for( k = 0; 
                         ( IsHTTPChr( hpsz[ w ] ) || ( ( hpsz[ w ] == ',' || hpsz[ w ] == '\'' ) && 
                           !IsWBChar( hpsz[ w + 1 ] ) &&
                           !IsWBChar( hpsz[ w + 2 ] ) ) ) &&
                        k < cbMax /*&& w < cbLine*/; //!!SM!! if we're doing the wrap, then ignore line end
                        ++k, ++w )
                        pszText[ k ] = hpsz[ w ];

                     // SAM addition - if it's a Copied Message Hotlink, skip up over the message number
                     if(cbLength == 18)
                        {
                           while(pszText[ k ] == ' ')
                              {
                              pszText[ ++k ] = hpsz[ ++w ];
                              }
                           while(pszText[ k ] != ' ')
                              {
                              pszText[ ++k ] = hpsz[ ++w ];
                              }
                        }

                     if( k > 0 && IsURLTerminator( pszText[ k - 1 ] ) )
                        --k;
                     pszText[ k ] = '\0';

                     /* Store the rectangle enclosing the word. The whole URL
                      * thing must be longer than just http:// (ie. >7 chrs)
                      */
                     if( k > cbLength )
                        {
                        int cxWord;

                        cxWord = LOWORD( GetTabbedTextExtent( hdc, pszText, k, 0, NULL ) );
                        if( xWordStart + cxWord > ( x - ( 2 + lpbe->nXShift * lpbe->dxChar ) ) )
                           {
                           y = 2 + ( ( line - lpbe->nYShift ) * lpbe->dyLine );
                           if( NULL != prc )
                              SetRect( prc, xWordStart, y, xWordStart + cxWord, y + lpbe->dyLine );
                           SelectFont( hdc, hOldFont );
                           ReleaseDC( hwnd, hdc );
                           return( TRUE );
                           }
                        }
                     //break;
                     }
               ++c;
               }
            SelectFont( hdc, hOldFont );
            ReleaseDC( hwnd, hdc );
            }
         }
      }
   return( FALSE );
}

BOOL FASTCALL IsPrefixOK( char pChar ) 
{
   return 
      ( !pChar || 
         ( pChar && 
          ( 
            (
              !ispunct( pChar ) || 
              pChar == '(' || 
              pChar == '[' || 
              pChar == '<' || 
              pChar == ':' ||
              pChar == '\''
            ) 
            &&
            (
             !isalpha( pChar )
            )
          ) 
        )
      );

}
/* This function checks whether the specified text is a hotlink
 * prefix.
 */
BOOL FASTCALL IsHotlinkPrefix( char * pszText, int * cbMatch )
{
// if( !fHotLinks )
//    return( FALSE );
   if( _strnicmp( pszText, "http://", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "file://", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( ( _strnicmp( pszText, "www.", *cbMatch = 4 ) == 0 ) && IsPrefixOK( *( pszText - 1 ) ) && isalnum( *( pszText + 4 ) ) )
      return( TRUE );
   if( _strnicmp( pszText, "https://", *cbMatch = 8 ) == 0 )
      return( TRUE );
   if( ( _strnicmp( pszText, "cix:", *cbMatch = 4 ) == 0 ) && IsPrefixOK( *( pszText - 1 ) ) )
       return( TRUE );
   if( _strnicmp( pszText, "**COPIED FROM: >>>", *cbMatch = 18 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "ftp://", *cbMatch = 6 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "cixfile:", *cbMatch = 8 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "mailto:", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( ( _strnicmp( pszText, "news:", *cbMatch = 5 ) == 0 ) &&  IsPrefixOK( *( pszText - 1 ) ) && ( !ispunct( *( pszText + 5 ) ) || *( pszText + 5 ) =='/' ) )
      return( TRUE );
   if( _strnicmp( pszText, "gopher://", *cbMatch = 9 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "callto:", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "telnet://", *cbMatch = 9 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "irc://", *cbMatch = 6 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "cixchat://", *cbMatch = 10 ) == 0 )
      return( TRUE );
   return( FALSE );
}

/* This function copies the selection to a global memory block.
 */
HGLOBAL FASTCALL BigEditWnd_CopySelection( HWND hwnd )
{
   LPBIGEDIT lpbe;
   SELRANGE cbSelSize;
   SELRANGE lSelStart;
   SELRANGE lSelEnd;
   HGLOBAL hGlb;
   HPSTR hpText;

   /* Compute hpText to point to the start of the
    * current selection.
    */
   lpbe = GetBBlock( hwnd );
   ASSERT(lpbe->fSelection);
   BigEditWnd_GetSelection( lpbe, &lSelStart, &lSelEnd );
   hpText = lpbe->hpText + lSelStart;

   /* Allocate a global block of memory to hold the selection,
    * plus a NULL terminator.
    */
   cbSelSize = lSelEnd - lSelStart;
   if( hGlb = GlobalAlloc( GHND, cbSelSize + 1 ) )
      {
      HPSTR hpBuf;

      if( hpBuf = (HPSTR)GlobalLock( hGlb ) )
         {
         CopyFormattedLine( hpBuf, hpText, cbSelSize );
         GlobalUnlock( hGlb );
         return( hGlb );
         }
      GlobalFree( hGlb );
      }

   /* Replace with the specified text, then invalidate
    * to force a repaint.
    */
   return( NULL );
}

/* This function copies cStream bytes of characters from pSrc to pDest,
 * converting style characters.
 */
DWORD FASTCALL CopyFormattedLine( HPSTR pDest, HPSTR pSrc, DWORD cStream )
{
   HPSTR pDestStart;
   char chMarker;
// DWORD lCount;

   pDestStart = pDest;
   chMarker = 0;
   while( cStream-- )
   {
      if( *pSrc == CH_BOLD )
         *pDest++ = chMarker = '*';
      else if( *pSrc == CH_ITALIC )
         *pDest++ = chMarker = '/';
      else if( *pSrc == CH_UNDERLINE )
         *pDest++ = chMarker = '_';
      else if( ( *pSrc == CH_LINK ))
         ;
      else if( *pSrc == CH_NORMAL )
      {
         if( chMarker )
            *pDest++ = chMarker;
         chMarker = 0;
      }
      else 
      {
         *pDest++ = *pSrc;
      }
      ++pSrc;
   }
   *pDest = '\0';
   return( pDest - pDestStart );
}

/* This function retrieves the start and end offsets of the current
 * selection, arranged such that start < end.
 */
void FASTCALL BigEditWnd_GetSelection( LPBIGEDIT lpbe, SELRANGE * psrStart, SELRANGE * psrEnd )
{
   /* Adjust start and end so that they have values within the range
    * of the text. This fixes a bug where copy tried to copy text
    * from 23 to 0xFFFFFFFF for instance.
    * YH 08/05/96
    */

   /*
   if ( lpbe->lSelEnd > lpbe->lSelStart)
      lpbe->lSelStart -= CountFormatCharacters( lpbe->hpText, lpbe->lSelEnd - lpbe->lSelStart);
   else
      lpbe->lSelStart += CountFormatCharacters( lpbe->hpText, lpbe->lSelStart - lpbe->lSelEnd);
    */
   *psrStart = min( lpbe->cbTextLength, lpbe->lSelStart );
   *psrEnd = min( lpbe->cbTextLength, lpbe->lSelEnd );

   if( *psrStart > *psrEnd )
      {
      SELRANGE srTmp;

      srTmp = *psrEnd;
      *psrEnd = *psrStart;
      *psrStart = srTmp;
      }
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL BigEditWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   BigEditWnd_ShowCaret( hwnd, lpbe );
   BigEditWnd_SetCursor( hwnd );
}

/* This function handles the WM_KILLFOCUS message.
 */
void FASTCALL BigEditWnd_OnKillFocus( HWND hwnd, HWND hwndNewFocus )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   BigEditWnd_HideCaret( hwnd, lpbe );
}

/* This function shows the caret if we have it.
 */
void FASTCALL BigEditWnd_ShowCaret( HWND hwnd, LPBIGEDIT lpbe )
{
   if ( lpbe == NULL)
      return;

   if( !lpbe->fWndHasCaret )
      {
      CreateCaret( hwnd, NULL, 2, lpbe->dyLine );
      ShowCaret( hwnd );
      lpbe->fHasCaret = TRUE;
      lpbe->fWndHasCaret = TRUE;
      }
}

/* This function hides the caret if we have it.
 */
void FASTCALL BigEditWnd_HideCaret( HWND hwnd, LPBIGEDIT lpbe )
{
   if ( lpbe == NULL)
      return;

   if( lpbe->fWndHasCaret )
      {
      HideCaret( hwnd );
      DestroyCaret();
      lpbe->fHasCaret = FALSE;
      lpbe->fWndHasCaret = FALSE;
      }
}

/* This function adjusts the caret position in the big edit window.
 */
void FASTCALL BigEditWnd_SetCursor( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   
   if ( lpbe == NULL)
      return;

   if( lpbe->fWndHasCaret )
      {
      BOOL fCaretInView;

      fCaretInView = TRUE;
      if( fCaretInView && !lpbe->fHasCaret )
         {
         CreateCaret( hwnd, NULL, 2, lpbe->dyLine );
         ShowCaret( hwnd );
         lpbe->fHasCaret = TRUE;
         }
      else if( !fCaretInView && lpbe->fHasCaret )
         {
         HideCaret( hwnd );
         DestroyCaret();
         lpbe->fHasCaret = FALSE;
         }
      if( lpbe->fHasCaret )
         {
         int x, y;

         OffsetToPoint( hwnd, lpbe->lSelStart, &x, &y );
         SetCaretPos( x, y );
         }
      }
}

/* This function converts a pixel offset in the big edit client
 * window into a line offset from the start of the text.
 */
SELRANGE FASTCALL PointToOffset( HWND hwnd, int x, int y, BOOL fScroll )
{
   LPBIGEDIT lpbe;
   int line;
   LPSELRANGE lpib;

   /* First compute the line.
    */
   lpbe = GetBBlock( hwnd );
   
   if (lpbe == NULL) 
      return( DOORSTOP );

   line = min( ( ( y - 2 ) / lpbe->dyLine ) + lpbe->nYShift, lpbe->cLines - 1 );

   /* If the line is above or below the view window, scroll
    * the window as appropriate.
    */
   if( line < lpbe->nYShift )
      {
      POINT pt;

      if( !fScroll )
         return( DOORSTOP );
      BigEditWnd_HideCaret( hwnd, lpbe );
      SendMessage( hwnd, WM_VSCROLL, SB_LINEUP, 0L );
      BigEditWnd_ShowCaret( hwnd, lpbe );
      ScrollDelay();
      GetCursorPos( &pt );
      ScreenToClient( hwnd, &pt );
      }
   else if( line >= lpbe->nYShift + lpbe->cyPage )
      {
      POINT pt;

      if( !fScroll )
         return( DOORSTOP );
      BigEditWnd_HideCaret( hwnd, lpbe );
      SendMessage( hwnd, WM_VSCROLL, SB_LINEDOWN, 0L );
      BigEditWnd_ShowCaret( hwnd, lpbe );
      ScrollDelay();
      GetCursorPos( &pt );
      ScreenToClient( hwnd, &pt );
      }

   /* Allocate and prepare selector for text.
    */
   if( line >= 0 )
      {
      lpib = &lpbe->lplIndex[ line ];
      if( lpib[ 1 ] != DOORSTOP )
         {
         HPSTR hpszStart;
         HPSTR hpsz;
         int cbLine;
         int col;
         int cx;
         int c;

         /* Scan hpsz until we find our
          * horizontal offset.
          */
         hpsz = lpbe->hpText + *lpib;
         hpszStart = hpsz;
         cbLine = (int)( lpib[ 1 ] - lpib[ 0 ] );
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         if( cbLine && IsLBChar(hpsz[ cbLine - 1 ]) )
            --cbLine;
         cx = 0;
         c = 0;
         col = 0;
         while( c < cbLine )
            {
            char ch;

            ch = *hpsz;
            if( IsStyleChr( ch ) )
               ch = *( hpsz + 1 );
            if( '\t' == ch )
               cx += ( 8 - ( col % 8 ) ) * lpbe->dxChar;
            else
               {
               cx += lpbe->lpChrWidths[ (BYTE)ch ];
               ++col;
               }
            if( cx > ( x - 2 + lpbe->nXShift * lpbe->dxChar ) )
               break;
            if( IsStyleChr( *hpsz ) )
               {
               ++hpsz;
               ++c;
               }
            ++hpsz;
            ++c;
            }
         return( *lpib + ( hpsz - hpszStart ) );
         }
      }
   return( DOORSTOP );
}

/* This function issues a short delay between scrolling
 */
void FASTCALL ScrollDelay( void )
{
   DWORD dwTicks;

   dwTicks = GetTickCount();
   while( GetTickCount() - dwTicks < 50 );
}

/* This function converts a character offset in the big edit client
 * window into a pixel offset from the top of the window.
 */
void FASTCALL OffsetToPoint( HWND hwnd, SELRANGE lOffset, int * px, int * py )
{
   LPSELRANGE lpib;
   LPBIGEDIT lpbe;
   HPSTR hpsz;
   int cx;

   /* First compute the vertical.
    */
   if ( !IsWindow(hwnd) )
      return;

   lpbe = GetBBlock( hwnd );
   *py = 2;
   *px = 2;
   
   lpib = &lpbe->lplIndex[ lpbe->nYShift ];
   if( *lpib != DOORSTOP )
      {
      SELRANGE cbLen;

      /* Locate the line on which the offset
       * appears.
       */
      while( lpib[ 1 ] != DOORSTOP && lOffset >= lpib[ 1 ] )
         {
         *py += lpbe->dyLine;
         ++lpib;
         }

      /* Allocate and prepare selector for text.
       */
      if( lpib[ 1 ] != DOORSTOP )
         {
         int col;

         lOffset -= *lpib;
         hpsz = lpbe->hpText + *lpib;
         cbLen = lpib[ 1 ] - lpib[ 0 ];
         if( lOffset > cbLen )
            lOffset = cbLen;
         cx = 0;
         col = 0;
         while( lOffset-- )
            {
            char ch;

            ch = *hpsz++;
            if( !IsStyleChr( ch ) )
               if( '\t' == ch )
                  cx += ( 8 - ( col % 8 ) ) * lpbe->dxChar;
               else
                  {
                  cx += lpbe->lpChrWidths[ (BYTE)ch ];
                  ++col;
                  }
            }
         *px += cx - lpbe->nXShift * lpbe->dxChar;
         }
      }
}

/* This function handles the WM_GETFONT message.
 */
HFONT FASTCALL BigEditWnd_OnGetFont( HWND hwndCtl )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwndCtl );
   return( lpbe->hFont );
}

/* This function handles the WM_SETFONT message.
 */
void FASTCALL BigEditWnd_OnSetFont( HWND hwndCtl, HFONT hfont, BOOL fRedraw )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwndCtl );
   BigEdit_DeleteCachedFonts( lpbe );
   lpbe->hFont = hfont ? hfont : GetStockFont( SYSTEM_FONT );
   ComputeCharWidths( hwndCtl, lpbe );
   CalculateLongestLine( hwndCtl );
   AdjustScrolls( hwndCtl, FALSE );
   if( fRedraw )
      {
      InvalidateRect( hwndCtl, NULL, TRUE );
      UpdateWindow( hwndCtl );
      }
}

/* This function computes the widths of every character in the
 * newly selected font.
 */
void FASTCALL ComputeCharWidths( HWND hwnd, LPBIGEDIT lpbe )
{
   if( NULL != lpbe->lpChrWidths )
      FreeMemory( &lpbe->lpChrWidths );
   if( fNewMemory( &lpbe->lpChrWidths, 256 * sizeof(int) ) )
      {
      HFONT hOldFont;
      HDC hdc;

      hdc = GetDC( hwnd );
      hOldFont = SelectFont( hdc, lpbe->hFont );
   #ifdef WIN32
      if( !fWin95 )
         GetCharWidth32( hdc, 0, 255, lpbe->lpChrWidths );
      else
         GetCharWidth( hdc, 0, 255, lpbe->lpChrWidths );
   #else
      GetCharWidth( hdc, 0, 255, lpbe->lpChrWidths );
   #endif
      SelectFont( hdc, hOldFont );
      ReleaseDC( hwnd, hdc );
      }
}

/* This function handles the WM_NCCREATE message.
 */
BOOL FASTCALL BigEditWnd_OnNCCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
   /* If WS_BORDER specified, remove it and
    * set BES_3DBORDER instead.
    */
   if( lpCreateStruct->style & WS_BORDER )
      {
      lpCreateStruct->style &= ~WS_BORDER;
      lpCreateStruct->style |= BES_3DBORDER;
      SetWindowStyle( hwnd, lpCreateStruct->style );
      }
   return( FORWARD_WM_NCCREATE( hwnd, lpCreateStruct, DefWindowProc ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL BigEditWnd_OnCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
   LPBIGEDIT lpbe;

   INITIALISE_PTR(lpbe);

   /* Get and save Windows version number
    */
   wWinVer = LOWORD( GetVersion() );
   wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );

   fWin95 = wWinVer >= 0x035F;

   if( fNewMemory( &lpbe, sizeof( BIGEDIT ) ) )
      {
      INITIALISE_PTR(lpbe->hpText);
      INITIALISE_PTR(lpbe->lplIndex);

      /* Initialise the pointers.
       */
      if( fNewMemory32( &lpbe->hpText, 4 ) )
         {
         if( fNewMemory32( &(HPSTR)lpbe->lplIndex, MAX_LINES * sizeof(LPSELRANGE) ) )
            {
            TEXTMETRIC tm;
            HFONT hOldFont;
            HDC hdc;

            /* Mark the end of the lines.
             */
            lpbe->lplIndex[ 0 ] = 0;
            lpbe->lplIndex[ 1 ] = DOORSTOP;

            /* Store the font.
             */
            lpbe->hCachedUFont = NULL;
            lpbe->hCachedIFont = NULL;
            lpbe->hCachedBFont = NULL;
            lpbe->hFont = GetStockFont( SYSTEM_FONT );
            lpbe->lpChrWidths = NULL;
            ComputeCharWidths( hwnd, lpbe );
            hdc = GetDC( hwnd );
            if( lpCreateStruct->style & BES_USEATTRIBUTES )
               {
               hOldFont = SelectFont( hdc, BigEdit_NeedUnderlineFont( lpbe ) );
               GetTextMetrics( hdc, &tm );
               lpbe->dyLine = tm.tmHeight;
               SelectFont( hdc, BigEdit_NeedBoldFont( lpbe ) );
               GetTextMetrics( hdc, &tm );
               lpbe->dyLine = max( tm.tmHeight, lpbe->dyLine );
               }
            else
               {
               hOldFont = SelectFont( hdc, lpbe->hFont );
               GetTextMetrics( hdc, &tm );
               lpbe->dyLine = tm.tmHeight;
               }
            lpbe->dxChar = tm.tmAveCharWidth;
//          lpbe->dxChar = tm.tmMaxCharWidth;
            SelectFont( hdc, hOldFont );
            ReleaseDC( hwnd, hdc );

            /* Set the default colours.
             */
            lpbe->crMsgWnd = GetSysColor( COLOR_WINDOW );
            lpbe->crMsgText = GetSysColor( COLOR_WINDOWTEXT );

            /* Save the edit record.
             */
            lpbe->fColourQuotes = FALSE;
            lpbe->fOverHotspot = FALSE;
            lpbe->pszQuotedText = NULL;
            lpbe->fSelection = FALSE;
            lpbe->fUseHotlinks = FALSE;
            lpbe->fWordWrap = FALSE;
            lpbe->fHasCaret = FALSE;
            lpbe->fWndHasCaret = FALSE;
            lpbe->nYShift = 0;
            lpbe->nXShift = 0;
            lpbe->cbTextLength = 0;
            PutBBlock( hwnd, lpbe );
            SetScrollRange( hwnd, SB_VERT, 0, 0, TRUE );
            return( TRUE );
            }
         FreeMemory32( &lpbe->hpText );
         }
      FreeMemory( &lpbe );
      }
   return( FALSE );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL BigEditWnd_OnDestroy( HWND hwnd )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   BigEdit_DeleteCachedFonts( lpbe );
   if( NULL != lpbe->pszQuotedText )
      FreeMemory( &lpbe->pszQuotedText );
   FreeMemory32( &lpbe->hpText );
   FreeMemory32( &(HPSTR)lpbe->lplIndex );
   FreeMemory( &lpbe );
   PutBBlock( hwnd, NULL );
}

/* This function handles erasing the terminal window
 * background.
 */
BOOL FASTCALL BigEditWnd_OnEraseBkGnd( HWND hwnd, HDC hdc )
{
   return( TRUE );
}

/* This function handles the WM_NCPAINT message.
 */
void FASTCALL BigEditWnd_OnNCPaint( HWND hwnd, HRGN hRgn )
{
   if( GetWindowStyle( hwnd ) & BES_3DBORDER )
      {
      RECT rc;
      HDC hdc;

      /* Since we're doing NC painting, client things do not
       * apply to us.
       */
      hdc = GetDC( GetParent( hwnd ) );
      GetWindowRect( hwnd, &rc );
      MapWindowPoints( NULL, GetParent( hwnd ), (LPPOINT)&rc, 2 );
      rc.right += 2;
      rc.bottom += 2;
      rc.left -= 2;
      rc.top -= 2;
      Draw3DFrame( hdc, &rc );
      ReleaseDC( GetParent( hwnd ), hdc );
      }
   FORWARD_WM_NCPAINT( hwnd, hRgn, DefWindowProc );
}


DWORD FASTCALL Ameol_TabbedTextOut(HDC hDC, int X, int Y, LPCSTR lpString, int nCount, int nTabPositions, LPINT lpnTabStopPositions, int nTabOrigin)
{
   LPSTR lString;
   int i,j;
   DWORD ret;

   INITIALISE_PTR(lString);

   i = 0;
   j = 0;
   if( fNewMemory32( &lString, nCount + 1 ) )
   {
      while( i < nCount)
      {
         if(!IsStyleChr(lpString[i]) )
         {
            lString[j] = lpString[i];
            j++;
         }
         i++;
      }
      ret = TabbedTextOut(hDC, X, Y, lString, j, nTabPositions, lpnTabStopPositions, nTabOrigin);
      FreeMemory32( &lString );
      return ret;
   }
   else
      return 0;
}

/* This function handles the WM_PAINT message.
 */
void FASTCALL BigEditWnd_OnPaint( HWND hwnd )
{
   LPBIGEDIT lpbe;
   HPSTR hpsz;
   LPSELRANGE lpib;
   PAINTSTRUCT wps;
   RECT rect;
   int dx, y;
   SELRANGE lSelStart;
   SELRANGE lSelEnd;
   BOOL fInSel;
   HFONT hOldFont;
   BOOL fCtlMarkers;

   /* Begin painting.
    */
   BeginPaint( hwnd, &wps );
   lpbe = GetBBlock( hwnd );
   hOldFont = SelectFont( wps.hdc, lpbe->hFont );

   /* Get line number and map entry.
    */
   hpsz = lpbe->hpText;
   fCtlMarkers = ( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES ) ? TRUE : FALSE;
   lpib = &lpbe->lplIndex[ lpbe->nYShift ];
   dx = lpbe->dxChar * lpbe->nXShift;

   /* Get the selection.
    */
   BigEditWnd_GetSelection( lpbe, &lSelStart, &lSelEnd );

   /* Loop over lines in client area.
    */
   GetClientRect( hwnd, &rect );
   SetTextColor( wps.hdc, lpbe->crMsgText );
   SetBkColor( wps.hdc, lpbe->crMsgWnd );
   fInSel = FALSE;

   /* Erase the top margin if it is within the clipping region.
    */
   if( wps.rcPaint.top < 2 )
      {
      RECT rc;

      SetRect( &rc, 0, 0, wps.rcPaint.right, 2 );
      ExtTextOut( wps.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
      }

   /* Erase the left margin if it is within the clipping region.
    */
   if( wps.rcPaint.left < 2 )
      {
      RECT rc;

      SetRect( &rc, 0, 0, 2, wps.rcPaint.bottom );
      ExtTextOut( wps.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
      }

   /* Handle selection starting before top line.
    */
   if( lpbe->fSelection && lSelStart <= lpib[ 0 ] && lSelEnd > lpib[ 0 ] )
      {
      SetTextColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
      SetBkColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHT ) );
      fInSel = TRUE;
      }

#ifdef MULTILINESTYLE
   fInStyle = FALSE;
#endif MULTILINESTYLE

   /* Loop for every line in the clipping region and update it.
    */
   for( y = 2; y + lpbe->dyLine <= rect.bottom; y += lpbe->dyLine, lpib++ )
      {
      SELRANGE lpibStart;
      char * pszQuotePtr;
      char * pszLinePtr;
      char szLine[ 512 ];
      SELRANGE lpibEnd;
      BOOL fPaintLine;
      HPSTR hpszPtr;
      BOOL fInQuote;
      int cbLine;
      int cDiff;
      int x;

      /* Initialise for each line.
       */
      if( DOORSTOP == lpib[ 1 ] )
         break;
      fPaintLine = y + lpbe->dyLine > wps.rcPaint.top;
      cbLine = (int)( lpib[ 1 ] - lpib[ 0 ] );
      x = 2 - dx;
      hpszPtr = hpsz + *lpib;
      if( cbLine && IsLBChar(hpszPtr[ cbLine - 1 ]) )
         --cbLine;
      if( cbLine && IsLBChar(hpszPtr[ cbLine - 1 ] ) )
         --cbLine;
      if( cbLine && IsLBChar(hpszPtr[ cbLine - 1 ] ) )
         --cbLine;

      /* Make a local copy of the line.
       */
      if( cbLine > 511 )
         cbLine = 511;
      memcpy( szLine, hpszPtr, cbLine );
      szLine[ cbLine ] = '\0';

      /* Quoting. See if this line starts a quote and, if so, set the
       * message colour to the quote colour.
       */
      pszLinePtr = szLine;
      pszQuotePtr = szLine;
      while( *pszQuotePtr && *pszQuotePtr == ' ' )
         ++pszQuotePtr;
      fInQuote = FALSE;
      if( lpbe->fColourQuotes && strchr( lpbe->pszQuotedText, *pszQuotePtr ) != NULL )
         fInQuote = TRUE;

      /* Indexes within segments
       */
      lpibStart = lpib[ 0 ];
      lpibEnd = lpib[ 1 ];
      cDiff = 0;

      /* Scan and look for text emphasis characters. To speed things up, we'll
       * work in chunks comprising everything up to the TEC, then the TEC, then
       * repeat until the whole line is done.
       */
      do {
         STYLETABLE style;
         UINT cbSegment;
         UINT cbToSelEnd;

         /* Locate the segment we're going to process.
          */
         style.pszLine = pszLinePtr;
         cbSegment = LocateNextSegment( lpbe, &style, fCtlMarkers, &cDiff );
         lpibEnd = lpibStart + cbSegment + cDiff;
         if( 0 == cbLine - cbSegment )
            lpibEnd += 2;
         SelectFont( wps.hdc, style.hFont );

         /* Quote colour always overrides foreground colour
          */
         if( fInQuote )
            style.crText = lpbe->crQuoteText;

         /* Handle selection starting on this line.
          */
         if( ( lpbe->fSelection ) && ( lSelStart  >=  lpibStart ) && ( lSelStart < lpibEnd )  && !fInSel )
         {
            UINT cbToSel;

            /* The selected text starts on this line. An added complication is that
             * it may also finish on the same line. So paint up to the selection in
             * the normal colours then revert to the highlight colours for painting
             * the selected text. Then, if the selection finishes on this line, revert
             * back to the normal colours.
             */
            cbToSel = 0;
            if( fPaintLine )
            {
               if( lSelStart > lpibStart )
                  cbToSel = (int)( lSelStart - lpibStart );
               else
                  cbToSel = 0;
               SetTextColor( wps.hdc, style.crText );
               x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine, cbToSel, 0, NULL, 2 - dx ) );
            }
            if( (lSelEnd >= lpibStart) && (lSelEnd < lpibEnd) )
            {
               if( fPaintLine )
               {

                  cbToSelEnd = (int)( lSelEnd - lpibStart ) - cbToSel ;
                  SetTextColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
                  SetBkColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHT ) );
//                if( IsStyleChr( *(style.pszLine + (cbToSel +  cbToSelEnd))) )
//                   x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine + cbToSel, cbToSelEnd - 1, 0, NULL, 2 - dx ) );
//                else
                  x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine + cbToSel, cbToSelEnd, 0, NULL, 2 - dx ) );
                  SetTextColor( wps.hdc, style.crText );
                  SetBkColor( wps.hdc, style.crBack );
                  cbToSelEnd += cbToSel;
                  if( cbToSelEnd < cbSegment )
                     x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine + cbToSelEnd, cbSegment - cbToSelEnd, 0, NULL, 2 - dx ) );
               }
            }
            else
            {
               cbToSelEnd = (int)( lSelEnd - lpibStart ) - cbToSel ;

               SetTextColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
               SetBkColor( wps.hdc, GetSysColor( COLOR_HIGHLIGHT ) );
               if( fPaintLine && cbToSel < cbSegment )
                  x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine + cbToSel, cbSegment - cbToSel, 0, NULL, 2 - dx ) );
               fInSel = TRUE;
            }
         }
         else if( lpbe->fSelection && lSelEnd >= lpibStart && lSelEnd < lpibEnd && fInSel )
            {
            if( !fPaintLine )
               {
               SetTextColor( wps.hdc, style.crText );
               SetBkColor( wps.hdc, style.crBack );
               fInSel = FALSE;
               }
            else
               {
               int cbToSelEnd;

               /* The selected text finishes on this line. Draw up to the selection end
                * in the highlight colour, then revert back to the normal colours for painting
                * the rest of the line.
                */
               cbToSelEnd = (int)( lSelEnd - lpibStart );
               if( cDiff && cbToSelEnd )
                  --cbToSelEnd;
               x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine, cbToSelEnd, 0, NULL, 2 - dx ) );
               SetTextColor( wps.hdc, style.crText );
               SetBkColor( wps.hdc, style.crBack );
               fInSel = FALSE;
               x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine + cbToSelEnd, cbSegment - cbToSelEnd, 0, NULL, 2 - dx ) );
               }
            }
         else if( fPaintLine )
            {
            if( !fInSel )
               SetTextColor( wps.hdc, style.crText );
            x += LOWORD( Ameol_TabbedTextOut( wps.hdc, x, y, style.pszLine, cbSegment, 0, NULL, 2 - dx ) );
            }

         /* Move to next segment.
          */
         cbLine -= cbSegment + cDiff;
         pszLinePtr = style.pszLine + cbSegment;
         if( cDiff )
            ++pszLinePtr;
         lpibStart += cbSegment + cDiff;
         }
      while( cbLine > 0 );

      /* Invalidate to right of end of line.
       */
      if( x < wps.rcPaint.right )
         {
         COLORREF crBk;
         RECT rc;

         SetRect( &rc, x, y, wps.rcPaint.right, y + lpbe->dyLine );
         if( !fInSel )
            ExtTextOut( wps.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
         else
            {
            crBk = SetBkColor( wps.hdc, lpbe->crMsgWnd );
            ExtTextOut( wps.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
            SetBkColor( wps.hdc, crBk );
            }
         }
      }

   /* Erase the area below the last line if it is within the
    * clipping region.
    */
   if( y < rect.bottom )
      {
      RECT rc;

      if( fInSel )
         {
         SetTextColor( wps.hdc, lpbe->crMsgText );
         SetBkColor( wps.hdc, lpbe->crMsgWnd );
         }
      SetRect( &rc, 0, y, wps.rcPaint.right, wps.rcPaint.bottom );
      ExtTextOut( wps.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
      }
   SelectFont( wps.hdc, hOldFont );
   EndPaint( hwnd, &wps );
}

/* This function scans a line looking for a segment. A segment
 * is everything up to or from a text emphasis string. The function
 * returns the length of the segment and fills out the colour and
 * font table for the segment.
 */
UINT FASTCALL LocateNextSegment( LPBIGEDIT lpbe, STYLETABLE * pStyle, BOOL fCtlMarkers, int * pDiff )
{
   int cbSegment;
   int cbLinkSize;
   char * pSegEnd = NULL;

   /* Scan looking for <CTL> marker and if found, use that.
    */
   cbLinkSize = 0;
   *pDiff = 0;
   while( fCtlMarkers && (pSegEnd = LocateStyleChr( pStyle->pszLine )))
      {
      char chMarker;

      /* Found. Everything between here and the start of the URL is
       * drawn in normal display.
       */
      if( pSegEnd > pStyle->pszLine )
         {
#ifdef MULTILINESTYLE
         if (!fInStyle)      /*!!SM!!*/
         {
#endif MULTILINESTYLE
            pStyle->crText = lpbe->crMsgText;
            pStyle->crBack = lpbe->crMsgWnd;
            pStyle->hFont = lpbe->hFont;
#ifdef MULTILINESTYLE
         }
         else
            fInStyle = FALSE; /*!!SM!!*/
#endif MULTILINESTYLE
         return( pSegEnd - pStyle->pszLine );
         }

      /* We're point to a segment, so decode the character
       */
      *pDiff = 2;
      pStyle->pszLine += 1;
      switch( chMarker = *pSegEnd++ )
         {
         case CH_BOLD:
            pStyle->crText = lpbe->crMsgText;
            pStyle->crBack = lpbe->crMsgWnd;
            pStyle->hFont = BigEdit_NeedBoldFont( lpbe );
#ifdef MULTILINESTYLE
            fInStyle = TRUE;
#endif MULTILINESTYLE
            break;

         case CH_UNDERLINE:
            pStyle->crText = lpbe->crMsgText;
            pStyle->crBack = lpbe->crMsgWnd;
            pStyle->hFont = BigEdit_NeedUnderlineFont( lpbe );
#ifdef MULTILINESTYLE
            fInStyle = TRUE;
#endif MULTILINESTYLE
            break;

         case CH_ITALIC:
            pStyle->crText = lpbe->crMsgText;
            pStyle->crBack = lpbe->crMsgWnd;
            pStyle->hFont = BigEdit_NeedItalicFont( lpbe );
#ifdef MULTILINESTYLE
            fInStyle = TRUE;
#endif MULTILINESTYLE
            break;

         case CH_LINK:
            pStyle->crText = lpbe->crHotlink;
            pStyle->crBack = lpbe->crMsgWnd;
            pStyle->hFont = BigEdit_NeedUnderlineFont( lpbe );
#ifdef MULTILINESTYLE
            fInStyle = TRUE;
#endif MULTILINESTYLE
            break;
         }

      /* Scan for the end of this segment.
       */
      if( chMarker != CH_NORMAL )
      {
         while( *pSegEnd && *pSegEnd != CH_NORMAL )
            ++pSegEnd;
#ifdef MULTILINESTYLE
         fInStyle = *pSegEnd != CH_NORMAL;
#endif MULTILINESTYLE
         return( pSegEnd - pStyle->pszLine );
      }
#ifdef MULTILINESTYLE
      else
         fInStyle = FALSE;
#endif MULTILINESTYLE
      }

   /* Locate a URL on this line
    */
   if( !lpbe->fUseHotlinks || fCtlMarkers )
      pSegEnd = NULL;
   else
      {
      cbLinkSize = 7;
      pSegEnd = stristr( pStyle->pszLine, "http://" );
      if( NULL == pSegEnd )
         {
         cbLinkSize = 6;
         pSegEnd = stristr( pStyle->pszLine, "ftp://" );
         if( NULL == pSegEnd )
            {
            cbLinkSize = 4;
            pSegEnd = stristr( pStyle->pszLine, "cix:" );
         if( NULL == pSegEnd )
            {
            cbLinkSize = 18;
            pSegEnd = stristr( pStyle->pszLine, "**COPIED FROM: >>>" );
            if( NULL == pSegEnd )
               {
               cbLinkSize = 8;
               pSegEnd = stristr( pStyle->pszLine, "https://" );
               if( NULL == pSegEnd )
                  {
                  cbLinkSize = 8;
                  pSegEnd = stristr( pStyle->pszLine, "cixfile:" );
                  if( NULL == pSegEnd )
                     {
                     cbLinkSize = 7;
                     pSegEnd = stristr( pStyle->pszLine, "mailto:" );
                     if( NULL == pSegEnd )
                        {
                        cbLinkSize = 5;
                        pSegEnd = stristr( pStyle->pszLine, "news:" );
                        if( NULL == pSegEnd )
                           {
                           cbLinkSize = 9;
                           pSegEnd = stristr( pStyle->pszLine, "gopher://" );
                              if( NULL == pSegEnd )
                              {
                              cbLinkSize = 4;
                              pSegEnd = stristr( pStyle->pszLine, "www." );
                                 if( NULL == pSegEnd )
                                 {
                                 cbLinkSize = 7;
                                 pSegEnd = stristr( pStyle->pszLine, "callto:" );
                                    if( NULL == pSegEnd )
                                    {
                                    cbLinkSize = 7;
                                    pSegEnd = stristr( pStyle->pszLine, "file://" );
                                       if( NULL == pSegEnd )
                                       {
                                       cbLinkSize = 9;
                                       pSegEnd = stristr( pStyle->pszLine, "telnet://" );
                                          if( NULL == pSegEnd )
                                          {
                                          cbLinkSize = 6;
                                          pSegEnd = stristr( pStyle->pszLine, "irc://" );
                                             if( NULL == pSegEnd )
                                             {
                                             cbLinkSize = 10;
                                             pSegEnd = stristr( pStyle->pszLine, "cixchat://" );
                                             }
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
   if( NULL != pSegEnd )
      {
      if( pSegEnd > pStyle->pszLine )
         {
         /* Found. Everything between here and the start of the URL is
          * drawn in normal display.
          */
         pStyle->crText = lpbe->crMsgText;
         pStyle->crBack = lpbe->crMsgWnd;
         pStyle->hFont = lpbe->hFont;
         cbSegment = pSegEnd - pStyle->pszLine;
#ifdef MULTILINESTYLE
         fInStyle = FALSE;
#endif MULTILINESTYLE
         return( cbSegment );
         }

#ifdef MULTILINESTYLE
      fInStyle = TRUE;
#endif MULTILINESTYLE
      /* We're at the URL, so everything here up
       * to the next space is drawn in the URL colour.
       */
      for( pSegEnd = pStyle->pszLine + 1; 
         ( IsHTTPChr( *pSegEnd ) || ( ( *pSegEnd == ',' || *pSegEnd == '\'' ) && 
         IsWBChar(*( pSegEnd + 1 )) &&
         IsWBChar(*( pSegEnd + 2 ))
         ) ); 
         ++pSegEnd );
      // SAM addition - if it's a Copied Message Hotlink, skip up over the message number
      if(cbLinkSize == 18)
         {
         while(*pSegEnd == ' ')++pSegEnd;
         while(*pSegEnd != ' ')++pSegEnd;
         }
      if( pSegEnd > pStyle->pszLine + 1 && IsURLTerminator( *( pSegEnd - 1) ) )
         --pSegEnd;
      cbSegment = pSegEnd - pStyle->pszLine;
      if( cbSegment > cbLinkSize )
         {
         pStyle->crText = lpbe->crHotlink;
         pStyle->crBack = lpbe->crMsgWnd;
         pStyle->hFont = BigEdit_NeedUnderlineFont( lpbe );
         return( cbSegment );
         }
      }

   /* No URL here, so draw in the normal display
    * colours.
    */
   cbSegment = strlen( pStyle->pszLine );
#ifdef MULTILINESTYLE
   if ( !fInStyle )
   {
#endif MULTILINESTYLE
      pStyle->crText = lpbe->crMsgText;
      pStyle->crBack = lpbe->crMsgWnd;
      pStyle->hFont = lpbe->hFont;
#ifdef MULTILINESTYLE
   }
   else
      fInStyle = FALSE;
#endif MULTILINESTYLE
   return( cbSegment );
}

/* This function locates a style character in the current
 * text stream.
 */
char * FASTCALL LocateStyleChr( char * pSrc )
{
   while( *pSrc )
      {
      if( IsStyleChr( *pSrc ) )
         return( pSrc );
      ++pSrc;
      }
   return( NULL );
}

/* This function returns the cached underlined font, or
 * creates one and then caches it.
 */
HFONT FASTCALL BigEdit_NeedUnderlineFont( LPBIGEDIT lpbe )
{
   if( NULL == lpbe->hCachedUFont )
      {
      LOGFONT lf;

      GetObject( lpbe->hFont, sizeof(LOGFONT), &lf );
      lf.lfUnderline = TRUE;
      lpbe->hCachedUFont = CreateFontIndirect( &lf );
      }
   return( lpbe->hCachedUFont );
}

/* This function returns the cached bold font, or
 * creates one and then caches it.
 */
HFONT FASTCALL BigEdit_NeedBoldFont( LPBIGEDIT lpbe )
{
   if( NULL == lpbe->hCachedBFont )
      {
      LOGFONT lf;

      GetObject( lpbe->hFont, sizeof(LOGFONT), &lf );
      lf.lfWeight = FW_BOLD;
      lpbe->hCachedBFont = CreateFontIndirect( &lf );
      }
   return( lpbe->hCachedBFont );
}

/* This function returns the cached bold font, or
 * creates one and then caches it.
 */
HFONT FASTCALL BigEdit_NeedItalicFont( LPBIGEDIT lpbe )
{
   if( NULL == lpbe->hCachedIFont )
      {
      LOGFONT lf;

      GetObject( lpbe->hFont, sizeof(LOGFONT), &lf );
      lf.lfItalic = TRUE;
      lpbe->hCachedIFont = CreateFontIndirect( &lf );
      }
   return( lpbe->hCachedIFont );
}

/* This function deletes all cached fonts.
 */
void FASTCALL BigEdit_DeleteCachedFonts( LPBIGEDIT lpbe )
{
   if( NULL != lpbe->hCachedUFont )
      {
      DeleteFont( lpbe->hCachedUFont );
      lpbe->hCachedUFont = NULL;
      }
   if( NULL != lpbe->hCachedBFont )
      {
      DeleteFont( lpbe->hCachedBFont );
      lpbe->hCachedBFont = NULL;
      }
   if( NULL != lpbe->hCachedIFont )
      {
      DeleteFont( lpbe->hCachedIFont );
      lpbe->hCachedIFont = NULL;
      }
}

/* This function handles the WM_VSCROLL message.
 */
void FASTCALL BigEditWnd_OnVScroll( HWND hwnd, HWND hwndCtl, UINT code, int iThumbPos )
{
   LPBIGEDIT lpbe;
   int clinLim;
   int clinRect;
   RECT rect;
   int nScratch;
   BOOL fCanScrollDown;

   /* Get the scroll range. If it's zero, then there is
    * no scrolling permitted.
    */
   PortableGetScrollRange( hwnd, SB_VERT, &clinLim, &nScratch );
   if( 0 == clinLim )
      return;

   /* Figure out the height of one line.
    */
   lpbe = GetBBlock( hwnd );
   GetClientRect( hwnd, &rect );
   clinRect = ( ( ( rect.bottom - rect.top ) - 2 ) / lpbe->dyLine );

   /* The addition of this variable fixes the bug where the use can scroll
    * down even though there are no scroll bars, thus losing the top line
    * YH 26/06/96 13:50
    */
   fCanScrollDown = ( clinRect < lpbe->cLines );

   /* Now compute and update the new scroll position.
    */
   /*if( code != SB_THUMBTRACK )*/
      {
      int pos;
      int newpos;
      int nPageDnPos;

      newpos = pos = GetScrollPos( hwnd, SB_VERT );
      switch( code )
         {
         default:
            return;

         case SB_THUMBPOSITION:
         case SB_THUMBTRACK:
            newpos = iThumbPos;
            break;

         case SB_BOTTOM:
            newpos = clinLim;
            break;

         case SB_TOP:
            newpos = 0;
            break;

         case SB_LINEDOWN:
            if( pos < clinLim && fCanScrollDown )
               newpos = pos + 1;
            break;

         case SB_LINEUP:
            if( pos > 0 )
               newpos = pos - 1;
            break;

         case SB_PAGEDOWN:
            if( fCanScrollDown )
               {
               nPageDnPos = pos + ( clinRect - 1 );

               /* This line means that we cannot page down past a certain point
                */
               newpos = nPageDnPos <= clinLim ? nPageDnPos : pos;
               }
            break;

         case SB_PAGEUP:
            newpos = max( 0, pos - ( clinRect - 1 ) );
            break;
         }
      if( newpos - pos != 0 )
         BigEditWnd_VShift( hwnd, newpos );
      }
}

/* This function handles the BEM_SCROLL message.
 */
void FASTCALL BigEditWnd_OnScroll( HWND hwnd, LPBEC_SCROLL lpscr )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   if( lpscr->cbVScroll )
      BigEditWnd_VShift( hwnd, lpbe->nYShift - lpscr->cbVScroll );
   if( lpscr->cbHScroll )
      BigEditWnd_HShift( hwnd, lpbe->nXShift - lpscr->cbHScroll );
}

/* This function handles the WM_HSCROLL message.
 */
void FASTCALL BigEditWnd_OnHScroll( HWND hwnd, HWND hwndCtl, UINT code, int iThumbPos )
{
   LPBIGEDIT lpbe;
   int nScratch;
   int clinLim;
   int clinRect;
   RECT rect;

   /* Get the scroll range. If it's zero, then there is
    * no scrolling permitted.
    */
   PortableGetScrollRange( hwnd, SB_HORZ, &clinLim, &nScratch );
   if( 0 == clinLim )
      return;

   /* Figure out the width of the page.
    */
   lpbe = GetBBlock( hwnd );
   GetClientRect( hwnd, &rect );
   clinRect = rect.right / lpbe->dxChar;

   /* Now compute and update the new scroll position.
    */
   /*if( code != SB_THUMBTRACK )*/
      {
      int pos;
      int newpos;

      newpos = pos = GetScrollPos( hwnd, SB_HORZ );
      switch( code )
         {
         default:
            return;

         case SB_THUMBPOSITION:
         case SB_THUMBTRACK:
            newpos = iThumbPos;
            break;

         case SB_BOTTOM:
            newpos = clinLim;
            break;

         case SB_TOP:
            newpos = 0;
            break;

         case SB_LINEDOWN:
            if( pos < clinLim )
               newpos = pos + 1;
            break;

         case SB_LINEUP:
            if( pos > 0 )
               newpos = pos - 1;
            break;

         case SB_PAGEDOWN:
            newpos = min( clinLim, ( pos + clinRect ) - 1 );
            break;

         case SB_PAGEUP:
            newpos = max( 0, ( pos - clinRect ) - 1 );
            break;
         }
      if( newpos - pos != 0 )
         BigEditWnd_HShift( hwnd, newpos );
      }
}


/* This function shifts the edit view vertically to the specified
 * position, where 0 is the first row.
 */
void FASTCALL BigEditWnd_VShift( HWND hwnd, int newpos )
{
   LPBIGEDIT lpbe;
   int clinRect;
   RECT rect;
   int diff;

   /* Initialise.
    */
   lpbe = GetBBlock( hwnd );
   if( newpos < 0 )
      newpos = 0;
   else if( newpos > lpbe->cLines - 1 )
      newpos = lpbe->cLines - 1;
   diff = newpos - lpbe->nYShift;
   lpbe->nYShift = newpos;
   SetScrollPos( hwnd, SB_VERT, newpos, TRUE );
   GetClientRect( hwnd, &rect );
   clinRect = ( ( ( rect.bottom - rect.top ) - 2 ) / lpbe->dyLine );
   if( diff < 0 && -diff < clinRect )
      {
      RECT rc;

      /* If diff is negative, then we're scrolling up towards
       * the first line.
       */
      SetRect( &rc, 0, 2, rect.right, 2 + ( clinRect * lpbe->dyLine ) );
      ScrollWindow( hwnd, 0, -diff * lpbe->dyLine, NULL, &rc );
      }
   else if( diff > 0 && diff < clinRect )
      {
      RECT rc;

      /* If diff is positive, then we're scrolling down towards
       * the last line.
       */
      SetRect( &rc, 0, 2, rect.right, 2 + ( clinRect * lpbe->dyLine ) );
      ScrollWindow( hwnd, 0, -diff * lpbe->dyLine, NULL, &rc );
      }
   else
      InvalidateRect( hwnd, NULL, TRUE );
   UpdateWindow( hwnd );
   BigEditWnd_SetCursor( hwnd );
}

/* This function shifts the edit view horizontally to the specified
 * position, where 0 is the first column.
 */
void FASTCALL BigEditWnd_HShift( HWND hwnd, int newpos )
{
   LPBIGEDIT lpbe;
   int clinRect;
   RECT rect;
   int diff;

   /* Initialise.
    */
   lpbe = GetBBlock( hwnd );
   diff = newpos - lpbe->nXShift;
   lpbe->nXShift = newpos;
   SetScrollPos( hwnd, SB_HORZ, newpos, TRUE );
   GetClientRect( hwnd, &rect );
   clinRect = rect.right / lpbe->dxChar;
   if( diff < 0 && -diff < clinRect )
      {
      RECT rc;

      /* If diff is negative, then we're scrolling left towards
       * the first column.
       */
      SetRect( &rc, 2, 2, rect.right, rect.bottom );
      ScrollWindow( hwnd, -diff * lpbe->dxChar, 0, NULL, &rc );
      }
   else if( diff > 0 && diff < clinRect )
      {
      RECT rc;

      /* If diff is positive, then we're scrolling down towards
       * the last line.
       */
      SetRect( &rc, 2, 2, rect.right, rect.bottom );
      ScrollWindow( hwnd, -diff * lpbe->dxChar, 0, NULL, &rc );
      }
   else
      InvalidateRect( hwnd, NULL, TRUE );
   UpdateWindow( hwnd );
   BigEditWnd_SetCursor( hwnd );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL BigEditWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   LPBIGEDIT lpbe;

   lpbe = GetBBlock( hwnd );
   lpbe->cyPage = ( cy - 4 ) / lpbe->dyLine;
   if( 0 != lpbe->cbTextLength )
      BuildLineIndexTable( hwnd, lpbe );
   AdjustScrolls( hwnd, TRUE );
}

/* This function handles the WM_SETTEXT message.
 */
void FASTCALL BigEditWnd_OnSetText( HWND hwnd, LPCSTR hpText )
{
   LPBIGEDIT lpbe;
   DWORD cb;
   HPSTR hpch;

   /* Store ptr to allocated text.
    */
   lpbe = GetBBlock( hwnd );
   cb = hstrlen( (HPSTR)hpText ) + 1;
   hpch = lpbe->hpText;
   if( fResizeMemory32( &hpch, cb ) )
      {
      /* Copy the text to our own buffer.
       */
      hmemcpy( hpch, hpText, cb );
      lpbe->hpText = hpch;
      lpbe->cbTextLength = cb - 1;

      /* Scan looking for hotlinks and other formatting
       * sequences, if any.
       */
      if( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES )
         BuildFormatText( hwnd, lpbe );

      /* Remove any selection.
       */
      lpbe->fSelection = FALSE;
      lpbe->lSelStart = 0;
      lpbe->lSelEnd = 0;

      /* Build the line index table.
       */
      BuildLineIndexTable( hwnd, lpbe );
      AdjustScrolls( hwnd, FALSE );

      /* Reset scroll positions.
       */
      SetScrollPos( hwnd, SB_HORZ, 0, TRUE );
      SetScrollPos( hwnd, SB_VERT, 0, TRUE );

      /* Invalidate the window.
       */
      if( GetFocus() == hwnd )
         SetCursor( LoadCursor( NULL, IDC_IBEAM ) );

      if( lpbe->fUseHotlinks )
      {
      char szText[ 500 ];
      POINT pt;

      /* See if we're over an URL link and change the cursor
       * to the hand cursor if so.
       */
      GetCursorPos( &pt );
      ScreenToClient( hwnd, &pt );
      if( lpbe->fOverHotspot )
         {
         /* If we're over a hotspot, then lpbe->rcHotspot is
          * valid so see if we've moved out of it.
          */
         if( PtInRect( &lpbe->rcHotspot, pt ) )
            return;
         lpbe->fOverHotspot = FALSE;
         }

      /* Locate the hotspot nearest to the cursor
       */
      if( TestForHotlink( hwnd, pt.x, pt.y, szText, sizeof(szText), &lpbe->rcHotspot ) )
         {
         SetCursor( LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_HOTSPOT) ) );
         lpbe->fOverHotspot = TRUE;
         }
      else
      {
         lpbe->fOverHotspot = FALSE;
         SetCursor( LoadCursor( NULL, IDC_ARROW ) );
      }
      }

      InvalidateRect( hwnd, NULL, TRUE );
      UpdateWindow( hwnd );
      }
}

/* Build the line index table.
 */
void FASTCALL BuildLineIndexTable( HWND hwnd, LPBIGEDIT lpbe )
{
   LPSELRANGE lprgib;
   HPSTR hpchLine;
   DWORD cb;
   int dwSize;
   HPSTR hpch;
   int ichLine;
   DWORD ib;
   RECT rc;

   /* Get buffer and size.
    */
   hpch = lpbe->hpText;
   cb = lpbe->cbTextLength;

   /* Kill any selection.
    */
// lpbe->fSelection = FALSE;
// lpbe->lSelStart = 0;
// lpbe->lSelEnd = 0;
   lpbe->fOverHotspot = FALSE;

   /* Build up line map.
    */
   lprgib = lpbe->lplIndex;
   lpbe->cLines = 0;
   lpbe->cLongestLine = 0;
   GetClientRect( hwnd, &rc );
   ib = 0;

   /* Loop and fill lplIndex with the offset of the
    * start of each line. Also set lpbe->cLongestLine with
    * the length of the longest line found so far.
    */
   lprgib[ lpbe->cLines++ ] = 0;
   hpchLine = hpch;
   dwSize = 0;
   for( ichLine = 0, ib = 0; lpbe->cLines < MAX_LINES-2 && ib < cb; ichLine++ )
      {
      BOOL fEndOfLine;

      /* Wrapping? Check whether we've reached the edge.
       */
      fEndOfLine = hpch[ ib ] == '\r' && ib < cb - 1 && ( IsLBChar(hpch[ ib + 1 ] ) );
      if( !fEndOfLine && lpbe->fWordWrap )
         {
         if( '\t' == hpch[ ib ] )
            dwSize += ( 8 - ( ichLine % 8 ) ) * lpbe->dxChar;
         else
            dwSize += lpbe->lpChrWidths[ (BYTE)hpch[ ib ] ];
         if( dwSize > rc.right - 23 )
            {
            int ichLineTmp;
            DWORD ibTmp;

            /* Scan back until we find the last space
             */
            ibTmp = ib;
            ichLineTmp = ichLine;
            while( ichLine && ib && hpch[ ib ] != ' ' )
               {
               --ib;
               --ichLine;
               }
            if( 0 == ichLine )
               {
               ib = ibTmp;
               ichLine = ichLineTmp;
               }
            fEndOfLine = TRUE;
            }
         }
      if( fEndOfLine )
         {
         /* New line or max line length.
          */
         ++ib;
         if( hpch[ ib ] == '\n' )
            ++ib;
         else if( hpch[ ib ] == '\r' && hpch[ ib + 1 ] == '\n' )
            ib += 2;
         ichLine = 0;
         hpchLine = &hpch[ ib ];
         lprgib[ lpbe->cLines++ ] = ib;
         dwSize = 0;
         }
      else
         ++ib;
      }
   if( '\0' != *hpchLine )
      lprgib[ lpbe->cLines++ ] = ib;          /* Must be the index of the NULL */
   lprgib[ lpbe->cLines ] = DOORSTOP;

   /* Reset scroll offsets.
    */
   lpbe->nYShift = 0;
   lpbe->nXShift = 0;

   /* The longest line calculated in pixels rather than characters.
    */
   CalculateLongestLine( hwnd );
}

BOOL FASTCALL ValidTrailingChar(char ch)
{
   switch(ch)
   {
      case '\'':
      case '.':
      case ',':
      case '?':
      case '!':
      case ')':
      case ' ':
//    case 10:
//    case 13:
      case 00:
      case '\t':
         return TRUE;
      default:
         return FALSE;
   }
}

BOOL FASTCALL MatchFormatCharacter(HPSTR hpText)
{
   int lchk=0;
   char chMarker;

   // Check that there is a terminating format character on the line.

   chMarker = *hpText;
   while(*hpText && !IsLBChar(*hpText) )
   {
      hpText++;
      if(*hpText == chMarker && ValidTrailingChar(*(hpText+1)) && *(hpText-1) != ' ')
         if ( *(hpText-1) == '.' && *(hpText-2) == chMarker)
            return FALSE;
         else
         {
         return TRUE;
         }
      if(*hpText == chMarker && !ValidTrailingChar(*(hpText+1)))
      {
         return FALSE;
      }
   }
   return FALSE;
}

/* This function scans the text looking for hotlinks and special
 * formatting sequences.
 */
void FASTCALL BuildFormatText( HWND hwnd, LPBIGEDIT lpbe )
{
   HPSTR hpTextStart;
   LINEBUF lbf;
   DWORD cbTextStart;
   int cStylesAdded;
   BOOL fInWhitespace;
   HPSTR hpText;

   /* Delete any existing buffer.
    */
   INITIALISE_PTR(lbf.hpFmtText);
   lbf.cbFmtText = 0;

   /* Scan the current buffer looking for sequences
    * that mark a formatting sequence.
    */
   hpText = lpbe->hpText;
   hpTextStart = hpText;
   cbTextStart = 0;
   cStylesAdded = 0;
   fInWhitespace = TRUE;
   while( *hpText )
      {
      int cbMatch;

      /* Handle style attributes.
       */
      if( lpbe->fHotLinks && IsHotlinkPrefix( hpText, &cbMatch ) )
      {
         int cbParm;

         /* First add everything up to here.
          */
         if( cbTextStart )
            {
            AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
            hpTextStart = hpText;
            cbTextStart = 0;
            }

         /* Skip to the end of the hotlink.
          */
         hpText += cbMatch;
         cbTextStart += cbMatch;
         cbParm = 0;
         while( *hpText && (IsHTTPChr( *hpText ) || (( *hpText == ',' || *hpText == '\'' ) && !IsWBChar(*( hpText + 1 )) && 
               !IsWBChar(*( hpText + 2 )))))
            {
            ++hpText;
            ++cbTextStart;
            ++cbParm;
            }
         // SAM addition - if it's a Copied Message Hotlink, skip up over the message number
         if(cbMatch == 18)
            {
            while(*hpText == ' ')
               {
               ++hpText;
               ++cbTextStart;
               ++cbParm;
               }
            while(*hpText != ' ')
               {
               ++hpText;
               ++cbTextStart;
               ++cbParm;
               }
            }
         if( IsURLTerminator( *(hpText-1) ) )
            {
            --hpText;
            --cbTextStart;
            --cbParm;
            }

         /* Add the link control code only if there is
          * anything after the hotlink prefix.
          */
         if( cbParm )
            {
            AddToFormatBuffer( &lbf, pCtlLink, sizeof(pCtlLink) );
            AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
            hpTextStart = hpText;
            cbTextStart = 0;
            AddToFormatBuffer( &lbf, pCtlNormal, sizeof(pCtlNormal) );
            ++cStylesAdded;
            }
         else
            {
            AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
            hpTextStart = hpText;
            cbTextStart = 0;
            }
         fInWhitespace = FALSE;
      }

      // SAM addition - Added in a check for full stop and a space after the format character

//    else if( ( *hpText == '*' || *hpText == '_' || *hpText == '/' ) && fInWhitespace )
      else if( ( *hpText == '*' || *hpText == '_' || *hpText == '/' ) && fInWhitespace /*&& MatchFormatCharacter(hpText)*/)
         {
         char chMarker;
         char ch;

         /* First add everything up to here.
          */
         if( cbTextStart )
            {
            AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
            hpTextStart = hpText;
            cbTextStart = 0;
            }

         /* Now skip up to the next whitespace, end of line or marker,
          */
         chMarker = *hpText++;
//       while( *hpText && *hpText != chMarker && !isspace( (BYTE)*hpText ) )
         while( *hpText && *hpText != chMarker && !IsWBChar(*hpText))
            {
            ++hpText;
            ++cbTextStart;
            }

         /* If we stopped at a marker, then make sure chr after marker is
          * a whitespace.
          */
         if( ch = *hpText )
            ch = *(hpText+1);

         if( *hpText == chMarker && !isspace( ch ) && !ValidTrailingChar(ch)) 
            ++cbTextStart;

         /* Make sure we stopped at a marker.
          */
         else if( *hpText != chMarker || 0 == cbTextStart )
            ++cbTextStart;
         else
            {
            switch( chMarker )
               {
               case '*':
                  AddToFormatBuffer( &lbf, pCtlBold, sizeof(pCtlBold) );
                  break;

               case '_':
                  AddToFormatBuffer( &lbf, pCtlUnderline, sizeof(pCtlUnderline) );
                  break;

               case '/':
                  AddToFormatBuffer( &lbf, pCtlItalic, sizeof(pCtlItalic) );
                  break;
               }
            ++cStylesAdded;
            ++hpText;
            ++hpTextStart;

            /* Now add the text being bold-faced.
             */
            if( cbTextStart )
               {
               AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
               hpTextStart = hpText;
               cbTextStart = 0;
               }

            /* Next revert back to normal style.
             */
            AddToFormatBuffer( &lbf, pCtlNormal, sizeof(pCtlNormal) );
            if( *hpText == chMarker )
               {
               ++hpText;
               ++hpTextStart;
               }
            }
         fInWhitespace = FALSE;
         }
      else if( isspace( *hpText ) )
         {
         fInWhitespace = TRUE;
         ++cbTextStart;
         ++hpText;
         }
      else
         {
         fInWhitespace = FALSE;
         ++cbTextStart;
         ++hpText;
         }
      }

   /* If nothing added, the hpFmtText buffer won't exist. Otherwise
    * replace the current edit buffer with the formatted version.
    */
   if( cStylesAdded )
      {
      if( cbTextStart )
         AddToFormatBuffer( &lbf, hpTextStart, cbTextStart );
      FreeMemory32( &lpbe->hpText );
      lpbe->hpText = lbf.hpFmtText;
      lpbe->cbTextLength = lbf.cbFmtText - 1;
      }
}

/* This function appends the text stream to the hpFmtText buffer
 */
void FASTCALL AddToFormatBuffer( LPLINEBUF lplbf, const HPSTR hpTextToAdd, DWORD cbTextToAdd )
{
   if( 0 == lplbf->cbFmtText )
      {
      lplbf->cbFmtText = cbTextToAdd + 1;
      if( fNewMemory32( &lplbf->hpFmtText, lplbf->cbFmtText ) )
         {
         hmemcpy( lplbf->hpFmtText, hpTextToAdd, cbTextToAdd );
         lplbf->hpFmtText[ cbTextToAdd ] = '\0';
         }
      }
   else
      {
      DWORD cbPos;

      cbPos = lplbf->cbFmtText - 1;
      lplbf->cbFmtText += cbTextToAdd;
      if( fResizeMemory32( &lplbf->hpFmtText, lplbf->cbFmtText ) )
         {
         hmemcpy( lplbf->hpFmtText + cbPos, hpTextToAdd, cbTextToAdd );
         lplbf->hpFmtText[ lplbf->cbFmtText - 1 ] = '\0';
         }
      }
}

/* This function handles the WM_KEYDOWN and WM_KEYUP messages.
 */
void FASTCALL BigEditWnd_OnKey( HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags )
{
   LPBIGEDIT lpbe;

   if( fDown )
      switch( vk )
         {
         case VK_INSERT:
            if( GetKeyState( VK_CONTROL ) & 0x8000 )
               SendMessage( hwnd, WM_COPY, 0, 0L );
            else if( GetKeyState( VK_SHIFT ) & 0x8000 )
               SendMessage( hwnd, WM_PASTE, 0, 0L );
            break;

         case VK_DELETE:
            if( GetKeyState( VK_SHIFT ) & 0x8000 )
               SendMessage( hwnd, WM_CUT, 0, 0L );
            break;

         case VK_UP:
            if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
               BigEditWnd_SetCaret( hwnd, 0, -1 );
            else
               SendMessage( hwnd, WM_VSCROLL, SB_LINEUP, 0L );
            break;

         case VK_DOWN:
            if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
               BigEditWnd_SetCaret( hwnd, 0, 1 );
            else
               SendMessage( hwnd, WM_VSCROLL, SB_LINEDOWN, 0L );
            break;

         case VK_LEFT: //!!SM!! Need to implement Ctrl+Left
            if( !BigEditWnd_SetCaret( hwnd, -1, 0 ) )
               BigEditWnd_SetCaret( hwnd, 9999, -1 );
            break;

         case VK_RIGHT: //!!SM!! Need to implement Ctrl+Right
            if( !BigEditWnd_SetCaret( hwnd, 1, 0 ) )
               BigEditWnd_SetCaret( hwnd, -9999, 1 );
            break;

         case VK_HOME:
            lpbe = GetBBlock( hwnd );
            if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
               BigEditWnd_SetCaret( hwnd, -9999, 0 );
            else
               BigEditWnd_SetHome( hwnd );
            break;

         case VK_NEXT:
            lpbe = GetBBlock( hwnd );
            BigEditWnd_SetCaret( hwnd, 0, lpbe->cyPage );
            break;

         case VK_PRIOR:
            lpbe = GetBBlock( hwnd );
            BigEditWnd_SetCaret( hwnd, 0, -lpbe->cyPage );
            break;

         case VK_END:
            lpbe = GetBBlock( hwnd );
            if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
               BigEditWnd_SetCaret( hwnd, 9999, 0 );
            else
               BigEditWnd_SetEnd( hwnd );
            break;
         }
}

/* This function places the cursor in the home
 * position.
 */
void FASTCALL BigEditWnd_SetHome( HWND hwnd )
{
   LPBIGEDIT lpbe;
   BOOL fShift;

   /* If the shift key isn't down, remove the current
    * selection.
    */
   lpbe = GetBBlock( hwnd );
   fShift = GetAsyncKeyState( VK_SHIFT ) & 0x8000;
   if( !fShift && lpbe->fSelection )
      {
      lpbe->fSelection = FALSE;
      BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
      }

   /* If we're not at the start, move to the start
    */
   if( lpbe->lSelEnd != 0 )
      {
      SELRANGE lSelLast;

      /* Reset
       */
      lSelLast = lpbe->lSelEnd;
      lpbe->lSelEnd = 0L;
      BigEditWnd_VShift( hwnd, -lpbe->nYShift );
      BigEditWnd_HShift( hwnd, -lpbe->nXShift );

      /* Hide the caret if we're beginning a selection.
       */
      if( fShift && !lpbe->fSelection )
         {
         BigEditWnd_HideCaret( hwnd, lpbe );
         lpbe->fSelection = TRUE;
         }

      /* If there's already a selection, update it.
       */
      if( lpbe->fSelection )
         BigEditWnd_InvalidateSelection( hwnd, TRUE, lSelLast, lpbe->lSelEnd );
      else
         {
         BigEditWnd_ShowCaret( hwnd, lpbe );
         lpbe->lSelStart = lpbe->lSelEnd;
         BigEditWnd_SetCursor( hwnd );
         }
      }
}

/* This function places the cursor in the end
 * position.
 */
void FASTCALL BigEditWnd_SetEnd( HWND hwnd )
{
   LPBIGEDIT lpbe;
   BOOL fShift;

   /* If the shift key isn't down, remove the current
    * selection.
    */
   lpbe = GetBBlock( hwnd );
   fShift = GetAsyncKeyState( VK_SHIFT ) & 0x8000;
   if( !fShift && lpbe->fSelection )
      {
      lpbe->fSelection = FALSE;
      BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
      }

   /* If we're not at the end, move to the end
    */
   if( lpbe->lSelEnd != lpbe->cbTextLength - 1 )
      {
      SELRANGE lSelLast;
      int clinRect;
      RECT rect;

      /* Reset
       */
      lSelLast = lpbe->lSelEnd;
      lpbe->lSelEnd = lpbe->cbTextLength - 1;
      GetClientRect( hwnd, &rect );
      clinRect = ( ( ( rect.bottom - rect.top ) - 4 ) / lpbe->dyLine );
      lpbe->nXShift = 0;
      BigEditWnd_VShift( hwnd, max( lpbe->cLines - clinRect, 0 ) );
      if( BigEditWnd_SetCaret( hwnd, 9999, 0 ) )
         {
         /* Hide the caret if we're beginning a selection.
          */
         if( fShift && !lpbe->fSelection )
            {
            BigEditWnd_HideCaret( hwnd, lpbe );
            lpbe->fSelection = TRUE;
            }

         /* If there's already a selection, update it.
          */
         if( lpbe->fSelection )
            BigEditWnd_InvalidateSelection( hwnd, TRUE, lSelLast, lpbe->lSelEnd );
         else
            {
            BigEditWnd_ShowCaret( hwnd, lpbe );
            lpbe->lSelStart = lpbe->lSelEnd;
            BigEditWnd_SetCursor( hwnd );
            }
         }
      }
}

/* This function adjusts the caret in the specified directions.
 */
BOOL FASTCALL BigEditWnd_SetCaret( HWND hwnd, int dx, int dy )
{
   LPBIGEDIT lpbe;
   SELRANGE lSelPos;
   SELRANGE lSelLast;
   BOOL fMoved;
   BOOL fShift;
   int x, y;

   /* If the shift key isn't down, remove the current
    * selection.
    */
   lpbe = GetBBlock( hwnd );
   fShift = GetAsyncKeyState( VK_SHIFT ) & 0x8000;
   if( !fShift && lpbe->fSelection )
      {
      lpbe->fSelection = FALSE;
      BigEditWnd_InvalidateSelection( hwnd, TRUE, lpbe->lSelStart, lpbe->lSelEnd );
      }

   /* Do vertical adjustments first. Note that rather than move the caret
    * to the same horizontal character offset, we are going to move it to the
    * same horizontal pixel offset. This is a more appropriate way to move up
    * through proportional text.
    */
   fMoved = FALSE;
   lSelLast = lpbe->lSelEnd;
   OffsetToPoint( hwnd, lpbe->lSelEnd, &x, &y );
   if( dy )
      {
      int clinRect;
      RECT rect;

      GetClientRect( hwnd, &rect );
      clinRect = ( ( ( rect.bottom - rect.top ) - 4 ) / lpbe->dyLine );
      if( dy > 0 )
         {
         if( dy < clinRect )
            y += lpbe->dyLine * dy;
         else
            while( dy > 0 )
               {
               SendMessage( hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L );
               dy -= clinRect;
               }
         }
      else
         {
         if( -dy < clinRect )
            y += lpbe->dyLine * dy;
         else
            while( dy < 0 )
               {
               SendMessage( hwnd, WM_VSCROLL, SB_PAGEUP, 0L );
               dy += clinRect;
               }
         }
      lSelPos = PointToOffset( hwnd, x, y, TRUE );
      if( lSelPos != DOORSTOP && lSelPos != lpbe->lSelEnd )
         {
         lpbe->lSelEnd = lSelPos;
         fMoved = TRUE;
         }
      }

   /* Now do horizontal adjustments, if needed.
    */
   if( dx )
      {
      HPSTR hpsz;

      hpsz = lpbe->hpText + lpbe->lSelEnd;
      if( dx < 0 )
         {
         if( 0 == lpbe->lSelEnd )
            fMoved = TRUE;
         else while( dx && lpbe->lSelEnd > 0 && *(hpsz - 1) != '\n' )
            {
            if( !IsStyleChr( *( hpsz - 1 ) ) )
               ++dx;
            --lpbe->lSelEnd;
            --hpsz;
            fMoved = TRUE;
            }
         }
      else
         {
         if( '\0' == *hpsz || lpbe->lSelEnd == lpbe->cbTextLength )
            fMoved = TRUE;
         else while( dx && lpbe->lSelEnd < lpbe->cbTextLength && *hpsz && *hpsz != '\r' )
            {
            if( !IsStyleChr( *hpsz ) )
               --dx;
            ++lpbe->lSelEnd;
            ++hpsz;
            fMoved = TRUE;
            }
         }
      }

   /* If the caret moved, update the caret position.
    */
   if( fMoved )
      {
      /* Hide the caret if we're beginning a selection.
       */
      if( fShift && !lpbe->fSelection )
         {
         BigEditWnd_HideCaret( hwnd, lpbe );
         lpbe->fSelection = TRUE;
         }

      /* If there's already a selection, update it.
       */
      if( lpbe->fSelection )
         BigEditWnd_InvalidateSelection( hwnd, TRUE, lSelLast, lpbe->lSelEnd );
      else
         {
         BigEditWnd_ShowCaret( hwnd, lpbe );
         lpbe->lSelStart = lpbe->lSelEnd;
         BigEditWnd_SetCursor( hwnd );
         }
      }
   return( fMoved );
}

/* Calling this function under 16 bit will use 16 bit API and under 32 bit
 * will use WIN32.
 */
void FASTCALL PortableSetScrollRange( HWND hwnd, int nBar, int nMax, int nPage, BOOL fRedraw )
{
#ifdef WIN32
   if( fNewShell )
      {
      SCROLLINFO scrollInfo;

      scrollInfo.cbSize = sizeof( SCROLLINFO );
      scrollInfo.fMask = SIF_PAGE | SIF_RANGE;
      scrollInfo.nMin = 0;

      /* This next line means that you can keep a space below the last line
       */
      scrollInfo.nMax = nMax + nPage - 2;

      /* This next line means that there is no space below the last line.
       */
      scrollInfo.nPage = nPage;
      SetScrollInfo( hwnd, nBar, &scrollInfo, fRedraw );
      return;
      }
#endif
   /* This next line means that you can keep a space below the last line
    */
   SetScrollRange( hwnd, nBar, 0, nMax - 1, fRedraw );
}

/* This function implements the GetScrollRange function for all versions
 * of Windows.
 */
void FASTCALL PortableGetScrollRange( HWND hwnd, int nBar, int *pnMax, int *pnPage )
{
   LPBIGEDIT lpbe;
   int nScratch;
   int nPage;
   RECT rc;
   
#ifdef WIN32
   if( fNewShell )
      {
      SCROLLINFO scrollInfo;

      scrollInfo.cbSize = sizeof( SCROLLINFO );
      scrollInfo.fMask = SIF_PAGE | SIF_RANGE;
      GetScrollInfo( hwnd, nBar, &scrollInfo );
      *pnMax = scrollInfo.nMax - scrollInfo.nPage + 1; 
      *pnPage = scrollInfo.nPage;
      }
#endif
   GetClientRect( hwnd, &rc );
   lpbe = GetBBlock( hwnd );
   nPage = ( rc.bottom - rc.top ) / lpbe->dyLine;
   GetScrollRange( hwnd, nBar, &nScratch, pnMax );
   *pnMax = *pnMax;
}

/* This will calculate the longest line in the control, in pixels. It will also
 * calculate the number of lines per page
 */
void FASTCALL CalculateLongestLine( HWND hwnd )
{
   LPBIGEDIT lpbe;
   HPSTR hpsz;
   HFONT hOldFont;
   int cx;
   int cbLen;
   HDC hdc;
   LPSELRANGE lpib;
   TEXTMETRIC tm;
   RECT rc;

   lpbe = GetBBlock( hwnd );
   hdc = GetDC( hwnd );
   if( GetWindowStyle( hwnd ) & BES_USEATTRIBUTES )
      {
      hOldFont = SelectFont( hdc, BigEdit_NeedUnderlineFont( lpbe ) );
      GetTextMetrics( hdc, &tm );
      lpbe->dyLine = tm.tmHeight;
      SelectFont( hdc, BigEdit_NeedBoldFont( lpbe ) );
      GetTextMetrics( hdc, &tm );
      lpbe->dyLine = max( tm.tmHeight, lpbe->dyLine );
      }
   else
      {
      hOldFont = SelectFont( hdc, lpbe->hFont );
      GetTextMetrics( hdc, &tm );
      lpbe->dyLine = tm.tmHeight;
      }
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );

   /* Average char and char height
    */
   lpbe->dyLine = tm.tmHeight;
   lpbe->dxChar = tm.tmAveCharWidth;

   /* Lines per page
    */
   GetClientRect( hwnd, &rc );
   lpbe->cyPage = ( ( rc.bottom - rc.top ) - 4 ) / lpbe->dyLine;

   /* Longest line in pixels
    */
   lpib = &lpbe->lplIndex[ 0 ];
   lpbe->cLongestLine = 0;
   while( lpib[ 1 ] != DOORSTOP )
      {
      int col;

      hpsz = lpbe->hpText + lpib[ 0 ];
      cbLen = (int) ( lpib[ 1 ] - lpib[ 0 ] );
      cx = 0;
      col = 0;
      while( cbLen-- )
         {
         char ch;

         ch = *hpsz++;
         if( !IsStyleChr( ch ) )
            if( '\t' == ch )
               cx += ( 8 - ( col % 8 ) ) * lpbe->dxChar;
            else
               {
               cx += lpbe->lpChrWidths[ (BYTE)ch ];
               ++col;
               }
         }
      lpbe->cLongestLine = max( cx, lpbe->cLongestLine );
      lpib++;
      }
}

/* This function will adjust the vertical and horizontal scroll ranges
 */
void FASTCALL AdjustScrolls( HWND hwnd, BOOL fRedraw )
{
   LPBIGEDIT lpbe;
   int pos;
   int cxPage;
   RECT rc;

   /* Adjust the vertical scroll range.
    */
   lpbe = GetBBlock( hwnd );
   pos = lpbe->nYShift;
   if( lpbe->cLines - 1 < lpbe->cyPage )
      {
      /* Gets here if there's no need for vertical sccroll bar
       */
      SetScrollPos( hwnd, SB_VERT, 0, FALSE );
      lpbe->nYShift = 0;
      SetScrollRange( hwnd, SB_VERT, 0, 0, fRedraw );
      }
   else
      {
      int clinLim;

      /* Gets here if we need vertical scroll bar
       */
      clinLim = lpbe->cLines - 1;
      PortableSetScrollRange( hwnd, SB_VERT, clinLim, lpbe->cyPage, FALSE );
      lpbe->nYShift = min( pos, clinLim );
      SetScrollPos( hwnd, SB_VERT, lpbe->nYShift, fRedraw );
      }

   /* Adjust horizontal scroll range.
    */
   GetClientRect( hwnd, &rc );
   cxPage = ( ( rc.right - rc.left ) - 4 ) / lpbe->dxChar;
   if( lpbe->fWordWrap || ( cxPage >= lpbe->cLongestLine / lpbe->dxChar ) )
      {
      /* Get here if there's no need for a horizontal scroll bar
       */
      SetScrollPos( hwnd, SB_HORZ, 0, FALSE );
      lpbe->nXShift = 0;
      SetScrollRange( hwnd, SB_HORZ, 0, 0, fRedraw );
      }
   else
      PortableSetScrollRange( hwnd, SB_HORZ, lpbe->cLongestLine / lpbe->dxChar, cxPage, fRedraw );
}

/* This function scrolls a window. If we've enabled animation, do
 * a smooth scroll otherwise do a normal scroll.
 *
 * BUGBUG: Doesn't work very well, so I've fallen back on normal
 * scrolling until this is fixed.
 */
void FASTCALL AnimScrollWindow( HWND hwnd, int nXAmount, int nYAmount, RECT * lpRect, RECT * lpClip )
{
   ScrollWindow( hwnd, nXAmount, nYAmount, lpRect, lpClip );
}
