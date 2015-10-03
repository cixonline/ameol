/* STRS.C - String functions
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

#include <windows.h>
#include "amlib.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "..\Ameol232\main.h" // !!SM!! 2039

#define  THIS_FILE   __FILE__

typedef BOOL(FAR PASCAL * IMPORTPROC)(LPSTR, LPSTR);
typedef void(FAR PASCAL * QUICKREPLACEPROC)(LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * CHECKFORBLINKERRORSPROC)(LPSTR, LPSTR, int);

// PROCEDURE QuickReplace(pSource, pDest, pSearchFor, pReplaceWith: PChar); STDCALL;

//**********************************************************************
// strcmpiw
// =======
// Just like _strcmpi except it handles wildcards * and ?
//**********************************************************************

static char IStrMap[256] =
{
   '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
      '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
      '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
      '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
      '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', //  !"#$%&'
      '\x28', '\x29', '\x2a', '\x2b', '\x2c', '\x2d', '\x2e', '\x2f', //  ()*+,-./
      '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37', //  01234567
      '\x38', '\x39', '\x3a', '\x3b', '\x3c', '\x3d', '\x3e', '\x3f', //  89:;<=>?
      '\x40', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', //  @ABCDEFG
      '\x68', '\x69', '\x6a', '\x6b', '\x6c', '\x6d', '\x6e', '\x6f', //  HIJKLMNO
      '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', //  PQRSTUVW
      '\x78', '\x79', '\x7a', '\x5b', '\x5c', '\x5d', '\x5e', '\x5f', //  XYZ[\]^_
      '\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', //  `abcdefg
      '\x68', '\x69', '\x6a', '\x6b', '\x6c', '\x6d', '\x6e', '\x6f', //  hijklmno
      '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', //  pqrstuvw
      '\x78', '\x79', '\x7a', '\x7b', '\x7c', '\x7d', '\x7e', '\x7f', //  xyz{|}~
      '\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',     
      '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
      '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
      '\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
      '\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
      '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
      '\xb0', '\xb1', '\xd2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
      '\xb8', '\xd9', '\xba', '\xbb', '\xdc', '\xbd', '\xbe', '\xbf',
      '\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
      '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xed', '\xce', '\xef',
      '\xf0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xf6', '\xd7',
      '\xd8', '\xf9', '\xfa', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
      '\x00', '\xe1', '\xe2', '\x03', '\x04', '\xe5', '\xe6', '\xe7',
      '\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
      '\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
      '\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff'
};


BOOL ChkExp(LPSTR ARegExpr, LPSTR AInputStr)
{
   IMPORTPROC lpImpProc;
   BOOL lResult;
   
   lResult = FALSE;
   
   
   if (hRegLib < (HINSTANCE)32)
      return (FALSE);
   
      /* if ((hImpLib = LoadLibrary("REGEXP.DLL")) <(HINSTANCE)32)
      {
      return (FALSE);
      }
   */
   
   
   // FUNCTION (ARegExpr: PChar; AInputStr: PChar): BOOL; STDCALL;
    /* Find the import procedure entry point in the
   * main executable.
   */
   if (NULL ==(lpImpProc = (IMPORTPROC)GetProcAddress(hRegLib, "CheckRegExpr")))
   {
      return (FALSE);
   }
   
   /* If we get here, we've got an import procedure entry point, so
   * call it and wait for it to do the biz.
   */
   lResult = lpImpProc(ARegExpr, AInputStr);
   
   /* Clean up after us.
   */
   // FreeLibrary( hImpLib );
   
   return lResult;
}


void FASTCALL QuickReplace(LPSTR pSource, LPSTR pDest, LPSTR pSearchFor, LPSTR pReplaceWith) // 20060228 - 2.56.2049.06
{
   QUICKREPLACEPROC lpImpProc;
   
   if (hRegLib < (HINSTANCE)32)
      return;
   
   if (NULL ==(lpImpProc = (QUICKREPLACEPROC)GetProcAddress(hRegLib, "QuickReplace")))
      return;
   
   lpImpProc(pSource, pDest, pSearchFor, pReplaceWith);
}

BOOL FASTCALL CheckForBlinkErrors(LPSTR pSource, LPSTR pResults, int pLen)
{
   CHECKFORBLINKERRORSPROC lpImpProc;
   
   if (hRegLib < (HINSTANCE)32)
      return FALSE;
   
   if (NULL ==(lpImpProc = (CHECKFORBLINKERRORSPROC)GetProcAddress(hRegLib, "CheckForBlinkErrors")))
      return FALSE;
   
   return lpImpProc(pSource, pResults, pLen);
}

