/* WINDOWS.C - Functions that handle windows
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
#include "amlib.h"
#include <string.h>
#include "tti.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

typedef struct tagTTBUF {
   struct tagTTBUF FAR * lpNext;    /* Pointer to next tooltip buffer */
   HWND hwndTooltip;                /* Handle of tooltip window */
   HWND hwndItem;                   /* Handle of subclassed window */
   WNDPROC lpfnDefWindowProc;       /* Pointer to subclassed window proc */
} TTBUF, FAR * LPTTBUF;

LPTTBUF lpFirstTTBuf = NULL;        /* Pointer to first tooltip buffer */
HMENU hEditPopupMenu;               /* Edit popup menu handle */

LRESULT CALLBACK TooltipWndProc( HWND, UINT, WPARAM, LPARAM );

/* Switches any semi-colons or commas for a space in mail addresses
 */
void FASTCALL CleanAddresses( RECPTR FAR * pAddr, DWORD pLen ) // !!SM!! 2.55.2032
{
   DWORD i = 0;
   
   QuickReplace(pAddr->hpStr, pAddr->hpStr,"mailto:", ""); // FS#155

   while(i < pLen)
   {
      if ((pAddr->hpStr[i] == ',') || (pAddr->hpStr[i] == ';'))
         pAddr->hpStr[i] = ' ';
      i++;
   }
}

/*
 Ideally we'd check for @ etc, but since CIX addresses can just be the username, that's not really an option.
 */
BOOL FASTCALL CheckValidMailAddress(HWND pEdit)
{
LPSTR hpText;
UINT cbText;
BOOL ret;
size_t i;

   INITIALISE_PTR(hpText);
   ret = FALSE;

   cbText = Edit_GetTextLength( pEdit );
   if( cbText > 0 )
   {
      if( fNewMemory( &hpText, cbText + 1 ) )
      {
         Edit_GetText( pEdit, hpText, cbText + 1 );
      
//       QuickReplace(hpText, hpText,"mailto:", "");

         i = 0;
         while(i < strlen(hpText))
         {
            if(hpText[i] != ' ')
               ret = TRUE;
            i++;
         }
         
//       Edit_SetText( pEdit, hpText );

         FreeMemory( &hpText );
      }
      else
         ret = FALSE;
   }
   return ret;
}

BOOL FASTCALL CheckForAttachment(HWND pEdit, HWND pAttach)
{
HPSTR hpText;
UINT cbText;
BOOL ret;

   if (fCheckForAttachments)
   {
      INITIALISE_PTR(hpText);
      ret = FALSE;

      if ( Edit_GetTextLength( pAttach ) == 0 )
      {
         cbText = Edit_GetTextLength( pEdit );
         if( fNewMemory32( &hpText, cbText + 1 ) )
         {
            Edit_GetText( pEdit, hpText, cbText + 1 );
            if ( Hstrcmpiwild("*attach*", hpText, FALSE) == 0 )
            {
               if ( fMessageBox( GetParent(pEdit), 0, GS(IDS_STRING8507), MB_YESNO|MB_ICONINFORMATION ) == IDNO )
                  ret = FALSE;
               else 
                  ret = TRUE;
            }
            else 
               ret = TRUE;

            FreeMemory32( &hpText );
         }
         else
            ret = TRUE;
      }  
      else
         ret = TRUE;
   }
   else
      ret = TRUE;
   return ret;
}

/* This function resizes the formatting rectangle of the specified
 * edit control to accommodate the given number of characters. It
 * is really only reliable in fixed pitch font mode.
 */
int FAR PASCAL IsAscii(int ch)
{
// if (( ch >= 48 && ch <= 57) || ( ch >= 65 && ch <= 90 ) || ( ch >= 97 && ch <= 122 ))
   if (ch != ' ')
// if (ch == ' ' && ch == '.' && ch == ',')
      return 1;
   else
      return 0;
}

/*  if current char is whitespace, skip it
   if current char not whitespace, skip non whitespace, then skip whitespace
 */
