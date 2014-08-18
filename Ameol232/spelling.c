/* SPELLING.C - The Ameol spelling checker
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

#define  SSCE_DLL_

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <commdlg.h>
#include <ctype.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <memory.h>
#include "ssce.h"
#include "amlib.h"
#include "editmap.h"

#define  THIS_FILE   __FILE__

#define  MAX_SCORES              10
#define  MAX_CUST_DICTS          10
#define  MAX_REQ_SSCEVER         2
#define  MIN_REQ_SSCEVER         0

#define  SPEC_DUPWORD            0
#define  SPEC_MISSPELLED_WORD    1

static BOOL fDefDlgEx = FALSE;

#if !defined(S16)
#define S16 signed short
#endif
#if !defined(S32)
#define S16 signed long
#endif

#ifndef MAKEWORD
#define MAKEWORD(low, high) ((WORD)(((BYTE)(low)) | (((WORD)((BYTE)(high))) << 8)))
#endif

/* Definitions that changed between v3.20 and v4.20
 */
#define SSCE_OLD_REPORT_UNCAPPED_OPT         (1L << 16)
#define SSCE_OLD_REPORT_MIXED_CASE_OPT       (1L << 17)
#define SSCE_OLD_REPORT_MIXED_DIGITS_OPT     (1L << 18)
#define SSCE_OLD_REPORT_SPELLING_OPT         (1L << 19)
#define SSCE_OLD_REPORT_DOUBLED_WORD_OPT     (1L << 20)

/* Since we can't use #defines for any of the above, we
 * use vars and set their values depending on which version
 * we're running.
 */
unsigned long nSSCEReportUncappedOpt;
unsigned long nSSCEReportMixedCaseOpt;
unsigned long nSSCEReportMixedDigitsOpt;
unsigned long nSSCEReportSpellingOpt;
unsigned long nSSCEReportDoubledWordOpt;

/* New style API (v4.20 and later) 16-bit and 32-bit.
 * Old style API (v3.20) 16-bit.
 */
S16 (WINAPI * fSSCE_OpenSession)(void);
S16 (WINAPI * fSSCE_CloseSession)(S16);
S16 (WINAPI * fSSCE_CheckWord)(S16, unsigned long, const char FAR *, char FAR *, size_t );
S16 (WINAPI * fSSCE_SuggestOld)(S16, const char FAR *, S16, char FAR *, int, S16 FAR *, S16 );
S16 (WINAPI * fSSCE_SuggestNew)(S16, const char FAR *, S16, char FAR *, S32, S16 FAR *, S16 );
S16 (WINAPI * fSSCE_CreateLex)(S16, const char FAR *, S16, S16 );
S16 (WINAPI * fSSCE_OpenLex)(S16, const char FAR *, S32 );
S16 (WINAPI * fSSCE_CloseLex)(S16, S16 );
S16 (WINAPI * fSSCE_AddToLex)(S16, S16, const char FAR *, const char FAR *);
S16 (WINAPI * fSSCE_SetOption)(S16, U32, U32);
S16 (WINAPI * fSSCE_Version)(S16 *, S16 *);

/* Old style API (v3.20) 32-bit.
 */
#ifdef WIN32
int (WINAPI * fSSCE32_OpenSession)(void);
int (WINAPI * fSSCE32_CloseSession)(int);
int (WINAPI * fSSCE32_CheckWord)(int, unsigned long, const char FAR *, char FAR *, size_t );
int (WINAPI * fSSCE32_Suggest)(int, const char FAR *, int, char FAR *, size_t, int FAR *, size_t );
int (WINAPI * fSSCE32_CreateLex)(int, const char FAR *, int, int );
int (WINAPI * fSSCE32_OpenLex)(int, const char FAR *, long );
int (WINAPI * fSSCE32_CloseLex)(int, int );
int (WINAPI * fSSCE32_AddToLex)(int, int, const char FAR *, const char FAR *);
#endif

/* Mapping functions.
 */
S16 FASTCALL A2SSCE_OpenSession( void );
S16 FASTCALL A2SSCE_CloseSession( S16 );
S16 FASTCALL A2SSCE_CheckWord(S16, unsigned long, const char FAR *, char FAR *, size_t );
S16 FASTCALL A2SSCE_Suggest(S16, const char FAR *, S16, char FAR *, S32, S16 FAR *, S16 );
S16 FASTCALL A2SSCE_CreateLex(S16, const char FAR *, S16, S16 );
S16 FASTCALL A2SSCE_OpenLex(S16, const char FAR *, S32 );
S16 FASTCALL A2SSCE_CloseLex(S16, S16 );
S16 FASTCALL A2SSCE_AddToLex(S16, S16, const char FAR *, const char FAR *);

typedef struct tagCUSTDICT {
   struct tagCUSTDICT FAR * lpNextCustDict;     /* Pointer to next custom dictionary */
   struct tagCUSTDICT FAR * lpPrevCustDict;     /* Pointer to previous custom dictionary */
   char szCustDict[ _MAX_PATH ];                /* Full pathname to custom dictionary */
   int nIndex;                                  /* Index of basename in dictionary path */
   S16 langId;                                  /* Lexicon ID */
   BOOL fEnabled;                               /* Set TRUE if dictionary enabled for use */
} CUSTDICT;

typedef CUSTDICT FAR * LPCUSTDICT;

