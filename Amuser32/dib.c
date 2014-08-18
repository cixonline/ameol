/* DIB.C - Handles reading and displaying DIB files |
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

#define NOCOMM
#define NOKANJI

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "pdefs.h"
#include "dib.h"

#define BFT_BITMAP 0x4d42        /* 'BM' */

/* Macro to determine to round off the given value to the closest byte */
#define WIDTHBYTES(i)   ((i+31)/32*4)

#define PALVERSION              0x300
#define MAXPALETTE      256       /* max. # supported palette entries */

/* macro to determine if resource is a DIB */
#define ISDIB(bft) ((bft) == BFT_BITMAP)

LPSTR FASTCALL FindDIBBits(LPSTR);

HBITMAP WINAPI EXPORT Amuser_LoadBitmapFromDisk( LPSTR lpszFileName, HPALETTE FAR * phPal )
{
   unsigned fh;
   BITMAPINFOHEADER bi;
   LPBITMAPINFOHEADER lpbi;
   DWORD dwLen = 0;
   DWORD dwBits;
   HBITMAP hbm;
   HANDLE hdib;
   HANDLE h;
   OFSTRUCT of;
   HDC hdc;
   
   /* Open the file and read the DIB information
    */
   fh = OpenFile( lpszFileName, &of, OF_READ);
   if( fh == -1 )
      return( NULL );

   hdib = ReadDibBitmapInfo(fh);
   if( !hdib )
      {
      _lclose( fh );
      return( NULL );
      }
   DibInfo(hdib, &bi);
   
   /* Calculate the memory needed to hold the DIB
    */
   dwBits = bi.biSizeImage;
   dwLen = bi.biSize + (DWORD)PaletteSize( &bi ) + dwBits;

   /* Try to increase the size of the bitmap info. buffer to hold the DIB
    */
   h = GlobalReAlloc( hdib, dwLen, GHND );
   if( NULL == h )
      hbm = NULL;
   else
      {
      hdib = h;
      lpbi = (VOID FAR *)GlobalLock( hdib );
      _hread( fh, (LPSTR)lpbi + (WORD)lpbi->biSize + PaletteSize(lpbi), dwBits );
      hdc = GetDC( NULL );
      *phPal = CreateBIPalette( lpbi );
      if (*phPal)
         {
         HPALETTE hOldPal;
         HWND hActWin;
         HDC hActDC;

         hOldPal = SelectPalette( hdc, *phPal, FALSE );
         RealizePalette( hdc );
      #ifdef WIN32
         hActWin = GetForegroundWindow();
      #else
         hActWin = GetActiveWindow();
      #endif
         hActDC = GetDC( hActWin );
         SelectPalette( hActDC, *phPal, FALSE );
         RealizePalette( hActDC );
         hbm = CreateDIBitmap( hdc, lpbi, (LONG)CBM_INIT,
                               (LPSTR)lpbi + lpbi->biSize + PaletteSize( lpbi ),
                               (LPBITMAPINFO)lpbi, DIB_RGB_COLORS );
         ReleaseDC( hActWin, hActDC );
         if( hOldPal )
            SelectPalette( hdc, hOldPal, FALSE );
         }
      else
         hbm = CreateDIBitmap( hdc, lpbi, (LONG)CBM_INIT,
                               (LPSTR)lpbi + lpbi->biSize + PaletteSize( lpbi ),
                               (LPBITMAPINFO)lpbi, DIB_RGB_COLORS );
      ReleaseDC(NULL, hdc);
      GlobalUnlock( hdib );
      }
   GlobalFree( hdib );
   _lclose(fh);
   return( hbm );
}

/****************************************************************************
 *                             *
 *  FUNCTION   : DibInfo(HANDLE hbi,LPBITMAPINFOHEADER lpbi)          *
 *                             *
 *  PURPOSE    : Retrieves the DIB info associated with a CF_DIB      *
 *     format memory block.                   *
 *                             *
 *  RETURNS    : TRUE  - if successful.                *
 *     FALSE - otherwise                   *
 *                             *
 ****************************************************************************/

BOOL FASTCALL DibInfo( HANDLE hbi, LPBITMAPINFOHEADER lpbi )
{
   if (hbi) {
      *lpbi = *(LPBITMAPINFOHEADER) GlobalLock(hbi);

   /* fill in the default fields */
      if (lpbi->biSize != sizeof(BITMAPCOREHEADER)) {
         if (lpbi->biSizeImage == 0L)
            lpbi->biSizeImage = 
                WIDTHBYTES(lpbi->biWidth*lpbi->biBitCount) * lpbi->biHeight;

         if (lpbi->biClrUsed == 0L)
            lpbi->biClrUsed = DibNumColors(lpbi);
         }
      GlobalUnlock(hbi);
      return TRUE;
      }
   return FALSE;
}

