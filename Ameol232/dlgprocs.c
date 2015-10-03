/* DLGPROCS.C - A miscellaneous set of functions
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>
#include <stdio.h>
#include "amlib.h"
#include <ctype.h>
#include <sys\types.h>
#include <sys\stat.h>
#include "parse.h"

#define  THIS_FILE   __FILE__

void FASTCALL BroadcastMessage( HWND, UINT, WPARAM, LPARAM );
void FASTCALL AdjustNameToFitMiddle( HDC, LPSTR, int, int, SIZE * );
void FASTCALL AdjustNameToFitRight( HDC, LPSTR, int, int, SIZE * );

char FAR szExpandedQuote[ 15 ];
char FAR szExpandedQuoteMailHeader[ 15 ];
char FAR szExpandedQuoteNewsHeader[ 15 ];

/* This function gets an integer value from the dialog box edit control,
 * checks that the value is a valid numerical value, then returns it. If
 * the value is not a valid integer - it throws up an error box and leaves
 * the offending edit field highlighted.
 */
BOOL FASTCALL GetDlgInt( HWND hwnd, WORD wID, LPINT lpiValue, int iMin, int iMax )
{
   BOOL fTranslate;
   int iValue;

   iValue = GetDlgItemInt( hwnd, wID, &fTranslate, FALSE );
   if( !fTranslate || ( fTranslate && ( iValue < iMin || iValue > iMax ) ) )
      {
      if( !fTranslate )
         fMessageBox( hwnd, 0, GS(IDS_STR1130), MB_OK|MB_ICONEXCLAMATION );
      else
         {
         wsprintf( lpTmpBuf, GS(IDS_STR190), iValue, iMin, iMax );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      HighlightField( hwnd, wID );
      return( FALSE );
      }
   *lpiValue = iValue;
   return( TRUE );
}

/* This function writes a floating-point value to a dialog control.
 */
void FASTCALL SetDlgItemFloat( HWND hwnd, int wID, float value )
{
   char sz[ 20 ];

   sprintf( sz, "%2.2f", value );
   SetDlgItemText( hwnd, wID, sz );
}

/* This function reads a floating-point value from a dialog control.
 */
float FASTCALL GetDlgItemFloat( HWND hwnd, int wID )
{
   char sz[ 20 ];
   float value;

   GetDlgItemText( hwnd, wID, sz, 20 );
   sscanf( sz, "%f", &value );
   return( value );
}

/* This function gets an integer value from the dialog box edit control,
 * checks that the value is a valid numerical value, then returns it. If
 * the value is not a valid integer - it throws up an error box and leaves
 * the offending edit field highlighted.
 *
 * PW 08/08/2002: Amended to validate against supplied min/max values.
 *
 */
BOOL FASTCALL GetDlgLongInt( HWND hwnd, WORD wID, LPLONG lplValue, LONG lMin, LONG lMax )
{
   BOOL fTranslate;
   LONG lValue;

   lValue = GetDlgItemLongInt( hwnd, wID, &fTranslate, FALSE );
   if( !fTranslate || ( fTranslate && ( lValue < lMin || lValue > lMax ) ) )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR497), MB_OK|MB_ICONEXCLAMATION );
      HighlightField( hwnd, wID );
      return( FALSE );
      }
   *lplValue = lValue;
   return( TRUE );
}

void FASTCALL SetDlgItemLongInt( HWND hwnd, int wID, DWORD value )
{
   char sz[ 20 ];

   wsprintf( sz, "%lu", value );
   SetDlgItemText( hwnd, wID, sz );
}

LONG FASTCALL GetDlgItemLongInt( HWND hwnd, int wID, BOOL * pfTranslate, BOOL fSigned )
{
   char sz[ 20 ];
   LONG lInt;
   char * psz;

   GetDlgItemText( hwnd, wID, sz, 20 );
   psz = sz;
   *pfTranslate = FALSE;
   while( *psz == ' ' )
      ++psz;
   if( *psz == '-' && fSigned )
      ++psz;
   lInt = 0L;
   while( isdigit( *psz ) )
      {
      lInt = lInt * 10 + ( *psz++ - '0' );
      *pfTranslate = TRUE;
      }
   while( *psz == ' ' )
      ++psz;
   if( *psz != '\0' )
      *pfTranslate = FALSE;
   return( lInt );
}