BOOL EXPORT CALLBACK SpellCheck( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL SpellCheck_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SpellCheck_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SpellCheck_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL SpellCheck_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL SpellCheck_DefProc( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL SpellCheck_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL SpellOpts_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SpellOpts_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SpellOpts_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL SpellOpts_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL SpellOpts_OnNotify( HWND, int, LPNMHDR );

LRESULT EXPORT FAR PASCAL CustDictsListProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CustDictsListProc_OnLButtonDown( HWND, BOOL, int, int, UINT );
void FASTCALL CustDictsListProc_OnKey( HWND, UINT, BOOL, int, UINT );

LRESULT EXPORT FAR PASCAL SuggestionsListProc( HWND, UINT, WPARAM, LPARAM );

int FASTCALL DoCheck( void );
void FASTCALL DoTidyUp( BOOL );
void FASTCALL DoSuggestions( HWND, char * );
void FASTCALL SetLangType( void );
void FASTCALL GetDictPath( char * );
void FASTCALL SkipCheckedLine( void );
void FASTCALL FillCustDictionariesList( HWND );
void FASTCALL SaveSpellCheckDialogPosition( HWND );

LPCUSTDICT FASTCALL AddCustomDictionary( LPSTR, BOOL );
void FASTCALL DeleteCustomDictionary( LPCUSTDICT );
LPCUSTDICT FASTCALL GetCustomDictionary( LPSTR );
void FASTCALL GetDictionaryPath( char *, char * );
LPCUSTDICT FASTCALL CreateDefaultCustomDictionary( void );

static char * szLangTypes[] = {  "American English",  "English"         };
static char * szLangDicts[] = {  "SSCEAM",            "SSCEBR"          };

#define maxLangTypes (sizeof(szLangTypes) / sizeof(char *) )

static LPSTR lpText = NULL;                     /* Pointer to entire text being checked */
static HWND hwndCheckEdit;                      /* Handle of edit window being checked */
static HEDIT hCheckEdit;

static BOOL fAddPreQuote = FALSE; // 2.56.2055
static BOOL fAddPostQuote = FALSE; // 2.56.2055
static UINT off;                                /* Index of next word being checked */
static UINT sel;                                /* Index of next word being checked in edit control */
static BOOL fDupWord = FALSE;                   /* TRUE if found a duplicate word */
static int wordlen;                             /* Length of word being deleted/replaced */
static S16 sid = -1;                            /* Spell check session ID */
static S16 freqLexId = -1;                      /* Frequent word lexicon */
static S16 langLexId = -1;                      /* American/British lexicon */
static S16 replAllLexId = -1;                   /* Replace All lexicon */
static S16 ignAllLexId = -1;                    /* Ignore All lexicon */
static HWND hwndParent;                         /* Parent of spell checker window */
static WNDPROC lpProcListCtl ;                  /* Subclassed window list proc */
static BOOL fLineStart;                         /* TRUE if we're at the start of a line */

static WORD wSSCEVersion;                       /* Version number of SSCE library */
static long lOpenLexAll;                        /* SSCE_OpenLex */
static BOOL fUseOldStyle32;                     /* TRUE if we use 32-bit old style APIs */

static char FAR lastword[ SSCE_MAX_WORD_SZ ];   /* Last word checked */
static char FAR word[ SSCE_MAX_WORD_SZ ];       /* Current word checked */
static char FAR replWord[ SSCE_MAX_WORD_SZ ];   /* Replacement word */
static int errCondition;                        /* Error condition */
static int selStart;                            /* Index of start of selection */

static BOOL fTestForDupWords = TRUE;
static BOOL fIgnoreAllCaps = TRUE;
static BOOL fSkipQuotedLines = TRUE;

static LPCUSTDICT lpFirstCustDict;              /* Pointer to first custom dictionary */
static LPCUSTDICT lpLastCustDict;               /* Pointer to last custom dictionary */
static LPCUSTDICT lpDefaultCustDict;            /* Pointer to default custom dictionary */
static int cCustDict;                           /* Number of custom dictionaries */
static WNDPROC lpListProc;                      /* Subclassed list proc */

static char FAR szLangDict[ _MAX_FNAME ] = "SSCEBR";     /* Language lexicon */
static char FAR szLangType[ 30 ];                        /* Language type (American or British) */

void FASTCALL CheckQuotes( char * replWord ) // 2.56.2054
{
   if ( fAddPreQuote )
   {
      if( replWord[ 0 ] != '\'' )
      {
         _strrev(replWord);
         strcat(replWord, "\'");
         _strrev(replWord);
      }
   }
   if ( fAddPostQuote )
   {
      if( replWord[ strlen(replWord) - 1 ] != '\'' )
      {
         strcat(replWord, "\'");
      }
   }
}

/* This function initialises the spell checker module.
 */
void FASTCALL InitSpelling( void )
{
   if( fUseDictionary )
      {
      hSpellLib = AmLoadLibrary( szSSCELib );
      if( NULL != hSpellLib )
         {
         int maxSSCEVer = 0;
         int minSSCEVer = 0;
         register int c;

         /* Start with some version numbering stuff.
          */
         (FARPROC)fSSCE_Version = GetProcAddress( hSpellLib, "SSCE_Version" );

         /* Get the SSCE library version.
          */
         if( NULL != fSSCE_Version )
            {
            fSSCE_Version( (S16 *)&maxSSCEVer, (S16 *)&minSSCEVer );
            wSSCEVersion = MAKEWORD( minSSCEVer, maxSSCEVer );
            }

         /* If this is version 3.20, get the old style functions.
          */         
         (FARPROC)fSSCE_OpenSession = GetProcAddress( hSpellLib, "SSCE_OpenSession" );
         (FARPROC)fSSCE_OpenLex = GetProcAddress( hSpellLib, "SSCE_OpenLex" );
         (FARPROC)fSSCE_CreateLex = GetProcAddress( hSpellLib, "SSCE_CreateLex" );
         (FARPROC)fSSCE_SetOption = GetProcAddress( hSpellLib, "SSCE_SetOption" );
         (FARPROC)fSSCE_CloseLex = GetProcAddress( hSpellLib, "SSCE_CloseLex" );
         (FARPROC)fSSCE_CloseSession = GetProcAddress( hSpellLib, "SSCE_CloseSession" );
         (FARPROC)fSSCE_CheckWord = GetProcAddress( hSpellLib, "SSCE_CheckWord" );
         (FARPROC)fSSCE_SuggestOld = GetProcAddress( hSpellLib, "SSCE_Suggest" );
         (FARPROC)fSSCE_SuggestNew = GetProcAddress( hSpellLib, "SSCE_Suggest" );
         (FARPROC)fSSCE_AddToLex = GetProcAddress( hSpellLib, "SSCE_AddToLex" );
         if( fSSCE_OpenSession == NULL || fSSCE_OpenLex == NULL || fSSCE_CreateLex == NULL ||
             fSSCE_CloseLex == NULL || fSSCE_CloseSession == NULL || fSSCE_CheckWord == NULL ||
             fSSCE_SuggestOld == NULL || fSSCE_AddToLex == NULL )
            {
            if( NULL != fSSCE_Version )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR509), MAX_REQ_SSCEVER, MIN_REQ_SSCEVER, (LPSTR)szSSCELib, maxSSCEVer, minSSCEVer );
               fMessageBox( hwndActive, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               }
            else {
               wsprintf( lpTmpBuf, GS(IDS_STR510), (LPSTR)szSSCELib );
               fMessageBox( hwndActive, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               }
            FreeLibrary( hSpellLib );
            hSpellLib = NULL;
            }
         else {
            /* Other initialisation
             */
            SetLangType();
            fTestForDupWords = Amuser_GetPPInt( szSpelling, "DupWords", TRUE );
            fIgnoreAllCaps = Amuser_GetPPInt( szSpelling, "IgnoreAllCaps", TRUE );
            fSkipQuotedLines = Amuser_GetPPInt( szSpelling, "SkipQuotedLines", TRUE );
            fAutoSpellCheck = Amuser_GetPPInt( szSpelling, "AutoCheck", TRUE );

            /* Set the old-style 32-bit functions.
             */
         #ifdef WIN32
            (FARPROC)fSSCE32_OpenSession = (FARPROC)fSSCE_OpenSession;
            (FARPROC)fSSCE32_OpenLex = (FARPROC)fSSCE_OpenLex;
            (FARPROC)fSSCE32_CreateLex = (FARPROC)fSSCE_CreateLex;
            (FARPROC)fSSCE32_CloseLex = (FARPROC)fSSCE_CloseLex;
            (FARPROC)fSSCE32_CloseSession = (FARPROC)fSSCE_CloseSession;
            (FARPROC)fSSCE32_CheckWord = (FARPROC)fSSCE_CheckWord;
            (FARPROC)fSSCE32_Suggest = (FARPROC)fSSCE_SuggestOld;
            (FARPROC)fSSCE32_AddToLex = (FARPROC)fSSCE_AddToLex;
         #endif

            /* Set parameter for OpenLex depending on version
             * number.
             */
            lOpenLexAll = ( wSSCEVersion <= 0x0302 ) ? -1 : 0;
         #ifdef WIN32
            fUseOldStyle32 = wSSCEVersion <= 0x0302;
         #endif

            /* Set options that changed between DLLs.
             */
            if( wSSCEVersion <= 0x302 )
               {
               nSSCEReportUncappedOpt = SSCE_OLD_REPORT_UNCAPPED_OPT;
               nSSCEReportMixedCaseOpt = SSCE_OLD_REPORT_MIXED_CASE_OPT;
               nSSCEReportMixedDigitsOpt = SSCE_OLD_REPORT_MIXED_DIGITS_OPT;
               nSSCEReportSpellingOpt = SSCE_OLD_REPORT_SPELLING_OPT;
               nSSCEReportDoubledWordOpt = SSCE_OLD_REPORT_DOUBLED_WORD_OPT;
               }
            else
               {
               nSSCEReportUncappedOpt = SSCE_REPORT_UNCAPPED_OPT;
               nSSCEReportMixedCaseOpt = SSCE_REPORT_MIXED_CASE_OPT;
               nSSCEReportMixedDigitsOpt = SSCE_REPORT_MIXED_DIGITS_OPT;
               nSSCEReportSpellingOpt = SSCE_REPORT_SPELLING_OPT;
               nSSCEReportDoubledWordOpt = SSCE_REPORT_DOUBLED_WORD_OPT;
               }

            /* Load list of custom dictionaries
             */
            BEGIN_PATH_BUF;
            lpFirstCustDict = NULL;
            lpLastCustDict = NULL;
            for( c = cCustDict = 0; c < MAX_CUST_DICTS; ++c )
               {
               char sz[ 7 ];
               char * pEnabled;
               BOOL fEnabled = TRUE;
               
               wsprintf( sz, "Dict%d", c + 1 );
               if( !Amuser_GetPPString( szSpelling, sz, "", lpPathBuf, _MAX_PATH ) )
                  break;
               if( NULL != ( pEnabled = strchr( lpPathBuf, ',' ) ) )
                  {
                  *pEnabled++ = '\0';
                  fEnabled = atoi( pEnabled );
                  }
               AddCustomDictionary( lpPathBuf, fEnabled );
               }
            END_PATH_BUF;

            /* Get the default dictionary name. If none exists, use
             * the first one and update the config file.
             */
            Amuser_GetPPString( szSpelling, "Default", "", lpPathBuf, _MAX_PATH );
            if( *lpPathBuf )
            {
               lpDefaultCustDict = GetCustomDictionary( lpPathBuf );
               if( NULL == lpDefaultCustDict && NULL != lpFirstCustDict )
                  {
                  lpDefaultCustDict = lpFirstCustDict;
                  Amuser_WritePPString( szSpelling, "Default", lpFirstCustDict->szCustDict );
                  }
            }
            else if( NULL != lpFirstCustDict )
               {
               lpDefaultCustDict = lpFirstCustDict;
               Amuser_WritePPString( szSpelling, "Default", lpFirstCustDict->szCustDict );
               }
            }
         }
      else if( FindLibrary( "sscewd.dll" ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR503), szSSCELib );
         fMessageBox( hwndActive, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   else
      hSpellLib = NULL;
}

void FASTCALL GetDictionaryPath( char * pszPath, char * pszLangDict )
{
   wsprintf( pszPath, "%s%s%sM.CLX", (LPSTR)pszAmeolDir, (LPSTR)"DICT\\", (LPSTR)pszLangDict );
}

void FASTCALL LoadDictionaries( void )
{
   char sz[ 100 ];

   /* Tell the user what we're doing
    */
   if( hwndStatus )
      OfflineStatusText( GS(IDS_STR542) );

   /* Initialise spelling checker and load main dictionaries
    */
   if( ( sid = A2SSCE_OpenSession() ) < 0 )
   {
      fMessageBox( NULL, 0, GS(IDS_STR504), MB_OK|MB_ICONEXCLAMATION );
      FreeLibrary( hSpellLib );
      hSpellLib = NULL;
   }
   else 
   {
      GetDictionaryPath( lpTmpBuf, szLangDict );
      if( ( langLexId = A2SSCE_OpenLex( sid, lpTmpBuf, lOpenLexAll ) ) < 0 )
      {
         A2SSCE_CloseSession( sid );
         sid = -1;
         wsprintf( sz, GS(IDS_STR505), (LPSTR)lpTmpBuf );
         fMessageBox( NULL, 0, sz, MB_OK|MB_ICONEXCLAMATION );
         FreeLibrary( hSpellLib );
         hSpellLib = NULL;
      }
      else 
      {
         wsprintf( lpTmpBuf, "%s%s%sC.TLX", (LPSTR)pszAmeolDir, (LPSTR)"DICT\\", (LPSTR)szLangDict );
         if( ( freqLexId = A2SSCE_OpenLex( sid, lpTmpBuf, lOpenLexAll ) ) < 0 )
         {
            A2SSCE_CloseLex( sid, langLexId );
            langLexId = -1;
            A2SSCE_CloseSession( sid );
            sid = -1;
            wsprintf( sz, GS(IDS_STR505), (LPSTR)lpTmpBuf );
            fMessageBox( NULL, 0, sz, MB_OK|MB_ICONEXCLAMATION );
            FreeLibrary( hSpellLib );
            hSpellLib = NULL;
         }
      }
   }
   if( hwndStatus )
      OfflineStatusText( "" );
}

/* This function terminates the spell checker module.
 */
void FASTCALL TermSpelling( void )
{
   if( fUseDictionary )
   {
      if( hSpellLib )   
      {
         LPCUSTDICT lpCustDict;
         register int c;
         char sz[ 7 ];
      
         if( freqLexId >= 0 ) 
         {
            A2SSCE_CloseLex( sid, freqLexId );
            freqLexId = -1;
         }
         if( langLexId >= 0 ) 
         {
            A2SSCE_CloseLex( sid, langLexId );
            langLexId = -1;
         }
         if( sid >= 0 ) 
         {
            A2SSCE_CloseSession( sid );
            sid = -1;
         }
         for( c = 0, lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict, ++c )
         {
            BEGIN_PATH_BUF;
            wsprintf( sz, "Dict%d", c + 1 );
            wsprintf( lpPathBuf, "%s,%d", lpCustDict->szCustDict, lpCustDict->fEnabled );
            Amuser_WritePPString( szSpelling, sz, lpPathBuf );
            END_PATH_BUF;
         }
         while( c < MAX_CUST_DICTS ) 
         {
            wsprintf( sz, "Dict%d", c + 1 );
            Amuser_WritePPString( szSpelling, sz, NULL );
            ++c;
         }
         Amuser_WritePPInt( szSpelling, "DupWords", fTestForDupWords );
         Amuser_WritePPInt( szSpelling, "IgnoreAllCaps", fIgnoreAllCaps );
         Amuser_WritePPInt( szSpelling, "SkipQuotedLines", fSkipQuotedLines );
         FreeLibrary( hSpellLib );
         hSpellLib = NULL;
      }
   }
}

/* This function sets the name of the language specific dictionary
 *  to use.
 */
void FASTCALL SetLangType( void )
{
   register int c;

   Amuser_GetPPString( szSpelling, "LangType", szLangTypes[ 1 ], szLangType, sizeof( szLangType ) );
   for( c = 0; c < maxLangTypes; ++c )
   {
      if( _strcmpi( szLangTypes[ c ], szLangType ) == 0 )
      {
         strcpy( szLangDict, szLangDicts[ c ] );
         break;
      }
   }
}

/* This function performs spell checking on the active edit window.
 */
int WINAPI EXPORT Ameol2_SpellCheckDocument( HWND hwnd, HWND hwndEdit, UINT wFlags )
{
   UINT cbText;
   char sz[ 100 ];
   int spReturn = SP_OK;
   LPCUSTDICT lpCustDict;

   /* Make sure we initialised OK.
    */
   if( !hSpellLib || fQuitting )
      return( SP_FINISH );

   if( langLexId == -1 )
      LoadDictionaries();

   /* Create two temporary lexicons. One for a list of all words to ignore,
    * and another for all words to replace.
    */
   if( !( wFlags & SC_FL_USELASTSESSION ) )
   {
      if( ( ignAllLexId = A2SSCE_CreateLex( sid, NULL, SSCE_IGNORE_LEX_TYPE, SSCE_BRIT_ENGLISH_LANG  ) ) < 0 )
      {
         DoTidyUp( TRUE );
         fMessageBox( hwnd, 0, GS(IDS_STR520), MB_OK|MB_ICONEXCLAMATION );
         spReturn = SP_CANCEL;
      }
      if( spReturn == SP_OK && ( replAllLexId = A2SSCE_CreateLex( sid, NULL, SSCE_CHANGE_LEX_TYPE, SSCE_BRIT_ENGLISH_LANG ) ) < 0 )
      {
         DoTidyUp( TRUE );
         fMessageBox( hwnd, 0, GS(IDS_STR520), MB_OK|MB_ICONEXCLAMATION );
         spReturn = SP_CANCEL;
      }
   }

   /* Finally open all the custom dictionaries.
    */
   for( lpCustDict = lpFirstCustDict; spReturn != SP_CANCEL && lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
   {
      if( lpCustDict->fEnabled )
      {
         LPSTR lpDictDir;

         BEGIN_PATH_BUF;
         if( lpCustDict->nIndex == 0 )
         {
            lpDictDir = lpPathBuf;
            GetDictPath( lpPathBuf );
            strcat( lpPathBuf, "\\" );
            lstrcat( lpPathBuf, lpCustDict->szCustDict );
         }
         else
            lpDictDir = lpCustDict->szCustDict;
         if( ( lpCustDict->langId = A2SSCE_OpenLex( sid, lpDictDir, lOpenLexAll ) ) < 0 )
         {
            DoTidyUp( TRUE );
            wsprintf( sz, GS(IDS_STR505), lpCustDict->szCustDict );
            fMessageBox( hwnd, 0, sz, MB_OK|MB_ICONEXCLAMATION );
            spReturn = SP_CANCEL;
         }
         END_PATH_BUF;
      }
   }
   /* Set options
    */
   if( NULL != fSSCE_SetOption )
      fSSCE_SetOption( sid, SSCE_SUGGEST_SPLIT_WORDS_OPT, SSCE_SUGGEST_SPLIT_WORDS_OPT );

   Amuser_GetPPString( szSettings, "ExpandedQuoteMailHeader", "", szExpandedQuoteMailHeader, sizeof(szQuoteMailHeader) );
   Amuser_GetPPString( szSettings, "ExpandedQuoteNewsHeader", "", szExpandedQuoteNewsHeader, sizeof(szQuoteMailHeader) );

   /* Start the spell check. Get the block of text from the current edit window
    * and break it into words.
    */
   if( spReturn != SP_CANCEL )
   {
      hwndCheckEdit = hwndEdit;
      hwndParent = hwnd;
      hCheckEdit = Editmap_Open( hwndCheckEdit );
      cbText = Editmap_GetTextLength( hCheckEdit, hwndCheckEdit );
      if( fNewMemory( &lpText, cbText + 1 ) )
      {
         BOOL fCheckingSelection;
         BEC_SELECTION dwSel;
         BOOL fDone;
   
         fDone = FALSE;
         fCheckingSelection = FALSE;
         while( !fDone )
         {
            off = sel = 0;
            fDupWord = FALSE;
            fLineStart = TRUE;
            
            Editmap_GetText( hCheckEdit, hwndCheckEdit, cbText + 1, lpText );
            
            Editmap_GetSelection(hCheckEdit, hwndCheckEdit, &dwSel);
            if( dwSel.lo != dwSel.hi && !fCheckingSelection )
            {
               UINT lenSel;
      
               lenSel =  dwSel.hi - dwSel.lo;
               if( dwSel.lo )
                  {
                  if( lpText[ (int)dwSel.lo - 1 ] == '\n' )
                     fLineStart = TRUE;
                  _fmemcpy( lpText, lpText + dwSel.lo, lenSel );
                  }
               lpText[ lenSel ] = '\0';
               sel = dwSel.lo;
               fCheckingSelection = TRUE;
            }
            else
               fCheckingSelection = FALSE;
            if( ( spReturn = DoCheck() ) == SP_CANCEL )
               spReturn = DialogBox( hRscLib, MAKEINTRESOURCE( IDDLG_SPELLCHECK ), hwnd, (DLGPROC)SpellCheck );
            fDone = TRUE;
            Editmap_SetSel(hCheckEdit, hwndCheckEdit, &dwSel );
            if( spReturn == SP_OK && ( wFlags & SC_FL_DIALOG ) )
            {
               if( !fCheckingSelection )
                  fMessageBox( hwnd, 0, GS(IDS_STR507), MB_OK|MB_ICONEXCLAMATION );
               else
               {
                  /* If checking selection, offer to check the rest of the
                   * document.
                   */
                  if( IDYES == fMessageBox( hwnd, 0, GS(IDS_STR502), MB_YESNO|MB_ICONEXCLAMATION ) )
                     fDone = FALSE;
               }
            }
         }
         if( spReturn != SP_OK )
            wFlags &= ~SC_FL_KEEPSESSION;
         DoTidyUp( ( wFlags & SC_FL_KEEPSESSION ) ? FALSE : TRUE );
      }
      else
      {
         DoTidyUp( TRUE );
         fMessageBox( hwnd, 0, GS(IDS_STR506), MB_OK|MB_ICONEXCLAMATION );
         spReturn = SP_CANCEL;
      }
   }

   /* Tidy up at the end.
    */
   if( spReturn == SP_CANCEL )
      SetFocus( hwndEdit );
   return( spReturn );
}

int FASTCALL DoSpellingPopup( HWND hwndEdit, HMENU hPopupMenu )
{
   UINT cbText;
   LPCUSTDICT lpCustDict;
   int count = 0;
   HMENU hSpellMenu;
   register int c;
   BOOL fOk = TRUE;

   /* Make sure we initialised OK.
    */
   if( !hSpellLib )
      return( FALSE );
   if( langLexId == -1 )
      LoadDictionaries();

   /* Clear out the spell popup menu.
    */
   count = GetMenuItemCount( hPopupMenu );
   hSpellMenu = GetSubMenu( hPopupMenu, count - 5 );
   count = GetMenuItemCount( hSpellMenu );
   for( c = count - 1; c  >= 0; --c )
      DeleteMenu( hSpellMenu, c, MF_BYPOSITION );

   /* Finally open all the custom dictionaries.
    */
   for( lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
   {
      if( lpCustDict->fEnabled )
      {
         LPSTR lpDictDir;

         BEGIN_PATH_BUF;
         if( lpCustDict->nIndex == 0 )
         {
            lpDictDir = lpPathBuf;
            GetDictPath( lpPathBuf );
            strcat( lpPathBuf, "\\" );
            lstrcat( lpPathBuf, lpCustDict->szCustDict );
         }
         else
            lpDictDir = lpCustDict->szCustDict;
         if( ( lpCustDict->langId = A2SSCE_OpenLex( sid, lpDictDir, lOpenLexAll ) ) < 0 )
         {
            DoTidyUp( TRUE );
            fOk = FALSE;
         }
         END_PATH_BUF;
      }
   }
   /* Start the spell check. Get the selected text from the edit window.
    */
   if( fOk )
   {
      fOk = FALSE;
      cbText = Edit_GetTextLength( hwndEdit );
      if( fNewMemory( &lpText, cbText + 1 ) )
      {
         DWORD dwSel;
         WORD wStart;
         WORD wEnd;
   
         Edit_GetText( hwndEdit, lpText, cbText + 1 );
         dwSel = Edit_GetSel( hwndEdit );
         wStart = LOWORD( dwSel );
         wEnd = HIWORD( dwSel );
         if( wStart != wEnd )
         {
            S16 scores[ MAX_SCORES ];
            char * pRepWord;
            register int d;
            UINT lenSel;

            lenSel =  wEnd - wStart;
            if( wStart )
               _fmemcpy( lpText, lpText + wStart, lenSel );
            lpText[ lenSel ] = '\0';
            wSpellSelStart = wStart;
            for( c = 0; lpText[ c ] && !IsCharAlpha( lpText[ c ] ) && lpText[ c ] != '\''; )
            {
               ++c;
               ++wSpellSelStart;
            }
            for( d = c, wSpellSelEnd = wSpellSelStart; IsCharAlpha( lpText[ d ] ) || lpText[ d ] == '\''; )
            {
               ++d;
               ++wSpellSelEnd;
            }
            lpText[ d ] = '\0';
            if( *lpText )
            {
               A2SSCE_Suggest( sid, lpText + c, SSCE_AUTO_SEARCH_DEPTH, lpTmpBuf, LEN_TEMPBUF, scores, MAX_SCORES );
               if( *lpTmpBuf )
               {
                  for( c = 0, pRepWord = lpTmpBuf; scores[ c ] >= 60 && *pRepWord; pRepWord += strlen( pRepWord ) + 1, ++c )
                     AppendMenu( hSpellMenu, MF_STRING, IDM_SPELLWORD + c, pRepWord );
                  fOk = TRUE;
               }
            }
         }
      }
      DoTidyUp( TRUE );
   }
   if( !fOk )
   {
      AppendMenu( hSpellMenu, MF_STRING, IDM_SPELLWORD, GS(IDS_STR611) );
      MenuEnable( hSpellMenu, IDM_SPELLWORD, FALSE );
   }
   return( fOk );
}

void FASTCALL DoTidyUp( BOOL fUnloadDict )
{
   LPCUSTDICT lpCustDict;

   for( lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
      if( lpCustDict->langId >= 0 )
         {
         A2SSCE_CloseLex( sid, lpCustDict->langId );
         lpCustDict->langId = -1;
         }
   if( lpText ) {
      FreeMemory( &lpText );
      lpText = NULL;
      }
   if( fUnloadDict )
      {
      if( ignAllLexId >= 0 ) {
         A2SSCE_CloseLex( sid, ignAllLexId );
         ignAllLexId = -1;
         }
      if( replAllLexId >= 0 ) {
         A2SSCE_CloseLex( sid, replAllLexId );
         replAllLexId = -1;
         }
      }
}

/* Checks the spelling of the current edit block.
 */
int FASTCALL DoCheck()
{
   BOOL fSkipLines;
   BOOL fJustSkipped = FALSE;

   lastword[ 0 ] = '\0';
   fSkipLines = FALSE;
   while( lpText[ off ] )
      {
      int checkResult;
      register int n;
      BOOL fAllCaps;
      BOOL fSpaces;

      /* If we're at the end of a line, skip it.
       */
      if( lpText[ off ] == 13 )
         {
         ++off;
         ++sel;
         if( lpText[ off ] == 10 )
            {
            ++off;
            ++sel;
            }
         fLineStart = TRUE;
         }

      /* If start of line, look for PGP stuff and if found,
       * skip everything in between.
       */
      if( fLineStart )
         {
         if( strncmp( &lpText[ off ], "-----BEGIN PGP ", 15 ) == 0 )
            fSkipLines = TRUE;
         if( strncmp( &lpText[ off ], "-----END PGP ", 13 ) == 0 )
            {
            SkipCheckedLine();
            fSkipLines = FALSE;
            continue;
            }
         }

      /* If this line begins a quote and we skip quoted lines, then
       * skip to the end of the line.
       */
      if( fSkipQuotedLines && fLineStart && lpText[ off ] == '>' )
         {
         SkipCheckedLine();
         fJustSkipped = TRUE;
         continue;
         }

      if( fLineStart )
      {
      if( strncmp( &lpText[ off ], szExpandedQuoteMailHeader, 15 ) == 0 )
      {
         SkipCheckedLine();
         SkipCheckedLine();
         continue;
      }
      if( strncmp( &lpText[ off ], szExpandedQuoteNewsHeader, 15 ) == 0 )
      {
         SkipCheckedLine();
         SkipCheckedLine();
         continue;
      }

      }

      /* If we're skipping lines, continue.
       */
      if( fSkipLines )
         {
         SkipCheckedLine();
         continue;
         }
      fLineStart = FALSE;
      fSpaces = TRUE;
      while( lpText[ off ] && !IsCharAlpha( lpText[ off ] ) && lpText[ off ] != '\'' )
         {
         if( lpText[ off ] == 13 )
         {
            fJustSkipped = FALSE;
            break;
         }
         if( !isspace( lpText[ off ] ) )
            fSpaces = FALSE;
         ++off;
         ++sel;
         }
      if( lpText[ off ] == 13 )
         continue;
      if( !fSpaces )
         lastword[ 0 ] = '\0';
      selStart = sel;
      fAllCaps = TRUE;
      for( n = 0; lpText[ off ] != '\0' && ( IsCharAlpha( lpText[ off ] ) || lpText[ off ] == '\'') && n < SSCE_MAX_WORD_SZ - 1; ++n )
         {
         if( !IsCharUpper( lpText[ off ] ) && lpText[ off ] != 's' )
            fAllCaps = FALSE;
         word[ n ] = lpText[ off++ ];
         ++sel;
         }
      if( fIgnoreAllCaps && fAllCaps )
         n = 0;
      word[ n ] = '\0';
      fAddPostQuote = FALSE;
      fAddPreQuote = FALSE;
      if( word[ 0 ] == '\'' )    // 2.56.2055
      {
         StripLeadingChars( word, '\'' );
         fAddPreQuote = TRUE;
         n--;
      }
      if( word[ n - 1 ] == '\'' ) // 2.56.2055
      {
         StripTrailingChars( word, '\'' );
         fAddPostQuote = TRUE;
      }
      if( ( wordlen = strlen( word ) ) > 1 )
      {
         checkResult = A2SSCE_CheckWord( sid,
                                 nSSCEReportSpellingOpt | nSSCEReportMixedCaseOpt |
                                 nSSCEReportMixedDigitsOpt,
                                 word, replWord, sizeof(replWord) );
         if( checkResult & SSCE_CHANGE_WORD_RSLT )
            {
            BEC_SELECTION dwSel;
//          Edit_SetSel( hwndCheckEdit, selStart, selStart + ( sel - selStart ) );
//          Edit_ScrollCaret( hwndCheckEdit );
//          Edit_ReplaceSel( hwndCheckEdit, replWord );
            
//          dwSel.lo = LOWORD(sel);                   // !!SM!! 2045
//          dwSel.hi = HIWORD(sel);                   // !!SM!! 2045
            dwSel.lo = selStart;                   // !!SM!! 2045
            dwSel.hi = selStart + ( sel - selStart );    // !!SM!! 2045
            Editmap_SetSel( hCheckEdit, hwndCheckEdit, &dwSel );
            Edit_ScrollCaret( hwndCheckEdit );
            CheckQuotes( (char *)&replWord ); // 2.56.2054
            Editmap_ReplaceSel( hCheckEdit, hwndCheckEdit, replWord );
            if (fAddPreQuote || fAddPostQuote) // 2.56.2055
            {
               sel += strlen( replWord ) - wordlen;
               if (fAddPreQuote)
                  sel--;
               if (fAddPostQuote)
                  sel--;
            }
            else
               sel += strlen( replWord ) - wordlen;
            }
         if( fTestForDupWords && strcmp( word, lastword ) == 0 && !fJustSkipped )
            {
            errCondition = SPEC_DUPWORD;
            return( SP_CANCEL );
            }
         if( checkResult & SSCE_MISSPELLED_WORD_RSLT )
            {
            errCondition = SPEC_MISSPELLED_WORD;
            return( SP_CANCEL );
            }
         }
      strcpy( lastword, word );
      }
   return( SP_OK );
}

void FASTCALL SkipCheckedLine( void )
{
   while( lpText[ off ] && lpText[ off ] != 13 )
      {
      ++off;
      ++sel;
      }
   if( lpText[ off ] == 13 )
      {
      ++off;
      ++sel;
      if( lpText[ off ] == 10 )
         {
         ++off;
         ++sel;
         }
      }
}

void FASTCALL DisplayErrorStatus( HWND hwnd )
{
   HWND hwndBadWord;

   switch( errCondition )
      {
      case SPEC_DUPWORD:
         {
         BEC_SELECTION dwSel;
         if( !fDupWord )
            {
            fDupWord = TRUE;
            SetDlgItemText( hwnd, IDD_CHANGE, GS(IDS_STR607) );
            SetDlgItemText( hwnd, IDD_PAD2, GS(IDS_STR608) );
            EnableControl( hwnd, IDD_CHANGEALL, FALSE );
            ListBox_ResetContent( GetDlgItem( hwnd, IDD_LIST ) );
            SetDlgItemText( hwnd, IDD_CHANGETO, "" );
            PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDD_IGNORE ), (LPARAM)TRUE );
            }
         hwndBadWord = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_ResetContent( hwndBadWord );
         ListBox_AddString( hwndBadWord, word );
//       Edit_SetSel( hwndCheckEdit, selStart, selStart + ( sel - selStart ) );
         
         dwSel.lo = selStart;
         dwSel.hi = selStart + ( sel - selStart );

         Editmap_SetSel( hCheckEdit, hwndCheckEdit, &dwSel );
         Edit_ScrollCaret( hwndCheckEdit );
         MakeEditSelectionVisible( hwnd, hwndCheckEdit );
         SendMessage( hwnd, DM_SETDEFID, IDD_CHANGE, 0L );
         break;
         }
      case SPEC_MISSPELLED_WORD:
         {
         BEC_SELECTION dwSel;
         if( fDupWord )
            {
            fDupWord = FALSE;
            SetDlgItemText( hwnd, IDD_CHANGE, GS(IDS_STR609) );
            SetDlgItemText( hwnd, IDD_PAD2, GS(IDS_STR610) );
            EnableControl( hwnd, IDD_CHANGEALL, TRUE );
            PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDD_CHANGE ), (LPARAM)TRUE );
            }
         hwndBadWord = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_ResetContent( hwndBadWord );
         ListBox_AddString( hwndBadWord, word );
         DoSuggestions( hwnd, word );
//       Edit_SetSel( hwndCheckEdit, selStart, selStart + ( sel - selStart ) );
         dwSel.lo = selStart;
         dwSel.hi = selStart + ( sel - selStart );
         if (fAddPreQuote) // 2.56.2055
            dwSel.lo++;
         if (fAddPostQuote) // 2.56.2055
            dwSel.hi--;
         Editmap_SetSel( hCheckEdit, hwndCheckEdit, &dwSel );
         Edit_ScrollCaret( hwndCheckEdit );
         MakeEditSelectionVisible( hwnd, hwndCheckEdit );
         SendMessage( hwnd, DM_SETDEFID, IDD_IGNORE, 0L );
         break;
         }
      }
}

void FASTCALL DoSuggestions( HWND hwnd, char * pWord )
{
   HWND hwndList;
   S16 scores[ MAX_SCORES ];
   int count = 0;
   char * pRepWord;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   SetWindowRedraw( hwndList, FALSE );
   ListBox_ResetContent( hwndList );
   A2SSCE_Suggest( sid, pWord, SSCE_AUTO_SEARCH_DEPTH, lpTmpBuf, LEN_TEMPBUF, scores, MAX_SCORES );
   if( *lpTmpBuf )
      {
      HWND hwndEdit;
      char word[ SSCE_MAX_WORD_SZ ];
      register int c;

      for( c = 0, pRepWord = lpTmpBuf; scores[ c ] >= 60 && *pRepWord; pRepWord += strlen( pRepWord ) + 1, ++c )
         {
         ListBox_InsertString( hwndList, -1, pRepWord );
         ++count;
         }
      if( count ) {
         ListBox_SetCurSel( hwndList, 0 );

         ListBox_GetText( hwndList, 0, word );
         hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
         Edit_SetText( hwndEdit, word );
         Edit_SetSel( hwndEdit, 0, 32767 );
   
         SetFocus( hwndEdit );
         }
      }
   if( count == 0 )
      {
      HWND hwndEdit;

      ListBox_AddString( hwndList, GS(IDS_STR611) );
      hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
      Edit_SetText( hwndEdit, "" );
      EnableControl( hwnd, IDD_CHANGE, FALSE );
      EnableControl( hwnd, IDD_CHANGEALL, FALSE );
      SetFocus( hwndEdit );
      }
   SetWindowRedraw( hwndList, TRUE );
}

/* This function dispatches messages for the spell checker.
 */
BOOL EXPORT CALLBACK SpellCheck( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SpellCheck_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SpellCheck_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, SpellCheck_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, SpellCheck_OnMeasureItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSPELLCHECK );
         break;
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SpellCheck_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;
   register int x, y;
   RECT rcMDI;
   RECT rcDef;

   GetClientRect( GetDesktopWindow(), &rcMDI );
   rcDef.left = rcMDI.right / 2;
   rcDef.top = rcMDI.bottom / 2;
   x = Amuser_GetPPInt( szSettings, "SpellCheckDialogX", 0 );
   y = Amuser_GetPPInt( szSettings, "SpellCheckDialogY", 0 );
   if( ( ( x + 405 ) > rcMDI.right ) || x < 0 )
      x = rcDef.left;
   if( ( ( y + 217 ) > rcMDI.bottom ) || y < 0 )
      y = rcDef.top;
   if( x > 0 && y > 0 )
      SetWindowPos( hwnd, NULL, x, y, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );
   FillCustDictionariesList( hwnd );
   hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
   lpProcListCtl = SubclassWindow( hwndEdit, SuggestionsListProc );
   DisplayErrorStatus( hwnd );
   return( TRUE );
}

void FASTCALL FillCustDictionariesList( HWND hwnd )
{
   HWND hwndList;
   LPCUSTDICT lpCustDict;
   int cDicts;
   int index;

   hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
   ComboBox_ResetContent( hwndList );
   SetWindowRedraw( hwndList, FALSE );
   for( cDicts = 0, lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
      if( lpCustDict->fEnabled )
         {
         ComboBox_InsertString( hwndList, -1, lpCustDict );
         ++cDicts;
         }
   SetWindowRedraw( hwndList, TRUE );
   if( cDicts )
      if( lpDefaultCustDict->szCustDict )
      {
         if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)lpDefaultCustDict ) ) )
               ComboBox_SetCurSel( hwndList, index );
      }
      else
         ComboBox_SetCurSel( hwndList, 0 );
   EnableControl( hwnd, IDD_PAD1, cDicts > 0 );
   EnableControl( hwnd, IDD_CUSTDICTS, cDicts > 0 );
}