int FASTCALL strcmpiwild(LPSTR Pattern, LPSTR Source, BOOL UseRegExp)
{ 
   int i;
   LPSTR patptr, strptr;
   BOOL Literal;
   
   if (UseRegExp)
   {
      if (ChkExp(Pattern, Source))
      {
         return 0;
      }
      else
      {
         return 1;
      }
   }
   else
   {
      patptr = Pattern;
      strptr = Source;
      
      while (*patptr != '\0' || *strptr != '\0')
      {
         if (*patptr == '\\') 
         {
            ++patptr;
            Literal = TRUE;
         }
         else
         {
            Literal = FALSE;
         }
         
         if ((*patptr == '*') && !Literal)
         { 
            while (*patptr == '*')
               patptr++;
            
            if (*patptr == '\0')
               return (0);
            
            while ((i = strcmpiwild(patptr, strptr, UseRegExp)) != 0)
               if (*strptr++ == '\0')
                  return (i);
         }
         
         else if ((*patptr == '?')  && !Literal)
         {
         }
         
         else
         { 
            if (IStrMap[*patptr] > IStrMap[*strptr])
               return (1);
            
            if (IStrMap[*patptr] < IStrMap[*strptr])
               return (-1);
         }
         ++patptr;
         ++strptr;
      }
      
      return (0);
   }
}

int FASTCALL Hstrcmpiwild(LPSTR  Pattern, HPSTR Source, BOOL UseRegExp)
{ 
   int i;
   LPSTR patptr;
   HPSTR strptr;
   
   if (UseRegExp)
   {
      if (ChkExp(Pattern, Source))
      {
         return 0;
      }
      else
      {
         return 1;
      }
   }
   else
   {
      patptr = Pattern;
      strptr = Source;
      
      while (*patptr != '\0' || *strptr != '\0')
      {
         if (*patptr == '*')
         { 
            while (*patptr == '*')
               patptr++;
            
            if (*patptr == '\0')
               return (0);
            
            while ((i = Hstrcmpiwild(patptr, strptr, UseRegExp)) != 0)
               if (*strptr++ == '\0')
                  return (i);
         }
         
         else if (*patptr == '?')
         {
         }
         
         else
         { 
            if (IStrMap[*patptr] > IStrMap[*strptr])
               return (1);
            
            if (IStrMap[*patptr] < IStrMap[*strptr])
               return (-1);
         }
         
         ++patptr;
         ++strptr;
      }
      
      return (0);
   }
}

/* Far model version of string pattern matching function. The text
 * pointed by lpStr1 must not be larger than 64K.
 */
int FASTCALL FStrMatch(LPSTR lpStr1, LPSTR lpStr2, BOOL fCase, BOOL fWordMatch)
{
   LPSTR lpStrStart;
   int count = 0;
   BOOL fSpace;
   
   lpStrStart = lpStr1;
   fSpace = TRUE;
   while (*lpStr1)
   {
      if (!(fWordMatch && !fSpace))
         if (IsEq(*lpStr1, *lpStr2, fCase))
         {
            LPSTR lpStr1Start;
            LPSTR lpStr2Start;
            
            lpStr2Start = lpStr2;
            lpStr1Start = lpStr1;
            ++lpStr2;
            ++lpStr1;
            while (*lpStr1 != '\0' && *lpStr2 != '\0')
            {
               if (*lpStr2 != '?')
                  if (!IsEq(*lpStr1, *lpStr2, fCase))
                     break;
                  ++lpStr1;
                  ++lpStr2;
            }
            if (*lpStr2 == '\0')
               if (fWordMatch)
               {
                  if (!isalnum(*lpStr1))
                     return (count);
               }
               else
                  return (count);
               lpStr2 = lpStr2Start;
               lpStr1 = lpStr1Start;
         }
         if (fWordMatch)
            fSpace = !isalnum(*lpStr1);
         if (++count == 0)
            break;
         ++lpStr1;
   }
   return (-1);
}

/* Huge model version of string pattern matching function.
 */
