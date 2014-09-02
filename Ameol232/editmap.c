/* EDITMAP.C - Edit controls mapping layer
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
#include <windows.h>
#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "editmap.h"
#include <string.h>
#include "amlib.h"
#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

#ifndef USEBIGEDIT

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

HGLOBAL hgTable = NULL;          /* Memory block listing acronyms */

#define  IDT_CLICKTIMER          42

// Maximum size of an acronym expansion string
#define  MAX_ACRONYM             512

static const int SCE_STYLE_ORANGE = 11;
static const int SCE_STYLE_PURPLE = 12;
static const int SCE_STYLE_BLUE   = 13;
static const int SCE_STYLE_BLACK  = 14;

static const COLORREF white   = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF blue    = RGB(0, 0, 0xFF);
static const COLORREF purple  = RGB(0xFF, 0, 0xFF);
static const COLORREF orange  = RGB(0xFF, 128, 0);
static const COLORREF black   = RGB(0, 0, 0);

static WNDPROC lpEditCtlProc = NULL;   /* Procedure address of edit window */

BOOL FASTCALL CustomTranslateAccelerator( HWND hwnd, LPMSG lpmsg );

void FASTCALL GetCurrentURL( HWND hwnd, POINT pPT, LPSTR pURL ) // 2.56.2051 FS#96
{
   char lBuf[255];
   char ch;
   int start ;
   int i = 0, sCur;
   
   start= SendMessage(hwnd, SCI_POSITIONFROMPOINT, pPT.x, pPT.y);
   
   sCur = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
   if(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
   {
      sCur = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
      while(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
      {
         sCur = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
         start--;
      }
      
      start++;
      start++;
      
      sCur = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
      while(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
      {
         ch = (char)SendMessage(hwnd, SCI_GETCHARAT, start, 0);
         start++;
         if (ch != '\n' && ch != '\r')
            lBuf[i++] = ch;
         sCur = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
      }
      lBuf[i] = '\x0';
      strcpy(pURL,lBuf);
   }
}

LRESULT FASTCALL Lexer_HotspotClick( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   SCNotificationStruct * notify = (SCNotificationStruct*)lpNmHdr;
   char lBuf[4096];
   char ch;
   int start = notify->position;
   int i = 0, sCur;
   LRESULT l;
   HWND lEdit;
// HWND lMDICli;

   //lEdit = GetDlgItem( hwnd, lpNmHdr->idFrom );
   lEdit = lpNmHdr->hwndFrom;
   sCur = SendMessage(lEdit, SCI_GETSTYLEAT, start, 0);
   if(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
   {

      sCur = SendMessage(lEdit, SCI_GETSTYLEAT, start, 0);
      while(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
      {
         sCur = SendMessage(lEdit, SCI_GETSTYLEAT, start, 0);
         start--;
      }

      start++;
      start++;

      sCur = SendMessage(lEdit, SCI_GETSTYLEAT, start, 0);
      while(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
      {
         ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
         start++;
         if (ch != '\n' && ch != '\r')
            lBuf[i++] = ch;
         sCur = SendMessage(lEdit, SCI_GETSTYLEAT, start, 0);
      }
      lBuf[i] = '\x0';

      start = notify->position;

   // SendMessage( lEdit, SCI_SETMOUSEDOWNCAPTURES, 0, (LPARAM)0 );
   // SendMessage(lEdit, SCI_SETSEL, start, start);
   // SendMessage(hwnd, WM_CANCELMODE, 0, 0);
   // SendMessage(lEdit, SCI_CANCEL, 0, 0);  
   // SendMessage(lEdit, SCI_SETFOCUS, 1, 0);   
   // SendMessage(lEdit, WM_LBUTTONUP, 0, 0);

      l = SendMessage(lEdit, SCI_GETCURRENTPOS,0,0);

      PostMessage(lEdit, SCI_SETFOCUS, 0, 0);   
      
      switch( CommonHotspotHandler( hwnd, lBuf ) ) // !!SM!! 2033
      {
      case AMCHH_NOCHANGE:
         {
            break;
         }
      case AMCHH_MOVEDMESSAGE:
         {
//          PostMessage(lEdit, WM_LBUTTONUP, 0, 0);
//          PostMessage( lEdit, SCI_GOTOPOS, start, start);
//          PostMessage( lEdit, SCI_SETSEL, start, start);
            break;
         }
      case AMCHH_OPENEDMDI:
         {
   /*       char szText[255];
            
            lMDICli = SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
            if (IsWindow(lMDICli))
            {
               SendMessage( lMDICli, WM_GETTEXT, (WPARAM)255, (LPARAM)(char *)&szText );
               fMessageBox( lMDICli, 0, szText, MB_OK|MB_ICONEXCLAMATION );
               SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)lMDICli, 0L );
            }*/
            break;
         }
      case AMCHH_LAUNCHEXTERNAL:
         {
//          PostMessage( lEdit, WM_LBUTTONUP, 0, 0);
//          PostMessage( lEdit, SCI_GOTOPOS, start, start);
//          PostMessage( lEdit, SCI_SETSEL, start, start);
            break;
         }
      }                    // !!SM!! 2032

   // PostMessage( lEdit, SCI_SETMOUSEDOWNCAPTURES, 1, (LPARAM)0 );

   // PostMessage(lEdit, SCI_CANCEL, 0, 0);  
   // SetCapture(hwndFrame);
   // ReleaseCapture();
   // PostMessage( GetParent(lEdit), WM_KILLFOCUS, (WPARAM)(HWND)GetForegroundWindow(),0);
   // ReleaseCapture();
   // SendMessage(lEdit, WM_LBUTTONUP, 0, 0);
   // SendMessage(lEdit, SCI_SETSEL, start, start);

      return 1;
   }
   return 0;

}
// SendMessage(notify->nmhdr.hwndFrom, WM_CANCELMODE, 0, 0);
/*

BOOL FAR PASCAL AmLoadAcronymListOld( void )
{
HNDFILE hfile;                   // The active file handle 
HNDFILE ufile;                   // The active file handle 
DWORD   wSize1;
DWORD   wSize2; //!!SM!! 2.55.2035
char    lPath[_MAX_PATH];

   // Load the acronym list from a file. Rather than waste time building arrays,
   // read the list into a global memory buffer, then compress the buffer into a
   // series of null-terminated words. The last word in the buffer is terminated
   // by two successive NULLs.


   wSize1 = 0;
   wSize2 = 0;
   hfile = HNDFILE_ERROR;
   ufile = HNDFILE_ERROR;

   wsprintf(lPath, "%s%s", pszAmeolDir, "ACRONYMS.LST");

   if ( Amfile_QueryFile( lPath ) == FALSE )
      wsprintf(lPath, "%s\\%s", pszAddonsDir, "ACRONYMS.LST");

   if( Amfile_QueryFile( lPath ) == TRUE && ( hfile = Amfile_Open( lPath, AOF_READ ) ) != HNDFILE_ERROR )
   {
      wSize1 = Amfile_GetFileSize(hfile);
   }

   wsprintf(lPath, "%s%s", pszAmeolDir, "ACRONYMS.USR");

   if ( Amfile_QueryFile( lPath ) == FALSE )
      wsprintf(lPath, "%s\\%s", pszAddonsDir, "ACRONYMS.USR");

   if( Amfile_QueryFile( lPath ) == TRUE && ( ufile = Amfile_Open( lPath, AOF_READ ) ) != HNDFILE_ERROR )
   {                            
      wSize2 = Amfile_GetFileSize(ufile);
   }

   if (wSize1 > 0 || wSize2 > 0)
   {
      // Allocate a buffer for each name.
      
      if( ( hgTable = GlobalAlloc( GHND, wSize1 + wSize2 + 1 ) ) != NULL )
      {
         LPSTR lps;
         LPSTR lpdst;
         DWORD c;
         BOOL lFound = 0; // !!SM!! 2.55.2035 

         lps = GlobalLock( hgTable );

         if (ufile != HNDFILE_ERROR)
         {

            Amfile_SetPosition( ufile, 0L, ASEEK_BEGINNING );

            c = 0;
            if( Amfile_Read32( ufile, (LPSTR)lps, wSize2 ) == wSize2 )
            {
               lFound++;
               lps[ wSize2 ] = '\0';
               lpdst = lps;
               while( *lps && c < wSize2)
               {
                  if (*lps == '[')
                  {
                     while( *lps && *lps != ']' && *lps != '\n' && *lps != '\r')
                        ++lps, ++c;
                     ++lps, ++c;
                  }
                  while( *lps && isspace( *lps ) && c < wSize2)
                     ++lps, ++c;
                  while( !isspace( *lps ) && c < wSize2)
                     *lpdst++ = *lps++, ++c;
                  *lpdst++ = '\0';
                  ++lps;
                  ++c;
                  while( *lps && isspace( *lps ) && c < wSize2)
                     ++lps, ++c;
                  while( *lps && *lps != '\r' && *lps != '\n' && c < wSize2)
                     *lpdst++ = *lps++, ++c;
                  *lpdst++ = '\0';
                  ++lps;
                  ++c;
                  while( *lps && isspace( *lps ) && c < wSize2)
                     ++lps, ++c;
               }
//             *lpdst = '\0';
            }
            Amfile_Close( ufile );
         }

         if (hfile != HNDFILE_ERROR)
         {

   //       wSize = Amfile_SetPosition( hfile, 0L, ASEEK_END );
            Amfile_SetPosition( hfile, 0L, ASEEK_BEGINNING );

            // Allocate a buffer for each name.
            
            c = 0;
   //       lps = GlobalLock( hgTable );
            if( Amfile_Read32( hfile, (LPSTR)lps, wSize1 ) == wSize1 )
            {
               lFound++;
               lps[ wSize1 ] = '\0';
               lpdst = lps;
               while( *lps && c < wSize1)
               {
                  if (*lps == '[')
                  {
                     while( *lps && *lps != ']')
                        ++lps, ++c;
                     ++lps, ++c;
                  }
                  while( *lps && isspace( *lps ) && c < wSize1 && *lps != '\n' && *lps != '\r')
                     ++lps, ++c;
                  while( !isspace( *lps ) && c < wSize1)
                     *lpdst++ = *lps++, ++c;
                  *lpdst++ = '\0';
                  ++lps;
                  ++c;
                  while( *lps && isspace( *lps ) && c < wSize1)
                     ++lps, ++c;
                  while( *lps && *lps != '\r' && *lps != '\n' && c < wSize1)
                     *lpdst++ = *lps++, ++c;
                  *lpdst++ = '\0';
                  ++lps;
                  ++c;
                  while( *lps && isspace( *lps ) && c < wSize1)
                     ++lps, ++c;
               }
//             *lpdst = '\0';
            }
            Amfile_Close( hfile );
         }
         if(lFound > 0)
            *lpdst = '\0';

         GlobalUnlock( hgTable );
         return( TRUE );
      }
      else
         return( FALSE );
   }
   else
      return( FALSE );
}
*/

DWORD FAR PASCAL AmParseAcronymList(LPSTR pIn, LPSTR pOut, DWORD pSize)
{
DWORD c, d;

    c = 0;
   d = 0;
   while( *pIn && c < pSize)
   {
      
//    if ( AmReadAcronymLine(pIn, pOut, pSize, &c) )

      if (*pIn == '[')
      {
         ++pIn, ++c;
         while( *pIn && *pIn != ']' && *pIn != '\n' && *pIn != '\r')
            ++pIn, ++c;
         ++pIn, ++c;
         while( *pIn && isspace( (unsigned char)*pIn ) && c < pSize)
            ++pIn, ++c;
      }
      if (*pIn == 'Ý')
      {
         ++pIn, ++c;
         while( *pIn && *pIn != 'Ý' )
            ++pIn, ++c;
         ++pIn, ++c;
         while( *pIn && isspace( (unsigned char)*pIn ) && c < pSize)
            ++pIn, ++c;
      }
      while( *pIn && isspace( (unsigned char)*pIn ) && c < pSize)
         ++pIn, ++c;

      if (*pIn != '[')
      {
         while( !isspace( (unsigned char)*pIn ) && c < pSize)
            *pOut++ = *pIn++, ++c, d++;
         *pOut++ = '\0';
         ++pIn;
         ++c;
         d++;
         while( *pIn && isspace( (unsigned char)*pIn ) && c < pSize)
            ++pIn, ++c;
         while( *pIn && *pIn != '\r' && *pIn != '\n' && c < pSize)
            *pOut++ = *pIn++, ++c, d++;
         *pOut++ = '\0';
         ++pIn;
         ++c;
         d++;
      }
      while( *pIn && isspace( (unsigned char)*pIn ) && c < pSize)
         ++pIn, ++c;
   }
// *pOut = '\0';
// d++;
   return d;
}

BOOL FAR PASCAL AmLoadAcronymList( void )
{
HNDFILE hfile;                   /* The active file handle */
HNDFILE ufile;                   /* The active file handle */ 
DWORD   wSize1;
DWORD   wSize2; //!!SM!! 2.55.2035
DWORD   lStart;
char    lPath[_MAX_PATH];

   // Load the acronym list from a file. Rather than waste time building arrays,
   // read the list into a global memory buffer, then compress the buffer into a
   // series of null-terminated words. The last word in the buffer is terminated
   // by two successive NULLs.


   wSize1 = 0;
   wSize2 = 0;
   hfile = HNDFILE_ERROR;
   ufile = HNDFILE_ERROR;

   wsprintf(lPath, "%s%s", pszAmeolDir, "ACRONYMS.LST"); // VistaAlert

   if ( Amfile_QueryFile( lPath ) == FALSE )
      wsprintf(lPath, "%s\\%s", pszAddonsDir, "ACRONYMS.LST");

   if( Amfile_QueryFile( lPath ) == TRUE && ( hfile = Amfile_Open( lPath, AOF_READ ) ) != HNDFILE_ERROR )
   {
      wSize1 = Amfile_GetFileSize(hfile);
   }

   wsprintf(lPath, "%s%s", pszAmeolDir, "ACRONYMS.USR"); // VistaAlert  plus should be user dir

   if ( Amfile_QueryFile( lPath ) == FALSE )
      wsprintf(lPath, "%s\\%s", pszAddonsDir, "ACRONYMS.USR");

   if( Amfile_QueryFile( lPath ) == TRUE && ( ufile = Amfile_Open( lPath, AOF_READ ) ) != HNDFILE_ERROR )
   {                            
      wSize2 = Amfile_GetFileSize(ufile);
   }

   if (wSize1 > 0 || wSize2 > 0)
   {
      /* Allocate a buffer for each name.
       */
      if( ( hgTable = GlobalAlloc( GHND, wSize1 + wSize2 + 1 ) ) != NULL )
      {
         LPSTR lps;
         LPSTR lpdst;
         
         lStart = 0;
         lps = GlobalLock( hgTable );
         lpdst = lps;
         if (ufile != HNDFILE_ERROR)
         {
            Amfile_SetPosition( ufile, 0L, ASEEK_BEGINNING );
            if( Amfile_Read32( ufile, (LPSTR)lps, wSize2 ) == wSize2 )
            {
               lps[ lStart + wSize2 ] = '\0';
               lStart = AmParseAcronymList(lps + lStart, lpdst + lStart, wSize2);
            }
            Amfile_Close( ufile );
         }

         if (hfile != HNDFILE_ERROR)
         {
            Amfile_SetPosition( hfile, 0L, ASEEK_BEGINNING );
            if( Amfile_Read32( hfile, (LPSTR)lps + lStart, wSize1 ) == wSize1 )
            {
               lps[ lStart + wSize1 ] = '\0';
               lStart = AmParseAcronymList(lps + lStart, lpdst + lStart, wSize1);
            }
            Amfile_Close( hfile );
         }
//       *lpdst = '\0';
         GlobalUnlock( hgTable );
         return( TRUE );
      }
      else
         return( FALSE );
   }
   else
      return( FALSE );
}

BOOL WINAPI LookupAcronym(LPSTR sWord, LPSTR result, int cchMax)
{
LPSTR lps;
DWORD i,d;
   lps = GlobalLock( hgTable );
   if(lps != NULL)
   {
      d = GlobalSize(hgTable);
      i = 0;
      while( /**lps &&*/ (i < d) )
      {
         if( lstrcmp( lps, sWord ) == 0 )
         {
               lps += lstrlen( lps ) + 1;
               strncpy_s(result, cchMax, lps, _TRUNCATE);
               GlobalUnlock( hgTable );
               return TRUE;
         }
         i += lstrlen( lps ) + 1;
         lps += lstrlen( lps ) + 1;
         i += lstrlen( lps ) + 1;
         lps += lstrlen( lps ) + 1;
      }
      GlobalUnlock( hgTable );
   }
   return FALSE;
}

LRESULT FASTCALL Lexer_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
// POINT lp;

// GetCursorPos(&lp);

   switch (lpNmHdr->code)        
   {
   case SCN_STYLENEEDED:
      {
/*       SCNotificationStruct * notify = (SCNotificationStruct*)lpNmHdr;
         const int pos = SendMessage(hwnd, SCI_GETENDSTYLED, 0, 0);
         const int line_number = SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
         const int start_pos = SendMessage(hwnd, SCI_POSITIONFROMLINE, (WPARAM)line_number, 0L);
         const int end_pos = notify->position;
         int line_length = SendMessage(hwnd, SCI_LINELENGTH, (WPARAM)line_number, 0L);
         
//             if (line_length > 0) 
         {
            char first_char = (char)SendMessage(hwnd, SCI_GETCHARAT, (WPARAM)start_pos, 0L);

            // The SCI_STARTSTYLING here is important
            SendMessage(hwnd, SCI_STARTSTYLING, start_pos, 0x1f);

            switch (first_char)
            {
               case '-':
                  SendMessage(hwnd, SCI_SETSTYLING, line_length, SCE_STYLE_ORANGE);
                  break;

               case '/':
                  SendMessage(hwnd, SCI_SETSTYLING, line_length, SCE_STYLE_PURPLE);
                  break;

               case '*':
                  SendMessage(hwnd, SCI_SETSTYLING, line_length, SCE_STYLE_BLUE);
                  break;
              default:
                  SendMessage(hwnd, SCI_SETSTYLING, line_length, SCE_STYLE_BLACK);
                  break;
            }
         }*/
         break;
      }
      
   case SCN_CHARADDED:
      {
         SCNotificationStruct * notify = (SCNotificationStruct*)lpNmHdr;
         HWND hwndEdit = notify->nmhdr.hwndFrom;
         int eolMode = SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
         int curPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
         int curLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, curPos, 0);
         int lastLine = curLine - 1;
         int indentAmount = 0;
         
         if( notify->ch < 32 && notify->ch != 10 && notify->ch != 13 && notify->ch != 10 && notify->ch != 9)
         {
            SendMessage(hwndEdit, SCI_DELETEBACK, 0, 0);
            SendMessage(GetParent(hwndEdit), WM_KEYDOWN, notify->ch, 0);
            return TRUE;
         }

         if (fAutoIndent) 
         {
            if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && notify->ch == '\n') ||  (eolMode == SC_EOL_CR && notify->ch == '\r'))
            {

               while (lastLine >= 0 && SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, lastLine, 0) - 
                                 SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lastLine, 0) == 0)
                  lastLine--;
               if (lastLine >= 0) 
               {
                  indentAmount = SendMessage(hwndEdit, SCI_GETLINEINDENTATION, lastLine, 0);
               }
               if (indentAmount > 0) 
               {
                  int posBefore = SendMessage(hwndEdit, SCI_GETLINEINDENTPOSITION, curLine, 0);
                  int posAfter;
                  int posDifference;
                  CharacterRange crange;

                  crange.cpMax = SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
                  crange.cpMin = SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
                   

                  SendMessage(hwndEdit, SCI_SETLINEINDENTATION, curLine, indentAmount);

                  posAfter = SendMessage(hwndEdit, SCI_GETLINEINDENTPOSITION, curLine, 0);
                  posDifference = posAfter - posBefore;
 
                  if (posAfter > posBefore) 
                  {
                     // Move selection on
                     if (crange.cpMin >= posBefore) 
                     {
                        crange.cpMin += posDifference;
                     }
                     if (crange.cpMax >= posBefore) 
                     {
                        crange.cpMax += posDifference;
                     }
                  } 
                  else if (posAfter < posBefore) 
                  {
                     // Move selection back
                     if (crange.cpMin >= posAfter) 
                     {
                        if (crange.cpMin >= posBefore)
                           crange.cpMin += posDifference;
                        else
                           crange.cpMin = posAfter;
                     }
                     if (crange.cpMax >= posAfter) 
                     {
                        if (crange.cpMax >= posBefore)
                           crange.cpMax += posDifference;
                        else
                           crange.cpMax = posAfter;
                     }
                  }
                  SendMessage(hwndEdit, SCI_SETSEL, crange.cpMin, crange.cpMax);

               }
            }
         }
         break;
      }
   case SCN_SAVEPOINTREACHED:
      {
//             fMessageBox( hwnd, 0, "SCN_SAVEPOINTREACHED", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_SAVEPOINTLEFT:
      {
//             fMessageBox( hwnd, 0, "SCN_SAVEPOINTLEFT", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_MODIFYATTEMPTRO: // R/O attribute changed
      {
//             fMessageBox( hwnd, 0, "SCN_MODIFYATTEMPTRO", MB_OK|MB_ICONEXCLAMATION );
         break;
      } 
   case SCN_KEY: // GTK+
      {
//             fMessageBox( hwnd, 0, "SCN_KEY", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_DOUBLECLICK:
      {
//             fMessageBox( hwnd, 0, "SCN_DOUBLECLICK", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_UPDATEUI: // Either the text or styling of the document has changed or the selection range has changed
      {
//             fMessageBox( hwnd, 0, "SCN_UPDATEUI", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_MODIFIED: // This notification is sent when the text or styling of the document changes or is about to change
      {
//             fMessageBox( hwnd, 0, "SCN_MODIFIED", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_MACRORECORD:
      {
//             fMessageBox( hwnd, 0, "SCN_MACRORECORD", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_MARGINCLICK:
      {
//             fMessageBox( hwnd, 0, "SCN_MARGINCLICK", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_NEEDSHOWN:
      {
//             fMessageBox( hwnd, 0, "SCN_NEEDSHOWN", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_PAINTED:
      {
//             fMessageBox( hwnd, 0, "SCN_PAINTED", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_USERLISTSELECTION:
      {
//             fMessageBox( hwnd, 0, "SCN_USERLISTSELECTION", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_URIDROPPED: // GTK+
      {
//             fMessageBox( hwnd, 0, "SCN_URIDROPPED", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_DWELLSTART:
      {
         SCNotificationStruct * notify = (SCNotificationStruct*)lpNmHdr;
         char lBuf[255];
         char ch;
         int start = notify->position;
         int i = 0;
         HWND lEdit;
         POINT pt;

         //fMessageBox( hwnd, 0, "SCN_DWELLSTART", MB_OK|MB_ICONEXCLAMATION );

         if (start>0 && fShowAcronyms)
         {
            HWND lWnd;

            lEdit = GetDlgItem( hwnd, lpNmHdr->idFrom );

            GetCursorPos(&pt);

            lWnd = WindowFromPoint(pt);

            if( lEdit == lWnd )
            {
               ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
               while( start > 0 && isalnum(ch) )
               {
                  ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
                  start--;
               }
               while ( !isalnum(ch) && ch != '\x0' )
               {
                  start++;
                  ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
               }
               
               ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
               while(isalnum(ch) && ch != '\x0' && i < 254)
               {
                  ch = (char)SendMessage(lEdit, SCI_GETCHARAT, start, 0);
                  start++;
                  lBuf[i++] = ch;
               }
               lBuf[i] = '\x0';
               i--;
               while(!isalnum(lBuf[i]) && i > 0)
               {
                  i--;
               }
               lBuf[i+1] = '\x0';

               if( lBuf[i] != '\x0' && lBuf[0] != '\x0')
               {
                  char * lFound = malloc(MAX_ACRONYM);
                  if(lFound != NULL && LookupAcronym(lBuf, lFound, MAX_ACRONYM) ) 
                  {
                     SendMessage(lEdit, SCI_CALLTIPSHOW, notify->position, (LPARAM)lFound);
                     free(lFound);
                  }
               }
            }
         }
         break;
      }
   case SCN_DWELLEND:
      {
         SCNotificationStruct * notify = (SCNotificationStruct*)lpNmHdr;

         if ( SendMessage(GetDlgItem( hwnd, lpNmHdr->idFrom ), SCI_CALLTIPACTIVE, 0, 0) == 1 )
            SendMessage(GetDlgItem( hwnd, lpNmHdr->idFrom ), SCI_CALLTIPCANCEL, 0, 0);
         break;
      }
   case SCN_ZOOM:
      {
//             fMessageBox( hwnd, 0, "SCN_ZOOM", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCN_HOTSPOTDOUBLECLICK:
      {
//       if (!fSingleClickHotspots)
//          return ( Lexer_HotspotClick( hwnd, idCode, lpNmHdr ) );
         break;
      }
   case SCN_HOTSPOTCLICK:
      {
//       if (fSingleClickHotspots)
//          return ( Lexer_HotspotClick( hwnd, idCode, lpNmHdr ) );
         break;
      }
   case SCN_CALLTIPCLICK:
      {
//             fMessageBox( hwnd, 0, "SCN_CALLTIPCLICK", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCEN_SETFOCUS:
      {
//             fMessageBox( hwnd, 0, "SCN_CALLTIPCLICK", MB_OK|MB_ICONEXCLAMATION );
         break;
      }
   case SCEN_KILLFOCUS:
      {
//       ReleaseCapture();
         break;
      }
   }
   return( FALSE );
}


WORD fLow, fHigh;
BOOL fDblClicked = FALSE;

/* This is the subclassed message pane procedure.
 */
LRESULT EXPORT FAR PASCAL EditWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{

   LONG lEditCtlProc;   /* Procedure address of edit window */
   SCNotificationStruct lpNmHdr;

   switch( msg )
   {
      case WM_TIMER: // 20060307 - 2.56.2049.11
         if (wParam == IDT_CLICKTIMER)
         {
            KillTimer( hwnd, IDT_CLICKTIMER );

            if (fSingleClickHotspots && fHotLinks) // 20060228 - 2.56.2049.01
            {
               lpNmHdr.position = SendMessage(hwnd, SCI_POSITIONFROMPOINTCLOSE, fLow, fHigh);
               if (lpNmHdr.position >= 0)
               {
                  lpNmHdr.x = LOWORD(lParam);
                  lpNmHdr.y = HIWORD(lParam);
                  lpNmHdr.nmhdr.hwndFrom = hwnd;
                  lpNmHdr.nmhdr.idFrom = IDD_MESSAGE;
                  if( Lexer_HotspotClick( GetParent(hwnd), 0, (LPNMHDR )&lpNmHdr ) )
                     return 0;
               }
            }
         }      
         break;

      case WM_LBUTTONDBLCLK:
         {
            KillTimer( hwnd, IDT_CLICKTIMER );      // 20060307 - 2.56.2049.11
            
            fDblClicked = TRUE;                     // 20060307 - 2.56.2049.11 

            if (!fSingleClickHotspots && fHotLinks) // 20060228 - 2.56.2049.01
            {
               lpNmHdr.position = SendMessage(hwnd, SCI_POSITIONFROMPOINTCLOSE, LOWORD(lParam), HIWORD(lParam));
               if (lpNmHdr.position >= 0)
               {
                  lpNmHdr.x = LOWORD(lParam);
                  lpNmHdr.y = HIWORD(lParam);
                  lpNmHdr.nmhdr.hwndFrom = hwnd;
                  lpNmHdr.nmhdr.idFrom = IDD_MESSAGE;
                  if( Lexer_HotspotClick( GetParent(hwnd), 0, (LPNMHDR )&lpNmHdr ) )
                     return 0;
               }
            }
         }
         break;
      // case WM_LBUTTONDOWN:
      case WM_LBUTTONUP: // 20060307 - 2.56.2049.11
         {
            int sCur;

            sCur = SendMessage(hwnd, SCI_GETSTYLEAT, SendMessage(hwnd, SCI_POSITIONFROMPOINT, LOWORD(lParam), HIWORD(lParam)), 0);
            if(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
            {
               if (fSingleClickHotspots && fHotLinks) // 20060307 - 2.56.2049.11
               {
                  // User is holding down Ctrl, so activate immediately // 2.56.2051 FS#135
                  if (( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000)
                  {
                     fDblClicked = FALSE;
                     lpNmHdr.position = SendMessage(hwnd, SCI_POSITIONFROMPOINTCLOSE, LOWORD(lParam), HIWORD(lParam));
                     if (lpNmHdr.position >= 0)
                     {
                        lpNmHdr.x = LOWORD(lParam);
                        lpNmHdr.y = HIWORD(lParam);
                        lpNmHdr.nmhdr.hwndFrom = hwnd;
                        lpNmHdr.nmhdr.idFrom = IDD_MESSAGE;
                        if( Lexer_HotspotClick( GetParent(hwnd), 0, (LPNMHDR )&lpNmHdr ) )
                           return 0;
                     }
                  }
                  else
                  {
                     fLow = LOWORD(lParam);
                     fHigh = HIWORD(lParam);
                     if (!fDblClicked)
                        SetTimer( hwnd, IDT_CLICKTIMER, GetDoubleClickTime(), NULL );
                     fDblClicked = FALSE;
                  }
               }
            }
         }
         break;
      case WM_KEYDOWN:
         {
            MSG lmsg; 
            lmsg.hwnd = hwnd;
            lmsg.lParam = lParam;
            lmsg.wParam = wParam;
            lmsg.message = msg;
            if(!CustomTranslateAccelerator( hwnd, &lmsg ))
               break;
            else
               return 0;

         }

   }

   lEditCtlProc = GetWindowLong(hwnd, GWL_USERDATA);
   if (lEditCtlProc > 0)
      return ( CallWindowProc( (WNDPROC)lEditCtlProc, hwnd, msg, wParam, lParam ) );
   else
      return 0;
}

void FASTCALL SetEditorDefaults(HWND hwndEdit, BOOL readOnly, LOGFONT lfFont, BOOL pWordWrap, BOOL pHotLinks , BOOL pMessageStyles, COLORREF col, COLORREF bgCol )
{

   char lWordChars[] = "_/*abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
   int i;

//    SendMessage( hwndEdit, SCI_SETMOUSEDOWNCAPTURES, 0, (LPARAM)0 );

//    SetWindowLong(hwndEdit, GWL_STYLE, GetWindowLong( hwndEdit, GWL_STYLE ) & ES_WANTRETURN);

      /*    
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
      */

      SendMessage( hwndEdit, SCI_SETCODEPAGE, 0, 0); // 2.56.2053 FS#116
   
      SendMessage( hwndEdit, SCI_SETCARETPERIOD, GetCaretBlinkTime(), 0);  // 200604416 - 2.56.2051 #119

      if (fPageBeyondEnd) // FS#57
         SendMessage( hwndEdit, SCI_SETENDATLASTLINE, 0, 0); // 20060228 - 2.56.2049.02
      else
         SendMessage( hwndEdit, SCI_SETENDATLASTLINE, 1, 0); 

      SendMessage( hwndEdit, SCI_SETVSCROLLBAR, 1, 0); // 20060325 - 2.56.2051.21
      
   
      SendMessage( hwndEdit, SCI_SETUSETABS, 0, (LPARAM)0 );

      SendMessage( hwndEdit, SCI_SETLAYOUTCACHE, SC_CACHE_NONE , (LPARAM)0 );
//    SendMessage( hwndEdit, SCI_SETLAYOUTCACHE, SC_CACHE_CARET, (LPARAM)0 );
//    SendMessage( hwndEdit, SCI_SETLAYOUTCACHE, SC_CACHE_PAGE, (LPARAM)0 );
//    SendMessage( hwndEdit, SCI_SETLAYOUTCACHE, SC_CACHE_DOCUMENT, (LPARAM)0 );
      
//    SendMessage( hwndEdit, SCI_SETTWOPHASEDRAW, !readOnly, 0);
      SendMessage( hwndEdit, SCI_SETTWOPHASEDRAW, 1, 0);
//    SendMessage( hwndEdit, SCI_SETBUFFEREDDRAW, !readOnly, 0);
      SendMessage( hwndEdit, SCI_SETBUFFEREDDRAW, 1, 0);
   
      SendMessage( hwndEdit, SCI_SETREADONLY,  (WPARAM)readOnly, (LPARAM)0L );
//    SendMessage( hwndEdit, SCI_SETEDGECOLUMN,  (WPARAM)211, (LPARAM)0L );
//    SendMessage( hwndEdit, SCI_SETEDGEMODE,  (WPARAM)1, (LPARAM)1L );
//    SendMessage( hwndEdit, SCI_SETEDGECOLOUR,  (WPARAM)1, (LPARAM)1L );

//    SendMessage( hwndEdit, SCI_SETLEXER, SCLEX_CONTAINER, 0L);
      SendMessage( hwndEdit, SCI_SETLEXER, SCLEX_CIX, 0L);

      if(fShowWrapCharacters)
      {
         SendMessage( hwndEdit, SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END, 0L); /*SC_WRAPVISUALFLAG_START*/ 
      }
      else
      {
         SendMessage( hwndEdit, SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_NONE, 0L);
      }
      SendMessage( hwndEdit, SCI_SETWRAPVISUALFLAGSLOCATION, /*SC_WRAPVISUALFLAGLOC_START_BY_TEXT*/SC_WRAPVISUALFLAGLOC_END_BY_TEXT, 0L);


      SendMessage( hwndEdit, SCI_SETWORDCHARS, 0, (LPARAM)(LPSTR)&lWordChars);

      //SendMessage( hwndEdit, SCI_STYLECLEARALL, 0, 0L);
/*
      SendMessage( hwndEdit, SCI_STYLESETFORE, STYLE_DEFAULT, black);
      SendMessage( hwndEdit, SCI_STYLESETBACK, STYLE_DEFAULT, white);

      SendMessage( hwndEdit, SCI_STYLESETFORE, SCE_STYLE_BLACK,  black);
      SendMessage( hwndEdit, SCI_STYLESETFORE, SCE_STYLE_ORANGE, orange);
      SendMessage( hwndEdit, SCI_STYLESETFORE, SCE_STYLE_PURPLE, purple);
      SendMessage( hwndEdit, SCI_STYLESETFORE, SCE_STYLE_BLUE,   blue);

      SendMessage( hwndEdit, SCI_STYLESETFORE, (WPARAM)0, crMsgText);
      SendMessage( hwndEdit, SCI_STYLESETBACK, (WPARAM)0, crMsgWnd);
      SendMessage( hwndEdit, SCI_SETHOTSPOTACTIVEFORE, (WPARAM)0, crHotlinks);
*/    

//SC_CACHE_NONE 0 No lines are cached. 
//SC_CACHE_CARET 1 The line containing the text caret. This is the default. 
//SC_CACHE_PAGE 2 Visible lines plus the line containing the caret. 
//SC_CACHE_DOCUMENT 3 All lines in the document. 

// if (!((fgColour >> 24) & 0xFF))
//  execute(SCI_STYLESETFORE, styleID, fgColour);

// if (!((bgColour >> 24) & 0xFF))
//  execute(SCI_STYLESETBACK, styleID, bgColour);


      for (i=0;i<18;i++)
      {
/*
         SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)i, readOnly ? crMsgText : crEditText);
         SendMessage( hwndEdit, SCI_STYLESETBACK,      (WPARAM)i, readOnly ? crMsgWnd  : crEditWnd);
         SendMessage( hwndEdit, SCI_SETWHITESPACEFORE, (WPARAM)i, readOnly ? crMsgText : crEditText);
         SendMessage( hwndEdit, SCI_SETWHITESPACEBACK, (WPARAM)i, readOnly ? crMsgWnd  : crEditWnd);
         SendMessage( hwndEdit, SCI_STYLESETEOLFILLED, (WPARAM)i, TRUE);
*/
         SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)i, col);
         SendMessage( hwndEdit, SCI_STYLESETBACK,      (WPARAM)i, bgCol);
         SendMessage( hwndEdit, SCI_SETWHITESPACEFORE, (WPARAM)i, col);
         SendMessage( hwndEdit, SCI_SETWHITESPACEBACK, (WPARAM)i, bgCol);
         SendMessage( hwndEdit, SCI_STYLESETEOLFILLED, (WPARAM)i, TRUE);
      }
/*    
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)STYLE_DEFAULT, readOnly ? crMsgText : crEditText);
      SendMessage( hwndEdit, SCI_STYLESETBACK,      (WPARAM)STYLE_DEFAULT, readOnly ? crMsgWnd  : crEditWnd);
      SendMessage( hwndEdit, SCI_SETWHITESPACEFORE, (WPARAM)STYLE_DEFAULT, readOnly ? crMsgText : crEditText);
      SendMessage( hwndEdit, SCI_SETWHITESPACEBACK, (WPARAM)STYLE_DEFAULT, readOnly ? crMsgWnd  : crEditWnd);
      SendMessage( hwndEdit, SCI_STYLESETEOLFILLED, (WPARAM)STYLE_DEFAULT, TRUE);
*/
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)STYLE_DEFAULT, col);
      SendMessage( hwndEdit, SCI_STYLESETBACK,      (WPARAM)STYLE_DEFAULT, bgCol);
      SendMessage( hwndEdit, SCI_SETWHITESPACEFORE, (WPARAM)STYLE_DEFAULT, col);
      SendMessage( hwndEdit, SCI_SETWHITESPACEBACK, (WPARAM)STYLE_DEFAULT, bgCol);
      SendMessage( hwndEdit, SCI_STYLESETEOLFILLED, (WPARAM)STYLE_DEFAULT, TRUE);

      SendMessage( hwndEdit, SCI_SETSELFORE, 1, GetSysColor(COLOR_HIGHLIGHTTEXT)); 
      SendMessage( hwndEdit, SCI_SETSELBACK, 1, GetSysColor(COLOR_HIGHLIGHT));

      if(readOnly)
      {
         SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_INVISIBLE, pMessageStyles ? bgCol : crMsgText ); // !!SM!! 2.55.2037
         SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_INVISIBLE, (LPARAM)(LPSTR)lfFont.lfFaceName );
         SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_INVISIBLE, pMessageStyles ? (LPARAM)0 : (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY )));     
      }

      SendMessage( hwndEdit, SCI_STYLESETBACK,      (WPARAM)SCE_CIX_INVISIBLE, readOnly ? crMsgWnd  : crEditWnd);
/*
      SendMessage( hwndEdit, SCI_SETCARETFORE,      readOnly ? crMsgText : crEditText, 0);
      SendMessage( hwndEdit, SCI_SETCARETLINEBACK,  readOnly ? crMsgWnd  : crEditWnd, 0);
*/
      SendMessage( hwndEdit, SCI_SETCARETFORE,      col, 0);
      SendMessage( hwndEdit, SCI_SETCARETLINEBACK,  bgCol, 0);

      SendMessage( hwndEdit, SCI_SETHOTSPOTACTIVEFORE, (WPARAM)0, crHotlinks);

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_BOLD,      (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_BOLD,      (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_BOLD,      (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_ITALIC,    (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_ITALIC,    (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_ITALIC,    (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_BOLD_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_BOLD_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_BOLD_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_BOLD_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_BOLD_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_BOLD_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_BOLD_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_BOLD_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_ITALIC_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_ITALIC_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_ITALIC_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_ITALIC_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );


      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_BOLD_UNDERLINE_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );
      
      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_QUOTED_BOLD, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_BOLD, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_BOLD, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_BOLD, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_QUOTED_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_UNDERLINE, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_QUOTED_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_ITALIC, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_QUOTED_BOLD_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_QUOTED_BOLD_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_BOLD_ITALIC, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_BOLD_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_BOLD_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_QUOTED_ITALIC_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_QUOTED_ITALIC_UNDERLINE, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_ITALIC_UNDERLINE, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_ITALIC_UNDERLINE, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_ITALIC_UNDERLINE, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );
            
      SendMessage( hwndEdit, SCI_STYLESETBOLD,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC , (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETITALIC,    (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC, (LPARAM)pMessageStyles);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC, pMessageStyles ? crQuotedText : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );
        
      SendMessage( hwndEdit, SCI_STYLESETHOTSPOT,   (WPARAM)SCE_CIX_QUOTED_HOTLINK, (LPARAM)pHotLinks);
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED_HOTLINK, pHotLinks ? pMessageStyles ? crQuotedText : crHotlinks : col);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED_HOTLINK, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED_HOTLINK, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      //    SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)SCE_CIX_QUOTED, (LPARAM)TRUE);
      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_QUOTED, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_QUOTED, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_QUOTED, pMessageStyles ? crQuotedText : col);

      SendMessage( hwndEdit, SCI_STYLESETFONT,      (WPARAM)SCE_CIX_HOTLINK, (LPARAM)(LPSTR)lfFont.lfFaceName );
      SendMessage( hwndEdit, SCI_STYLESETSIZE,      (WPARAM)SCE_CIX_HOTLINK, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );
      SendMessage( hwndEdit, SCI_STYLESETFORE,      (WPARAM)SCE_CIX_HOTLINK, pHotLinks ? crHotlinks : col);
      SendMessage( hwndEdit, SCI_STYLESETHOTSPOT,   (WPARAM)SCE_CIX_HOTLINK, pHotLinks);


      SendMessage( hwndEdit, SCI_SETPROPERTY, (WPARAM)"MultiLineURLs", (LPARAM)(fMultiLineURLs ? "1":"0"));

      SendMessage( hwndEdit, SCI_SETHOTSPOTSINGLELINE,  (WPARAM)0L, (LPARAM)0L );
      // SendMessage( hwndEdit, SCI_SETVIEWWS, SCWS_VISIBLEALWAYS, (LPARAM)0L ); // Show special characters (Space, Tab)

      if (fShowLineNumbers)
         SendMessage( hwndEdit, SCI_SETMARGINWIDTHN, 0, readOnly? 0 : SendMessage( hwndEdit, SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)(LPSTR)"_999999")); // Line Numbers
      else
         SendMessage( hwndEdit, SCI_SETMARGINWIDTHN, 0, 0); // Line Numbers

      SendMessage( hwndEdit, SCI_SETMARGINWIDTHN, 1, 0); // Non-Folding Symbols
      SendMessage( hwndEdit, SCI_SETMARGINWIDTHN, 2, 0); // Folding Symbols

      SendMessage( hwndEdit, SCI_STYLESETFONT, (WPARAM)0, readOnly?(LPARAM)(LPSTR)lfFont.lfFaceName : (LPARAM)(LPSTR)lfEditFont.lfFaceName);
      SendMessage( hwndEdit, SCI_STYLESETSIZE, (WPARAM)0, (LPARAM)-MulDiv( lfFont.lfHeight, 72, GetDeviceCaps( GetDC(hwndEdit), LOGPIXELSY ) ) );

      if( lfFont.lfWeight >= FW_BOLD )
         SendMessage( hwndEdit, SCI_STYLESETBOLD, (WPARAM)0, (LPARAM)TRUE);
      else
         SendMessage( hwndEdit, SCI_STYLESETBOLD, (WPARAM)0, (LPARAM)FALSE);

        if( lfFont.lfItalic)
         SendMessage( hwndEdit, SCI_STYLESETITALIC, (WPARAM)0, (LPARAM)TRUE);
      else
         SendMessage( hwndEdit, SCI_STYLESETITALIC, (WPARAM)0, (LPARAM)FALSE);

        if( lfFont.lfUnderline)
         SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)0, (LPARAM)TRUE);
      else
         SendMessage( hwndEdit, SCI_STYLESETUNDERLINE, (WPARAM)0, (LPARAM)FALSE);


      if (pWordWrap && !readOnly)
         SetEditWindowWidth( hwndEdit, iWrapCol); 
      else if (pWordWrap && readOnly)
         SetEditWindowWidth( hwndEdit, irWrapCol); 
      else 
         SetEditWindowWidth( hwndEdit, 9999 );

      SendMessage( hwndEdit, SCI_SETTABWIDTH,  iTabSpaces, (LPARAM)0L );

      SendMessage( hwndEdit, SCI_SETWRAPMODE,  readOnly ? (WPARAM)pWordWrap : TRUE, (LPARAM)0L );
      
      //SendMessage( hwndEdit, SCI_CLEARCMDKEY,  SCK_END);

      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_END + (SCMOD_ALT << 16), SCI_LINEEND );
      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_END, SCI_LINEENDDISPLAY );
      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_END + (SCMOD_SHIFT << 16), SCI_LINEENDDISPLAYEXTEND );
      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_HOME + (SCMOD_ALT << 16), SCI_HOME );
      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_HOME, SCI_HOMEDISPLAY );
      SendMessage( hwndEdit, SCI_ASSIGNCMDKEY, SCK_HOME + (SCMOD_SHIFT << 16), SCI_HOMEDISPLAYEXTEND );
      
      SendMessage( hwndEdit, SCI_COLOURISE, 0, -1);


      SendMessage( hwndEdit, SCI_CALLTIPSETBACK, GetSysColor(COLOR_INFOBK), 0);
      SendMessage( hwndEdit, SCI_CALLTIPSETFORE, GetSysColor(COLOR_INFOTEXT), 0);
      SendMessage( hwndEdit, SCI_SETMOUSEDWELLTIME, 1000, 0);
      
      if (GetWindowLong(hwndEdit, GWL_USERDATA) == 0)
      {
         lpEditCtlProc = SubclassWindow( hwndEdit, EditWndProc );
         SetWindowLong(hwndEdit, GWL_USERDATA, (LONG)lpEditCtlProc);
      }
//    SendMessage( hwndEdit, SCI_STYLESETHOTSPOT, 0, fHotLinks);
/*    if (!readOnly)
      {
//       SendMessage( hwndEdit, SCI_LINESSPLIT, SendMessage( hwndEdit, SCI_TEXTWIDTH, STYLE_DEFAULT, (LPARAM)(LPSTR)"A") * iWrapCol, 0);
         i = SendMessage( hwndEdit, SCI_TEXTWIDTH, STYLE_DEFAULT, (LPARAM)(LPSTR)" ") * iWrapCol;
         SendMessage( hwndEdit, SCI_SETEDGEMODE, EDGE_LINE, 0);
         SendMessage( hwndEdit, SCI_SETEDGECOLUMN, iWrapCol, 0);
//       SendMessage( hwndEdit, SCI_LINESSPLIT, i);
      }*/
}
#endif USEBIGEDIT

BOOL FASTCALL EditMap_GetModified(HWND hwndEdit)
{
   char szClsName[ 16 ];
   GetClassName( hwndEdit, szClsName, sizeof(szClsName) );

#ifdef USEBIGEDIT
   if( _strcmpi( szClsName, WC_BIGEDIT ) == 0 )
      return(Edit_GetModify(hwndEdit));
#else USEBIGEDIT
#ifdef GNU
   if( _strcmpi( szClsName, "Scintilla" ) == 0 )
#else GNU
   if( _strcmpi( szClsName, WC_BIGEDIT ) == 0 )    
#endif GNU
      return(SendMessage(hwndEdit, SCI_GETMODIFY, 0, 0));
#endif USEBIGEDIT

   return(Edit_GetModify(hwndEdit));

}

/* This function returns a handle for use in subsequent edit mapping
 * functions. The handle identifies the type of edit control.
 */
HEDIT FASTCALL Editmap_Open( HWND hwndEdit )
{
   if( NULL != hwndEdit )
      {
      char szClsName[ 16 ];

      GetClassName( hwndEdit, szClsName, sizeof(szClsName) );
#ifdef USEBIGEDIT
      if( _strcmpi( szClsName, WC_BIGEDIT ) == 0 )
         return( ETYP_BIGEDIT );
#else USEBIGEDIT
#ifdef GNU
      if( _strcmpi( szClsName, "Scintilla" ) == 0 )
#else GNU
      if( _strcmpi( szClsName, WC_BIGEDIT ) == 0 )
#endif GNU
      {
         return( ETYP_BIGEDIT );
      }
#endif USEBIGEDIT

      if( _strcmpi( szClsName, "edit" ) == 0 )
         return( ETYP_SMALLEDIT );
      }
   return( ETYP_NOTEDIT );
}
               
         
void FASTCALL Editmap_FormatLines( HEDIT hedit, HWND hwndEdit )
{
   if( ETYP_BIGEDIT == hedit )
   {
#ifdef USEBIGEDIT
      Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
      int i, c, m, w, l;
      if (fBreakLinesOnSave)  // !!SM!! 2.55.2045
      {
         m = SendMessage( hwndEdit, SCI_TEXTWIDTH, SCE_CIX_DEFAULT, (LPARAM)(LPSTR)"_999999999") / 10;
         m = (m * (iWrapCol+1)); //!!SM!! 2038 use n-1 characters to allow adding the CR on afterwards.

         i = 0;
         c = SendMessage(hwndEdit, SCI_GETLINECOUNT,0,0);
   //    for( i = 0 ; i < SendMessage(hwndEdit, SCI_GETLINECOUNT,0,0) ; i++ )
         while (i < c )
         {

            w = SendMessage(hwndEdit, SCI_POSITIONFROMLINE, i, 0);
            SendMessage( hwndEdit, SCI_SETTARGETSTART, w, 0);

   //       l = SendMessage( hwndEdit, SCI_GETLINEENDPOSITION, i, 0) - SendMessage( hwndEdit, SCI_POSITIONFROMLINE, i, 0);
            l = SendMessage( hwndEdit, SCI_LINELENGTH, i, 0);
            SendMessage( hwndEdit, SCI_SETTARGETEND, w + l, 0);

            SendMessage(hwndEdit, SCI_LINESSPLIT, m, 0);
            c = SendMessage(hwndEdit, SCI_GETLINECOUNT,0,0);
            i++;
         }
      }
#endif USEBIGEDIT
   }
   else
      Edit_FmtLines( hwndEdit, TRUE );

}
/* This function returns the selection offsets in the specified BEC_SELECTION
 * structure. It handles the big and small edit controls appropriately.
 */
void FASTCALL Editmap_GetSelection( HEDIT hedit, HWND hwndEdit, LPBEC_SELECTION lpbsel )
{
   if( ETYP_BIGEDIT == hedit )
   {
#ifdef USEBIGEDIT
      SendMessage( hwndEdit, BEM_GETSEL, 0, (LPARAM)lpbsel );
#else USEBIGEDIT
      lpbsel->lo = SendMessage( hwndEdit, SCI_GETSELECTIONSTART, 0, 0 );
      lpbsel->hi = SendMessage( hwndEdit, SCI_GETSELECTIONEND, 0, 0 );
#endif USEBIGEDIT
   }
   else
      {
      DWORD dwSel;

      dwSel = Edit_GetSel( hwndEdit );
      lpbsel->lo = LOWORD( dwSel );
      lpbsel->hi = HIWORD( dwSel );
      }
}

/* Returns whether or not the current editing action can be
 * undone.
 */
BOOL FASTCALL Editmap_CanUndo( HEDIT hedit, HWND hwndEdit )
{
   if( ETYP_BIGEDIT == hedit )
#ifdef USEBIGEDIT
      return( FALSE );
#else USEBIGEDIT
       return (BOOL)SendMessage(hwndEdit, SCI_CANUNDO, 0, 0);
#endif USEBIGEDIT
   return( Edit_CanUndo( hwndEdit ) );
}

// 20060302 - 2.56.2049.08
/* Returns whether or not the current editing action can be
 * redone.
 */
BOOL FASTCALL Editmap_CanRedo( HEDIT hedit, HWND hwndEdit )
{
   if( ETYP_BIGEDIT == hedit )
#ifdef USEBIGEDIT
      return( FALSE );
#else USEBIGEDIT
       return (BOOL)SendMessage(hwndEdit, SCI_CANREDO, 0, 0);
#endif USEBIGEDIT
   return( FALSE );
}

/* This function returns the length of the text in the specified edit
 * control. The return value is of type SELRANGE since that accurately
 * describes the extent of the text.
 */
SELRANGE FASTCALL Editmap_GetTextLength( HEDIT hedit, HWND hwndEdit )
{
   if( ETYP_BIGEDIT == hedit )
#ifdef USEBIGEDIT
      return( SendMessage( hwndEdit, BEM_GETTEXTLENGTH, 0, 0L ) );
#else USEBIGEDIT
      return( SendMessage( hwndEdit, SCI_GETTEXTLENGTH, 0, 0L ) );
#endif USEBIGEDIT
   return( (SELRANGE)SendMessage( hwndEdit, WM_GETTEXTLENGTH, 0, 0L ) );
}

/* This function retrieves text from the edit control. For the small edit control,
 * it returns a block of text less than 32K. For the large edit control, the text
 * may be more than 64K.
 */
void FASTCALL Editmap_GetText( HEDIT hedit, HWND hwndEdit, SELRANGE cchMaxText, HPSTR lpText )
{
   if( ETYP_BIGEDIT == hedit )
      {
#ifdef USEBIGEDIT
      BEC_GETTEXT bgt;

      bgt.cchMaxText = cchMaxText;
      bgt.hpText = lpText;
      bgt.bsel.lo = 0;
      bgt.bsel.hi = 0;
      SendMessage( hwndEdit, BEM_GETTEXT, 0, (LPARAM)(LPSTR)&bgt );
#else USEBIGEDIT                 
      SendMessage( hwndEdit, SCI_GETTEXT, cchMaxText, (LPARAM)(LPSTR)lpText );
#endif USEBIGEDIT
      }
   else
      SendMessage( hwndEdit, WM_GETTEXT, (WPARAM)cchMaxText, (LPARAM)lpText );
}

/* This function retrieves the selected text from the edit control. If nothing
 * is selected, the returned string is empty.
 */
void FASTCALL Editmap_GetSel( HEDIT hedit, HWND hwndEdit, SELRANGE cchMaxText, HPSTR lpText )
{
   if( ETYP_BIGEDIT == hedit )
      {
#ifdef USEBIGEDIT
      BEC_GETTEXT bgt;

      bgt.cchMaxText = cchMaxText;
      bgt.hpText = lpText;
      SendMessage( hwndEdit, BEM_GETSEL, 0, (LPARAM)&bgt.bsel );
      SendMessage( hwndEdit, BEM_GETTEXT, 0, (LPARAM)(LPSTR)&bgt );
#else USEBIGEDIT                 
      SendMessage( hwndEdit, SCI_GETSELTEXT, cchMaxText, (LPARAM)(LPSTR)lpText );
#endif USEBIGEDIT
      }
   else
      SendMessage( hwndEdit, WM_GETTEXT, (WPARAM)cchMaxText, (LPARAM)lpText );
}


void FASTCALL Editmap_SetWindowText(HWND hwndEdit, HPSTR hpText)
{
   if( ETYP_BIGEDIT == Editmap_Open( hwndEdit ) )
   {
#ifdef USEBIGEDIT
//    SetWindowText( hwndEdit, GS(IDS_STR55) );
      SetWindowText( hwndEdit, hpText );
#else USEBIGEDIT
       int ro;

      ro = SendMessage( hwndEdit, SCI_GETREADONLY,  (WPARAM)0L, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_SETREADONLY,  (WPARAM)FALSE, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_CLEARALL, 0, 0);
      SendMessage( hwndEdit, SCI_CLEARDOCUMENTSTYLE, 0, 0);
      SendMessage( hwndEdit, SCI_STYLESETHOTSPOT,   (WPARAM)SCE_CIX_HOTLINK, fHotLinks); // 20060306 - 2.56.2049.10
      SendMessage( hwndEdit, SCI_ALLOCATE, lstrlen( hpText ), 0);
      SendMessage( hwndEdit, SCI_SETTEXT, 0, (LPARAM)(LPSTR)hpText);
      SendMessage( hwndEdit, SCI_SETREADONLY,  (WPARAM)ro, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_SETXOFFSET, (WPARAM)0, (LPARAM)0L);

#endif USEBIGEDIT
   }
   else
      SetWindowText( hwndEdit, hpText );

}

/* This function replaces the current selection in the edit control with
 * the specified text. If nothing is selected, the text is inserted at the
 * edit position.
 */
void FASTCALL Editmap_ReplaceSel( HEDIT hedit, HWND hwndEdit, HPSTR hpText )
{
   if( ETYP_BIGEDIT == hedit )
   {
#ifdef USEBIGEDIT
      SendMessage( hwndEdit, BEM_REPLACESEL, 0, (LPARAM)hpText );
#else USEBIGEDIT
       int ro;

      ro = SendMessage( hwndEdit, SCI_GETREADONLY,  (WPARAM)0L, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_SETREADONLY,  (WPARAM)FALSE, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_REPLACESEL, 0, (LPARAM)(LPSTR)hpText );
      SendMessage( hwndEdit, SCI_SETREADONLY,  (WPARAM)ro, (LPARAM)0L );
      SendMessage( hwndEdit, SCI_SETXOFFSET, (WPARAM)0, (LPARAM)0L);
#endif USEBIGEDIT
   }
   else
      Edit_ReplaceSel( hwndEdit, hpText );
}

void FASTCALL Editmap_SetSel( HEDIT hedit, HWND hwndEdit, LPBEC_SELECTION lpbsel )
{
   if( ETYP_BIGEDIT == hedit )
   {
#ifdef USEBIGEDIT
      SendMessage( hwndEdit, BEM_SETSEL, 0, (LPARAM)lpbsel );
#else USEBIGEDIT     
      SendMessage( hwndEdit, SCI_SETSEL, lpbsel->lo, lpbsel->hi);
#endif USEBIGEDIT
   }
   else
      {
      Edit_SetSel( hwndEdit, (UINT)lpbsel->lo, (UINT)lpbsel->hi );
      Edit_ScrollCaret( hwndEdit );
      }
}

void FASTCALL Editmap_SelectAll( HEDIT hedit, HWND hwndEdit )
{
   BEC_SELECTION bsel;

   bsel.lo = 0;
   bsel.hi = 0xFFFFFFFF;

   if( ETYP_BIGEDIT == hedit )
   {
#ifdef USEBIGEDIT
      Editmap_SetSel( hedit, hwndEdit, &bsel );
#else USEBIGEDIT
      SendMessage(hwndEdit, SCI_SELECTALL, 0, 0);
#endif USEBIGEDIT

   }
   else
   {
      Edit_SetSel( hwndEdit, (UINT)bsel.lo, (UINT)bsel.hi );
      Edit_ScrollCaret( hwndEdit );
   }
}

/* This function handles scrolling the edit control by the specified number of
 * lines and columns.
 */
void FASTCALL Editmap_Scroll( HEDIT hedit, HWND hwndEdit, int cbVScroll, int cbHScroll )
{
   if( ETYP_BIGEDIT == hedit )
      {
#ifdef USEBIGEDIT
      BEC_SCROLL bscr;

      bscr.cbVScroll = cbVScroll;
      bscr.cbHScroll = cbHScroll;
      SendMessage( hwndEdit, BEM_SCROLL, 0, (LPARAM)(LPSTR)&bscr );
#else USEBIGEDIT                 
//    SendMessage( hwndEdit, SCI_LINESCROLL, 0, 0 );
      SendMessage( hwndEdit, SCI_LINESCROLL, cbHScroll, cbVScroll );
#endif USEBIGEDIT
      }
   else
      Edit_Scroll( hwndEdit, (UINT)cbVScroll, (UINT)cbHScroll );
}

/* This function handles scrolling the edit control by the specified number of
 * lines and columns.
 */
SELRANGE FASTCALL Editmap_GetFirstVisibleLine( HEDIT hedit, HWND hwndEdit )
{
   if( ETYP_BIGEDIT == hedit )
#ifdef USEBIGEDIT
      return( SendMessage( hwndEdit, BEM_GETFIRSTVISIBLELINE, 0, 0L ) );
#else USEBIGEDIT                 
      return( SendMessage( hwndEdit, SCI_GETFIRSTVISIBLELINE, 0, 0L ) );
#endif USEBIGEDIT
   return( Edit_GetFirstVisibleLine( hwndEdit ) );
}

/* This function returns the edit rectangle, which may be separate from the
 * actual rectangle of the control.
 */
void FASTCALL Editmap_GetRect( HEDIT hedit, HWND hwndEdit, LPRECT lprcEditNC )
{
#ifdef USEBIGEDIT
   if( ETYP_BIGEDIT == hedit )
      SendMessage( hwndEdit, BEM_GETRECT, 0, (LPARAM)lprcEditNC );
   else
#endif USEBIGEDIT
      Edit_GetRect( hwndEdit, lprcEditNC );
}

/* This function returns the index of the line containing the specified
 * offset.
 */
SELRANGE FASTCALL Editmap_LineFromChar( HEDIT hedit, HWND hwndEdit, SELRANGE chOffset )
{
   if( ETYP_BIGEDIT == hedit )
#ifdef USEBIGEDIT
      return( SendMessage( hwndEdit, BEM_LINEFROMCHAR, 0, (LPARAM)chOffset ) );
#else USEBIGEDIT
      return( SendMessage( hwndEdit, SCI_LINEFROMPOSITION, chOffset, 0 ) );
#endif USEBIGEDIT
   return( Edit_LineFromChar( hwndEdit, (UINT)chOffset ) );
}


int FindInTarget(HEDIT hedit, HWND hwndEdit, const char *findWhat, int lenFind, int startPosition, int endPosition) 
{
   int posFind;

   SendMessage(hwndEdit, SCI_SETTARGETSTART, startPosition, 0);
   SendMessage(hwndEdit, SCI_SETTARGETEND, endPosition, 0);
   
   posFind = SendMessage(hwndEdit, SCI_SEARCHINTARGET, lenFind, (LPARAM)findWhat);

   return posFind;
}

void FASTCALL Editmap_Searchfor(HEDIT hedit, HWND hwndEdit, LPSTR lpText, int pCount, int pStart, int pFlags)
{
   struct TextToFind tf;
   int i = 0;
   int lPos;
   BEC_SELECTION sel;
   
   tf.chrg.cpMin = 0;
   tf.chrg.cpMax = 99999999;

   tf.lpstrText = lpText;

   SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, pFlags, 0);

   lPos = pStart;

   while( i <= pCount )
   {
      lPos = FindInTarget(hedit, hwndEdit, lpText, strlen(lpText), lPos + 1, 99999999) ;
      i++;
   }

   sel.lo = lPos;
   sel.hi = lPos + strlen(lpText);
   Editmap_SetSel( hedit, hwndEdit, &sel );
}