LRESULT EXPORT FAR PASCAL SuggestionsListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   if( msg == WM_KEYDOWN )
      switch( wParam )
         {
         case VK_DOWN:
         case VK_UP: {
            HWND hwndParent = GetParent( hwnd );
            HWND hwndList = GetDlgItem( hwndParent, IDD_LIST );

            SetFocus( hwndList );
            PostMessage( hwndList, msg, wParam, lParam );
            break;
            }
         }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL SpellCheck_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_CUSTDICTS:
         if( lpDrawItem->itemAction == ODA_FOCUS )
            DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
         else if( lpDrawItem->itemID != -1 ) {
            LPCUSTDICT lpCustDict;
            COLORREF tmpT;
            COLORREF tmpB;
            COLORREF T;
            COLORREF B;
            HBRUSH hbr;
            BOOL fDefault;
            HFONT hOldFont;
            RECT rc;
   
            /* Get dictionary handle and set fDefault if this is
             * the default dictionary.
             */
            lpCustDict = (LPCUSTDICT)ComboBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
            fDefault = lpCustDict == lpDefaultCustDict;
            rc = lpDrawItem->rcItem;

            /* Get the foreground and background colour.
             */
            GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
            hbr = CreateSolidBrush( B );
   
            /* Erase the entire drawing area
             */
            FillRect( lpDrawItem->hDC, &rc, hbr );
            tmpT = SetTextColor( lpDrawItem->hDC, T );
            tmpB = SetBkColor( lpDrawItem->hDC, B );
   
            /* Display the dictionary name
             */
            hOldFont = SelectFont( lpDrawItem->hDC, fDefault ? hSys8Font : hHelv8Font );
            rc.left = 2;
            DrawText( lpDrawItem->hDC, lpCustDict->szCustDict + lpCustDict->nIndex, -1, &rc, DT_VCENTER|DT_LEFT|DT_SINGLELINE|DT_NOPREFIX );
   
            /* Restore things back to normal.
             */
            SelectFont( lpDrawItem->hDC, hOldFont );
            SetTextColor( lpDrawItem->hDC, tmpT );
            SetBkColor( lpDrawItem->hDC, tmpB );
            DeleteBrush( hbr );
            }
         break;

      case IDD_BADWORD:
         if( lpDrawItem->itemAction == ODA_FOCUS )
            DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
         else if( lpDrawItem->itemID != -1 ) {
            char sz[ 100 ];
            COLORREF tmpT;
            COLORREF tmpB;
            COLORREF T;
            COLORREF B;
            HBRUSH hbr;
            HFONT hOldFont;
            RECT rc;

            ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
            rc = lpDrawItem->rcItem;

            /* Get the foreground and background colour.
             */
            GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
            hbr = CreateSolidBrush( B );
   
            /* Erase the entire drawing area
             */
            FillRect( lpDrawItem->hDC, &rc, hbr );
            tmpT = SetTextColor( lpDrawItem->hDC, T );
            tmpB = SetBkColor( lpDrawItem->hDC, B );
   
            /* Display the misspelt word
             */
            hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
            rc.left = 1;
            DrawText( lpDrawItem->hDC, sz, -1, &rc, DT_VCENTER|DT_LEFT|DT_SINGLELINE|DT_NOPREFIX );
   
            /* Restore things back to normal.
             */
            SelectFont( lpDrawItem->hDC, hOldFont );
            SetTextColor( lpDrawItem->hDC, tmpT );
            SetBkColor( lpDrawItem->hDC, tmpB );
            DeleteBrush( hbr );
            }
         break;
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL SpellCheck_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   switch( lpMeasureItem->CtlID )
      {
      case IDD_CUSTDICTS: {
         TEXTMETRIC tm;
         HFONT hOldFont;
         HDC hdc;
      
         hdc = GetDC( hwnd );
         hOldFont = SelectFont( hdc, hHelvB8Font );
         GetTextMetrics( hdc, &tm );
         lpMeasureItem->itemHeight = tm.tmHeight;
         SelectFont( hdc, hOldFont );
         ReleaseDC( hwnd, hdc );
         break;
         }

      case IDD_BADWORD: {
         RECT rc;

         GetClientRect( GetDlgItem( hwnd, IDD_BADWORD ), &rc );
         lpMeasureItem->itemHeight = rc.bottom + 2;
         break;
         }
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SpellCheck_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   static BOOL fChangingSel = FALSE;

   switch( id )
      {
      case IDD_OPTIONS:
         if( PreferencesDialog( hwnd, ODU_SPELLING ) )
            FillCustDictionariesList( hwnd );
         break;

      case IDD_ADD: {
         char word[ SSCE_MAX_WORD_SZ ];
         char * pWord;
         HWND hwndEdit;
         HWND hwndList;
         int index;

         /* Get the word we're going to add.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_GetText( hwndEdit, 0, pWord = word );

         /* Get the custom dictionary. Offer to create one if
          * none are available.
          */
         hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
         if( ( index = ComboBox_GetCurSel( hwndList ) ) == CB_ERR )
            {
            LPCUSTDICT lpCustDict;

            if( IDNO == fMessageBox( hwnd, 0, GS(IDS_STR1147), MB_YESNO|MB_ICONQUESTION ) )
               break;
            if( NULL == ( lpCustDict = CreateDefaultCustomDictionary() ) )
               break;
            index = ComboBox_InsertString( hwndList, -1, lpCustDict );
            ComboBox_SetCurSel( hwndList, index );
            lpDefaultCustDict = lpCustDict;
            Amuser_WritePPString( szSpelling, "Default", lpDefaultCustDict->szCustDict );
            EnableControl( hwnd, IDD_PAD1, TRUE );
            EnableControl( hwnd, IDD_CUSTDICTS, TRUE );
            }

         /* Okay, save the word to the lexicon.
          */
         if( index != CB_ERR )
            {
            LPCUSTDICT lpCustDict;

            lpCustDict = (LPCUSTDICT)ComboBox_GetItemData( hwndList, index );
            if( *pWord== '\'' )
               ++pWord;
            if( pWord[ strlen( pWord ) - 1 ] == '\'' )
               pWord[ strlen( pWord ) - 1 ] = '\0';
            A2SSCE_AddToLex( sid, lpCustDict->langId, pWord, NULL );
            }
         goto I1;
         }

      case IDD_CHANGETO:
         if( codeNotify == EN_CHANGE && !fChangingSel )
            {
            if( fDupWord )
               if( Edit_GetTextLength( hwndCtl ) == 0 )
                  SetDlgItemText( hwnd, IDD_CHANGE, GS(IDS_STR607) );
               else
                  SetDlgItemText( hwnd, IDD_CHANGE, GS(IDS_STR609) );
            EnableControl( hwnd, IDD_CHANGE, TRUE );
            EnableControl( hwnd, IDD_CHANGEALL, TRUE );
            EnableControl( hwnd, IDD_SUGGEST, TRUE );
            SendMessage( hwnd, DM_SETDEFID, IDD_CHANGE, 0L );
            }
         break;

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            char word[ SSCE_MAX_WORD_SZ ];
            int index;

            index = ListBox_GetCurSel( hwndCtl );
            ListBox_GetText( hwndCtl, index, word );
            if( strcmp( word, GS(IDS_STR611) ) == 0 )
               ListBox_SetCurSel( hwndCtl, -1 );
            else
               {
               HWND hwndEdit;

               hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
               fChangingSel = TRUE;
               Edit_SetText( hwndEdit, word );
               fChangingSel = FALSE;
               if( fDupWord )
                  SetDlgItemText( hwnd, IDD_CHANGE, GS(IDS_STR609) );
               }
            }
         else if( codeNotify == LBN_DBLCLK )
            goto I2;
         break;

      case IDD_SUGGEST: {
         char word[ SSCE_MAX_WORD_SZ ];
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
         Edit_GetText( hwndEdit, word, SSCE_MAX_WORD_SZ );

         if( !fDupWord ) {
            hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
            ListBox_ResetContent( hwndEdit );
            ListBox_AddString( hwndEdit, word );
            }

         DoSuggestions( hwnd, word );
         break;
         }

      case IDD_BADWORD:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndEdit;
            char word[ SSCE_MAX_WORD_SZ ];
   
            hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
            ListBox_GetText( hwndEdit, 0, word );
            ListBox_SetCurSel( hwndEdit, -1 );
            hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
            Edit_SetText( hwndEdit, word );
            SetFocus( hwndEdit );
            }
         break;

      case IDD_IGNOREALL: {
         HWND hwndEdit;
         char word[ SSCE_MAX_WORD_SZ ];
         char * pWord;

         hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_GetText( hwndEdit, 0, pWord = word );

         if( *pWord== '\'' )
            ++pWord;
         if( pWord[ strlen( pWord ) - 1 ] == '\'' )
            pWord[ strlen( pWord ) - 1 ] = '\0';
         A2SSCE_AddToLex( sid, ignAllLexId, pWord, NULL );
         goto I1;
         }

      case IDD_CHANGEALL: {
         HWND hwndEdit;
         char word[ SSCE_MAX_WORD_SZ ];
         char replWord[ SSCE_MAX_WORD_SZ ];

         hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_GetText( hwndEdit, 0, word );

         hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
         Edit_GetText( hwndEdit, replWord, SSCE_MAX_WORD_SZ );
//       Edit_ReplaceSel( hwndCheckEdit, replWord );
         CheckQuotes( (char *)&replWord ); // 2.56.2054
         Editmap_ReplaceSel( hCheckEdit, hwndCheckEdit, replWord );

         if (fAddPreQuote || fAddPostQuote) // 2.56.2055
         {
            sel += strlen( replWord ) - wordlen;
            if (fAddPreQuote)
               sel--;
            if (fAddPostQuote)
               sel--;
         }
         else
            sel += strlen( replWord ) - wordlen;

         A2SSCE_AddToLex( sid, replAllLexId, word, replWord );
         fCancelToClose( hwnd, IDCANCEL );
         goto I1;
         }

      case IDD_CHANGE: {
         char word[ SSCE_MAX_WORD_SZ ];
         char replWord[ SSCE_MAX_WORD_SZ ];
         HWND hwndEdit;
         BEC_SELECTION dwSel;

I2:      hwndEdit = GetDlgItem( hwnd, IDD_BADWORD );
         ListBox_GetText( hwndEdit, 0, word );

         hwndEdit = GetDlgItem( hwnd, IDD_CHANGETO );
         Edit_GetText( hwndEdit, replWord, sizeof( replWord ) );
         if( *replWord == '\0' )
            {
            while( lpText[ off ] && isspace( lpText[ off ] ) )
               {
               ++off;
               ++sel;
               ++wordlen;
               }
//          Edit_SetSel( hwndCheckEdit, selStart, selStart + ( sel - selStart ) );
            dwSel.lo = selStart;
            dwSel.hi = selStart + ( sel - selStart );
            Editmap_SetSel( hCheckEdit, hwndCheckEdit, &dwSel );
            }
//       Edit_ReplaceSel( hwndCheckEdit, replWord );
         CheckQuotes( (char *)&replWord ); // 2.56.2054
         Editmap_ReplaceSel( hCheckEdit, hwndCheckEdit, replWord );
         if (fAddPreQuote || fAddPostQuote) // 2.56.2055
         {
            sel += strlen( replWord ) - wordlen;
            if (fAddPreQuote)
               sel--;
            if (fAddPostQuote)
               sel--;
         }
         else
            sel += strlen( replWord ) - wordlen;

         fCancelToClose( hwnd, IDCANCEL );
         goto I1;
         }

      case IDD_IGNORE:
