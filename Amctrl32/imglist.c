/* IMGLIST.C - Implements image lists
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <ctype.h>
#include "amctrls.h"
#include "amctrli.h"

/* Defines one image list.
 */
typedef struct _IMAGELIST {
   int cGrow;                    /* Is the number of items by which the list grows */
   int cImages;                  /* Is the number of items in the list */
   int cSlots;                   /* Is the number of slots in the list */
   int cx;                       /* Width of each image */
   int cy;                       /* Height of each image */
   int flags;                    /* Specifies the flags for this image lisy */
   COLORREF clrBk;               /* The image list background colour */
   COLORREF crTransparent;       /* Transparent colour in bitmaps */
   HBITMAP hbmp;                 /* Specifies the handle to the imagelist bitmap */
   HBITMAP hbmpMask;             /* Specifies the handle of the imagelist masks */
} IMAGELIST;

static HIMAGELIST himlDrag = NULL;     /* Handle of drag imagelist */
static int drgdxHotSpot;               /* Drag X hotspot */
static int drgdyHotSpot;               /* Drag Y hotspot */
static HWND hwndDrag;                  /* Handle of drag window */
static int drgx;                       /* X offset of drag image */
static int drgy;                       /* Y offset of drag image */
static HBITMAP hbmpSaved;              /* Handle of background bitmap */
static HDC hdcSaved;                   /* Handle of DC */
static HBITMAP hbmpOldSaved;           /* Bitmap in hdcSaved before we selected hbmpSaved */
static BOOL fBackSaved;                /* TRUE if background has been saved */
static POINT ptLast;                   /* Coordinates of last drag */

static void NEAR PASCAL CreateEmptyBitmaps( HIMAGELIST, int, int );
static void NEAR PASCAL DeleteBitmaps( HIMAGELIST );
static HBITMAP NEAR PASCAL IconToBitmap( HINSTANCE, LPCSTR );

/* Create a new image list.
 */
WINCOMMCTRLAPI HIMAGELIST WINAPI EXPORT Amctl_ImageList_Create( int cx, int cy, UINT flags, int cInitial, int cGrow )
{
   static HIMAGELIST himl;

   INITIALISE_PTR(himl);
   if( fNewMemory( &himl, sizeof( IMAGELIST ) ) )
      {
      himl->cGrow = cGrow;
      himl->cSlots = 0;
      himl->cx = cx;
      himl->cy = cy;
      himl->flags = flags;
      himl->clrBk = CLR_DEFAULT;
      CreateEmptyBitmaps( himl, cInitial, cGrow ? ( ( cInitial / cGrow ) + 1 ) * cGrow : cInitial );
      }
   return( himl );
}

/* Destroys an image list.
 */
BOOL WINAPI EXPORT Amctl_ImageList_Destroy( HIMAGELIST himl )
{
   DeleteBitmaps( himl );
   FreeMemory( &himl );
   return( TRUE );
}

/* Adds an image or images to an image list, generating a mask from the
 * given bitmap.
 */