int FAR PASCAL NextWord(LPSTR lpszEditText, int ichCurrent, int chEditText)
{
   char FAR *lpCurrentChar; 
   int   nIndex;
   
   lpCurrentChar = lpszEditText + ichCurrent;
   nIndex = ichCurrent; 
   
   if ( !IsAscii(*lpCurrentChar) )
   {
      while ( !IsAscii(*lpCurrentChar) && nIndex < chEditText) 
      {
         lpCurrentChar = AnsiNext(lpCurrentChar);
         nIndex++;
      }
   }
   else
   {
      while ( IsAscii(*lpCurrentChar) && nIndex < chEditText) 
      {
         lpCurrentChar = AnsiNext(lpCurrentChar);
         nIndex++;
      }
      while ( !IsAscii(*lpCurrentChar) && nIndex < chEditText) 
      {
         lpCurrentChar = AnsiNext(lpCurrentChar);
         nIndex++;
      }
   }
   return nIndex;
}

int FAR PASCAL PrevWord(LPSTR lpszEditText, int ichCurrent, int chEditText)
{
   char FAR *lpCurrentChar;
   int   nIndex;
   
   lpCurrentChar = lpszEditText + ichCurrent;
   nIndex = ichCurrent; 
   
   if ( !IsAscii(*lpCurrentChar) )
   {
      
      while ( !IsAscii(*lpCurrentChar) && nIndex > 0) 
      {
         lpCurrentChar = AnsiPrev(lpszEditText, lpCurrentChar);
         nIndex--;
      }
      while ( IsAscii(*lpCurrentChar) && nIndex > 0) 
      {
         lpCurrentChar = AnsiPrev(lpszEditText, lpCurrentChar);
         nIndex--;
      }
      return nIndex;
   }
   else
   {
      while ( IsAscii(*lpCurrentChar) && nIndex > 0) 
      {
         lpCurrentChar = AnsiPrev(lpszEditText, lpCurrentChar);
         nIndex--;
      }
      return nIndex;
   }
}

/*
Ctrl+Right = 
Get Delimeter then 
if Non Ascii send WB_RIGHT to go to beginning of next word, then another one to go to next word, 
if Ascii, sends WB_LEFT to go to beginning of current word, then WB_RIGHT to go to next word

Ctrl+Left =
Always sends WB_LEFT followed by WB_RIGHT
*/
#ifdef SMWORDBREAK
int FAR PASCAL WordBreakProc(LPSTR lpszEditText, int ichCurrent, int chEditText, int wActionCode)
{

   switch (wActionCode) {
      
   case WB_LEFT:
      /*  if current char is whitespace, skip it
          then continue skipping until more whitespace found
       */
      return PrevWord(lpszEditText, ichCurrent, chEditText);
      
   case WB_RIGHT:
      /*  if current char is whitespace, skip it
          if current char not whitespace, skip non whitespace, then skip whitespace
       */
      return NextWord(lpszEditText, ichCurrent, chEditText);
      
   case WB_ISDELIMITER:
      
      // Is the character at the current position a delimiter?
      if ( !IsAscii(lpszEditText[ichCurrent]) )
      {
         return TRUE; // Sends WB_RIGHT to go to next word
      }
      else
      {
         return FALSE; // Sends WB_LEFT to go to beginning of word
      }
      
   default:
      return FALSE;
   }
}
#endif SMWORDBREAK