I1:      if( DoCheck() != SP_CANCEL )
            {
            SaveSpellCheckDialogPosition( hwnd );
            EndDialog( hwnd, SP_OK );
            break;
            }
         DisplayErrorStatus( hwnd );
         break;

      case IDOK:
         SaveSpellCheckDialogPosition( hwnd );
         EndDialog( hwnd, SP_FINISH );
         break;

      case IDCANCEL:
         SaveSpellCheckDialogPosition( hwnd );
         EndDialog( hwnd, SP_CANCEL );
         break;
      }
}

void FASTCALL SaveSpellCheckDialogPosition( HWND hwnd )
{
   RECT rc;

   GetWindowRect( hwnd, &rc );
   Amuser_WritePPInt( szSettings, "SpellCheckDialogX", rc.left );
   Amuser_WritePPInt( szSettings, "SpellCheckDialogY", rc.top );
}

/* This function dispatches messages for the spell checker page
 * in the Preferences dialog.
 */
BOOL EXPORT CALLBACK SpellOpts( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SpellOpts_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SpellOpts_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SpellOpts_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, SpellOpts_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, CheckList_OnMeasureItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SpellOpts_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   int index;
   register int c;

   /* No spelling checker? Disable this page.
    */
   if( NULL == hSpellLib )
      {
      HWND hwndTab;

      hwndTab = PropSheet_GetTabControl( GetParent( hwnd ) );
      TabCtrl_EnableTab( hwndTab, ODU_SPELLING, FALSE );
      return( TRUE );
      }

   /* Fill list of languages */
   hwndList = GetDlgItem( hwnd, IDD_LANGLIST );
   for( c = 0; c < maxLangTypes; ++c )
      ComboBox_AddString( hwndList, szLangTypes[ c ] );
   index = ComboBox_FindString( hwndList, -1, szLangType );
   ComboBox_SetCurSel( hwndList, index );

   /* Fill list of custom dictionaries */
   EnableControl( hwnd, IDD_NEW, cCustDict < MAX_CUST_DICTS );
   EnableControl( hwnd, IDD_ADD, cCustDict < MAX_CUST_DICTS );
   EnableControl( hwnd, IDD_DELETE, cCustDict );
   EnableControl( hwnd, IDD_DEFAULT, cCustDict );
   hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
   if( cCustDict )
      {
      LPCUSTDICT lpCustDict;

      for( lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
         ListBox_InsertString( hwndList, -1, lpCustDict );
      ListBox_SetCurSel( hwndList, 0 );
      }

   /* Enable options */
   CheckDlgButton( hwnd, IDD_IGNOREALLCAPS, fIgnoreAllCaps );
   CheckDlgButton( hwnd, IDD_TESTFORDUPWORDS, fTestForDupWords );
   CheckDlgButton( hwnd, IDD_AUTOCHECK, fAutoSpellCheck );
   CheckDlgButton( hwnd, IDD_SKIPQUOTEDLINES, fSkipQuotedLines );

   /* Subclass the custom dictionary list box */
   lpListProc = SubclassWindow( hwndList, CustDictsListProc );
   return( TRUE );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL SpellOpts_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      LPCUSTDICT lpCustDict;
      COLORREF tmpT;
      COLORREF tmpB;
      COLORREF T;
      COLORREF B;
      BOOL fDefault;
      HBRUSH hbr, hbr2;
      HFONT hOldFont;
      RECT rc, rc2;
      int nMode;
      SIZE size;

      lpCustDict = (LPCUSTDICT)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
      fDefault = lpCustDict == lpDefaultCustDict;
      rc = lpDrawItem->rcItem;

      /* Get the foreground and background colour.
       */
      GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
      hbr = CreateSolidBrush( B );

      /* Draw the enabled/disabled check box
       */
      rc.bottom -= 1;
      SetRect( &rc2, rc.left + 1, rc.top, rc.left + ( rc.bottom - rc.top ), rc.bottom );
      hbr2 = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      FillRect( lpDrawItem->hDC, &rc2, hbr2 );
      DeleteBrush( hbr2 );
      nMode = lpCustDict->fEnabled ? CXB_SELECTED : CXB_NORMAL;
      DrawCheckBox( lpDrawItem->hDC, rc2.left, rc2.top, nMode, &size );
      rc.left += size.cx + 4;

      /* Erase the entire drawing area
       */
      FillRect( lpDrawItem->hDC, &rc, hbr );
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, B );

      /* Display the dictionary name
       */
      BEGIN_PATH_BUF;
      hOldFont = SelectFont( lpDrawItem->hDC, fDefault ? hSys8Font : hHelv8Font );
      lstrcpy( lpPathBuf, lpCustDict->szCustDict );
      FitPathString( lpDrawItem->hDC, lpPathBuf, rc.right - rc.left );
      DrawText( lpDrawItem->hDC, lpPathBuf, -1, &rc, DT_VCENTER|DT_LEFT|DT_NOPREFIX );
      END_PATH_BUF;

      /* Show the focus
       */
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&rc );

      /* Restore things back to normal.
       */
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SpellOpts_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LANGLIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_IGNOREALLCAPS:
      case IDD_TESTFORDUPWORDS:
      case IDD_AUTOCHECK:
      case IDD_SKIPQUOTEDLINES:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_DEFAULT: {
         HWND hwndList;
         int index;

         /* Make the selected dictionary the default.
          */
         hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            /* Update the config file, then redraw the listbox.
             */
            lpDefaultCustDict = (LPCUSTDICT)ListBox_GetItemData( hwndList, index );
            Amuser_WritePPString( szSpelling, "Default", lpDefaultCustDict->szCustDict );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         break;
         }

      case IDD_DELETE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            LPCUSTDICT lpCustDict;

            lpCustDict = (LPCUSTDICT)ListBox_GetItemData( hwndList, index );
            DeleteCustomDictionary( lpCustDict );
            ListBox_DeleteString( hwndList, index );
            if( index == ListBox_GetCount( hwndList ) )
               --index;
            ListBox_SetCurSel( hwndList, index );
            if( index == LB_ERR )
               {
               EnableControl( hwnd, IDD_DELETE, FALSE );
               EnableControl( hwnd, IDD_DEFAULT, FALSE );
               }
            if( index == 0 )
               {
               lpDefaultCustDict = (LPCUSTDICT)ListBox_GetItemData( hwndList, 0 );
               Amuser_WritePPString( szSpelling, "Default", lpDefaultCustDict->szCustDict );
               InvalidateRect( hwndList, NULL, TRUE );
               UpdateWindow( hwndList );
               }

            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }

      case IDD_ADD: {
         static OPENFILENAME ofn;
         static char sz[ _MAX_PATH ];
         static char strFilters[] = "Dictionaries (*.DIC)\0*.dic\0All Files (*.*)\0*.*\0\0";

         BEGIN_PATH_BUF;
         lpPathBuf[ 0 ] = '\0';
         GetDictPath( sz );
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = lpPathBuf;
         ofn.nMaxFile = _MAX_PATH;
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = sz;
         ofn.lpstrTitle = "Add Custom Dictionary";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = "DIC";
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetOpenFileName( &ofn ) )
            {
            HWND hwndList;
            S16 lexId;

            if( ( lexId = A2SSCE_OpenLex( sid, lpPathBuf, lOpenLexAll ) ) < 0 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR505), lpPathBuf );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               END_PATH_BUF;
               break;
               }
            A2SSCE_CloseLex( sid, lexId );
            hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
            if( GetCustomDictionary( lpPathBuf ) == NULL )
               {
               LPCUSTDICT lpCustDict;
               int index;
               int count;

               lpCustDict = AddCustomDictionary( lpPathBuf, TRUE );
               index = ListBox_GetCurSel( hwndList );
               ListBox_InsertString( hwndList, index, lpCustDict );
   
               count = ListBox_GetCount( hwndList );
               EnableControl( hwnd, IDD_NEW, count < MAX_CUST_DICTS );
               EnableControl( hwnd, IDD_ADD, count < MAX_CUST_DICTS );
               EnableControl( hwnd, IDD_DELETE, TRUE );
               EnableControl( hwnd, IDD_DEFAULT, TRUE );
               if( count == 1 )
               {
                  lpDefaultCustDict = (LPCUSTDICT)ListBox_GetItemData( hwndList, 0 );
                  Amuser_WritePPString( szSpelling, "Default", lpDefaultCustDict->szCustDict );
                  InvalidateRect( hwndList, NULL, TRUE );
                  UpdateWindow( hwndList );
               }
               PropSheet_Changed( GetParent( hwnd ), hwnd );
               }
            }
         END_PATH_BUF;
         break;
         }

      case IDD_NEW: {
         static OPENFILENAME ofn;
         static char sz[ _MAX_PATH ];
         static char strFilters[] = "Dictionaries (*.DIC)\0*.dic\0All Files (*.*)\0*.*\0\0";

         BEGIN_PATH_BUF;
         lpPathBuf[ 0 ] = '\0';
         GetDictPath( sz );
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = lpPathBuf;
         ofn.nMaxFile = _MAX_PATH;
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = sz;
         ofn.lpstrTitle = "Create Custom Dictionary";
         ofn.Flags = OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = "DIC";
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetSaveFileName( &ofn ) )
            {
            HWND hwndList;

            hwndList = GetDlgItem( hwnd, IDD_CUSTDICTS );
            if( GetCustomDictionary( lpPathBuf ) == NULL )
               {
               LPCUSTDICT lpCustDict;
               int index;
               int count;
               S16 lexId;

               lpCustDict = AddCustomDictionary( lpPathBuf, TRUE );
               index = ListBox_GetCurSel( hwndList );
               ListBox_InsertString( hwndList, index, lpCustDict );

               Amfile_Delete( lpPathBuf );
               lexId = A2SSCE_CreateLex( sid, lpPathBuf, SSCE_IGNORE_LEX_TYPE, SSCE_BRIT_ENGLISH_LANG );
               A2SSCE_CloseLex( sid, lexId );
   
               count = ListBox_GetCount( hwndList );
               EnableControl( hwnd, IDD_NEW, count < MAX_CUST_DICTS );
               EnableControl( hwnd, IDD_ADD, count < MAX_CUST_DICTS );
               EnableControl( hwnd, IDD_DELETE, TRUE );
               EnableControl( hwnd, IDD_DEFAULT, TRUE );
               PropSheet_Changed( GetParent( hwnd ), hwnd );
               }
            }
         END_PATH_BUF;
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SpellOpts_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_SPELLING );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_SPELLING;
         break;

      case PSN_APPLY: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LANGLIST );
         if( ( index = ComboBox_GetCurSel( hwndList ) ) != CB_ERR )
            {
            char szOldLangType[ 30 ];

            strcpy( szOldLangType, szLangType );
            ComboBox_GetLBText( hwndList, index, szLangType );
            Amuser_WritePPString( szSpelling, "LangType", szLangType );
            SetLangType();
            if( langLexId != -1 && strcmp( szLangType, szOldLangType ) )
               {
               A2SSCE_CloseLex( sid, langLexId );
               GetDictionaryPath( lpTmpBuf, szLangDict );
               if( ( langLexId = A2SSCE_OpenLex( sid, lpTmpBuf, lOpenLexAll ) ) < 0 )
                  {
                  char sz[ 100 ];

                  wsprintf( sz, GS(IDS_STR505), (LPSTR)lpTmpBuf );
                  fMessageBox( NULL, 0, sz, MB_OK|MB_ICONEXCLAMATION );
                  strcpy( szLangType, szOldLangType );
                  SetLangType();
                  GetDictionaryPath( lpTmpBuf, szLangDict );
                  langLexId = A2SSCE_OpenLex( sid, lpTmpBuf, lOpenLexAll );
                  /* If langLexId is -1 after this, then tough. If we can't re-open the
                   * original language dictionary, they'll have to do without.
                   */
                  }
               }
            }
         fIgnoreAllCaps = IsDlgButtonChecked( hwnd, IDD_IGNOREALLCAPS );
         fTestForDupWords = IsDlgButtonChecked( hwnd, IDD_TESTFORDUPWORDS );
         fAutoSpellCheck = IsDlgButtonChecked( hwnd, IDD_AUTOCHECK );
         fSkipQuotedLines = IsDlgButtonChecked( hwnd, IDD_SKIPQUOTEDLINES );
         Amuser_WritePPInt( szSpelling, "AutoCheck", fAutoSpellCheck );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