int FASTCALL HStrMatch(HPSTR hpStr1, LPSTR lpStr2, BOOL fCase, BOOL fWordMatch)
{
   HPSTR lpStrStart;
   int count = 0;
   BOOL fSpace;
   
   lpStrStart = hpStr1;
   fSpace = TRUE;
   while (*hpStr1)
   {
      if (!(fWordMatch && !fSpace))
         if (IsEq(*hpStr1, *lpStr2, fCase))
         {
            HPSTR hpStr1Start;
            LPSTR lpStr2Start;
            
            lpStr2Start = lpStr2;
            hpStr1Start = hpStr1;
            ++lpStr2;
            ++hpStr1;
            while (*hpStr1 != '\0' && *lpStr2 != '\0')
            {
               if (*lpStr2 != '?')
                  if (!IsEq(*hpStr1, *lpStr2, fCase))
                     break;
                  ++hpStr1;
                  ++lpStr2;
            }
            if (*lpStr2 == '\0')
               if (fWordMatch)
               {
                  if (!isalnum(*hpStr1))
                     return (count);
               }
               else
                  return (count);
               lpStr2 = lpStr2Start;
               hpStr1 = hpStr1Start;
         }
         if (fWordMatch)
            fSpace = !isalnum(*hpStr1);
         if (++count == 0)
            break;
         ++hpStr1;
   }
   return (-1);
}


/* This function removes any leading and trailing spaces from the string
 * pointed by the npStart parameter.
 */
void FASTCALL StripLeadingTrailingSpaces(char * npStart)
{
   StripLeadingTrailingChars(npStart, ' ');
}

/* This function removes any leading and trailing spaces from the string
 * pointed by the npStart parameter.
 */
void FASTCALL StripLeadingTrailingQuotes(char * npStart)
{
   StripLeadingTrailingChars(npStart, '"' );
}

/* This function strips any leading or trailing characters from the
 * string pointed by the npStart parameter.
 */
void FASTCALL StripLeadingTrailingChars(char * npStart, char chToStrip)
{
   char * np1;
   char * np2;
   
   np1 = np2 = npStart;
   while (*np2 == chToStrip)
      ++np2;
   while (*np2)
      *np1++ = *np2++;
   while (np1 > npStart && * (np1 - 1) == chToStrip)
      --np1;
   *np1 = '\0';
}

void FASTCALL StripLeadingChars(char * npStart, char chToStrip)
{
   char * np1;
   char * np2;
   
   np1 = np2 = npStart;
   while (*np2 == chToStrip)
      ++np2;
   while (*np2)
      *np1++ = *np2++;
   *np1 = '\0';
}

void FASTCALL StripTrailingChars(char * npStart, char chToStrip)  // !!SM!! 2.55.2032
{
   char * np1;
   char * np2;
   
   np1 = np2 = npStart;
   while (*np2)
      *np1++ = *np2++;
   while (np1 > npStart && * (np1 - 1) == chToStrip)
      --np1;
   *np1 = '\0';
}

/* This function strips all occurrences of the specified
 * character from the string.
 */
void FASTCALL StripCharacter(char * npStr, char chToStrip)
{
   char * np1;
   
   for (np1 = npStr; *npStr; ++npStr)
      if (*npStr != chToStrip)
         *np1++ = *npStr;
      *np1 = '\0';
}

/* This function strips all occurrences of the specified
 * character from the string.
 */
void FASTCALL StripBinaryCharacter(char * npStr, int count, char chToStrip)
{
   char * np1;
   
   for (np1 = npStr; count--; ++npStr)
      if (*npStr != chToStrip)
         *np1++ = *npStr;
      *np1 = '\0';
}

void FASTCALL StripBinaryCharacter32(char * npStr, LONG count, char chToStrip) //!!SM!! 2042
{
   char * np1;
   
   for (np1 = npStr; count--; ++npStr)
      if (*npStr != chToStrip)
         *np1++ = *npStr;
      *np1 = '\0';
}

/* This function strips all non - printable characters
 * from the string.
 */
void FASTCALL StripNonPrintCharacters(char * npStr, int count)
{
   char * np1;
   
   for (np1 = npStr; count--; ++npStr)
      if (*npStr > 0 && (isprint(*npStr)) || *npStr == 13 || *npStr == 10)
         *np1++ = *npStr;
      *np1 = '\0';
}

/* This function formats the number, dwNum, into the buffer
 * lpBuf with the appropriate thousands separator(taken from the
 * International section of WIN.INI) between each thousands group.
 * If lpBuf is NULL, a local buffer is used and the return value
 * points to this local buffer.
 */
