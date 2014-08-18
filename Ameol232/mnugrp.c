/* MNUGRP.C - Implements grouped menu buttons
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

#include "main.h"
#include "mnugrp.h"

static HBITMAP hBmp = NULL;

void FASTCALL CheckMenuGroup( HMENU hMenu, int idFirst, int idLast, int idCheck )
{
   register int c;

   if( hBmp == NULL )
      {
      DWORD dwDimensions;
      HDC hdc;
      HDC hdc2;
      int x1, y1;
      int nWidth;

      hdc = CreateDC( "DISPLAY", NULL, NULL, NULL );
      dwDimensions = GetMenuCheckMarkDimensions();
      hBmp = CreateBitmap( LOWORD( dwDimensions ), HIWORD( dwDimensions ), 1, 1, NULL );
      hdc2 = CreateCompatibleDC( hdc );
      DeleteDC( hdc );
      SelectBitmap( hdc2, hBmp );
      PatBlt( hdc2, 0, 0, LOWORD( dwDimensions ), HIWORD( dwDimensions ), WHITENESS );
      nWidth = ( LOWORD( dwDimensions ) / 4 ) * 2;
      x1 = ( nWidth / 2 ) + 1;
      y1 = ( HIWORD( dwDimensions ) - nWidth ) / 2;
      SelectPen( hdc2, GetStockObject( BLACK_PEN ) );
      SelectBrush( hdc2, GetStockObject( BLACK_BRUSH ) );
      Ellipse( hdc2, x1, y1, x1 + nWidth, y1 + nWidth );
      DeleteDC( hdc2 );
      }
   for( c = idFirst; c <= idLast; ++c )
      {
      SetMenuItemBitmaps( hMenu, c, MF_BYCOMMAND, NULL, hBmp );
      CheckMenuItem( hMenu, c, MF_BYCOMMAND | ( c == idCheck ) ? MF_CHECKED : MF_UNCHECKED );
      }
}

void FASTCALL DeleteCheckMenuItemGroupBitmap( void )
{
   if( hBmp )
      DeleteBitmap( hBmp );
}