LRESULT EXPORT FAR PASCAL CustDictsListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_LBUTTONDOWN, CustDictsListProc_OnLButtonDown );
      HANDLE_MSG( hwnd, WM_KEYDOWN, CustDictsListProc_OnKey );

      default:
         return( CallWindowProc(lpListProc, hwnd, message, wParam, lParam ) );
      }
   return( FALSE );
}

void FASTCALL CustDictsListProc_OnLButtonDown( HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags )
{
   int index;
   int nExtent;

   nExtent = ListBox_GetCount( hwnd ) - ListBox_GetTopIndex( hwnd );
   if( ( y / ListBox_GetItemHeight( hwnd, 0 ) ) < nExtent )
      {
      CallWindowProc( lpListProc, hwnd, WM_LBUTTONDOWN, (WPARAM)(UINT)keyFlags, MAKELPARAM( x, y ) );
      if( ( index = ListBox_GetCurSel( hwnd ) ) != LB_ERR )
         {
         LPCUSTDICT lpCustDict;
         RECT rc;
   
         lpCustDict = (LPCUSTDICT)ListBox_GetItemData( hwnd, index );
         lpCustDict->fEnabled = !lpCustDict->fEnabled;
         ListBox_GetItemRect( hwnd, index, &rc );
         InvalidateRect( hwnd, &rc, FALSE );
         }
      }
}