int WINAPI EXPORT Amctl_ImageList_AddMasked( HIMAGELIST himl, HBITMAP hbmImage, COLORREF crMask )
{
   HBITMAP hbmpOldDest;
   HBITMAP hbmpOldSrc;
   HBITMAP hbmpOldMask;
   BITMAP bmp;
   HDC hdcSrc;
   HDC hdcDest;
   HDC hdcMask;
   HDC hdc;
   int cImageOffset;
   int cImages;
   int dx;

   /* If no mask, can't do this.
    */
   if( !( himl->flags & ILC_MASK ) )
      return( -1 );

   /* Get the size of the bitmap. The number of items to be added will be
    * the width of the bitmap divided by the size of each one.
    */
   GetObject( hbmImage, sizeof( BITMAP ), &bmp );
   cImages = ( bmp.bmWidth + ( himl->cx - 1 ) ) / himl->cx;

   /* If we need more room, delete the existing bitmaps and create new ones
    * up to the specified size.
    */
   if( himl->cImages + cImages > himl->cSlots )
      if( himl->cGrow )
         {
         int cNewSize;

         DeleteBitmaps( himl );
         cNewSize = ( ( ( himl->cImages + cImages ) / himl->cGrow ) + 1 ) * himl->cGrow;
         CreateEmptyBitmaps( himl, himl->cImages + cImages, cNewSize );
         }
      else
         return( -1 );

   /* Offset of these new bitmaps.
    */
   cImageOffset = himl->cImages;
   dx = cImageOffset * himl->cx;

   /* Initialise working stuff.
    */
   hdc = GetDC( NULL );
   hdcSrc = CreateCompatibleDC( hdc );
   hdcDest = CreateCompatibleDC( hdc );
   hdcMask = CreateCompatibleDC( hdc );
   hbmpOldSrc = SelectBitmap( hdcSrc, hbmImage );

   /* Copy the image into the image bitmap.
    */
   hbmpOldDest = SelectBitmap( hdcDest, himl->hbmp );
   BitBlt( hdcDest, dx, 0, himl->cx * cImages, himl->cy, hdcSrc, 0, 0, SRCCOPY );

   /* Create the mask.
    */
   hbmpOldMask = SelectBitmap( hdcMask, himl->hbmpMask );
   SetBkColor( hdcSrc, crMask );
   BitBlt( hdcMask, dx, 0, himl->cx * cImages, himl->cy, hdcSrc, 0, 0, SRCCOPY );

   /* Set the total number of images in the list.
    */
   himl->cImages += cImages;
   himl->crTransparent = crMask;

   /* Clean up at the end.
    */
   SelectBitmap( hdcMask, hbmpOldMask );
   SelectBitmap( hdcDest, hbmpOldDest );
   SelectBitmap( hdcSrc, hbmpOldSrc );
   DeleteDC( hdcMask );
   DeleteDC( hdcSrc );
   DeleteDC( hdcDest );
   ReleaseDC( NULL, hdc );
   return( cImageOffset );
}

WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_Draw( HIMAGELIST himl, int i, HDC hdcDst, int x, int y, UINT fStyle )
{
   if( ( ( fStyle & ILD_TRANSPARENT ) || ( ( fStyle == ILD_NORMAL ) && himl->clrBk == CLR_NONE ) ) && himl->hbmpMask )
      {
      HBITMAP hbmpOld;
      HDC hdcSrc;
      HBITMAP hbmpOldMask;
      HDC hdcMask;

      /* Create DCs compatible with the target DC.
       */
      hdcSrc = CreateCompatibleDC( hdcDst );
      hbmpOld = SelectBitmap( hdcSrc, himl->hbmp );

      /* Otherwise we do it ourselves.
       */
      hdcMask = CreateCompatibleDC( hdcDst );
      hbmpOldMask = SelectBitmap( hdcMask, himl->hbmpMask );

      /* Convert the transparent colour in the source to black, then use that with the
       * mask to paint on the destination DC. Finally restore the source mask back to
       * normal when we're done.
       */
      SetBkColor( hdcSrc, RGB( 0, 0, 0 ) );
      SetTextColor( hdcSrc, RGB( 255, 255, 255 ) );
      BitBlt( hdcSrc, himl->cx * i, 0, himl->cx, himl->cy, hdcMask, himl->cx * i, 0, SRCAND );
      BitBlt( hdcDst, x, y, himl->cx, himl->cy, hdcMask, himl->cx * i, 0, SRCAND );
      BitBlt( hdcDst, x, y, himl->cx, himl->cy, hdcSrc, himl->cx * i, 0, SRCPAINT );

      SetBkColor( hdcSrc, himl->crTransparent );
      SetTextColor( hdcSrc, RGB( 0, 0, 0 ) );
      BitBlt( hdcSrc, himl->cx * i, 0, himl->cx, himl->cy, hdcMask, himl->cx * i, 0, SRCPAINT );

      SelectBitmap( hdcMask, hbmpOldMask );
      DeleteDC( hdcMask );
   
      /* Clean up before we quit.
       */
      SelectBitmap( hdcSrc, hbmpOld );
      DeleteDC( hdcSrc );
      }
   else if( ( fStyle == ILD_NORMAL ) && himl->clrBk != CLR_NONE )
      {
      HBITMAP hbmpOld;
      HDC hdcSrc;

      /* Create a DC and set the current background colour.
       */
      hdcSrc = CreateCompatibleDC( hdcDst );
      SetBkColor( hdcSrc, himl->clrBk );

      /* Straight bitblt of image to source.
       */
      hbmpOld = SelectBitmap( hdcSrc, himl->hbmp );
      BitBlt( hdcDst, x, y, himl->cx, himl->cy, hdcSrc, himl->cx * i, 0, SRCCOPY );
      SelectBitmap( hdcSrc, hbmpOld );

      /* Delete the DC.
       */
      DeleteDC( hdcSrc );
      }
   return( TRUE );
}

