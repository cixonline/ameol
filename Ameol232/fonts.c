/* FONTS.C - Handles fonts and the Fonts Settings dialog
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
#include "palrc.h"
#include "resource.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Font names.
 */
static char szArial[] = "Verdana";           /* Actual name of Arial font */
static char szCourier[] = "Courier New";     /* Actual name of Courier font */
static char szTerminal[] = "Terminal";       /* Actual name of Terminal font */

static BOOL fFontsCreated = FALSE;  /* TRUE if the fonts have been created */

HFONT hEditFont = NULL;             /* Edit window font */
HFONT hTermFont = NULL;             /* Terminal window font */
HFONT hThreadFont = NULL;           /* Thread window font */
HFONT hThreadBFont = NULL;          /* Thread window font (boldface) */
HFONT hMsgFont = NULL;              /* Message window font */
HFONT hHelv8Font = NULL;            /* 8-point Helv font */
HFONT hHelvB8Font = NULL;           /* 8-point bold Helv font */
HFONT hHelv10Font = NULL;           /* 10-point Helv font */
HFONT hHelvB10Font = NULL;          /* 10-point bold Helv font */
HFONT hSys8Font = NULL;             /* 8-point bold system font */
HFONT hSys10Font = NULL;            /* 10-point bold system font */
HFONT hStatusFont = NULL;           /* Status bar font */
HFONT hFixedFont = NULL;            /* Fixed pitch font */
HFONT hGrpListFont = NULL;          /* Newsgroup list font */
HFONT hUserListFont = NULL;            /* CIX Users list font */
HFONT hCIXListFont = NULL;          /* CIX Conferences list font */
HFONT hInBasketFont = NULL;            /* In-basket font */
HFONT hInBasketBFont = NULL;        /* In-basket (boldface) font */
HFONT hOutBasketFont = NULL;        /* Out-basket font */
HFONT hResumesFont = NULL;          /* Resumes font */
HFONT hPrinterFont = NULL;          /* Printer font */
HFONT hSchedulerFont = NULL;        /* Scheduler window font */

LOGFONT FAR lfEditFont;             /* Edit window font structure */
LOGFONT FAR lfTermFont;             /* Terminal window font structure */
LOGFONT FAR lfThreadFont;           /* Thread window font structure */
LOGFONT FAR lfThreadBFont;          /* Thread window font structure (boldface) */
LOGFONT FAR lfMsgFont;              /* Message window font structure */
LOGFONT FAR lfStatusFont;           /* Status bar font structure */
LOGFONT FAR lfFixedFont;            /* Fixed pitch font */
LOGFONT FAR lfInBasketFont;         /* In-basket font */
LOGFONT FAR lfInBasketBFont;        /* In-basket font (boldface) */
LOGFONT FAR lfGrpListFont;          /* Newsgroup list font */
LOGFONT FAR lfUserListFont;         /* User list font */
LOGFONT FAR lfCIXListFont;          /* CIX list font */
LOGFONT FAR lfOutBasketFont;        /* Out-basket font */
LOGFONT FAR lfResumesFont;          /* Resumes font */
LOGFONT FAR lfPrinterFont;          /* Printer font */
LOGFONT FAR lfSchedulerFont;        /* Scheduler font */
LOGFONT FAR lfAddrBook;             /* Address book font */

int FASTCALL ReadInteger( char ** );
int FASTCALL ReadString( char **, char *, int );

/* This function returns one of the predefined fonts used
 * by Ameol. Index numbers 13 and 14 are the same as for
 * 12, owing to the change of the group list window.
 */
HFONT EXPORT WINAPI Ameol2_GetStockFont( int index )
{
   switch( index )
      {
      case 0:  return( hEditFont );
      case 1:  return( hTermFont );
      case 2:  return( hThreadFont );
      case 3:  return( hThreadBFont );
      case 4:  return( hMsgFont );
      case 5:  return( hHelv8Font );
      case 6:  return( hSys8Font );
      case 7:  return( hHelv10Font );
      case 8:  return( hSys10Font );
      case 9:  return( hStatusFont );
      case 10: return( hInBasketFont );
      case 11: return( hInBasketBFont );
      case 12: return( hGrpListFont );
      case 13: return( hGrpListFont );
      case 14: return( hGrpListFont );
      case 15: return( hFixedFont );
      case 16: return( hOutBasketFont );
      case 17: return( hUserListFont );
      case 18: return( hCIXListFont );
      case 19: return( hResumesFont );
      case 20: return( hPrinterFont );
      case 21: return( hSchedulerFont );
      default: return( NULL );
      }
}