/* This function handles messages for the custom dictionary
 * list box.
 */
void FASTCALL CustDictsListProc_OnKey( HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags )
{
   int index;

   if( fDown )
      switch( vk )
         {
         case ' ':
            if( ( index = ListBox_GetCurSel( hwnd ) ) != LB_ERR )
               {
               LPCUSTDICT lpCustDict;
               RECT rc;
         
               lpCustDict = (LPCUSTDICT)ListBox_GetItemData( hwnd, index );
               lpCustDict->fEnabled = !lpCustDict->fEnabled;
               ListBox_GetItemRect( hwnd, index, &rc );
               InvalidateRect( hwnd, &rc, FALSE );
               }
            break;

         default:
            CallWindowProc( lpListProc, hwnd, WM_KEYDOWN, (WPARAM)(UINT)flags, MAKELPARAM(cRepeat, flags) );
            break;
         }
   else
      CallWindowProc( lpListProc, hwnd, WM_KEYDOWN, (WPARAM)(UINT)flags, MAKELPARAM(cRepeat, flags) );
}

/* This function retrieves the dictionary path. It is presently
 * hardcoded to be in the DICT subdirectory on the Ameol base path.
 */
void FASTCALL GetDictPath( char * pDictDir )
{
   strcat( strcpy( pDictDir, pszAmeolDir ), "DICT" );
}