void FASTCALL SetEditWindowWidth( HWND hwndEdit, int columns )
{
   int width;
   HDC hdc;
   HFONT hOldFont;
   RECT rc;
   RECT rcReal;
   
#ifndef USEBIGEDIT
   int i, j, k, l;
   char lWordChars[] = "_/*abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#else USEBIGEDIT
   int widths[115];
   int i;
#endif USEBIGEDIT
// char szTemp[100];

   hdc = GetDC( hwndEdit );
   hOldFont = SelectFont( hdc, hEditFont );
   GetClientRect( hwndEdit, &rcReal );

#ifdef USEBIGEDIT
   Edit_GetRect( hwndEdit, &rc );
   for (i=0;i<114;i++)
   {
      widths[i] = 0;;
   }

   if (GetCharWidth32( hdc, 32, 146, (LPINT)&widths )) 
   {
      width = 0;
      for (i=0;i<114;i++)
      {
         width = width+widths[i];
      }
      width = width / 114;
   }
   else
   {
      GetCharWidth( hdc, 'W', 'W', &width );
   }
   rc.right = ( rc.left * 2 ) + ( columns  * width );
   rc.left = 0;
   rc.bottom += rc.top;
   rc.top = 0;
   if( rc.right > rcReal.right )
      rc.right = rcReal.right;
   if( rc.bottom > rcReal.bottom )
      rc.bottom = rcReal.bottom;
//    wsprintf(szTemp,"%d %d %d %d - c%d w%d",  rc.top, rc.left, rc.bottom, rc.right, columns, width);
//    SetWindowText(GetParent(hwndEdit), szTemp);
   Edit_SetRect( hwndEdit, &rc );
#else USEBIGEDIT
   
   GetWindowRect( hwndEdit, &rc );

   i = strlen(lWordChars);
   width = SendMessage( hwndEdit, SCI_TEXTWIDTH, SCE_CIX_DEFAULT, (LPARAM)(LPSTR)lWordChars);
   width = (width * (columns - 1) / i) ;
   i = (rcReal.right - rcReal.left);

   j = SendMessage( hwndEdit, SCI_GETMARGINWIDTHN, 0, 0);

   if ( SendMessage( hwndEdit, SCI_GETVSCROLLBAR, 0, 0) )
   {
      k = GetSystemMetrics(SM_CYVTHUMB) ;
   }
   else
   {
      k = 0;
   }
   l = (GetSystemMetrics(SM_CYBORDER) * 4) + SendMessage( hwndEdit, SCI_GETMARGINLEFT, 0, 0);; 

   width = width + j + k - l;
   i = i - width;
   
// wsprintf(szTemp,"%d %d %d %d - c%d w%d r%d",  rc.top, rc.left, rc.bottom, rc.right, columns, width, i);
// SetWindowText(GetParent(hwndEdit), szTemp);

   SendMessage( hwndEdit, SCI_SETMARGINRIGHT, 0, i > 0 ? i: 1);

#endif USEBIGEDIT
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwndEdit, hdc );

}

/* This function handles indents in an edit control.
 */