/* This function deletes the fonts used by Ameol.
 */
void FASTCALL DeleteAmeolFonts( void )
{
   if( fFontsCreated )
      {
      DeleteFont( hEditFont );
      DeleteFont( hTermFont );
      DeleteFont( hThreadFont );
      DeleteFont( hThreadBFont );
      DeleteFont( hStatusFont );
      DeleteFont( hMsgFont );
      DeleteFont( hFixedFont );
      DeleteFont( hGrpListFont );
      DeleteFont( hInBasketFont );
      DeleteFont( hInBasketBFont );
      DeleteFont( hOutBasketFont );
      DeleteFont( hUserListFont );
      DeleteFont( hCIXListFont );
      DeleteFont( hResumesFont );
      DeleteFont( hPrinterFont );
      DeleteFont( hSchedulerFont );
      fFontsCreated = FALSE;
      }
}

/* This function deletes the Ameol system (non-editable)
 * fonts.
 */
void FASTCALL DeleteAmeolSystemFonts( void )
{
   DeleteFont( hHelv8Font );
   DeleteFont( hHelvB8Font );
   DeleteFont( hHelv10Font );
   DeleteFont( hHelvB10Font );
   DeleteFont( hSys8Font );
   DeleteFont( hSys10Font );
}

/* This function creates the specified Ameol font.
 */
void FASTCALL CreateAmeolFont( int index, LOGFONT * lpLogFont )
{
   int cPixelsPerInch;
   HDC hdc;

   /* Get the font metrics, depending on the display size.
    */
   hdc = GetDC( NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   ReleaseDC( NULL, hdc );

   /* Create the appropriate font
    */
   switch( index )
      {
      case 0:  /* Message font (lfMsgFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 1:  /* Edit font (lfEditFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
         strcpy( lpLogFont->lfFaceName, szCourier );
         break;

      case 2:  /* Terminal font (lfTermFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = OEM_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = DEFAULT_QUALITY;
         lpLogFont->lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
         strcpy( lpLogFont->lfFaceName, szTerminal );
         break;

      case 3:  /* Thread font (lfThreadFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 4: {   /* Status font (lfStatusFont) */
      #ifdef WIN32
         NONCLIENTMETRICS nclm;
      #endif

         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 400;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
      #ifdef WIN32
         nclm.cbSize = sizeof(NONCLIENTMETRICS);
         if( SystemParametersInfo( SPI_GETNONCLIENTMETRICS, 0, &nclm, 0 ) )
            *lpLogFont = nclm.lfStatusFont;
      #endif
         break;
         }

      case 5:  /* In-Basket font (lfInBasketFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 6:  /* Newsgroups list font (lfGrpListFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;
      
      case 7:  /* Fixed font (&lfFixedFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
         strcpy( lpLogFont->lfFaceName, szCourier );
         break;

      case 8:  /* Out-Basket font (&lfOutBasketFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 9:  /* Users list font (&lfUserListFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 10: /* CIX list font (&lfCIXListFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 11: /* Resumes font (&lfResumesFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
         strcpy( lpLogFont->lfFaceName, szCourier );
         break;

      case 12: /* Printer list font (&lfPrinterFont) */
         lpLogFont->lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
         strcpy( lpLogFont->lfFaceName, szCourier );
         break;

      case 13: /* Scheduler font (&lfSchedulerFont) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;

      case 14: /* Address book font (&lfAddrBook) */
         lpLogFont->lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
         lpLogFont->lfWidth = 0;
         lpLogFont->lfEscapement = 0;
         lpLogFont->lfOrientation = 0;
         lpLogFont->lfWeight = 0;
         lpLogFont->lfItalic = 0;
         lpLogFont->lfUnderline = 0;
         lpLogFont->lfStrikeOut = 0;
         lpLogFont->lfCharSet = ANSI_CHARSET;
         lpLogFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
         lpLogFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
         lpLogFont->lfQuality = PROOF_QUALITY;
         lpLogFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
         strcpy( lpLogFont->lfFaceName, szArial );
         break;
      }
}

/* This function creates the fonts used by Ameol.
 */
