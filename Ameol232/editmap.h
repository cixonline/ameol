/* EDITMAP.H - Bigedit/Edit control interface
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

#ifndef _H_EDITMAP

#define  ETYP_NOTEDIT      -1
#define  ETYP_BIGEDIT      0
#define  ETYP_SMALLEDIT    1


typedef int HEDIT;

// !!SM!! 2.55.2033
typedef unsigned long uptr_t;
typedef long sptr_t;

struct smNotifyHeader {
   // hwndFrom is really an environment specifc window handle or pointer
   // but most clients of Scintilla.h do not have this type visible.
   //WindowID hwndFrom;
   void *hwndFrom;
   unsigned int idFrom;
   unsigned int code;
};

typedef struct tagSCNotificationStruct {
   struct smNotifyHeader nmhdr;
   int position;  // SCN_STYLENEEDED, SCN_MODIFIED, SCN_DWELLSTART, SCN_DWELLEND
   int ch;     // SCN_CHARADDED, SCN_KEY
   int modifiers; // SCN_KEY
   int modificationType;   // SCN_MODIFIED
   const char *text; // SCN_MODIFIED
   int length;    // SCN_MODIFIED
   int linesAdded;   // SCN_MODIFIED
   int message;   // SCN_MACRORECORD
   uptr_t wParam; // SCN_MACRORECORD
   sptr_t lParam; // SCN_MACRORECORD
   int line;      // SCN_MODIFIED
   int foldLevelNow; // SCN_MODIFIED
   int foldLevelPrev;   // SCN_MODIFIED
   int margin;    // SCN_MARGINCLICK
   int listType;  // SCN_USERLISTSELECTION
   int x;         // SCN_DWELLSTART, SCN_DWELLEND
   int y;      // SCN_DWELLSTART, SCN_DWELLEND
} SCNotificationStruct;

typedef struct tagCharacterRange {
   long cpMin;
   long cpMax;
} CharacterRange;



void FASTCALL Editmap_GetSelection( HEDIT, HWND, LPBEC_SELECTION );
void FASTCALL Editmap_FormatLines( HEDIT hedit, HWND hwndEdit ); // !!SM!! 2.55.2032
HEDIT FASTCALL Editmap_Open( HWND );
SELRANGE FASTCALL Editmap_GetTextLength( HEDIT, HWND );
BOOL FASTCALL EditMap_GetModified(HWND hwndEdit);
void FASTCALL Editmap_GetText( HEDIT, HWND, SELRANGE, HPSTR );
void FASTCALL Editmap_GetSel( HEDIT, HWND, SELRANGE, HPSTR );
void FASTCALL Editmap_ReplaceSel( HEDIT, HWND, HPSTR );
void FASTCALL Editmap_SetSel( HEDIT, HWND, LPBEC_SELECTION );
void FASTCALL Editmap_SelectAll( HEDIT hedit, HWND hwndEdit ); //!!SM!! 2.55
void FASTCALL Editmap_SetWindowText(HWND hwndEdit, HPSTR hpText);  //!!SM!! 2.55.2032
void FASTCALL Editmap_Scroll( HEDIT, HWND, int, int );
SELRANGE FASTCALL Editmap_GetFirstVisibleLine( HEDIT, HWND );
void FASTCALL Editmap_GetRect( HEDIT, HWND, LPRECT );
SELRANGE FASTCALL Editmap_LineFromChar( HEDIT, HWND, SELRANGE );
BOOL FASTCALL Editmap_CanUndo( HEDIT, HWND );
BOOL FASTCALL Editmap_CanRedo( HEDIT, HWND );

#ifndef USEBIGEDIT
void FASTCALL GetCurrentURL( HWND hwnd, POINT pPT, LPSTR pURL );
LRESULT FASTCALL Lexer_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );
LRESULT FASTCALL Lexer_HotspotClick( HWND hwnd, int idCode, LPNMHDR lpNmHdr );
void FASTCALL SetEditorDefaults(HWND hwndEdit, BOOL readOnly, LOGFONT lfFont, BOOL pWordWrap, BOOL pHotLinks , BOOL pMessageStyles, COLORREF col, COLORREF bgCol );
void FASTCALL Editmap_Searchfor(HEDIT hedit, HWND hwndEdit, LPSTR lpText, int pCount, int pStart, int pFlags);
#endif USEBIGEDIT

#define _H_EDITMAP
#endif