LPSTR FASTCALL FormatThousands(DWORD dwNum, LPSTR lpBuf)
{
   static char sz[40];
   // static char szThousands[ 2 ];
   register int c;
   
   if (!lpBuf)
      lpBuf = sz;
   c = 0;
   // GetProfileString( "intl", "sThousand", ",", szThousands, sizeof( szThousands ) );
   do 
   {
      if ((c + 1) % 4 == 0)
         lpBuf[c++] = ',';
      lpBuf[c++] = (char)(int)(dwNum % 10) + '0';
      dwNum /= 10;
   }
   while (dwNum);
   lpBuf[c] = '\0';
   _strrev(lpBuf);
   return (lpBuf);
}

/* Adjust a file path spec string to fit within a given pixel width
 * when painted into a DC. The function needs the DC to have the
 * display font for the path string selected.
 */
void FASTCALL FitPathString(HDC hDC, LPSTR lpszStr, UINT uWidth)
{
   LPSTR lpWork;
   UINT u, v;
   SIZE size;
   
   INITIALISE_PTR(lpWork);
   if (fNewMemory(&lpWork, _MAX_PATH))
   {
      if ((int)uWidth < 0)
         lpszStr[0] = '\0';
      else
      {
         lstrcpy(lpWork, lpszStr);
         for (u = 0; GetTextExtentPoint(hDC, lpWork, lstrlen(lpWork), &size), size.cx > (int)uWidth; u++)
         {
            v = 0;
            if (u == 0)
            {
               for (; u < (UINT)lstrlen(lpszStr); v++, u++)
               {
                  lpWork[v] = lpszStr[u];
                  if (lpszStr[u] == '\\')
                     break;
               }
               if (u ==(UINT)lstrlen(lpszStr))
                  break;
               for (v++; v < u + 4; v++)
                  lpWork[v] = '.';            
            }                  
            for (; u < (UINT)lstrlen(lpszStr); u++)
               if (lpszStr[u] == '\\')
                  break;
               if (u ==(UINT)lstrlen(lpszStr))
                  break;
               lstrcpy(&lpWork[v], &lpszStr[u]);
         }
         lstrcpy(lpszStr, lpWork);
      }
      FreeMemory(&lpWork);
   }
}

/* This function converts a string into a compact string.
 */
LPSTR FASTCALL MakeCompactString(LPSTR lpsz)
{
   LPSTR lpszNew;
   LPSTR lpszOut;
   
   INITIALISE_PTR(lpszNew);
   if (fNewMemory(&lpszNew, lstrlen(lpsz) + 2))
   {
      BOOL fQuote;
      
      lpszOut = lpszNew;
      lstrcpy(lpszNew, lpsz);
      fQuote = FALSE;
      do 
      {
         while (isspace(*lpsz))
            ++lpsz;
         while (*lpsz)
         {
            if (*lpsz == '"' )
            {
               fQuote = !fQuote;
               *lpszOut++ = *lpsz++;
            }
            else 
            {
               if (!fQuote && (*lpsz == ',' || isspace(*lpsz)))
                  break;
               *lpszOut++ = *lpsz++;
            }
         }
         *lpszOut++ = '\0';
         while (isspace(*lpsz) || *lpsz == ',')
            ++lpsz;
      }
      while (*lpsz);
      *lpszOut = '\0';
   }
   return (lpszNew);
}

/* This function uncompacts the specified string,
 * replacing NULLs with spaces.
 */
void FASTCALL UncompactString(LPSTR lpszTo)
{
   while (*lpszTo)
   {
      lpszTo += lstrlen(lpszTo);
      if (* (lpszTo + 1))
         *lpszTo++ = ' ';
   }
}

/* This function copies a sub - 64K huge memory block to a far memory
 * block. It fails if the huge block is >64K.
 */
LPSTR FASTCALL HpLpStrCopy(HPSTR hpStr)
{
   LPSTR lpStr;
   
   ASSERT(hstrlen(hpStr) < 0x00010000);
   INITIALISE_PTR(lpStr);
   if (fNewMemory(&lpStr, lstrlen(hpStr) + 1))
      hstrcpy(lpStr, hpStr);
   return (lpStr);
}

/* This function concatenates two strings.
 */