void FASTCALL CreateAmeolFonts( BOOL fReset )
{
   /* If the fonts are already created, delete them.
    */
   if( fFontsCreated )
      DeleteAmeolFonts();

   /* Create the fonts.
    */
   CreateAmeolFont( 0, &lfMsgFont );
   CreateAmeolFont( 1, &lfEditFont );
   CreateAmeolFont( 2, &lfTermFont );
   CreateAmeolFont( 3, &lfThreadFont );
   CreateAmeolFont( 4, &lfStatusFont );
   CreateAmeolFont( 5, &lfInBasketFont );
   CreateAmeolFont( 6, &lfGrpListFont );
   CreateAmeolFont( 7, &lfFixedFont );
   CreateAmeolFont( 8, &lfOutBasketFont );
   CreateAmeolFont( 9, &lfUserListFont );
   CreateAmeolFont( 10, &lfCIXListFont );
   CreateAmeolFont( 11, &lfResumesFont );
   CreateAmeolFont( 12, &lfPrinterFont );
   CreateAmeolFont( 13, &lfSchedulerFont );
   CreateAmeolFont( 14, &lfAddrBook );

   /* Override any of the above from the configuration file.
    */
   ReadFontData( "Edit", &lfEditFont );
   ReadFontData( "Terminal", &lfTermFont );
   ReadFontData( "Message", &lfMsgFont );
   ReadFontData( "Thread", &lfThreadFont );
   ReadFontData( "In Basket", &lfInBasketFont );
   ReadFontData( "Out Basket", &lfOutBasketFont );
   ReadFontData( "Scheduler", &lfSchedulerFont );
   ReadFontData( "Newsgroup Lists", &lfGrpListFont );
   ReadFontData( "User List", &lfUserListFont );
   ReadFontData( "CIX List", &lfCIXListFont );
   ReadFontData( "Status Bar", &lfStatusFont );
   ReadFontData( "Fixed", &lfFixedFont );
   ReadFontData( "Resumes", &lfResumesFont );
   ReadFontData( "Printer", &lfPrinterFont );
   ReadFontData( "Address Book", &lfAddrBook );

   /* Create the fonts.
    */
   hEditFont = CreateFontIndirect( &lfEditFont );
   hResumesFont = CreateFontIndirect( &lfResumesFont );
   hTermFont = CreateFontIndirect( &lfTermFont );
   hMsgFont = CreateFontIndirect( &lfMsgFont );
   hThreadFont = CreateFontIndirect( &lfThreadFont );
   hInBasketFont = CreateFontIndirect( &lfInBasketFont );
   hOutBasketFont = CreateFontIndirect( &lfOutBasketFont );
   hSchedulerFont = CreateFontIndirect( &lfSchedulerFont );
   hGrpListFont = CreateFontIndirect( &lfGrpListFont );
   hUserListFont = CreateFontIndirect( &lfUserListFont );
   hCIXListFont = CreateFontIndirect( &lfCIXListFont );
   hStatusFont = CreateFontIndirect( &lfStatusFont );
   hFixedFont = CreateFontIndirect( &lfFixedFont );
   hPrinterFont = CreateFontIndirect( &lfPrinterFont );

   /* Create a boldface variant of the Thread font.
    */
   lfThreadBFont = lfThreadFont;
   lfThreadBFont.lfWeight = FW_BOLD;
   hThreadBFont = CreateFontIndirect( &lfThreadBFont );

   /* Create a boldface variant of the In Basket font.
    */
   lfInBasketBFont = lfInBasketFont;
   lfInBasketBFont.lfWeight = FW_BOLD;
   hInBasketBFont = CreateFontIndirect( &lfInBasketBFont );

   /* All done.
    */
   fFontsCreated = TRUE;
}

/* This function creates the default Ameol system (non-editable)
 * fonts.
 */