/* This function initiates an image-list drag.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_BeginDrag( HIMAGELIST himlTrack, int iTrack, int dxHotspot, int dyHotspot )
{
   if( NULL != himlDrag )
      return( FALSE );
   himlDrag = himlTrack;
   drgdxHotSpot = dxHotspot;
   drgdyHotSpot = dyHotspot;
   return( TRUE );
}

/* This function starts an image-list drag.
 * - Allocate space for saving the background.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_DragEnter( HWND hwndLock, int x, int y )
{
   HDC hdc;

   /* Save the parameters.
    */
   hwndDrag = hwndLock;
   drgx = x;
   drgy = y;

   /* Compute background size and create a bitmap.
    */
   hdc = GetDC( hwndLock );
   hbmpSaved = CreateCompatibleBitmap( hdc, himlDrag->cx, himlDrag->cy );
   hdcSaved = CreateCompatibleDC( hdc );
   hbmpOldSaved = SelectBitmap( hdcSaved, hbmpSaved );
   ReleaseDC( hwndLock, hdc );
   fBackSaved = FALSE;
   return( TRUE );
}

/* This finishes a drag operation.
 */
WINCOMMCTRLAPI void WINAPI EXPORT Amctl_ImageList_EndDrag( void )
{
   Amctl_ImageList_DragLeave( hwndDrag );
   SelectBitmap( hdcSaved, hbmpOldSaved );
   DeleteDC( hdcSaved );
   DeleteBitmap( hbmpSaved );
   Amctl_ImageList_Destroy( himlDrag );
   himlDrag = NULL;
}

/* This function restores the drag image background so that the caller can
 * draw in the window without obliterating the drag image.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_DragLeave( HWND hwndLock )
{
   if( fBackSaved )
      {
      HDC hdc;

      hdc = GetDC( hwndDrag );
      BitBlt( hdc, ptLast.x, ptLast.y, himlDrag->cx, himlDrag->cy, hdcSaved, 0, 0, SRCCOPY );
      fBackSaved = FALSE;
      ReleaseDC( hwndDrag, hdc );
      }
   return( TRUE );
}

/* This function moves the drag image. We simply restore the
 * background at the old drag position, then draw the image at the
 * new position.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_DragMove( int x, int y )
{
   HDC hdc;

   hdc = GetDC( hwndDrag );
   if( fBackSaved )
      {
      BitBlt( hdc, ptLast.x, ptLast.y, himlDrag->cx, himlDrag->cy, hdcSaved, 0, 0, SRCCOPY );
      fBackSaved = FALSE;
      }
   x += drgdxHotSpot;
   y += drgdyHotSpot;
   ptLast.x = x;
   ptLast.y = y;
   BitBlt( hdcSaved, 0, 0, himlDrag->cx, himlDrag->cy, hdc, x, y, SRCCOPY );
   fBackSaved = TRUE;
   Amctl_ImageList_Draw( himlDrag, 0, hdc, x, y, ILD_TRANSPARENT );
   ReleaseDC( hwndDrag, hdc );
   return( TRUE );
}

/* This function sets the current background colour for an image list.
 */
WINCOMMCTRLAPI COLORREF WINAPI EXPORT Amctl_ImageList_SetBkColor( HIMAGELIST himl, COLORREF clrBk )
{
   COLORREF clrPrevBk;

   clrPrevBk = himl->clrBk;
   himl->clrBk = clrBk;
   return( clrPrevBk );
}

/* This function retrieves the current background colour for an image list.
 */
WINCOMMCTRLAPI COLORREF WINAPI EXPORT Amctl_ImageList_GetBkColor( HIMAGELIST himl )
{
   return( himl->clrBk );
}

/* This function retrieves the number of images in an image list.
 */
WINCOMMCTRLAPI int WINAPI EXPORT Amctl_ImageList_GetImageCount( HIMAGELIST himl )
{
   return( himl->cImages );
}