LRESULT FASTCALL EditCtrlIndent( WNDPROC lpWndProc, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   if( msg == WM_RBUTTONDOWN || msg == WM_CONTEXTMENU /*!!SM!!*/)
      {
      UINT cbText;
      WORD wStart, wEnd;
      DWORD dwSel;
      POINT pt;
      AEPOPUP aep;
      LPSTR lpText = NULL;                   /* Pointer to entire text being checked */
      int cchSelectedText = 0;

      /* The popup menu we're going to use.
       */
      if( hPopupMenus )
         DestroyMenu( hPopupMenus );
      hPopupMenus = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_POPUPS ) );
      hEditPopupMenu = GetSubMenu( hPopupMenus, IPOP_EDITWINDOW );
      cbText = Edit_GetTextLength( hwnd );
      if( fNewMemory( &lpText, cbText + 1 ) )
      {
         Edit_GetText( hwnd, lpText, cbText + 1 );
         dwSel = Edit_GetSel( hwnd );
         wStart = LOWORD( dwSel );
         wEnd = HIWORD( dwSel );
         if( wStart != wEnd )
            {
            UINT lenSel;
      
            lenSel =  wEnd - wStart;
            if( wStart )
               _fmemcpy( lpText, lpText + wStart, lenSel );
            lpText[ lenSel ] = '\0';

            cchSelectedText = ( (wEnd - wStart) ) + 1;
//          if( fNewMemory( &pSelectedText, cchSelectedText ) )
//             Edit_GetSel( hwnd, pSelectedText, cchSelectedText );
//          else
//             cchSelectedText = 0;
            }
         else
            {
            cchSelectedText = 0;
            lpText = 0;
            }
      }

      if( hSpellLib )
         DoSpellingPopup( hwnd, hEditPopupMenu );

      /* Call the AE_POPUP event.
       */
      aep.wType = WIN_EDITS;
      aep.pFolder = NULL;
      aep.pSelectedText = lpText;
      aep.cchSelectedText = cchSelectedText;
      aep.hMenu = hEditPopupMenu;
      aep.nInsertPos = 11;
      Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );
      if( lpText ) 
      {
      FreeMemory( &lpText );
      lpText = NULL;
      }

      SetFocus( hwnd );
      GetCursorPos( &pt );
      TrackPopupMenu( hEditPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
      return( TRUE );
      }
   if( msg == WM_CHAR || msg == WM_LBUTTONUP || msg == WM_PASTE || msg == WM_KEYUP || msg == EM_REPLACESEL )
   {
            DWORD dwPos;
            DWORD dwLine, dwCol;
            char sz[ 10 ];

            dwPos = Edit_GetSel( hwnd );
            dwLine = Edit_LineFromChar( hwnd, HIWORD( dwPos ) ) + 1;
            dwCol = ( HIWORD( dwPos ) - Edit_LineIndex( hwnd, -1 ) + 1 );
            if( dwCol < 999 )
               wsprintf( sz, "%0.6lu", dwLine );
            else
               wsprintf( sz, "++++++");
            SendMessage( hwndStatus, SB_SETTEXT, idmsbRow, (LPARAM)(LPSTR)sz );
            if( dwCol < 999 )
               wsprintf( sz, "%0.3lu", dwCol );
            else
               wsprintf( sz, "+++");
            SendMessage( hwndStatus, SB_SETTEXT, idmsbColumn, (LPARAM)(LPSTR)sz );
            }

   if( msg == WM_CHAR && fAutoIndent )
      switch( wParam )
         {
         case VK_RETURN: {
            char szIndent[ 80 ];
            DWORD dwPos;
            int line;
            int pos;
            register int c;

            /* Get a copy of the current line (the one before the Return was processed)
             */
            dwPos = Edit_GetSel( hwnd );
            line = Edit_LineFromChar( hwnd, HIWORD( dwPos ) );
            pos = LOWORD( dwPos ) - Edit_LineIndex( hwnd, line );
            c = Edit_GetLine( hwnd, line, szIndent, sizeof( szIndent ) - 1 );
            szIndent[ c ] = '\0';

            /* Now scan the line for the first non-space and pop a NULL after it.
             * If the line was entirely spaces and/or tabs, reset the indent back to the line start
             */
            for( c = 0; c < pos && szIndent[ c ] == ' ' || szIndent[ c ] == '\t'; ++c );
            szIndent[ c ] = '\0';

            /* First let the RETURN key be processed.
             */
            CallWindowProc( lpWndProc, hwnd, msg, wParam, lParam );

            /* Now use EM_REPLACSEL to insert the spaces.
             */
            if( *szIndent )
               Edit_ReplaceSel( hwnd, szIndent );
            return( 0L );
            }
         }

   return( CallWindowProc( lpWndProc, hwnd, msg, wParam, lParam ) );
}

/* This function writes the specified text to the control. If the
 * text string is NULL, it writes the topic signature.
 */