BOOL FASTCALL fCancelToClose( HWND hwnd, int nID )
{
   HWND hButton;

   hButton = GetDlgItem( hwnd, nID );
   SetWindowText( hButton, "&Close" );
   return( TRUE );
}

void FASTCALL SendAllWindows( UINT msg, WPARAM wParam, LPARAM lParam )
{
   BroadcastMessage( hwndMDIClient, msg, wParam, lParam );
}

/* This function sends a message to all child windows of the specified
 * parent window.
 */
void FASTCALL BroadcastMessage( HWND hwndParent, UINT msg, WPARAM wParam, LPARAM lParam )
{
   HWND hwndCheck;

   hwndCheck = GetWindow( hwndParent, GW_CHILD );
   while( hwndCheck )
      {
      HWND hwndNext;

      /* Get the handle of the next window in case hwndCheck goes invalid
       * during the SendMessage (because the window is destroyed)
       */
      hwndNext = GetWindow( hwndCheck, GW_HWNDNEXT );
      if( !GetWindow( hwndCheck, GW_OWNER ) )
         SendMessage( hwndCheck, msg, wParam, lParam );
      hwndCheck = hwndNext;
      }
}

void FASTCALL AmMessageBeep( void )
{
   char sz[ 6 ];

   if( GetProfileString( "windows", "beep", "Yes", sz, sizeof( sz ) ) )
      if( lstrcmpi( sz, "no" ) == 0 )
         {
         register int c;

         for( c = 0; c < 6; ++c )
            {
            FlashWindow( hwndFrame, TRUE );
            GoToSleep( 150 );
            }
         FlashWindow( hwndFrame, FALSE );
         return;
         }
   MessageBeep( MB_ICONEXCLAMATION );
}

void FASTCALL HighlightField( HWND hwnd, int id )
{
   HWND hwndField;

   VERIFY( hwndField = GetDlgItem( hwnd, id ) );
   ASSERT( IsWindow( hwndField ) );
   SetFocus( hwndField );
   Edit_SetSel( hwndField, 0, 32767 );
}

int FASTCALL ComboBox_RealFindStringExact( HWND hwndCmb, LPCSTR lpsz )
{
   register int r = -1;
   char sz[ 100 ];

   while( ( r = ComboBox_FindStringExact( hwndCmb, r, lpsz ) ) != CB_ERR )
      {
      ComboBox_GetLBText( hwndCmb, r, sz );
      if( lstrcmp( sz, lpsz ) == 0 )
         break;
      }
   return( r );
}

/* This function parses a time in an input-field.
 */
BOOL FASTCALL BreakTime( HWND hwnd, int id, AM_TIME * ptm )
{
   BOOL fError = FALSE;
   HWND hwndEdit;
   char szTmp[ 20 ];
   char * pszTmp;

   /* First get the full input.
    */
   hwndEdit = GetDlgItem( hwnd, id );
   Edit_GetText( hwndEdit, szTmp, sizeof(szTmp) );
   pszTmp = szTmp;
   while( *pszTmp == ' ' )
      ++pszTmp;
   if( *pszTmp == '\0' )
      return( TRUE );
   ptm->iHour = 0;
   ptm->iMinute = 0;
   ptm->iSecond = 0;

   /* Extract the hour field.
    */
   if( !isdigit( *pszTmp ) )
      fError = TRUE;
   else
      {
      while( *pszTmp && isdigit( *pszTmp ) )
         ptm->iHour = ( ptm->iHour * 10 ) + ( *pszTmp++ - '0' );
      while( *pszTmp == ' ' )
         ++pszTmp;
      if( *pszTmp && *pszTmp != ':' )
         fError = TRUE;
      else
         {
         ++pszTmp;
         while( *pszTmp == ' ' )
            ++pszTmp;
         if( !isdigit( *pszTmp ) )
            fError = TRUE;
         else
            {
            /* Next, extract the minutes field.
             */
            while( *pszTmp && !isdigit( *pszTmp ) )
               ++pszTmp;
            while( *pszTmp && isdigit( *pszTmp ) )
               ptm->iMinute = ( ptm->iMinute * 10 ) + ( *pszTmp++ - '0' );
            if( *pszTmp )
               fError = TRUE;
            else
               {
               /* Now validate both fields.
                */
               if( ptm->iHour > 23 )
                  fError = TRUE;
               if( ptm->iMinute > 59 )
                  fError = TRUE;
               }
            }
         }
      }
   if( fError )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR94), MB_OK|MB_ICONEXCLAMATION );
      HighlightField( hwnd, id );
      }
   return( !fError );
}

