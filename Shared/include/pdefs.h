/* PDEFS.H - Common portability definitions
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

#ifndef _PDEFS_INCLUDED

#ifndef WIN32

/* Definitions for Win16
 */
#define  WIN32API
#define  EXPORT            __export
#define  GetTaskHandle()   (WORD)GetCurrentTask()
#define  SetBrushOrgEx(h,x,y,p ) SetBrushOrg((h),(x),(y))

#ifdef _DEBUG
#define  FASTCALL    __fastcall
#else
#define  FASTCALL    __fastcall
#endif

#else

/* Definitions for Win32
 */
#define  WIN32API                __declspec(dllexport)
#define  EXPORT
#define  FASTCALL                __fastcall
#define  GetTaskHandle()         LOWORD( GetCurrentThreadId() )

#endif

/* Unreferenced parameters.
 */
#define  UNREFERENCED_PARAM(l)      ((l)=(l))

/* Include special definitions for HUGE pointers.
 */
#ifndef _PDEF_DEFS

#ifndef WIN32
#define  HUGEPTR  __huge
#else
#define  HUGEPTR
#endif

#define  HPSTR char HUGEPTR *
#define  HPDWORD DWORD HUGEPTR *
#define  HPVOID void HUGEPTR *

#define  _PDEF_DEFS

#endif

#define HANDLE_DLGMSG(hwnd, message, fn)    \
    case (message): { \
      LRESULT l; \
      l = HANDLE_##message((hwnd), (wParam), (lParam), (fn)); \
      return( SetDlgMsgResult( (hwnd), (message), l ) ); \
      }

/* Win32 function for getting extended error
 * codes.
 */
#ifdef WIN32
#define REPORTLASTERROR(p) { char sz[ 100 ]; \
                             wsprintf( sz, "%s error %lu", p, GetLastError() ); \
                             MessageBox(GetFocus(), sz, NULL, MB_OK|MB_ICONEXCLAMATION ); \
                           }
#else
#define REPORTLASTERROR(p)
#endif

/* Redefine the assertion macro the way I want it.
 */
#undef  assert

#ifdef NDEBUG
#define ASSERT(exp)
#else 
#ifdef __cplusplus
extern "C" {
#endif 
BOOL __cdecl AssertMsg(void *, void *, unsigned);
#ifdef __cplusplus
}
#endif
#if defined(__BORLANDC__)
#define ASSERT(exp)  { if(exp) NULL; else if( AssertMsg(#exp, THIS_FILE, __LINE__) ) geninterrupt(3); }
#else
#define ASSERT(exp)  { if(exp) NULL; else if( AssertMsg(#exp, THIS_FILE, __LINE__) ) _asm int 3 }
#endif
#endif

/* Macro that tests bitflags.
 */
#define  TestF(p,w)     (((p)&(w))==(w))

/* The VERIFY macro checks API returns for non-zero'ness.
 */
#ifdef NDEBUG
#define  VERIFY(f)      ((void)(f))
#else
#define  VERIFY(f)      ASSERT((f) != 0)
#endif

/* Useful macros that Microsoft never thought to include...
 */
#define  EnableControl(hwnd,id,status)    \
               (EnableWindow( GetDlgItem((hwnd),(id)), (status)?TRUE:FALSE))
#define  MenuEnable(hmenu,id,bool) \
               (EnableMenuItem((hmenu),(id),((bool)?MF_ENABLED:MF_GRAYED)))
#define  MenuString(hmenu,id,str) \
               (ModifyMenu((hmenu),(id),MF_BYCOMMAND,(id),(str)))
#define  IsEq( ch1, ch2, fCase ) \
               ((fCase)?((ch1)==(ch2)):(tolower((ch1))==tolower((ch2))))

#define  SetWindowStyle(hwnd,s)    (SetWindowLong(hwnd, GWL_STYLE,(s)))

/* Creates a version number.
 */
#define  MAKEVERSION(x,y,z)      (DWORD)MAKELONG((z),(((x)<<8)|(y)))
#define  _PDEFS_INCLUDED

/* Macros to allow sending explicit WM_COMMAND messages
 */
#ifdef WIN32
#define  PostDlgCommand(hwnd, id, codeNotify) \
   PostMessage((hwnd), WM_COMMAND, MAKELPARAM((id),(codeNotify)),(LPARAM)GetDlgItem((hwnd),(id)))
#define  SendDlgCommand(hwnd, id, codeNotify) \
   SendMessage((hwnd), WM_COMMAND, MAKELPARAM((id),(codeNotify)),(LPARAM)GetDlgItem((hwnd),(id)))
#else
#define  PostDlgCommand(hwnd, id, codeNotify) \
   PostMessage((hwnd), WM_COMMAND, (WPARAM)(id), MAKELPARAM(GetDlgItem((hwnd),(id)), codeNotify))
#define  SendDlgCommand(hwnd, id, codeNotify) \
   SendMessage((hwnd), WM_COMMAND, (WPARAM)(id), MAKELPARAM(GetDlgItem((hwnd),(id)), codeNotify))
#endif

/* Macros to allow sending explicit WM_COMMAND messages
 */
#ifdef WIN32
#define  SendCommand(hwnd, id, codeNotify) \
   SendMessage((hwnd), WM_COMMAND, MAKELPARAM((id),(codeNotify)),(LPARAM)(NULL))
#define  PostCommand(hwnd, id, codeNotify) \
   PostMessage((hwnd), WM_COMMAND, MAKELPARAM((id),(codeNotify)),(LPARAM)(NULL))
#else
#define  SendCommand(hwnd, id, codeNotify) \
   SendMessage((hwnd), WM_COMMAND, (WPARAM)(id), MAKELPARAM((NULL), codeNotify))
#define  PostCommand(hwnd, id, codeNotify) \
   PostMessage((hwnd), WM_COMMAND, (WPARAM)(id), MAKELPARAM((NULL), codeNotify))
#endif

#endif