HPSTR FASTCALL ConcatenateStrings(HPSTR lpTmpBuf, HPSTR lpszLogText)
{
   HPSTR lpCatString;
   DWORD dwCatSize;
   
   lpCatString = NULL;
   dwCatSize = hstrlen(lpTmpBuf) + hstrlen(lpszLogText) + 1;
   if (fNewMemory32(&lpCatString, dwCatSize))
   {
      hstrcpy(lpCatString, lpTmpBuf);
      hstrcat(lpCatString, lpszLogText);
      return (lpCatString);
   }
   return (NULL);
}

/* Case insensitive version of strstr
 */
char * FASTCALL stristr(char * pszString, char * pszPattern)
{
   char * pptr;
   char * sptr;
   char * start;
   UINT slen;
   UINT plen;
   
   slen = strlen(pszString);
   plen = strlen(pszPattern);
   for (start = pszString, pptr = pszPattern; slen >= plen; start++, slen--)
   {
   /* find start of pattern in string
      */
      while (toupper(*start) != toupper(*pszPattern))
      {
         start++;
         slen--;
         
         /* if pattern longer than string
         */
         if (slen < plen)
            return (NULL);
      }
      sptr = start;
      pptr = pszPattern;
      while (toupper(*sptr) == toupper(*pptr))
      {
         sptr++;
         pptr++;
         
         /* if end of pattern then pattern was found
         */
         if ('\0' == *pptr)
            return (start);
      }
   }
   return (NULL);
}

/* Given a pointer to a title string, this function calculates whether
 * the title will fit into 79 characters. If the title is longer than
 * 79 characters, it is truncated to 76 characters and an ellipsis(...)
 * is appended to the title to show that it was truncated.
 */
void FASTCALL StoreTitle(LPSTR lpszDst, LPSTR lpszSrc)
{
   register int c;
   BOOL fTruncated;
   int nMax;
   
   while (*lpszSrc == 13 || *lpszSrc == ' ' || *lpszSrc == '\t')
   {
      for (c = 0; lpszSrc[c] == ' ' || lpszSrc[c] == '\t'; ++c)
         ;
      if (lpszSrc[c] == 13)
      {
         lpszSrc += c + 1;
         if (*lpszSrc == 10)
            ++lpszSrc;
      }
      else
         break;
   }
   fTruncated = RawLineLength(lpszSrc) > 79;
   nMax = fTruncated ? 76 : 79;
   for (c = 0; lpszSrc[c] && lpszSrc[c] != 13 && lpszSrc[c] != 10 && c < nMax; ++c)
      lpszDst[c] = lpszSrc[c];
   if (fTruncated)
   {
      lpszDst[c++] = '.';
      lpszDst[c++] = '.';
      lpszDst[c++] = '.';
   }
   lpszDst[c] = '\0';
}

/* Calculates the length of one line of the specified string. A line
 * is assumed to be delimited by any of CR, LF or a NULL.
 */
int FASTCALL RawLineLength(LPSTR lpszTxt)
{
   register int c;
   
   for (c = 0; lpszTxt[c] && lpszTxt[c] != 13 && lpszTxt[c] != 10; ++c)
      ;
   return (c);
}

/* This function advances to the text line.
 */
char * FASTCALL AdvanceLine(char * lpszTxt)
{
   while (*lpszTxt && *lpszTxt != '\r' && *lpszTxt != '\n')
      ++lpszTxt;
   if (*lpszTxt == '\r')
      ++lpszTxt;
   if (*lpszTxt == '\n')
      ++lpszTxt;
   return (lpszTxt);
}

/* This function copies pSrc to pDest, expanding any '&' chrs to '&&' so
 * that they display correctly.
 */
void FASTCALL StrExpandPrefix(char * pSrc, char * pDest, int cbDest)
{
   while (cbDest > 1 && *pSrc)
   {
      if (*pSrc == '&' && * (pSrc + 1) != '&' && cbDest > 2)
         *pDest++ = '&';
      *pDest++ = *pSrc++;
      --cbDest;
   }
   *pDest = '\0';
}

/* This function copies pSrc to pDest, converting any '&&' to just
 * '&'.
 */
void FASTCALL StrStripPrefix(char * pSrc, char * pDest, int cbDest)
{
   while (cbDest > 1 && *pSrc)
      if (*pSrc == '&' && * (pSrc + 1) == '&')
         ++pSrc;
      else
      {
         *pDest++ = *pSrc++;
         --cbDest;
      }
      *pDest = '\0';
}

