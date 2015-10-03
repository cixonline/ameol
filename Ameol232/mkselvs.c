/* MKSELVS.C - Implements MakeEditSelectionVisible
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
#include "editmap.h"
#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

void FASTCALL MakeEditSelectionVisible( HWND hwndTopWnd, HWND hwndEdit )
{

#ifdef USEBIGEDIT
   RECT rcTopWnd;
   RECT rcEdit;
   RECT rcEditNC;
   RECT rcOverlap;
   RECT rcSelOverlap;
   RECT rcSel;
   int nSelStartOffset;
   int nSelEndOffset;
   int nEditLineHeight;
   BEC_SELECTION bsel;
   HEDIT hedit;
   TEXTMETRIC tm;
   HFONT hwndEditFont;
   HFONT hOldFont;
   int nDist;
   HDC hdc;

   /* First of all, compute the overlap. If no overlap, then
    * we don't have to do anything further.
    */
   GetWindowRect( hwndTopWnd, &rcTopWnd );
   GetWindowRect( hwndEdit, &rcEdit );
   if( IntersectRect( &rcOverlap, &rcTopWnd, &rcEdit ) == 0 )
      return;

   /* Compute the screen coordinates of the edit selection (!)
    * Only bother with the top and bottom edges.
    */
   hedit = Editmap_Open( hwndEdit );
   Editmap_GetSelection( hedit, hwndEdit, &bsel );
   nSelStartOffset = (int)( Editmap_LineFromChar( hedit, hwndEdit, bsel.lo ) - Editmap_GetFirstVisibleLine( hedit, hwndEdit ) );
   nSelEndOffset = (int)( Editmap_LineFromChar( hedit, hwndEdit, bsel.lo ) - Editmap_GetFirstVisibleLine( hedit, hwndEdit ) );
   hwndEditFont = GetWindowFont( hwndEdit );
   hdc = GetDC( hwndEdit );
   hOldFont = SelectFont( hdc, hwndEditFont );
   GetTextMetrics( hdc, &tm );
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwndEdit, hdc );
   nEditLineHeight = tm.tmHeight;
   SetRect( &rcSel, 0, nSelStartOffset * nEditLineHeight, rcEdit.right - rcEdit.left, ( nSelEndOffset + 1 ) * nEditLineHeight );
   Editmap_GetRect( hedit, hwndEdit, &rcEditNC );
   ClientToScreen( hwndEdit, (LPPOINT)&rcSel );
   ClientToScreen( hwndEdit, (LPPOINT)(&rcSel) + 1 );
   rcSel.top += rcEditNC.top;
   rcSel.bottom += rcEditNC.top;

   /* Does the selection rectangle intesect the overlap?
    * If so, scroll the edit control down by the distance between the selection top edge
    * and the top window bottom edge.
    */
   if( IntersectRect( &rcSelOverlap, &rcOverlap, &rcSel ) == 0 )
      return;
   if( rcEdit.bottom > rcTopWnd.bottom )
      nDist = -( ( ( rcTopWnd.bottom - rcSel.top ) / nEditLineHeight ) + 1 );
   else
      nDist = ( ( rcSel.bottom - rcTopWnd.top ) / nEditLineHeight ) + 1;

   if( (long)Editmap_GetFirstVisibleLine( hedit, hwndEdit ) + nDist >= 0 )
      {
      Editmap_Scroll( hedit, hwndEdit, nDist, 0 );
      return;
      }

   /* Okay. We can't scroll the edit control because the top window is completely
    * overlapping the edit control, or the scroll would go past the first line. So
    * try and move the top window.
    */
   if( rcEdit.bottom > rcTopWnd.bottom )
      nDist = rcTopWnd.top - ( rcSel.bottom + ( 2 * nEditLineHeight ) );
   else
      nDist = ( rcTopWnd.top - rcSel.bottom ) + 1;
   if( rcTopWnd.top - nDist < 0 )
      nDist = rcTopWnd.top - rcSel.bottom;
   else if( rcTopWnd.bottom - nDist > GetSystemMetrics( SM_CYSCREEN ) )
      nDist = rcTopWnd.bottom - rcSel.top;
   SetWindowPos( hwndTopWnd, NULL, rcTopWnd.left, rcTopWnd.top - nDist, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );

#else USEBIGEDIT

   HEDIT hedit;
   BEC_SELECTION bsel;
   SELRANGE lLine, lCurLine; 

   hedit = Editmap_Open( hwndEdit );

   Editmap_GetSelection( hedit, hwndEdit, &bsel );

   lLine = Editmap_LineFromChar( hedit, hwndEdit, bsel.hi );
   lCurLine = Editmap_GetFirstVisibleLine( hedit, hwndEdit );

   if( lCurLine >= lLine )
   {
      SendMessage( hwndEdit, SCI_GOTOPOS, bsel.hi, 0);
      Editmap_SetSel( hedit, hwndEdit, &bsel );
//    Editmap_Scroll( hedit, hwndEdit, lLine, 0 );
      return;
   }

   SendMessage( hwndEdit, SCI_MOVECARETINSIDEVIEW, 0, 0);


#endif USEBIGEDIT
}