/* This function parses a date in an input-field.
 */
BOOL FASTCALL BreakDate( HWND hwnd, int id, AM_DATE * pdt )
{
   BOOL fError = FALSE;
   char szTmp[ 21 ];
   HWND hwndEdit;
   char * pszTmp;
   char chDateSep[ 2 ];
   int l;

   hwndEdit = GetDlgItem( hwnd, id );
   Edit_GetText( hwndEdit, szTmp, 21 );
   pszTmp = szTmp;
   while( *pszTmp == ' ' )
      ++pszTmp;
   if( *pszTmp == '\0' )
      return( TRUE );
   GetProfileString( "intl", "sDate", "/", chDateSep, sizeof( chDateSep ) );
   if( !isdigit( *pszTmp ) )
      {
      /* Intelligent parser tries to handle such things as 'today', 'last month'
       * and 'last 5 days'.
       */
      if( _strnicmp( pszTmp, "last ", 5 ) == 0 || _strnicmp( pszTmp, "previous ", 9 ) == 0 )
         {
         int nCount;

         pszTmp += 5;
         Amdate_GetCurrentDate( pdt );
         nCount = 0;
         if( isdigit( *pszTmp ) )
            {
            /* We're dealing with last x days|weeks|months|years...
             */
            while( isdigit( *pszTmp ) )
               nCount = nCount * 10 + ( *pszTmp++ - '0' );
            while( *pszTmp == ' ' )
               ++pszTmp;
            }
         else
            nCount = 1;
         if( _strnicmp( pszTmp, "day", 3 ) == 0 )
            Amdate_AdjustDate( pdt, -nCount );
         else if( _strnicmp( pszTmp, "week", 4 ) == 0 )
            Amdate_AdjustDate( pdt, -7 * nCount );
         else if( _strnicmp( pszTmp, "month", 5 ) == 0 )
            {
            while( nCount-- )
               if( ( pdt->iMonth -= 1 ) < 1 )
                  {
                  pdt->iMonth = 12;
                  --pdt->iYear;
                  }
            }
         else if( _strnicmp( pszTmp, "year", 4 ) == 0 )
            pdt->iYear -= nCount;
         else
            fError = TRUE;
         }
      else if( _strnicmp( pszTmp, "yesterday", 9 ) == 0 )
         {
         Amdate_GetCurrentDate( pdt );
         Amdate_AdjustDate( pdt, -1 );
         }
      else if( _strnicmp( pszTmp, "today", 5 ) == 0 )
         Amdate_GetCurrentDate( pdt );
      else
         fError = TRUE;
      }
   else 
   {
      pdt->iDay = *pszTmp++ - '0';
      if( *pszTmp != chDateSep[0] ) 
      {
         if( !isdigit( *pszTmp ) )
            fError = TRUE;
         else 
         {
            pdt->iDay = ( pdt->iDay * 10 ) + *pszTmp++ - '0';
            if( *pszTmp != chDateSep[0] )
               fError = TRUE;
         }
      }
      if( !fError ) 
      {
         ++pszTmp;
         if( !isdigit( *pszTmp ) )
            fError = TRUE;
         else 
         {
            pdt->iMonth = *pszTmp++ - '0';
            if( *pszTmp != chDateSep[0] ) 
            {
               if( !isdigit( *pszTmp ) )
                  fError = TRUE;
               else 
               {
                  pdt->iMonth = ( pdt->iMonth * 10 ) + *pszTmp++ - '0';
                  if( *pszTmp != chDateSep[0] )
                     fError = TRUE;
               }
            }
            if( !fError ) 
            {
               ++pszTmp;
               if( !isdigit( *pszTmp ) )
                  fError = TRUE;
               else 
               {
                  l = strlen(pszTmp);
                  
                  if(l > 2)
                  {

                     if(l == 4)
                     {
                        pdt->iYear = (*pszTmp++ - '0') * 1000;
                        pdt->iYear += (*pszTmp++ - '0') * 100;
                        pdt->iYear += (*pszTmp++ - '0') * 10;
                        pdt->iYear += (*pszTmp++ - '0') ;
                     }
                     else
                        fError = TRUE;
                  }
                  else
                  {
                     pdt->iYear = *pszTmp++ - '0';
                     if( *pszTmp ) 
                     {
                        if( !isdigit( *pszTmp ) )
                           fError = TRUE;
                        else
                           pdt->iYear = ( pdt->iYear * 10 ) + *pszTmp++ - '0';
                     }
                     pdt->iYear = FixupYear( pdt->iYear );
                     if( !fError )
                        if( *pszTmp )
                           fError = TRUE;
                  }
               }
            }
         }
      }
   }
   if( fError )
      {
      fMessageBox( hwnd, 0, GS( IDS_STR93 ), MB_OK|MB_ICONEXCLAMATION );
      HighlightField( hwnd, id );
      }
   return( !fError );
}