/****************************************************************************
 *                             *
 *  FUNCTION   : CreateBIPalette(LPBITMAPINFOHEADER lpbi)          *
 *                             *
 *  PURPOSE    : Given a Pointer to a BITMAPINFO struct will create a       *
 *     a GDI palette object from the color table.         *
 *                             *
 *  RETURNS    : A handle to the palette.              *
 *                             *
 ****************************************************************************/

HPALETTE FASTCALL CreateBIPalette( LPBITMAPINFOHEADER lpbi )
{
   LOGPALETTE NEAR      *pPal;
   HPALETTE            hpal = NULL;
   WORD                nNumColors;
   BYTE                red;
   BYTE                green;
   BYTE                blue;
   WORD                 i;
   RGBQUAD        FAR *pRgb;

   if (!lpbi)
      return NULL;

   if (lpbi->biSize != sizeof(BITMAPINFOHEADER))
      return NULL;

    /* Get a pointer to the color table and the number of colors in it */
   pRgb = (RGBQUAD FAR *) ((LPSTR) lpbi + (WORD) lpbi->biSize);
   nNumColors = DibNumColors(lpbi);

   if (nNumColors) {
   /* Allocate for the logical palette structure */
      pPal = (LOGPALETTE NEAR *) LocalAlloc(LPTR, sizeof(LOGPALETTE) + nNumColors * sizeof(PALETTEENTRY));
      if (!pPal)
         return NULL;

      pPal->palNumEntries = nNumColors;
      pPal->palVersion = PALVERSION;

   /* Fill in the palette entries from the DIB color table and
    * create a logical color palette.
    */
      for (i = 0;  i < nNumColors;  i++) {
         pPal->palPalEntry[i].peRed = pRgb[i].rgbRed;
         pPal->palPalEntry[i].peGreen = pRgb[i].rgbGreen;
         pPal->palPalEntry[i].peBlue = pRgb[i].rgbBlue;
         pPal->palPalEntry[i].peFlags = (BYTE) 0;
         }
      hpal = CreatePalette(pPal);
      LocalFree((HANDLE) pPal);
      }
   else if (lpbi->biBitCount == 24) {
   /* A 24 bitcount DIB has no color table entries so, set the number of
    * to the maximum value (256).
    */
      nNumColors = MAXPALETTE;
      pPal = (LOGPALETTE NEAR *) LocalAlloc(LPTR, sizeof(LOGPALETTE) + nNumColors * sizeof(PALETTEENTRY));
      if (!pPal)
         return NULL;

      pPal->palNumEntries = nNumColors;
      pPal->palVersion = PALVERSION;

      red = green = blue = 0;

   /* Generate 256 (= 8*8*4) RGB combinations to fill the palette
    * entries.
    */
      for (i = 0;  i < pPal->palNumEntries;  i++) {
         pPal->palPalEntry[i].peRed = red;
         pPal->palPalEntry[i].peGreen = green;
         pPal->palPalEntry[i].peBlue = blue;
         pPal->palPalEntry[i].peFlags = (BYTE) 0;

         if (!(red += 32))
            if (!(green += 32))
               blue += 64;
         }
      hpal = CreatePalette(pPal);
      LocalFree((HANDLE) pPal);
      }
   return hpal;
}

/****************************************************************************
 *                             *
 *  FUNCTION   : CreateDibPalette(HANDLE hbi)                *
 *                             *
 *  PURPOSE    : Given a Global HANDLE to a BITMAPINFO Struct         *
 *     will create a GDI palette object from the color table.     *
 *     (BITMAPINFOHEADER format DIBs only)                *
 *                             *
 *  RETURNS    : A handle to the palette.              *
 *                             *
 ****************************************************************************/

HPALETTE FASTCALL CreateDibPalette( HANDLE hbi )
{
   HPALETTE hpal;

   if (!hbi)
      return NULL;
   hpal = CreateBIPalette((LPBITMAPINFOHEADER) GlobalLock(hbi));
   GlobalUnlock(hbi);
   return hpal;
}

/****************************************************************************
 *                             *
 *  FUNCTION   : ReadDibBitmapInfo(int fh)                *
 *                             *
 *  PURPOSE    : Will read a file in DIB format and return a global HANDLE  *
 *     to it's BITMAPINFO.  This function will work with both     *
 *     "old" (BITMAPCOREHEADER) and "new" (BITMAPINFOHEADER)       *
 *     bitmap formats, but will always return a "new" BITMAPINFO  *
 *                             *
 *  RETURNS    : A handle to the BITMAPINFO of the DIB in the file.      *
 *                             *
 ****************************************************************************/