/* This function counts the size of a message. Newlines(CR/LF)
 * are treated as LF.
 */
DWORD FASTCALL CountSize(HPSTR hpText)
{
   DWORD dwSize;
   
   dwSize = 0;
   while (*hpText)
   {
//    if (*hpText != 13)
         ++dwSize;
      ++hpText;
   }
   return (dwSize);
}

/* This function initialises a string buffer
 */
BOOL FASTCALL StrBuf_Init(STRBUF FAR * lpstrbuf)
{
   lpstrbuf->hpBufText = NULL;
   return (TRUE);
}

/* This function appends a line to the string buffer
 */
BOOL FASTCALL StrBuf_AppendLine(STRBUF FAR * lpstrbuf, char * pszLine)
{
   return StrBuf_AppendText(lpstrbuf, pszLine, TRUE);
}

/* This function appends a line of wSize characters to the string buffer
 */
BOOL FASTCALL StrBuf_AppendText(STRBUF FAR * lpstrbuf, char * pszLine, BOOL fNewline)
{
   int wSize;
   
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
   */
   wSize = strlen(pszLine);
   if (lpstrbuf->hpBufText == NULL)
   {
      lpstrbuf->dwBufSize = 64000;
      lpstrbuf->dwBufLength = 0;
      fNewMemory32(&lpstrbuf->hpBufText, lpstrbuf->dwBufSize);
   }
   else if (lpstrbuf->dwBufLength + (DWORD)wSize + 2L >= lpstrbuf->dwBufSize)
   {
      lpstrbuf->dwBufSize += 2048;
      fResizeMemory32(&lpstrbuf->hpBufText, lpstrbuf->dwBufSize);
   }
   if (lpstrbuf->hpBufText)
   {
      hstrcpy(lpstrbuf->hpBufText + lpstrbuf->dwBufLength, pszLine);
      if (fNewline)
      {
         hstrcat(lpstrbuf->hpBufText, "\r\n");
         lpstrbuf->dwBufLength += 2;
      }
      lpstrbuf->dwBufLength += wSize;
      return (TRUE);
   }
   return (FALSE);
}

/* This function prepends a line to the string buffer
 */
BOOL FASTCALL StrBuf_PrependLine(STRBUF FAR * lpstrbuf, char * pszLine)
{
   int wSize;
   
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
   */
   wSize = strlen(pszLine);
   if (lpstrbuf->hpBufText == NULL)
   {
      lpstrbuf->dwBufSize = 64000;
      lpstrbuf->dwBufLength = 0;
      fNewMemory32(&lpstrbuf->hpBufText, lpstrbuf->dwBufSize);
   }
   else if (lpstrbuf->dwBufLength + (DWORD)wSize + 2L >= lpstrbuf->dwBufSize)
   {
      lpstrbuf->dwBufSize += 2048;
      fResizeMemory32(&lpstrbuf->hpBufText, lpstrbuf->dwBufSize);
   }
   if (lpstrbuf->hpBufText)
   {
      hmemmove(lpstrbuf->hpBufText + wSize + 2, lpstrbuf->hpBufText, lpstrbuf->dwBufLength);
      memcpy(lpstrbuf->hpBufText, pszLine, wSize);
      memcpy(lpstrbuf->hpBufText + wSize, "\r\n", 2);
      lpstrbuf->dwBufLength += wSize + 2;
      return (TRUE);
   }
   return (FALSE);
}

/* Returns the length of the text in the buffer.
 */
DWORD FASTCALL StrBuf_GetBufferSize(STRBUF FAR * lpstrbuf)
{
   return (lpstrbuf->dwBufLength);
}

/* Returns a pointer to the text in the buffer.
 */
HPSTR FASTCALL StrBuf_GetBuffer(STRBUF FAR * lpstrbuf)
{
   return (lpstrbuf->hpBufText);
}

int ExtractToken(char * source, char * dest, int token, char sep)
{
   int i = 0, j = 0, k = 0;
   
   while (i < token)
   {
      if (source[j] == sep)
         i++;
      j++;
      if (source[j] == '\x0')
      {
         dest[0] = '\x0';
         return 0;
      }
   }
   k = 0;
   while (source[j] != sep && source[j] != '\x0')
   {
      dest[k] = source[j];
      k++;
      j++;
   }
   dest[k] = '\x0';
   if (k == 0)
      return 0;
   else
      return 1;
}