LPCUSTDICT FASTCALL AddCustomDictionary( LPSTR pszCustDict, BOOL fEnabled )
{
   LPCUSTDICT lpAddDict;

   INITIALISE_PTR(lpAddDict);
   if( fNewMemory( &lpAddDict, sizeof( CUSTDICT ) ) )
      {
      char szDictPath[ _MAX_PATH ];
      LPSTR pBaseName;

      if( !lpFirstCustDict )
         lpFirstCustDict = lpAddDict;
      else
         lpLastCustDict->lpNextCustDict = lpAddDict;
      lpAddDict->lpPrevCustDict = lpLastCustDict;
      lpAddDict->lpNextCustDict = NULL;
      lpLastCustDict = lpAddDict;

      GetDictPath( szDictPath );
      pBaseName = GetFileBasename( pszCustDict );
      strcat( szDictPath, "\\" );
      lstrcat( szDictPath, pBaseName );
      if( lstrcmpi( szDictPath, pszCustDict ) == 0 )
         pszCustDict = pBaseName;

      lstrcpy( lpAddDict->szCustDict, pszCustDict );
      lpAddDict->fEnabled = fEnabled;
      lpAddDict->langId = -1;
      lpAddDict->nIndex = (int)( pBaseName - pszCustDict );
      ++cCustDict;
      }
   else
      {
      wsprintf( lpTmpBuf, GS(IDS_STR511), (LPSTR)pszCustDict );
      fMessageBox( GetFocus(), 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }
   return( lpAddDict );
}

/* This function returns the lpCustDict handle for the specified
 * dictionary.
 */
LPCUSTDICT FASTCALL GetCustomDictionary( LPSTR lpszCustDict )
{
   LPCUSTDICT lpCustDict;
   char szDictPath[ _MAX_PATH ];
   LPSTR pBaseName;

   GetDictPath( szDictPath );
   pBaseName = GetFileBasename( lpszCustDict );
   strcat( szDictPath, "\\" );
   lstrcat( szDictPath, pBaseName );
   if( lstrcmpi( szDictPath, lpszCustDict ) == 0 )
      lpszCustDict = pBaseName;
   for( lpCustDict = lpFirstCustDict; lpCustDict; lpCustDict = lpCustDict->lpNextCustDict )
      {
      if( lstrcmpi( lpszCustDict, lpCustDict->szCustDict ) == 0 )
         break;
      }
   return( lpCustDict );
}

/* This function deletes the specified custom dictionary
 * from memory. It does not remove it from the config file.
 */
void FASTCALL DeleteCustomDictionary( LPCUSTDICT lpCustDict )
{
   if( lpCustDict->lpPrevCustDict )
      lpCustDict->lpPrevCustDict->lpNextCustDict = lpCustDict->lpNextCustDict;
   else
      lpFirstCustDict = lpCustDict->lpNextCustDict;
   if( lpCustDict->lpNextCustDict )
      lpCustDict->lpNextCustDict->lpPrevCustDict = lpCustDict->lpPrevCustDict;
   else
      lpLastCustDict = lpCustDict->lpPrevCustDict;
   FreeMemory( &lpCustDict );
}

/* This function creates a default custom dictionary.
 */
LPCUSTDICT FASTCALL CreateDefaultCustomDictionary( void )
{
   LPCUSTDICT lpCustDict;
   int cIndex;

   /* Create a default custom dictionary file
    * name.
    */
   GetDictPath( lpPathBuf );
   strcat( lpPathBuf, "\\CUSTOM.DIC" );
   cIndex = 1;
   while( Amfile_QueryFile( lpPathBuf ) )
      {
      GetDictPath( lpPathBuf );
      wsprintf( lpPathBuf + strlen(lpPathBuf), "\\CUSTOM.%3.3u", cIndex++ );
      }

   /* Now create the dictionary.
    */
   if( NULL != ( lpCustDict = AddCustomDictionary( lpPathBuf, TRUE ) ) )
      {
      S16 lexId;

      Amfile_Delete( lpPathBuf );
      lexId = A2SSCE_CreateLex( sid, lpPathBuf, SSCE_IGNORE_LEX_TYPE, SSCE_BRIT_ENGLISH_LANG );
      A2SSCE_CloseLex( sid, lexId );
      }

   /* Open the dictionary before we return it.
    */
   lpCustDict->langId = A2SSCE_OpenLex( sid, lpPathBuf, lOpenLexAll );
   return( lpCustDict );
}

/* Open a spelling checker session.
 */
S16 FASTCALL A2SSCE_OpenSession( void )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_OpenSession() );
#endif
   return( fSSCE_OpenSession() );
}