void FASTCALL SetEditText( HWND hwnd, PTL ptl, int nID, HPSTR hpszText, BOOL fSetSig )
{
   if( hpszText && *hpszText )
      {
      HPSTR hpSrc;
      HPSTR hpDst;

      hpSrc = hpDst = hpszText;
      while( *hpSrc )
         if( *hpSrc == 13 && *(hpSrc + 1) == 13 && *(hpSrc + 2 ) == 10 )
            hpSrc += 3;
         else
            *hpDst++ = *hpSrc++;
      *hpDst = '\0';
      Edit_SetText( GetDlgItem( hwnd, nID ), hpszText );
      }
   if( fSetSig )
      {
      SetEditSignature( GetDlgItem( hwnd, nID ), ptl );
      Edit_SetModify( GetDlgItem( hwnd, nID ), FALSE );
#ifndef USEBIGEDIT
      SendMessage( GetDlgItem( hwnd, nID ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L );   
#endif USEBIGEDIT
      }
}

/* This function adds tooltips for the specified controls in lptti
 * in the specified window.
 */
void FASTCALL AddTooltipsToWindow( HWND hwnd, int ctti, LPTOOLTIPITEMS lptti )
{
   HWND hwndTooltip;
   register int x;

   hwndTooltip = CreateWindow( TOOLTIPS_CLASS, "", 0, 0, 0, 0, 0, hwnd, 0, hRscLib, 0L );
   if( NULL != hwndTooltip )
      for( x = 0; x < ctti; ++x )
         {
         WNDPROC lpfnDefWindowProc;
         LPTTBUF lpTTBuf;
         HWND hwndItem;
         TOOLINFO ti;

         INITIALISE_PTR(lpTTBuf);

         /* Subclass the control.
          */
         hwndItem = GetDlgItem( hwnd, lptti[ x ].uID );
         lpfnDefWindowProc = SubclassWindow( hwndItem, TooltipWndProc );

         /* Allocate a buffer in which to store the old procedure
          * address and tooltip window handle.
          */
         if( fNewMemory( &lpTTBuf, sizeof(TTBUF) ) )
            {
            lpTTBuf->lpNext = lpFirstTTBuf;
            lpFirstTTBuf = lpTTBuf;
            lpTTBuf->hwndTooltip = hwndTooltip;
            lpTTBuf->hwndItem = hwndItem;
            lpTTBuf->lpfnDefWindowProc = lpfnDefWindowProc;

            /* Create an entry for each control.
             */
            ti.cbSize = sizeof(TOOLINFO);
            ti.uFlags = TTF_IDISHWND;
            ti.lpszText = (LPSTR)lptti[ x ].uMsg;
            ti.hwnd = hwnd;
            ti.uId = (UINT)hwndItem;
            ti.hinst = hRscLib;
            SendMessage( hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)(LPSTR)&ti );
            }
         }
}

/* This function removes all tooltips associated with the specified
 * window.
 */
void FASTCALL RemoveTooltipsFromWindow( HWND hwnd )
{
   LPTTBUF lpTTBufNext;
   LPTTBUF * lpTTBufLast;
   HWND hwndTooltip;
   LPTTBUF lpTTBuf;

   lpTTBufLast = &lpFirstTTBuf;
   hwndTooltip = NULL;
   for( lpTTBuf = lpFirstTTBuf; lpTTBuf; lpTTBuf = lpTTBufNext )
      {
      lpTTBufNext = lpTTBuf->lpNext;
      if( GetParent( lpTTBuf->hwndItem ) == hwnd )
         {
         hwndTooltip = lpTTBuf->hwndTooltip;
         SubclassWindow( lpTTBuf->hwndItem, lpTTBuf->lpfnDefWindowProc );
         FreeMemory( &lpTTBuf );
         *lpTTBufLast = lpTTBufNext;
         }
      else
         lpTTBufLast = &lpTTBuf->lpNext;
      }
   ASSERT( NULL != hwndTooltip );
   DestroyWindow( hwndTooltip );
}

/* This function receives all mouse messages relating to the subclassed
 * tooltip control.
 */