void FASTCALL CreateAmeolSystemFonts( void )
{
   int cPixelsPerInch;
   int wWeight;
   LOGFONT lfSysFont;
   HDC hdc;

   /* Get the font metrics, depending on the display size.
    */
   hdc = GetDC( NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   GetObject( GetStockFont( ANSI_VAR_FONT ), sizeof( LOGFONT ), &lfSysFont );
   ReleaseDC( NULL, hdc );

   /* Create normal Helv 8-point and 10-point fonts.
    */
   lfSysFont.lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = FW_NORMAL;
   hHelv8Font = CreateFontIndirect( &lfSysFont );
   
   lfSysFont.lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = FW_NORMAL;
   hHelv10Font = CreateFontIndirect( &lfSysFont );
   
   /* Create bold-face Helv 8-point and 10-point fonts if
    * we're running Windows 3.1, or non-bold if we're running
    * Windows 95 or Windows NT.
    */
#ifdef WIN32
   wWeight = FW_NORMAL;
#else
   wWeight = ( wWinVer < 0x35F ) ? FW_BOLD : FW_NORMAL;
#endif

   lfSysFont.lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = wWeight;
   hHelvB8Font = CreateFontIndirect( &lfSysFont );

   lfSysFont.lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = wWeight;
   hHelvB10Font = CreateFontIndirect( &lfSysFont );

   /* Create 8 and 10 point copies of the system font.
    */
   lfSysFont.lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = FW_BOLD;
   hSys8Font = CreateFontIndirect( &lfSysFont );

   lfSysFont.lfHeight = -MulDiv( 10, cPixelsPerInch, 72 );
   lfSysFont.lfWeight = FW_BOLD;
   hSys10Font = CreateFontIndirect( &lfSysFont );
}

/* This function reads font configuration data.
 */
void FASTCALL ReadFontData( char * pszSectionName, LOGFONT FAR * lplf )
{
   char sz[ 80 ];

   if( Amuser_GetPPString( szFonts, pszSectionName, "", sz, sizeof( sz ) ) )
      {
      char * psz;

      psz = sz;
      lplf->lfHeight = ReadInteger( &psz );
      lplf->lfWidth = ReadInteger( &psz );
      lplf->lfEscapement = ReadInteger( &psz );
      lplf->lfOrientation = ReadInteger( &psz );
      lplf->lfWeight = ReadInteger( &psz );
      lplf->lfItalic = ReadInteger( &psz );
      lplf->lfUnderline = ReadInteger( &psz );
      lplf->lfStrikeOut = ReadInteger( &psz );
      lplf->lfCharSet = ReadInteger( &psz );
      lplf->lfOutPrecision = ReadInteger( &psz );
      lplf->lfClipPrecision = ReadInteger( &psz );
      lplf->lfQuality = ReadInteger( &psz );
      lplf->lfPitchAndFamily = ReadInteger( &psz );
      ReadString( &psz, lplf->lfFaceName, LF_FACESIZE-1 );
      }
}

/* This function writes font configuration data.
 */
void FASTCALL WriteFontData( char * pszSectionName, LOGFONT FAR * lplf )
{
   char sz[ 80 ];

   wsprintf( sz, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,\"%s\"",
             lplf->lfHeight, lplf->lfWidth, lplf->lfEscapement, lplf->lfOrientation, lplf->lfWeight,
             lplf->lfItalic, lplf->lfUnderline, lplf->lfStrikeOut, lplf->lfCharSet, lplf->lfOutPrecision,
             lplf->lfClipPrecision, lplf->lfQuality, lplf->lfPitchAndFamily, lplf->lfFaceName );
   Amuser_WritePPString( szFonts, pszSectionName, sz );
}

/* This function reads an integer from the string.
 */
int FASTCALL ReadInteger( char ** ppszStr )
{
   char * pszStr;
   int nValue;

   pszStr = *ppszStr;
   nValue = 0;
   while( isdigit( *pszStr ) )
      nValue = nValue * 10 + ( *pszStr++ - '0' );
   if( *pszStr == ',' )
      ++pszStr;
   *ppszStr = pszStr;
   return( nValue );
}

/* This function reads an integer from the string.
 */
int FASTCALL ReadString( char ** ppszStr, char * pBuf, int cbMax )
{
   char * pszStr;
   BOOL fQuote;
   int c;

   pszStr = *ppszStr;
   fQuote = FALSE;
   c = 0;
   while( *pszStr )
      {
      if( ( *pszStr == ',' || isspace( *pszStr ) ) && !fQuote )
         break;
      else if( *pszStr == '\"' )
         fQuote = !fQuote;
      else if( c < cbMax )
         pBuf[ c++ ] = *pszStr;
      ++pszStr;
      }
   pBuf[ c ] = '\0';
   if( *pszStr == ',' )
      ++pszStr;
   *ppszStr = pszStr;
   return( c );
}
