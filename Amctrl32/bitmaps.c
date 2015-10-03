/* BITMAPS.C - Code that manipulates bitmaps
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
#include "amlib.h"
#include "amctrls.h"

HPALETTE FASTCALL CreateDIBPalette( LPBITMAPINFO, LPINT );

/* This function replaces LoadBitmap. It loads a 256-colour bitmap from
 * the resource and returns the handle to a palette used to render the
 * bitmap.
 */
BOOL WINAPI EXPORT Amctl_LoadDIBBitmap( HINSTANCE hInstance, LPCSTR lpString, LPPICCTRLSTRUCT lppcs )
{
   LPBITMAPINFOHEADER lpbi;
   HGLOBAL hGlobal;
   int iNumColors;
   HRSRC hRsrc;
   HDC hdc;

   lppcs->hBitmap = NULL;
   if( hRsrc = FindResource( hInstance, lpString, RT_BITMAP ) )
      {
      hGlobal = LoadResource( hInstance, hRsrc );
      lpbi = (LPBITMAPINFOHEADER)LockResource( hGlobal );
      hdc = GetDC( NULL );
      lppcs->hPalette = CreateDIBPalette( (LPBITMAPINFO)lpbi, &iNumColors );
      if( lppcs->hPalette )
         {
         SelectPalette( hdc, lppcs->hPalette, FALSE );
         RealizePalette( hdc );
         }
      lppcs->hBitmap = CreateDIBitmap( hdc, (LPBITMAPINFOHEADER)lpbi, (LONG)CBM_INIT,
                                     (LPSTR)lpbi + lpbi->biSize + iNumColors * sizeof(RGBQUAD),
                                     (LPBITMAPINFO)lpbi,
                                     DIB_RGB_COLORS );
      ReleaseDC( NULL, hdc );
      UnlockResource( hGlobal );
      FreeResource( hGlobal );
      }
   return( lppcs->hBitmap != NULL );
}

/* This function creates a palette from a device-independent
 * bitmap.
 */
HPALETTE FASTCALL CreateDIBPalette( LPBITMAPINFO lpbmi, LPINT lpiNumColors )
{ 
   LPBITMAPINFOHEADER lpbi;
   LPLOGPALETTE lpPal;
   HANDLE hLogPal;
   HPALETTE hPal;
   int i;

   hPal = NULL;
   lpbi = (LPBITMAPINFOHEADER)lpbmi;
   if (lpbi->biBitCount <= 8)
      *lpiNumColors = (1 << lpbi->biBitCount);
   else
      *lpiNumColors = 0;
   if (*lpiNumColors)
      {
      hLogPal = GlobalAlloc( GHND, sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * (*lpiNumColors) );
      lpPal = (LPLOGPALETTE)GlobalLock( hLogPal );
      lpPal->palVersion    = 0x300;
      lpPal->palNumEntries = *lpiNumColors;
      for( i = 0; i < *lpiNumColors; i++ )
         {
         lpPal->palPalEntry[i].peRed = lpbmi->bmiColors[i].rgbRed;
         lpPal->palPalEntry[i].peGreen = lpbmi->bmiColors[i].rgbGreen;
         lpPal->palPalEntry[i].peBlue = lpbmi->bmiColors[i].rgbBlue;
         lpPal->palPalEntry[i].peFlags = 0;
         }
      hPal = CreatePalette(lpPal);
      GlobalUnlock( hLogPal );
      GlobalFree( hLogPal );
      }
   return( hPal );
}