LRESULT CALLBACK TooltipWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LPTTBUF lpTTBuf;

   /* Locate this window in the tooltip array.
    */
   for( lpTTBuf = lpFirstTTBuf; lpTTBuf; lpTTBuf = lpTTBuf->lpNext )
      if( lpTTBuf->hwndItem == hwnd )
         break;
   if( NULL == lpTTBuf )
      return( 0L );
   switch( message )
      {
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP: {
         MSG msg;

         msg.lParam = lParam;
         msg.wParam = wParam;
         msg.message = message;
         msg.hwnd = hwnd;
         SendMessage( lpTTBuf->hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
         break;
         }
      }
   return( CallWindowProc( lpTTBuf->lpfnDefWindowProc, hwnd, message, wParam, lParam ) );
}

/* This function overloads the Amuser_ReadWindowState function to ensure
 * that the attributes of the new window are not minimised and, if the active
 * window is maximized, the new window is also maximized.
 */
void FASTCALL ReadProperWindowState( char * pszWndName, RECT * prc, DWORD * pdwState )
{
   Amuser_ReadWindowState( pszWndName, prc, pdwState );
   if( !fInitialising )
      {
      if( *pdwState == WS_MINIMIZE )
         *pdwState = 0;
      if( hwndActive && IsMaximized( hwndActive ) )
         *pdwState = WS_MAXIMIZE;
      }
   ConstrainWindow( prc );
}

/* Make sure the MDI child window coordinates are not outside the
 * MDI client window.
 */
void FASTCALL ConstrainWindow( RECT * prc )
{
   if( !IsIconic( hwndFrame ) )
      {
      WINDOWPLACEMENT wndpl;
      int cx, cy;
      int diffx;
      int diffy;
      RECT rc;

      /* Get MDI client window width and height.
       */
      wndpl.length = sizeof(WINDOWPLACEMENT);
      GetWindowPlacement( hwndMDIClient, &wndpl );
      rc = wndpl.rcNormalPosition;
      OffsetRect( &rc, -wndpl.rcNormalPosition.left, -wndpl.rcNormalPosition.top );
      cx = rc.right - rc.left;
      cy = rc.bottom - rc.top;

      /* Constrain width. If the width is too wide,
       * move the left edge back so that the right edge
       * fits against the MDI client window right edge.
       * If the left edge has now gone past the left edge
       * of the MDI client window, reduce the window width
       * so that the whole window is constrained.
       */
      diffx = cx - prc->right;
      if( diffx < 0 )
         prc->left += diffx;
      diffx = prc->left - 0;
      if( diffx < 0 )
         {
         prc->left -= diffx;
         prc->right += diffx;
         }

      /* Constrain height. If the height is too tall,
       * move the bottom edge up so that the bottom edge
       * fits against the MDI client window bottom edge.
       * If the top edge has now gone past the top edge
       * of the MDI client window, reduce the window height
       * so that the whole window is constrained.
       */
      diffy = cy - prc->bottom;
      if( diffy < 0 )
         prc->top += diffy;
      diffy = prc->top - 0;
      if( diffy < 0 )
         {
         prc->top -= diffy;
         prc->bottom += diffy;
         }
      }
}

/* This function both shows and enables or hides and disables a
 * specified control.
 */
void FASTCALL ShowEnableControl( HWND hwnd, int idCtl, BOOL fEnable )
{
   ShowWindow( GetDlgItem( hwnd, idCtl ), fEnable ? SW_SHOW : SW_HIDE );
   EnableWindow( GetDlgItem( hwnd, idCtl ), fEnable );
}

/* This function returns whether or not the specified hwnd is an
 * edit window.
 */
BOOL FASTCALL IsEditWindow( HWND hwnd )
{
   char szClsName[ 16 ];

   GetClassName( hwnd, szClsName, sizeof(szClsName) );

   // !!SM!! 2.55.2036
   if (_strcmpi( szClsName, WC_BIGEDIT ) == 0)
   {
      return SendMessage( hwnd, SCI_GETREADONLY,  (WPARAM)0, (LPARAM)0L ) == 0;
   }
   else
      return( _strcmpi( szClsName, "edit" ) == 0) ; 

// return( _strcmpi( szClsName, "edit" ) == 0 ); 
}