/* This function retrieves information about an image.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_ImageList_GetImageInfo( HIMAGELIST himl, int i, IMAGEINFO FAR * pImageInfo )
{
   pImageInfo->hbmImage = himl->hbmp;
   pImageInfo->hbmMask = himl->hbmpMask;
   SetRect( &pImageInfo->rcImage, himl->cx * i, 0, himl->cx * ( i + 1 ), himl->cy );
   return( TRUE );
}

/* Creates an image list from the given bitmap, cursor or icon resource.
 */
WINCOMMCTRLAPI HIMAGELIST WINAPI EXPORT Amctl_ImageList_LoadImage( HINSTANCE hi, LPCSTR lpbmp, int cx, int cGrow,
   COLORREF crMask, UINT uType, UINT uFlags )
{
   HIMAGELIST himl;
   HBITMAP hbmImage;
   BITMAP bmp;
   int cy;
   int cImages;

   /* If lpbmp is an icon or cursor image, create a bitmap from it.
    */
   if( uType == IMAGE_ICON )
      hbmImage = IconToBitmap( hi, lpbmp );
   else
      hbmImage = LoadBitmap( hi, lpbmp );

   /* Fail if the resource couldn't be loaded.
    */
   if( !hbmImage )
      return( NULL );

   /* Get information about the bitmap to compute the number of images
    * in the bitmap and the height of each image.
    */
   GetObject( hbmImage, sizeof( BITMAP ), &bmp );
   cImages = ( bmp.bmWidth + ( cx - 1 ) ) / cx;
   cy = bmp.bmHeight;

   /* Create the image list, then add the bitmap to the list.
    */
   if( himl = Amctl_ImageList_Create( cx, cy, uFlags, cImages, cGrow ) )
      Amctl_ImageList_AddMasked( himl, hbmImage, crMask );

   /* Destroy the bitmap we loaded, then return the image list handle.
    */
   DeleteBitmap( hbmImage );
   return( himl );
}

WINCOMMCTRLAPI int WINAPI EXPORT Amctl_ImageList_ReplaceIcon( HIMAGELIST himl, int i, HICON hicon )
{
   return( -1 );
}

WINCOMMCTRLAPI int WINAPI EXPORT Amctl_ImageList_AddFromImageList( HIMAGELIST himlDest, HIMAGELIST himlSrc, int iSrc )
{
   return( -1 );
}

WINCOMMCTRLAPI int WINAPI EXPORT Amctl_ImageList_Add( HIMAGELIST himl, HBITMAP hbmImage, HBITMAP hbmMask )
{
   return( -1 );
}

/* Creates empty image bitmaps of the specified size. The function assumes that
 * the image list structure has been initialised with the flags and size before entry.
 */
static void NEAR PASCAL CreateEmptyBitmaps( HIMAGELIST himl, int cSize, int cSlots )
{
   if( himl->flags & ILC_MASK )
      himl->hbmpMask = CreateBitmap( himl->cx * cSize, himl->cy, 1, 1, NULL );
   else
      himl->hbmpMask = NULL;
   if( ( himl->flags & 0xFE ) == ILC_COLOR )
      {
      HDC hdc;

      hdc = GetDC( NULL );
      himl->hbmp = CreateCompatibleBitmap( hdc, himl->cx * cSize, himl->cy );
      ReleaseDC( NULL, hdc );
      }
   else
      {
      }
   himl->cImages = 0;
   himl->cSlots = cSlots;
}

/* Deletes the bitmaps in the image list.
 */
static void NEAR PASCAL DeleteBitmaps( HIMAGELIST himl )
{
   DeleteBitmap( himl->hbmp );
   if( himl->hbmpMask )
      DeleteBitmap( himl->hbmpMask );
}

static HBITMAP NEAR PASCAL IconToBitmap( HINSTANCE hi, LPCSTR lpbmp )
{
   HICON hIcon;
   HBITMAP hbmp;

   if( ( hIcon = LoadIcon( hi, lpbmp ) ) != NULL )
      {
      int cxIcon;
      int cyIcon;
      HDC hdc;

      cxIcon = GetSystemMetrics( SM_CXICON );
      cyIcon = GetSystemMetrics( SM_CYICON );
      hdc = GetDC( NULL );
      hbmp = CreateCompatibleBitmap( hdc, cxIcon, cyIcon );
      ReleaseDC( NULL, hdc );
      return( hbmp );
      }
   return( NULL );
}