HANDLE FASTCALL ReadDibBitmapInfo( int fh )
{
   DWORD     off;
   HANDLE    hbi = NULL;
   int       size;
   int       i;
   WORD      nNumColors;

   RGBQUAD FAR       *pRgb;
   BITMAPINFOHEADER   bi;
   BITMAPCOREHEADER   bc;
   LPBITMAPINFOHEADER lpbi;
   BITMAPFILEHEADER   bf;
   DWORD        dwWidth = 0;
   DWORD        dwHeight = 0;
   WORD         wPlanes, wBitCount;

   if (fh == -1)
      return NULL;

    /* Reset file pointer and read file header */
   off = _llseek(fh, 0L, SEEK_CUR);
   if (sizeof (bf) != _lread(fh, (LPSTR) &bf, sizeof (bf)))
      return FALSE;

    /* Do we have a RC HEADER? */
   if (! ISDIB (bf.bfType)) {
      bf.bfOffBits = 0L;
      _llseek(fh, off, SEEK_SET);
      }
   if (sizeof (bi) != _lread(fh, (LPSTR) &bi, sizeof (bi)))
      return FALSE;

   nNumColors = DibNumColors(&bi);

    /* Check the nature (BITMAPINFO or BITMAPCORE) of the info. block
     * and extract the field information accordingly. If a BITMAPCOREHEADER,
     * transfer it's field information to a BITMAPINFOHEADER-style block
     */
   switch (size = (int) bi.biSize) {
  case sizeof(BITMAPINFOHEADER):
      break;

  case sizeof(BITMAPCOREHEADER):

      bc = *(BITMAPCOREHEADER *) &bi;

      dwWidth = (DWORD) bc.bcWidth;
      dwHeight = (DWORD) bc.bcHeight;
      wPlanes = bc.bcPlanes;
      wBitCount = bc.bcBitCount;

      bi.biSize = sizeof(BITMAPINFOHEADER);
      bi.biWidth = dwWidth;
      bi.biHeight = dwHeight;
      bi.biPlanes = wPlanes;
      bi.biBitCount = wBitCount;

      bi.biCompression = BI_RGB;
      bi.biSizeImage = 0;
      bi.biXPelsPerMeter = 0;
      bi.biYPelsPerMeter = 0;
      bi.biClrUsed = nNumColors;
      bi.biClrImportant = nNumColors;

      _llseek(fh, (LONG) sizeof(BITMAPCOREHEADER) - 
          sizeof(BITMAPINFOHEADER), SEEK_CUR);
      break;

  default:
       /* Not a DIB! */
      return NULL;
      }

    /*   Fill in some default values if they are zero */
   if (bi.biSizeImage == 0) {
      bi.biSizeImage = WIDTHBYTES ((DWORD)bi.biWidth * bi.biBitCount)
           * bi.biHeight;
      }
   if (bi.biClrUsed == 0)
      bi.biClrUsed = DibNumColors(&bi);

    /* Allocate for the BITMAPINFO structure and the color table. */
   hbi = GlobalAlloc(GHND, (LONG) bi.biSize + nNumColors * sizeof(RGBQUAD));
   if (!hbi)
      return NULL;
   lpbi = (VOID FAR *) GlobalLock(hbi);
   *lpbi = bi;

    /* Get a pointer to the color table */
   pRgb = (RGBQUAD FAR *) ((LPSTR) lpbi + bi.biSize);
   if (nNumColors) {
      if (size == sizeof(BITMAPCOREHEADER)) {
       /* Convert a old color table (3 byte RGBTRIPLEs) to a new
        * color table (4 byte RGBQUADs)
             */
         _lread(fh, (LPSTR) pRgb, nNumColors * sizeof(RGBTRIPLE));

         for (i = nNumColors - 1;  i >= 0;  i--) {
            RGBQUAD rgb;

            rgb.rgbRed = ((RGBTRIPLE FAR *) pRgb)[i].rgbtRed;
            rgb.rgbBlue = ((RGBTRIPLE FAR *) pRgb)[i].rgbtBlue;
            rgb.rgbGreen = ((RGBTRIPLE FAR *) pRgb)[i].rgbtGreen;
            rgb.rgbReserved = (BYTE) 0;

            pRgb[i] = rgb;
            }
         }
      else 
         _lread(fh, (LPSTR) pRgb, nNumColors * sizeof(RGBQUAD));
      }

   if (bf.bfOffBits != 0L)
      _llseek(fh, off + bf.bfOffBits, SEEK_SET);

   GlobalUnlock(hbi);
   return hbi;
}

/****************************************************************************
 *                             *
 *  FUNCTION   :  PaletteSize(VOID FAR * pv)              *
 *                             *
 *  PURPOSE    :  Calculates the palette size in bytes. If the info. block  *
 *      is of the BITMAPCOREHEADER type, the number of colors is  *
 *      multiplied by 3 to give the palette size, otherwise the   *
 *      number of colors is multiplied by 4.                      *
 *                             *
 *  RETURNS    :  Palette size in number of bytes.           *
 *                             *
 ****************************************************************************/