/* Close a spelling checker session.
 */
S16 FASTCALL A2SSCE_CloseSession( S16 sid )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_CloseSession( (S32)sid ) );
#endif
   return( fSSCE_CloseSession( sid ) );
}

/* Check a word.
 */
S16 FASTCALL A2SSCE_CheckWord( S16 sid, unsigned long options, const char FAR * word, char FAR * replWord, size_t replWordSz )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_CheckWord( sid, options, word, replWord, replWordSz ) );
#endif
   return( fSSCE_CheckWord( sid, options, word, replWord, replWordSz ) );
}

/* Obtain a list of suggestions for a misspelt word.
 */
S16 FASTCALL A2SSCE_Suggest( S16 sid, const char FAR * word, S16 depth, char FAR * suggBufr, S32 suggBufrSz, S16 FAR * scores, S16 scoresSz )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      {
      int scores32[ MAX_SCORES ];
      register int c;
      int nRetCode;

      /* The old 32-bit DLL fills an array of 32-bit integers, while the new DLL uses
       * 16-bit integers. So need to copy between the two.
       */
      nRetCode = fSSCE32_Suggest( sid, word, depth, suggBufr, (int)suggBufrSz, scores32, MAX_SCORES );
      for( c = 0; c < scoresSz; ++c )
         scores[ c ] = (S16)scores32[ c ];
      return( nRetCode );
      }
#endif
   if( wSSCEVersion <= 0x0302 )
      return( fSSCE_SuggestOld( sid, word, depth, suggBufr, (int)suggBufrSz, scores, scoresSz ) );
   return( fSSCE_SuggestNew( sid, word, depth, suggBufr, suggBufrSz, scores, scoresSz ) );
}

/* Create a new lexicon.
 */
S16 FASTCALL A2SSCE_CreateLex( S16 sid, const char FAR * fileName, S16 type, S16 lang )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_CreateLex( sid, fileName, type, lang ) );
#endif
   return( fSSCE_CreateLex( sid, fileName, type, lang ) );
}

/* Open an existing lexicon.
 */
S16 FASTCALL A2SSCE_OpenLex( S16 sid, const char FAR * fileName, S32 memBudget )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_OpenLex( sid, fileName, memBudget ) );
#endif
   return( fSSCE_OpenLex( sid, fileName, memBudget ) );
}

/* Close an existing lexicon.
 */
S16 FASTCALL A2SSCE_CloseLex( S16 sid, S16 lexId )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_CloseLex( sid, lexId ) );
#endif
   return( fSSCE_CloseLex( sid, lexId ) );
}

/* Add a word to the lexicon.
 */
S16 FASTCALL A2SSCE_AddToLex( S16 sid, S16 lexId, const char FAR * word, const char FAR * otherWord )
{
#ifdef WIN32
   if( fUseOldStyle32 )
      return( fSSCE32_AddToLex( sid, lexId, word, otherWord ) );
#endif
   return( fSSCE_AddToLex( sid, lexId, word, otherWord ) );
}