LPSTR FASTCALL ExpandUnixTextToPCFormat( LPSTR lpszText )
{
   LPSTR lpszText2;
   int c;
   int d;

   /* First scan the text, counting the number of LFs not followed by a CR
    */
   INITIALISE_PTR(lpszText2);
   for( c = d = 0; lpszText[ c ]; ++c )
      if( c != 0 && lpszText[ c ] == 10 && lpszText[ c - 1 ] != 13 )
         ++d;
   if( d ) {
      d += c;
      if( fNewMemory32( &lpszText2, d + 1 ) )
         {
         for( d = c = 0; lpszText[ c ]; ++c )
            {
            if( c != 0 && lpszText[ c ] == 10 && lpszText[ c - 1 ] != 13 )
               lpszText2[ d++ ] = 13;
            lpszText2[ d++ ] = lpszText[ c ];
            }
         lpszText2[ d ] = '\0';
         FreeMemory32( &lpszText );
         lpszText = lpszText2;
         }
      }
   return( lpszText );
}

/* This is a version of CB_FINDSTRING that only compares the actual
 * data items even if the combobox has the CBS_HASSTRINGS flag.
 */
int FASTCALL RealComboBox_FindItemData( HWND hwndCombo, int index, LPARAM lParam )
{
   int count;

   count = ComboBox_GetCount( hwndCombo );
   while( ++index < count )
      if( ComboBox_GetItemData( hwndCombo, index ) == lParam )
         return( index );
   return( CB_ERR );
}

/* This is a version of LB_FINDSTRING that only compares the actual
 * data items even if the listbox has the LBS_HASSTRINGS flag.
 */
int FASTCALL RealListBox_FindItemData( HWND hwndList, int index, LPARAM lParam )
{
   int count;

   count = ListBox_GetCount( hwndList );
   while( ++index < count )
      if( ListBox_GetItemData( hwndList, index ) == lParam )
         return( index );
   return( LB_ERR );
}

/* This function sets a static text field to the specified path. If the
 * path is longer than would fit, the path is reduced first.
 */
void FASTCALL SetDlgItemPath( HWND hwnd, int id, char * pszPath )
{
   char szPath[ _MAX_PATH ];
   SIZE size;
   RECT rc;
   HDC hdc;

   strcpy( szPath, pszPath );
   hdc = GetDC( hwnd );
   GetClientRect( GetDlgItem( hwnd, id ), &rc );
   AdjustNameToFit( hdc, szPath, ( rc.right - rc.left ), TRUE, &size );
   ReleaseDC( hwnd, hdc );
   SetDlgItemText( hwnd, id, szPath );
}

/* This function adjusts the name in the specified buffer to fit
 * the specified pixel width.
 */