WORD FASTCALL PaletteSize( VOID FAR * pv )
{
   LPBITMAPINFOHEADER lpbi;
   WORD         NumColors;

   lpbi = (LPBITMAPINFOHEADER) pv;
   NumColors = DibNumColors(lpbi);

   if (lpbi->biSize == sizeof(BITMAPCOREHEADER))
      return NumColors * sizeof(RGBTRIPLE);
   else 
      return NumColors * sizeof(RGBQUAD);
}

/****************************************************************************
 *                             *
 *  FUNCTION   : DibNumColors(VOID FAR * pv)              *
 *                             *
 *  PURPOSE    : Determines the number of colors in the DIB by looking at   *
 *     the BitCount filed in the info block.           *
 *                             *
 *  RETURNS    : The number of colors in the DIB.            *
 *                             *
 ****************************************************************************/

WORD FASTCALL DibNumColors( VOID FAR * pv )
{
   int      bits;
   LPBITMAPINFOHEADER   lpbi;
   LPBITMAPCOREHEADER   lpbc;

   lpbi = ((LPBITMAPINFOHEADER) pv);
   lpbc = ((LPBITMAPCOREHEADER) pv);

    /*   With the BITMAPINFO format headers, the size of the palette
     *   is in biClrUsed, whereas in the BITMAPCORE - style headers, it
     *   is dependent on the bits per pixel ( = 2 raised to the power of
     *   bits/pixel).
     */
   if (lpbi->biSize != sizeof(BITMAPCOREHEADER)) {
      if (lpbi->biClrUsed != 0)
         return (WORD) lpbi->biClrUsed;
      bits = lpbi->biBitCount;
      }
   else 
      bits = lpbc->bcBitCount;

   switch (bits) {
  case 1:
      return 2;
  case 4:
      return 16;
  case 8:
      return 256;
  default:
      /* A 24 bitcount DIB has no color table */
      return 0;
      }
}

//---------------------------------------------------------------------
//
// Function:   DIBToBitmap
//
// Purpose:    Given a handle to global memory with a DIB spec in it,
//             and a palette, returns a device dependent bitmap.  The
//             The DDB will be rendered with the specified palette.
//
// Parms:      hDIB == HANDLE to global memory containing a DIB spec
//                     (either BITMAPINFOHEADER or BITMAPCOREHEADER)
//             hPal == Palette to render the DDB with.  If it's NULL,
//                     use the default palette.
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

HBITMAP DIBToBitmap (HANDLE hDIB, HPALETTE hPal)
{
   LPSTR    lpDIBHdr, lpDIBBits;
   HBITMAP  hBitmap;
   HDC      hDC;
   HPALETTE hOldPal = NULL;

   if (!hDIB)
      return NULL;

   lpDIBHdr  = GlobalLock (hDIB);
   lpDIBBits = FindDIBBits (lpDIBHdr);
   hDC       = GetDC (NULL);

   if (!hDC)
      {
      GlobalUnlock (hDIB);
      return NULL;
      }

   if (hPal)
      {
      HPALETTE hOldPal;
      HWND hActWin;
      HDC hActDC;

      hOldPal = SelectPalette( hDC, hPal, FALSE );
   #ifdef WIN32
      hActWin = GetForegroundWindow();
   #else
      hActWin = GetActiveWindow();
   #endif
      hActDC = GetDC( hActWin );
      SelectPalette( hActDC, hPal, FALSE );
      RealizePalette( hActDC );
      ReleaseDC( hActWin, hActDC );
      }
   hBitmap = CreateDIBitmap (hDC,
                             (LPBITMAPINFOHEADER) lpDIBHdr,
                             CBM_INIT,
                             lpDIBBits,
                             (LPBITMAPINFO) lpDIBHdr,
                             DIB_RGB_COLORS);
   if (hOldPal)
      SelectPalette (hDC, hOldPal, FALSE);
   DeletePalette( hPal );
   ReleaseDC (NULL, hDC);
   GlobalUnlock (hDIB);
   return hBitmap;
}

//---------------------------------------------------------------------
//
// Function:   FindDIBBits
//
// Purpose:    Given a pointer to a DIB, returns a pointer to the
//             DIB's bitmap bits.
//
// Parms:      lpbi == pointer to DIB header (either BITMAPINFOHEADER
//                       or BITMAPCOREHEADER)
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

LPSTR FASTCALL FindDIBBits (LPSTR lpbi)
{
   return (lpbi + *(LPDWORD)lpbi + PaletteSize (lpbi));
}
