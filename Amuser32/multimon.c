/* MULTIMON.C - Multiple Monitor functions
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
 * limitations under the License. on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <windows.h>
#include <windowsx.h>

#define COMPILE_MULTIMON_STUBS

#pragma warning(push)
#pragma warning(disable:4996)
#include <multimon.h>
#pragma warning(pop)

//
//  GetMonitorRect
//
//  gets the "screen" or work area of the monitor that the passed
//  window is on.  this is used for apps that want to clip or
//  center windows.
//
//  the most common problem apps have with multimonitor systems is
//  when they use GetSystemMetrics(SM_C?SCREEN) to center or clip a
//  window to keep it on screen.  If you do this on a multimonitor
//  system the window we be restricted to the primary monitor.
//
//  this is a example of how you used the new Win32 multimonitor APIs
//  to do the same thing.
//
void GetMonitorRect(HWND hwnd, LPRECT prc, BOOL fWork)
{
    MONITORINFO mi;

    mi.cbSize = sizeof(mi);
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

    if (fWork)
        *prc = mi.rcWork;
    else
        *prc = mi.rcMonitor;
}

//
// ClipRectToMonitor
//
// uses GetMonitorRect to clip a rect to the monitor that
// the passed window is on.
//
void ClipRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork)
{
    RECT rc;
    int  w = prc->right  - prc->left;
    int  h = prc->bottom - prc->top;

    if (hwnd != NULL)
    {
        GetMonitorRect(hwnd, &rc, fWork);
    }
    else
    {
        MONITORINFO mi;

        mi.cbSize = sizeof(mi);
        GetMonitorInfo(MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST), &mi);

        if (fWork)
            rc = mi.rcWork;
        else
            rc = mi.rcMonitor;
    }

    prc->left   = max(rc.left, min(rc.right-w,  prc->left));
    prc->top    = max(rc.top,  min(rc.bottom-h, prc->top));
    prc->right  = prc->left + w;
    prc->bottom = prc->top  + h;
}

//
// CenterRectToMonitor
//
// uses GetMonitorRect to center a rect to the monitor that
// the passed window is on.
//
void CenterRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork)
{
    RECT rc;
    int  w = prc->right  - prc->left;
    int  h = prc->bottom - prc->top;

    GetMonitorRect(hwnd, &rc, fWork);

    prc->left    = rc.left + (rc.right  - rc.left - w) / 2;
    prc->top    = rc.top  + (rc.bottom - rc.top  - h) / 2;
    prc->right    = prc->left + w;
    prc->bottom = prc->top  + h;
}

//
// CenterWindowToMonitor
//
void CenterWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    CenterRectToMonitor(hwndP, &rc, fWork);
    SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//
// ClipWindowToMonitor
//
void ClipWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    ClipRectToMonitor(hwndP, &rc, fWork);
    SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//
// IsWindowOnScreen
//
BOOL IsWindowOnScreen(HWND hwnd)
{
    HDC hdc;
    RECT rc;
    BOOL f;

    GetWindowRect(hwnd, &rc);
    hdc = GetDC(NULL);
    f = RectVisible(hdc, &rc);
    ReleaseDC(NULL, hdc);
    return f;
}

//
// MakeSureWindowIsVisible
//
void MakeSureWindowIsVisible(HWND hwnd)
{
    if (!IsWindowOnScreen(hwnd))
    {
    ClipWindowToMonitor(hwnd, hwnd, TRUE);
    }
}