void FASTCALL AdjustNameToFit( HDC hdc, LPSTR lpszText, int cbMax, BOOL fMiddle, SIZE * pSize )
{
   int length;

   /* Compute the current length. If it all fits,
    * quit now.
    */
   length = strlen( lpszText );
   GetTextExtentPoint( hdc, lpszText, length, pSize );
   if( pSize->cx > cbMax )
      {
      /* Otherwise call the appropriate function to
       * adjust the text. For performance reasons, I've
       * used separate functions rather than try to make
       * a general purpose one.
       */
      if( fMiddle )
         AdjustNameToFitMiddle( hdc, lpszText, length, cbMax, pSize );
      else
         AdjustNameToFitRight( hdc, lpszText, length, cbMax, pSize );
      }
}

/* Adjust the specified text to fit cbMax pixels by scooping
 * out from the middle.
 */
void FASTCALL AdjustNameToFitMiddle( HDC hdc, LPSTR lpszText, int length, int cbMax, SIZE * pSize )
{
   BOOL fTakeFromLeft;
   int middle;
   int left;
   int right;

   /* Jump to the middle of the name and replace the
    * middle 3 letter with '...'. Then start cutting
    * out text on the left and right until the text
    * fits.
    */
   left = ( length / 2 ) - 2;
   middle = left + 1;
   right = left + 2;
   lpszText[ left ] = '.';
   lpszText[ middle ] = '.';
   lpszText[ right ] = '.';
   GetTextExtentPoint( hdc, lpszText, length, pSize );
   fTakeFromLeft = TRUE;
   while( pSize->cx > cbMax && length > 5 )
      {
      if( fTakeFromLeft )
         {
         if( middle > 1 )
            {
            memcpy( &lpszText[ middle - 2 ], &lpszText[ middle - 1 ], ( length - middle ) + 2 );
            --length;
            --middle;
            }
         }
      else
         {
         if( middle < length - 1 )
            {
            memcpy( &lpszText[ middle + 2 ], &lpszText[ middle + 3 ], ( length - middle ) - 1 );
            --length;
            }
         }
      GetTextExtentPoint( hdc, lpszText, length, pSize );
      fTakeFromLeft = !fTakeFromLeft;
      }
}

/* Adjust the specified text to fit cbMax pixels by scooping
 * out from the right.
 */
void FASTCALL AdjustNameToFitRight( HDC hdc, LPSTR lpszText, int length, int cbMax, SIZE * pSize )
{
   /* Before we get here, we've determined that the string is too
    * long, so append ellipsis to the end and truncate until it
    * fits.
    */
   if( length > 3 )
      {
      lpszText[ length - 1 ] = '.';
      lpszText[ length - 2 ] = '.';
      lpszText[ length - 3 ] = '.';
      GetTextExtentPoint( hdc, lpszText, length, pSize );
      while( pSize->cx > cbMax && length > 5 )
         {
         int index;

         index = length - 3;
         memcpy( &lpszText[ index - 1 ], &lpszText[ index ], 4 );
         GetTextExtentPoint( hdc, lpszText, --length, pSize );
         }
      }
}

/* This function changes the default push button on a dialog
 * without changing the focus.
 */
void FASTCALL ChangeDefaultButton( HWND hwnd, int id )
{
   DWORD dwDefId;

   /* Get rid of current default push button.
    */
   dwDefId = SendMessage( hwnd, DM_GETDEFID, 0, 0L );
   if( HIWORD( dwDefId ) == DC_HASDEFID )
      SendDlgItemMessage( hwnd, (int)LOWORD( dwDefId ), BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, (LPARAM)TRUE );

   /* Set new default push button.
    */
   SendMessage( hwnd, DM_SETDEFID, id, 0L);
   SendDlgItemMessage( hwnd, id, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, (LPARAM)TRUE );
}

/* This function returns the background and text colours for
 * painting the selected list item.
 */
void FASTCALL GetOwnerDrawListItemColours( const DRAWITEMSTRUCT * lpDrawItem, COLORREF * pText, COLORREF * pBack )
{
   if( lpDrawItem->itemState & ODS_SELECTED )
      {
      if( GetFocus() == lpDrawItem->hwndItem )
         {
         *pText = GetSysColor( COLOR_HIGHLIGHTTEXT );
         *pBack = GetSysColor( COLOR_HIGHLIGHT );
         }
      else
         {
         *pText = GetSysColor( COLOR_WINDOWTEXT );
         *pBack = GetSysColor( COLOR_BTNFACE );
         }
      }
   else
      {
      *pText = GetSysColor( COLOR_WINDOWTEXT );
      *pBack = GetSysColor( COLOR_WINDOW );
      }
}

