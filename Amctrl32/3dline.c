/* 3dLINE.C - Implements the 3D line control.
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amctrls.h"
#include "amctrli.h"

#define  THIS_FILE   __FILE__

LRESULT EXPORT CALLBACK ThreeDLineWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the 3D Line window class.
 */
BOOL FASTCALL Register3DLineControl( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = THREED_CLASSNAME;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = ThreeDLineWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = 0;
   return( RegisterClass( &wc ) );
}

/* This is the 3D line window procedure.
 */
LRESULT EXPORT CALLBACK ThreeDLineWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_PAINT: {
         PAINTSTRUCT ps;
         RECT rc;
         HDC hdc;

         /* Begin painting,
          */
         hdc = BeginPaint( hwnd, &ps );
         GetClientRect( hwnd, &rc );
         if( rc.bottom > 2 )
            Draw3DFrame( hdc, &rc );
         else
            {
            HPEN hpenGray;
            HPEN hpenWhite;
            HPEN hpenOld;

            /* Special case for a 1/2 pixel thick line which Draw3DFrame can't
             * handle properly.
             */
            hpenGray = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNSHADOW ) );
            hpenWhite = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNHIGHLIGHT ) );
            hpenOld = SelectPen( hdc, hpenGray );
            MoveToEx( hdc, rc.left, rc.top, NULL );
            LineTo( hdc, rc.right, rc.top );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, rc.left, rc.top + 1, NULL );
            LineTo( hdc, rc.right, rc.top + 1 );
            SelectPen( hdc, hpenOld );
            DeletePen( hpenWhite );
            DeletePen( hpenGray );
            }
         EndPaint( hwnd, &ps );
         return( 0L );
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}
