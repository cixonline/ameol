/* AMCTRLI.H - Ameol common controls internal definitions
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

#include "amlib.h"
#include <memory.h>
#include "amuser.h"

#ifdef _DEBUG
#define DEBUG       1
#endif

#if !defined(MAKEWORD)
#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#endif

extern HINSTANCE hLibInst;
extern BOOL fWin95;
extern BOOL fNewShell;
extern BOOL fWinNT4;
extern BOOL fCtlAnimation;
extern int iScrollLines;

#define THREED_CLASSNAME        "amxctrl_3dstatic"

BOOL FASTCALL RegisterHotkeyClass( HINSTANCE );
BOOL FASTCALL RegisterStatusClass( HINSTANCE );
BOOL FASTCALL RegisterHeaderClass( HINSTANCE );
BOOL FASTCALL RegisterTabClass( HINSTANCE );
BOOL FASTCALL RegisterUpDownClass( HINSTANCE );
BOOL FASTCALL RegisterProgressClass( HINSTANCE );
BOOL FASTCALL RegisterPropertySheetClass( HINSTANCE );
BOOL FASTCALL RegisterToolbarClass( HINSTANCE );
BOOL FASTCALL RegisterTooltipClass( HINSTANCE );
BOOL FASTCALL RegisterTreeviewClass( HINSTANCE );
BOOL FASTCALL RegisterWellClass( HINSTANCE );
BOOL FASTCALL RegisterPicButtonClass( HINSTANCE );
BOOL FASTCALL RegisterPicCtrlClass( HINSTANCE );
BOOL FASTCALL RegisterSplitterClass( HINSTANCE );
BOOL FASTCALL RegisterBigEditClass( HINSTANCE );
BOOL FASTCALL RegisterTopicListClass( HINSTANCE );
BOOL FASTCALL RegisterVListBox( HINSTANCE );
BOOL FASTCALL Register3DLineControl( HINSTANCE );
BOOL FASTCALL RegisterListviewClass( HINSTANCE );

HBITMAP FASTCALL CreateDitherBitmap( void );
void FASTCALL Draw3DFrame( HDC, RECT FAR * );
void FASTCALL DrawButton( HDC, RECT *, HPEN, HPEN, HPEN, HPEN, HBRUSH, BOOL );
void FASTCALL DrawButtonBitmap( HDC, RECT *, HBITMAP, LPSTR, int, int, int, int, int, BOOL );
void FASTCALL PortableSetScrollRange( HWND hwnd, int nBar, int nMax, int nPage, BOOL fRedraw );
void FASTCALL PortableGetScrollRange( HWND hwnd, int nBar, int *pnMax, int *pnPage );
void FASTCALL AnimScrollWindow( HWND, int, int, RECT *, RECT * );
UINT FASTCALL GetNumMouseWheelScrollLines( void );
void FASTCALL InitialiseMouseWheel( void );