/* This function pauses for the specified number of
 * milliseconds.
 */
void FASTCALL GoToSleep( DWORD dwMilliseconds )
{
#ifdef WIN32
   Sleep( dwMilliseconds );
#else
   DWORD dwDelay;

   dwDelay = GetTickCount();
   while( ( GetTickCount() - dwDelay ) < dwMilliseconds );
#endif
}

/* This function writes an integer key setting to the specified
 * configuration file.
 */
BOOL FASTCALL WritePrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nValue, LPCSTR lpszIniFile )
{
   static char sBuf[ 7 ];

   _itoa( nValue, sBuf, 10 );
   return( WritePrivateProfileString( lpSection, lpKey, sBuf, lpszIniFile ) );
}

/* This function creates the standard Usenet quote and adds it to the
 * new follow-up message object.
 */
void FASTCALL CreateFollowUpQuote( PTH pth, RECPTR * rcpRecText, char * lpTemplate )
{
   HPSTR hpStr;

   /* First get the quoted message. We need to see how long it
    * is, y'see.
    */
   hpStr = CreateQuotedMessage( hwndQuote, TRUE );
   if( NULL != hpStr )
      {
      HPSTR hpText;
      DWORD cbSize;

      /* Allocate some memory into which to create
       * the quote.
       */
      INITIALISE_PTR(hpText);
      cbSize = 600 + strlen(hpStr);
      if( fNewMemory32( &hpText, cbSize ) )
         {
         LPSTR lpPtr;
         LPSTR lpTemplateSaved;
         UINT cbSpace;
         int count;

         /* Zero the memory! */
         memset( hpText, 0, cbSize );
         
         /* Scan the template and copy to lpText, filling out
          * markers such as %m, etc
          */
         lpPtr = hpText;
         cbSpace = 594;
         lpTemplateSaved = lpTemplate;

         while( *lpTemplate && cbSpace > 1 )
            if( *lpTemplate != '%' )
               *lpPtr++ = *lpTemplate++;
            else {
               ++lpTemplate;
               switch( *lpTemplate++ )
                  {
                  case 'd':
                     Amdb_GetMailHeaderField( pth, "Date", lpPtr, cbSpace, FALSE );
                     break;

                  case 'm':
                     Amdb_GetMailHeaderField( pth, "Message-Id", lpPtr, cbSpace, FALSE );
                     break;

                  case 'a':
                     Amdb_GetMailHeaderField( pth, "From", lpTmpBuf, LEN_TEMPBUF, FALSE );
                     ParseFromField( lpTmpBuf, lpPtr, NULL );
                     break;

                  case 'f':
                     Amdb_GetMailHeaderField( pth, "From", lpTmpBuf, LEN_TEMPBUF, FALSE );
                     ParseFromField( lpTmpBuf, NULL, lpPtr );
                     break;

                  case '%':
                     strcpy( lpPtr, "%" );
                     break;
                  }
               cbSpace -= strlen( lpPtr );
               lpPtr += strlen( lpPtr );
               }
         *lpPtr++ = '\r';
         *lpPtr++ = '\n';
         *lpPtr++ = '\r';
         *lpPtr++ = '\n';
         *lpPtr = '\0';
         for( count = 0; count < 15 && hpText[ count ] ; count++ )
            szExpandedQuote[ count ] = hpText[ count ];
         szExpandedQuote[ count ] = '\0';
         if( strcmp( lpTemplateSaved, szQuoteMailHeader ) == 0 )
            Amuser_WritePPString( szSettings, "ExpandedQuoteMailHeader", szExpandedQuote );
         else
            Amuser_WritePPString( szSettings, "ExpandedQuoteNewsHeader", szExpandedQuote );
         hstrcat( lpPtr, hpStr );
         Amob_CreateRefString( rcpRecText, hpText );
         FreeMemory32( &hpText );
         }
      FreeMemory32( &hpStr );
      }
}
