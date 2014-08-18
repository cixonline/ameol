/* RULES.C - Handles rules
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
#include <memory.h>
#include "ameol2.h"
#include <string.h>
#include <ctype.h>
#include "command.h"
#include "ini.h"
#include "amlib.h"
#include "rules.h"
#include "shared.h"
#include "admin.h"
#include "local.h"

#define  THIS_FILE   __FILE__

/* Intellimouse support.
 */
#ifdef WIN32
#include "zmouse.h"
#endif

static BOOL fDefDlgEx = FALSE;

typedef struct tagRULEITEM;

typedef struct tagRULEHEADER {
   struct tagRULEHEADER FAR * lprhNext;   /* Pointer to next rule header */
   struct tagRULEHEADER FAR * lprhPrev;   /* Pointer to previous rule header */
   struct tagRULEITEM FAR * lpriFirst;    /* First rule in this header */
   struct tagRULEITEM FAR * lpriLast;     /* First rule in this header */
   LPVOID pFolder;                        /* Folder to which this rule applies */
} RULEHEADER;

typedef struct tagRULEITEM {
   struct tagRULEITEM FAR * lpriNext;     /* Pointer to next rule */
   struct tagRULEITEM FAR * lpriPrev;     /* Pointer to previous rule */
   struct tagRULEHEADER FAR * lprh;       /* Pointer to rule header */
   RULE ru;                               /* Rule */
} RULEITEM;

RULEHEADER FAR * lprhFirst = NULL;        /* Pointer to first global rule item */
RULEHEADER FAR * lprhLast = NULL;         /* Pointer to last global rule item */

/* Added by YH 17/04/96 */
RULEHEADER FAR * lprhTempFirst = NULL;    /* Temp copy of list in case of cancel */
RULEHEADER FAR * lprhTempLast = NULL;     /* Temp copy of list in case of cancel */
BOOL gfRulesChanged;                      /* TRUE if rules were edited */
HWND hwndTooltip;                         /* Tooltip in listbox */
WNDPROC lpfnDefWindowProc;                /* Subclassed window proc handle */
LPSTR lpTTString = NULL;                  /* Tooltip string */

int FASTCALL b64decodePartial(char *d);

void FASTCALL DeleteRule( RULEITEM FAR * );
RULEHEADER FAR * GetRuleHeader( LPVOID );
HRULE FASTCALL InternalCreateRule( RULE FAR * );
int FASTCALL MatchOnePass( PTL *, HPSTR, HEADER FAR *, BOOL, BOOL );
void FASTCALL CreateDescription( RULEITEM FAR *, char * );
BOOL FASTCALL RuleCommand_Edit( HWND, HWND );
BOOL FASTCALL RuleCommand_Remove( HWND, HWND );
BOOL FASTCALL RuleCommand_New( HWND, HWND, LPVOID );
int FASTCALL AddressRuleMatch( LPSTR, LPSTR );
int FASTCALL GetStringExtent( HWND, char * );
void FASTCALL FillTooltipFromListbox( HWND );
void FASTCALL EnableRulesControls( HWND );
BOOL FASTCALL EnumApplyRules( PTL, LPARAM );

BOOL FASTCALL FolderPropRules_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL FolderPropRules_OnNotify( HWND, int, LPNMHDR );
void FASTCALL FolderPropRules_OnCommand( HWND, int, HWND, UINT );

LRESULT FASTCALL CmdRulesDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL CmdRulesDlg_OnNotify( HWND, int, LPNMHDR );
BOOL EXPORT CALLBACK CmdRulesDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CmdRulesDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CmdRulesDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL CmdRulesDlg_ListRules( HWND, LPVOID, BOOL );

BOOL EXPORT CALLBACK CmdEditRuleDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL CmdEditRuleDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdEditRuleDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CmdEditRuleDlg_OnInitDialog( HWND, HWND, LPARAM );

LRESULT CALLBACK ListBoxWndProc( HWND, UINT, WPARAM, LPARAM );

/* Added by YH 17/04/96 so that cancel can be handled properely
*/
void FASTCALL CreateTempList( void );
void FASTCALL DestroyTempList( void );
void FASTCALL MakeTempListPermanent( void );
void FASTCALL HandleDialogClose( HWND hwnd, int id );

BOOL FASTCALL DoCopyMsg( HWND, PTH, PTL, BOOL, BOOL, BOOL );
BOOL FASTCALL DoCopyLocalRange( HWND, PTH FAR *, int, PTL, BOOL, BOOL );
BOOL FASTCALL DoCopyMsgRange( HWND, PTH FAR *, int, PTL, BOOL, BOOL, BOOL );

/* This function loads all predefined rules. We look in the [Rules] section
 * for a list of rules and create a RULEITEM structure for each one.
 */
void FASTCALL InitRules( void )
{
   LPSTR pszRuleTitles;

   /* Load rules from config file.
    */
   INITIALISE_PTR(pszRuleTitles);
   if( fNewMemory( &pszRuleTitles, 5000 ) )
      {
      register int c;

      Amuser_GetPPString( "Rules", NULL, "", pszRuleTitles, 5000 );
      for( c = 0; pszRuleTitles[ c ]; ++c )
         {
         char szRule[ 400 ];
         char * psz;
         int nFlags;
         RULE fi;

         /* Get the rule description.
          */
         Amuser_GetPPString( "Rules", &pszRuleTitles[ c ], "", szRule, sizeof(szRule) );
         memset( &fi, 0, sizeof( RULE ) );

         /* Parse off the text. This may be quoted.
          */
         psz = szRule;
         psz = IniReadText( psz, fi.szToText, sizeof(fi.szToText) - 1 );
         psz = IniReadInt( psz, &nFlags );
         fi.wFlags = nFlags;

         /* Parse off the folder pathname.
          * BUGBUG: Assumes destination folder is always News!
          */
         psz = ParseFolderPathname( psz, &fi.pData, TRUE, FTYPE_NEWS );

         /* Skip the separators.
          */
         if( *psz == ',' )
            ++psz;

         /* Parse off the target folder pathname.
          * BUGBUG: Assumes destination folder is always News!
          */
         psz = ParseFolderPathname( psz, &fi.pMoveData, TRUE, FTYPE_NEWS );

         /* Skip the separators.
          */
         if( *psz == ',' )
            ++psz;

         /* Retrieve other fields.
          */
         psz = IniReadText( psz, fi.szFromText, sizeof(fi.szFromText) - 1 );
         psz = IniReadText( psz, fi.szSubjectText, sizeof(fi.szSubjectText) - 1 );
         psz = IniReadText( psz, fi.szBodyText, sizeof(fi.szBodyText) - 1 );
         psz = IniReadText( psz, fi.szMailForm, sizeof(fi.szMailForm) - 1 );

         /* Create the rule.
          */
         if( fi.pData )
            InternalCreateRule( &fi );

         /* Skip to the next rule.
          */
         c += strlen( &pszRuleTitles[ c ] );
         }
      FreeMemory( &pszRuleTitles );
      }
}

/* This function parses a folder path of the form:
 *
 *    \DATABASE\FOLDER\TOPIC
 *
 * The leading backslash may be omitted. The value returned is the
 * handle of the item at the end of the path; the rest of the path
 * may be deduced from the FolderFromTopic and DatabaseFromFolder
 * functions.
 */
LPSTR FASTCALL ParseFolderPathname( LPSTR lpszFolderPath, LPVOID * pv, BOOL fCreate, WORD wType )
{
   char sz[ 100 ];
   register int c;
   LPVOID pData;
   BOOL fQuote;
   PCAT pcat;
   PCL pcl;

   /* Skip any quotes.
    */
   fQuote = FALSE;
   if( *lpszFolderPath == '"' )
      {
      fQuote = TRUE;
      ++lpszFolderPath;
      }

   /* Skip initial backslash
    */
   if( *lpszFolderPath == '\\' )
      ++lpszFolderPath;

   /* Read off first name. Must be a category.
    */
   for( c = 0; *lpszFolderPath && *lpszFolderPath != '\\'; )
      {
      if( *lpszFolderPath == '"' )
         fQuote = FALSE;
      else
         {
         if( *lpszFolderPath == ',' && !fQuote )
            break;
         if( c < sizeof( sz ) - 1 )
            sz[ c++ ] = *lpszFolderPath;
         }
      ++lpszFolderPath;
      }
   sz[ c ] = '\0';

   /* Skip any backslash
    */
   if( *lpszFolderPath == '\\' )
      ++lpszFolderPath;

   /* End of path? Return category handle unless it is
    * invalid in which case we return the handle of the first
    * available category.
    */
   if( ( pcat = Amdb_OpenCategory( sz ) ) == NULL )
   {        
      if( fCreate )
            pcat = Amdb_CreateCategory( sz );
   }
   pData = pcat;
   if( *lpszFolderPath != '\0' && *lpszFolderPath != ',' )
      {
      /* Read off the next name. Must be a folder.
       */
      for( c = 0; *lpszFolderPath && *lpszFolderPath != '\\'; )
         {
         if( *lpszFolderPath == '"' )
            fQuote = FALSE;
         else
            {
            if( *lpszFolderPath == ',' && !fQuote )
               break;
            if( c < sizeof( sz ) - 1 )
               sz[ c++ ] = *lpszFolderPath;
            }
         ++lpszFolderPath;
         }
      sz[ c ] = '\0';
   
      /* Skip any backslash
       */
      if( *lpszFolderPath == '\\' )
         ++lpszFolderPath;
   
      /* End of path? Return category handle unless it is
       * invalid in which case we return the handle of the first
       * available category.
       */
      if( ( pcl = Amdb_OpenFolder( pcat, sz ) ) == NULL )
         {
         if( fCreate )
            pcl = Amdb_CreateFolder( pcat, sz, CFF_SORT );
         }
      if( pcl != NULL )
         {
         pData = pcl;
         if( *lpszFolderPath != '\0' && *lpszFolderPath != ',' )
            {
            PTL ptl;

            /* Read off the last name. Must be a topic.
             */
            for( c = 0; *lpszFolderPath && *lpszFolderPath != '\\'; )
               {
               if( *lpszFolderPath == '"' )
                  fQuote = FALSE;
               else
                  {
                  if( *lpszFolderPath == ',' && !fQuote )
                     break;
                  if( c < sizeof( sz ) - 1 )
                     sz[ c++ ] = *lpszFolderPath;
                  }
               ++lpszFolderPath;
               }
            sz[ c ] = '\0';
         
            /* Skip any backslash
             */
            if( *lpszFolderPath == '\\' )
               ++lpszFolderPath;

            /* End of path? Return category handle unless it is
             * invalid in which case we return the handle of the first
             * available category.
             */
            if( ( ptl = Amdb_OpenTopic( pcl, sz ) ) == NULL )
               {
               if( fCreate )
                  {
                  if( strcmp( Amdb_GetFolderName( pcl ), "Mail" ) == 0 )
                     wType = FTYPE_MAIL;
                  ptl = Amdb_CreateTopic( pcl, sz, wType );
                  }
               }
            if( ptl != NULL )
               pData = ptl;
            }
         }
      }
   if( *lpszFolderPath == '"' && fQuote )
      ++lpszFolderPath;
   *pv = pData;
   return( lpszFolderPath );
}

/* This function expand a folder pointer into a fully
 * fledged pathname to that folder.
 */
char * FASTCALL WriteFolderPathname( LPSTR pszBuf, LPVOID pData )
{
   PCAT pcat;
   PCL pcl;

   strcpy( pszBuf, "\\" );
   if( Amdb_IsCategoryPtr( pData ) )
      strcat( pszBuf, Amdb_GetCategoryName( (PCAT)pData ) );
   else if( Amdb_IsFolderPtr( pData ) )
      {
      pcat = Amdb_CategoryFromFolder( (PCL)pData );
      strcat( pszBuf, Amdb_GetCategoryName( pcat ) );
      strcat( pszBuf, "\\" );
      strcat( pszBuf, Amdb_GetFolderName( (PCL)pData ) );
      }
   else if( Amdb_IsTopicPtr( pData ) )
      {
      pcl = Amdb_FolderFromTopic( (PTL)pData );
      pcat = Amdb_CategoryFromFolder( pcl );
      strcat( pszBuf, Amdb_GetCategoryName( pcat ) );
      strcat( pszBuf, "\\" );
      strcat( pszBuf, Amdb_GetFolderName( pcl ) );
      strcat( pszBuf, "\\" );
      strcat( pszBuf, Amdb_GetTopicName( (PTL)pData ) );
      }
   return( pszBuf + strlen( pszBuf ) );
}

/* This function expand a folder pointer into a fully
 * fledged pathname to that folder.
 */
char * FASTCALL WriteShortFolderPathname( LPSTR pszBuf, LPVOID pData )
{
   PCL pcl;

   if( Amdb_IsFolderPtr( pData ) )
      strcat( pszBuf, Amdb_GetFolderName( (PCL)pData ) );
   else if( Amdb_IsTopicPtr( pData ) )
      {
      pcl = Amdb_FolderFromTopic( (PTL)pData );
      strcat( pszBuf, Amdb_GetFolderName( pcl ) );
      strcat( pszBuf, "\\" );
      strcat( pszBuf, Amdb_GetTopicName( (PTL)pData ) );
      }
   return( pszBuf + strlen( pszBuf ) );
}

/* This function attempts to apply a rule to a single message.
 * This can be very slow, because of all the testing that goes
 * on and the calls to the database engine.
 */
BOOL FASTCALL MatchRuleOnMessage( PTH pth )
{
   int nMatchResult;
   MSGINFO msginfo;
   HPSTR hpText;
   HEADER hdr;
   PTL ptl;

   /* Create the HEADER structure needed by MatchRule
    */
   Amdb_GetMsgInfo( pth, &msginfo );
   hpText = Amdb_GetMsgText( pth );
   memset( &hdr, 0, sizeof(HEADER) );
   hdr.ptl = Amdb_TopicFromMsg( pth );
   strncpy( hdr.szAuthor, msginfo.szAuthor, LEN_INTERNETNAME );
   strncpy( hdr.szTitle, msginfo.szTitle, LEN_TITLE );

   /* To optimise things, don't bother with hdr.szTo or hdr.szCC
    * unless hdr.ptl is a mail folder.
    */
   if( Amdb_GetTopicType( hdr.ptl ) == FTYPE_MAIL )
      {
      Amdb_GetMailHeaderFieldInText( hpText, "To", hdr.szTo, LEN_TOCCLIST, TRUE );
      Amdb_GetMailHeaderFieldInText( hpText, "CC", hdr.szCC, LEN_TOCCLIST, TRUE );
      if( msginfo.dwFlags & MF_OUTBOXMSG )
         hdr.fOutboxMsg = TRUE;
      else
         hdr.fOutboxMsg = FALSE;
      }

   /* Okay, save hdr.ptl so we know when it changes, then call
    * MatchRule to see what we have to do with this message.
    */
   ptl = hdr.ptl;
   nMatchResult = MatchRule( &hdr.ptl, hpText, &hdr, TRUE );
   if( nMatchResult & FLT_MATCH_DELETE )
      Amdb_DeleteMsg( pth, TRUE );
   else
      {
      if( nMatchResult & FLT_MATCH_PRIORITY )
         Amdb_MarkMsgPriority( pth, TRUE, TRUE );
      if( nMatchResult & FLT_MATCH_READ )
         Amdb_MarkMsgRead( pth, TRUE );
      if( nMatchResult & FLT_MATCH_IGNORE )
         Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
      if( nMatchResult & FLT_MATCH_WATCH )
         {
         /* Only get article if DESTINATION topic is a newsgroup
          * and we've currently only got the header.
          */
         if( Amdb_GetTopicType( hdr.ptl ) == FTYPE_NEWS )
            Amdb_MarkMsgWatch( pth, TRUE, TRUE );
         }
      if( nMatchResult & FLT_MATCH_READLOCK )
      {
         Amdb_MarkMsgRead( pth, FALSE );
         Amdb_MarkMsgReadLock( pth, TRUE );
      }
      if( nMatchResult & FLT_MATCH_KEEP )
         Amdb_MarkMsgKeep( pth, TRUE );
      if( nMatchResult & FLT_MATCH_MARK )
         Amdb_MarkMsg( pth, TRUE );
      if( nMatchResult & FLT_MATCH_GETARTICLE )
         {
         /* Only get article if DESTINATION topic is a newsgroup
          * and we've currently only got the header.
          */
         if( Amdb_GetTopicType( hdr.ptl ) == FTYPE_NEWS && Amdb_IsHeaderMsg( pth ) )
            Amdb_MarkMsgTagged( pth, TRUE );
         }
      if( ( ptl != hdr.ptl ) && ( nMatchResult & FLT_MATCH_MOVE ) )
         {
         PTH FAR * lppth;

         /* Okay, message got moved so move the message after
          * we've applied all the above.
          */
         INITIALISE_PTR(lppth);
         if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
            {
            lppth[ 0 ] = pth;
            lppth[ 1 ] = NULL;
            DoCopyLocalRange( hwndFrame, lppth, 1, hdr.ptl, TRUE, FALSE );
            }
         }
      if( ( ptl != hdr.ptl ) && ( nMatchResult & FLT_MATCH_COPY ) )
         {
         PTH FAR * lppth;

         /* Okay, message got moved so move the message after
          * we've applied all the above.
          */
         INITIALISE_PTR(lppth);
         if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
            {
            lppth[ 0 ] = pth;
            lppth[ 1 ] = NULL;
            DoCopyMsgRange( hwndFrame, lppth, 1, hdr.ptl, FALSE, FALSE, FALSE );
            }
         }
      Amdb_FreeMsgTextBuffer( hpText );
   }
   return( TRUE );
}

/* This function attempts to match the specified message against the rule.
 * The return value specifies whether or not a match was found and, if so,
 * what action the caller should take:
 *
 *   FLT_MATCH_NONE     - No match
 *   FLT_MATCH_DELETE   - Match, delete message
 *   FLT_MATCH_PRIORITY - Match, mark message priority
 *   FLT_MATCH_READ     - Match, mark message read
 *   FLT_MATCH_MOVE     - Match, move to folder specified by *pptl.
 */
 int FASTCALL MatchRule( PTL * pptl, HPSTR hpszMatchMsg, HEADER FAR * lpHdr, BOOL fSendForm )
 {
    int wFlags;
    
    wFlags = FLT_MATCH_NONE;
    if( fRulesEnabled )
    {
       BOOL fFirstPass;
       PTL ptlStart;
       DWORD cycleCount=0;
       
       ptlStart = *pptl;
       fFirstPass = TRUE;
       do   {
          int nMatchResult;
          
          /* Find one match. If the match is a move action and
          * we haven't cycled round to the start, then apply rules
          * in the destination topic.
          */
          nMatchResult = MatchOnePass( pptl, hpszMatchMsg, lpHdr, fFirstPass, fSendForm );
          wFlags |= nMatchResult;
          if( !( FLT_MATCH_MOVE & nMatchResult ) && !( FLT_MATCH_COPY & nMatchResult ) )
             return( wFlags );
          fFirstPass = FALSE;

          //2.56.2052 FS#141
          cycleCount++;       
          if ( cycleCount > 1000000 ) 
          {
             char szTemp[255];
             wsprintf(szTemp, "You may have a rule loop. Do you want to stop rule processing now?");
             if ( fMessageBox( hwndFrame, 0, szTemp, MB_YESNO|MB_ICONWARNING ) == IDYES )
                break;
             else
                cycleCount = 0;
          }
       }
       while( *pptl != ptlStart );
    }
    return( wFlags );
 } 


/* This function makes one pass through the rules, matching the
 * specified message against them.
 */
int FASTCALL MatchOnePass( PTL * pptl, HPSTR hpszMatchMsg, HEADER FAR * lpHdr, BOOL fApplyParentRules, BOOL fSendForm )
{
   RULEHEADER FAR * lprh;
   int wFlags;
   PCAT pcat;
   PCL pcl;
   char szSubject[ 501 ];

   pcl = Amdb_FolderFromTopic( *pptl );
   pcat = Amdb_CategoryFromFolder( pcl );
   wFlags = FLT_MATCH_NONE;
   for( lprh = lprhFirst; lprh; lprh = lprh->lprhNext )
      {
      RULEITEM FAR * lpri;

      /* Does this rule apply to the topic? Global rules always apply.
       */
      if( !fApplyParentRules && lprh->pFolder != *pptl )
         continue;
      else if( lprh->pFolder )
         {
         if( Amdb_IsCategoryPtr( lprh->pFolder ) && (PCAT)lprh->pFolder != pcat )
            continue;
         if( Amdb_IsFolderPtr( lprh->pFolder ) && (PCL)lprh->pFolder != pcl )
            continue;
         if( Amdb_IsTopicPtr( lprh->pFolder ) && (PTL)lprh->pFolder != *pptl )
            continue;
         }

      /* Search the rule list.
       */
      for( lpri = lprh->lpriFirst; lpri; lpri = lpri->lpriNext )
         {
         BOOL fMatch;
         BOOL fMatch1;
         BOOL fMatch2;
         BOOL fMatch3;
         BOOL fMatch4;

         if( lpri->ru.wFlags & FILF_OR )
            fMatch1 = fMatch2 = fMatch3 = fMatch4 = FALSE;
         else
            fMatch1 = fMatch2 = fMatch3 = fMatch4 = TRUE;
// !!SM!!

/*       if( *lpri->ru.szFromText ) 
            {
            fMatch1 = AddressRuleMatch( lpri->ru.szFromText, lpHdr->szAuthor ) != -1;
            if( !fMatch1 && lpHdr->fOutboxMsg )
               fMatch1 = AddressRuleMatch( lpri->ru.szFromText, lpHdr->szTo ) != -1;
            }
         if( *lpri->ru.szToText )
            {
            fMatch2 = AddressRuleMatch( lpri->ru.szToText, lpHdr->szTo ) != -1;
            if( !fMatch2 && (lpri->ru.wFlags & FILF_USECC) && *lpHdr->szCC )
               fMatch2 = AddressRuleMatch( lpri->ru.szToText, lpHdr->szCC ) != -1;
            }
         if( *lpri->ru.szSubjectText )
            fMatch3 = FStrMatch( lpHdr->szTitle, lpri->ru.szSubjectText, FALSE, FALSE ) != -1;
         if( *lpri->ru.szBodyText )
            fMatch4 = HStrMatch( hpszMatchMsg, lpri->ru.szBodyText, FALSE, FALSE ) != -1;
*/

         if (lpri->ru.wFlags & FILF_REGEXP)
         {
            if( *lpri->ru.szFromText )
               {
               fMatch1 = strcmpiwild(lpri->ru.szFromText, lpHdr->szAuthor, (lpri->ru.wFlags & FILF_REGEXP)) == 0;
               if( !fMatch1 && lpHdr->fOutboxMsg )
                  fMatch1 = strcmpiwild(lpri->ru.szFromText, lpHdr->szTo, (lpri->ru.wFlags & FILF_REGEXP)) == 0;
               }
            if( *lpri->ru.szToText )
               {
               fMatch2 = strcmpiwild( lpri->ru.szToText, lpHdr->szTo, (lpri->ru.wFlags & FILF_REGEXP) ) == 0;
               if( !fMatch2 && (lpri->ru.wFlags & FILF_USECC) && *lpHdr->szCC )
                  fMatch2 = strcmpiwild( lpri->ru.szToText, lpHdr->szCC, (lpri->ru.wFlags & FILF_REGEXP) ) == 0;
               }
            if( *lpri->ru.szSubjectText )
            {
               Amdb_GetMailHeaderFieldInText( (HPSTR)hpszMatchMsg, "Subject", (char *)&szSubject, 500, TRUE );
               if( szSubject[0] )
               {
                  b64decodePartial((char *)&szSubject); 
                  fMatch3 = strcmpiwild( lpri->ru.szSubjectText, szSubject, (lpri->ru.wFlags & FILF_REGEXP) ) == 0;
               }
               else
                  fMatch3 = strcmpiwild( lpri->ru.szSubjectText, lpHdr->szTitle, (lpri->ru.wFlags & FILF_REGEXP) ) == 0;
            }
            if( *lpri->ru.szBodyText )
               fMatch4 = Hstrcmpiwild( lpri->ru.szBodyText, hpszMatchMsg, (lpri->ru.wFlags & FILF_REGEXP) ) == 0;
         }
         else
         {
            if( *lpri->ru.szFromText ) 
               {
               fMatch1 = AddressRuleMatch( lpri->ru.szFromText, lpHdr->szAuthor ) != -1;
               if( !fMatch1 && lpHdr->fOutboxMsg )
                  fMatch1 = AddressRuleMatch( lpri->ru.szFromText, lpHdr->szTo ) != -1;
               }
            if( *lpri->ru.szToText )
               {
               fMatch2 = AddressRuleMatch( lpri->ru.szToText, lpHdr->szTo ) != -1;
               if( !fMatch2 && (lpri->ru.wFlags & FILF_USECC) && *lpHdr->szCC )
                  fMatch2 = AddressRuleMatch( lpri->ru.szToText, lpHdr->szCC ) != -1;
               }
            if( *lpri->ru.szSubjectText )
            {
               Amdb_GetMailHeaderFieldInText( (HPSTR)hpszMatchMsg, "Subject", (char *)&szSubject, 500, TRUE );
               if( szSubject[0] )
               {
                  b64decodePartial((char *)&szSubject); 
                  fMatch3 = FStrMatch( szSubject, lpri->ru.szSubjectText, FALSE, FALSE ) != -1;
               }
               else
                  fMatch3 = FStrMatch( lpHdr->szTitle, lpri->ru.szSubjectText, FALSE, FALSE ) != -1;
            }
            if( *lpri->ru.szBodyText )
               fMatch4 = HStrMatch( hpszMatchMsg, lpri->ru.szBodyText, FALSE, FALSE ) != -1;
         }
// !!SM!!
         if( lpri->ru.wFlags & FILF_OR )
         {
            if( !( *lpri->ru.szFromText ) && !( *lpri->ru.szToText ) && !( *lpri->ru.szSubjectText ) && !( *lpri->ru.szBodyText ) )
               fMatch = TRUE;
            else
               fMatch = fMatch1 || fMatch2 || fMatch3 || fMatch4;
         }
         else
            fMatch = fMatch1 && fMatch2 && fMatch3 && fMatch4;
         if( lpri->ru.wFlags & FILF_NOT )
            fMatch = !fMatch;
         if( fMatch )
            {
            /* We've a match! Return the actions
             */
            if( lpri->ru.wFlags & FILF_DELETE )
               wFlags |= FLT_MATCH_DELETE;
            if( lpri->ru.wFlags & FILF_PRIORITY )
               wFlags |= FLT_MATCH_PRIORITY;
            if( lpri->ru.wFlags & FILF_GETARTICLE )
               wFlags |= FLT_MATCH_GETARTICLE;
            if( lpri->ru.wFlags & FILF_IGNORE )
               wFlags |= FLT_MATCH_IGNORE;
            if( lpri->ru.wFlags & FILF_READ )
               wFlags |= FLT_MATCH_READ;
            if( lpri->ru.wFlags & FILF_WATCH )
               wFlags |= FLT_MATCH_WATCH;
            if( lpri->ru.wFlags & FILF_READLOCK )
               wFlags |= FLT_MATCH_READLOCK;
/*          if( lpri->ru.wFlags & FILF_WITHDRAWN ) !!SM!!
               wFlags |= FLT_MATCH_WITHDRAWN;*/
            if( lpri->ru.wFlags & FILF_KEEP )
               wFlags |= FLT_MATCH_KEEP;
            if( lpri->ru.wFlags & FILF_MARK )
               wFlags |= FLT_MATCH_MARK;

            if( fSendForm && ( lpri->ru.wFlags & FILF_MAILFORM ) && !( wFlags & FLT_MATCH_MOVE ) )
               {
               /* We match a mail form, so send out the
                * appropriate form.
                */
               ComposeMailForm( lpri->ru.szMailForm, FALSE, hpszMatchMsg, lpHdr );
               wFlags |= FLT_MATCH_MAILFORM;
               }
            if( ( lpri->ru.wFlags & FILF_COPY ) && lpri->ru.pMoveData != *pptl )
               {
               *pptl = lpri->ru.pMoveData;
               if( wFlags & FLT_MATCH_MOVE )
                  wFlags &= ~FLT_MATCH_MOVE;
               wFlags |= FLT_MATCH_COPY;
//             break;
               }
            if( ( lpri->ru.wFlags & FILF_MOVE ) && lpri->ru.pMoveData != *pptl )
               {
               *pptl = lpri->ru.pMoveData;
               if( wFlags & FLT_MATCH_COPY )
                  wFlags &= ~FLT_MATCH_COPY;
               wFlags |= FLT_MATCH_MOVE;
               break;
               }
            }
         }
      }
   return( wFlags );
}

/* This function gets the rule header associated with the
 * specified rule.
 */
RULEHEADER FAR * GetRuleHeader( LPVOID pFolder )
{
   RULEHEADER FAR * lprh;

   /* First look for the rule.
    */
   for( lprh = lprhFirst; lprh; lprh = lprh->lprhNext )
      if( lprh->pFolder == pFolder )
         break;

   /* If not found, create it.
    */
   if( NULL == lprh )
      if( fNewMemory( &lprh, sizeof( RULEHEADER ) ) )
         {
         if( NULL == lprhFirst )
            lprhFirst = lprh;
         else
            lprhLast->lprhNext = lprh;
         lprh->lprhNext = NULL;
         lprh->lprhPrev = lprhLast;
         lprhLast = lprh;
         lprh->lpriFirst = NULL;
         lprh->lpriLast = NULL;
         lprh->pFolder = pFolder;
         }
   return( lprh );
}

/* This function creates a new rule, then saves the
 * rules list.
 */
HRULE FASTCALL CreateRule( RULE FAR * pRule )
{
   RULEHEADER FAR * lprh;
   HRULE lpriNew;

   /* First see if this rule already exists. No point
    * in duplicating rules, is there?
    */
   lprh = GetRuleHeader( pRule->pData );
   if( lprh )
      {
      RULEITEM FAR * lpri;

      for( lpri = lprh->lpriFirst; lpri; lpri = lpri->lpriNext )
         if( memcmp( &lpri->ru, pRule, sizeof(RULE) ) == 0 )
            return( (HRULE)lpri );
      }

   /* Now create the rule.
    */
   if( NULL != ( lpriNew = InternalCreateRule( pRule ) ) )
      SaveRules();
   return( lpriNew );
}

/* This function creates a new rule.
 */
HRULE FASTCALL InternalCreateRule( RULE FAR * pRule )
{
   RULEHEADER FAR * lprh;
   RULEITEM FAR * lpriNew;

   INITIALISE_PTR(lpriNew);

   /* Get the header to which this rule belongs (or
    * create it.
    */
   lprh = GetRuleHeader( pRule->pData );

   /* Create the rule item.
    */
   if( lprh )
      if( fNewMemory( &lpriNew, sizeof( RULEITEM ) ) )
         {
         /* Link this rule structure to the end of the
          * linked list of rules.
          */
         if( NULL == lprh->lpriFirst )
            lprh->lpriFirst = lpriNew;
         else
            lprh->lpriLast->lpriNext = lpriNew;
         lpriNew->lpriPrev = lprh->lpriLast;
         lpriNew->lpriNext = NULL;
         lpriNew->lprh = lprh;
         lprh->lpriLast = lpriNew;

         /* Fill in the fields from the pRule structure.
          */
         lpriNew->ru = *pRule;
         }
   return( (HRULE)lpriNew );
}

/* This function saves the rules.
 */
void FASTCALL SaveRules( void )
{
   RULEHEADER FAR * lprh;
   RULEITEM FAR * lpri;
   register int c;

   /* Save all rules to the configuration file.
    */
   Amuser_WritePPString( "Rules", NULL, "" );
   for( c = 0, lprh = lprhFirst; lprh; lprh = lprh->lprhNext )
      for( lpri = lprh->lpriFirst; lpri; lpri = lpri->lpriNext )
         {
         char szSection[ 6 ];
         char sz[ 400 ];
         char * psz;
         
         wsprintf( szSection, "F%u", c );
         psz = sz;
         psz = IniWriteText( psz, lpri->ru.szToText );
         *psz++ = ',';
         psz = IniWriteValue( psz, lpri->ru.wFlags );
         *psz++ = ',';
         *psz++ = '"';
         psz = WriteFolderPathname( psz, lpri->ru.pData );
         *psz++ = '"';
         *psz++ = ',';
         *psz++ = '"';
         psz = WriteFolderPathname( psz, lpri->ru.pMoveData );
         *psz++ = '"';
         *psz++ = ',';
         psz = IniWriteText( psz, lpri->ru.szFromText );
         *psz++ = ',';
         psz = IniWriteText( psz, lpri->ru.szSubjectText );
         *psz++ = ',';
         psz = IniWriteText( psz, lpri->ru.szBodyText );
         *psz++ = ',';
         psz = IniWriteText( psz, lpri->ru.szMailForm );
         Amuser_WritePPString( "Rules", szSection, sz );
         ++c;
         }
}

/* This function is called before a category, folder or topic is
 * deleted. It scans the list of rules and deletes those that apply
 * to or refer to the category, folder or topic being deleted.
 */
void FASTCALL DeleteAllApplicableRules( LPVOID pData )
{
   RULEHEADER FAR * lprh;
   RULEHEADER FAR * lprhNext;
   RULEITEM FAR * lpri;
   RULEITEM FAR * lpriNext;
   BOOL fAnythingDeleted;

   /* Loop for each rule.
    */
   fAnythingDeleted = FALSE;
   for( lprh = lprhFirst; lprh; lprh = lprhNext )
      {
      BOOL fDeleteRuleHeader;

      /* Save next handle in case entire rule is deleted.
       */
      lprhNext = lprh->lprhNext;
      fDeleteRuleHeader = lprh->pFolder == pData;
      for( lpri = lprh->lpriFirst; lpri; lpri = lpriNext )
         {
         lpriNext = lpri->lpriNext;
         if( fDeleteRuleHeader || ( ( lpri->ru.wFlags & FILF_MOVE ) || ( lpri->ru.wFlags & FILF_COPY ) ) && lpri->ru.pMoveData == pData )
            {
            DeleteRule( lpri );
            fAnythingDeleted = TRUE;
            }
         }
      }

   /* Only save if anything deleted.
    */
   if( fAnythingDeleted )
      SaveRules();
}

/* This function deletes a rule.
 */
void FASTCALL DeleteRule( RULEITEM FAR * lpri )
{
   RULEHEADER FAR * lprh;

   /* Unlink the rule from the list.
    */
   lprh = lpri->lprh;
   if( NULL == lpri->lpriPrev )
      lprh->lpriFirst = lpri->lpriNext;
   else
      lpri->lpriPrev->lpriNext = lpri->lpriNext;
   if( NULL == lpri->lpriNext )
      lprh->lpriLast = lpri->lpriPrev;
   else
      lpri->lpriNext->lpriPrev = lpri->lpriPrev;

   /* If this was the last rule in the header, delete the header.
    */
   if( NULL == lprh->lpriFirst )
      {
      if( lprh->lprhPrev == NULL )
         lprhFirst = lprh->lprhNext;
      else
         lprh->lprhPrev->lprhNext = lprh->lprhNext;
      if( lprh->lprhNext == NULL )
         lprhLast = lprh->lprhPrev;
      else
         lprh->lprhNext->lprhPrev = lprh->lprhPrev;
      FreeMemory( &lprh );
      }

   /* Now delete the rule.
    */
   FreeMemory( &lpri );
}

/* This function displays the Rules dialog.
 */
void FASTCALL CmdRules( HWND hwnd )
{
/*
   HINSTANCE hImpLib;
   IMPORTPROC lpEditRules;

   if( TestPerm(PF_CANCONFIGURE) )
   {
      if( ( hImpLib = AmLoadLibrary( "REGEXP.DLL" ) ) < (HINSTANCE)32 )
         {
            return;
         }


      if( NULL == ( lpEditRules = (IMPORTPROC)GetProcAddress( hImpLib, "EditRules" ) ) )
         {
            return;
         }

      lResult = lpEditRules;//( ARegExpr, AInputStr );

      // Clean up after us.
       
      FreeLibrary( hImpLib );
   }
*/

   if( TestPerm(PF_CANCONFIGURE) )
      Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_RULEMANAGER), CmdRulesDlg, 0L );

}

/* Handles the Rules dialog.
 */
BOOL EXPORT CALLBACK CmdRulesDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CmdRulesDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Rules dialog.
 */
LRESULT FASTCALL CmdRulesDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CmdRulesDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CmdRulesDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, CmdRulesDlg_OnNotify );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsRULEMANAGER );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CmdRulesDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndTreeCtl;
   HIMAGELIST himl;
   HTREEITEM hItem;
   HWND hwndList;
   CURMSG curmsg;
   TV_ITEM tv;

   /* Create the image list.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
   himl = Amdb_CreateFolderImageList();
   if( himl )
      TreeView_SetImageList( hwndTreeCtl, himl, 0 );

   /* Fill list with in-basket contents.
    */
   Amdb_FillFolderTree( hwndTreeCtl, FTYPE_ALL|FTYPE_TOPICS, fShowTopicType );

   /* Find the active folder and select it.
    *
    * Now checks whether there is a current topic or folder. Fixed a GPF when
    * opening the rules dialog without an inbasket window. YH 17/04/96
    */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.ptl )
      hItem = Amdb_FindTopicItem( hwndTreeCtl, (PTL)curmsg.ptl );
   else if( NULL != curmsg.pcl )
      hItem = Amdb_FindFolderItem( hwndTreeCtl, (PCL)curmsg.pcl );
   else if( NULL != curmsg.pcat )
      hItem = Amdb_FindCategoryItem( hwndTreeCtl, (PCAT)curmsg.pcat );
   else
      hItem = TreeView_GetRoot( hwndTreeCtl );
   if( 0L == hItem )
      {
      EndDialog( hwnd, FALSE );
      return( TRUE );
      }
   TreeView_SelectItem( hwndTreeCtl, hItem );

   /* Create a tooltip for the list window.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
   lpfnDefWindowProc = SubclassWindow( hwndList, ListBoxWndProc );

   /* Show rules for selected item.
    */
   tv.hItem = TreeView_GetSelection( hwndTreeCtl );
   tv.mask = TVIF_PARAM;
   VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
   CmdRulesDlg_ListRules( hwnd, (LPVOID)tv.lParam, TRUE );
   FillTooltipFromListbox( hwndList );

   /* Creates a temp list of rules that is a copy of the main list and
    * used instead of the main list in case the user cancels changes
    * made. YH 17/04/96
    */
   CreateTempList();
   gfRulesChanged = FALSE;

   /* Are rules enabled?
    */
   CheckDlgButton( hwnd, IDD_DISABLERULES, !fRulesEnabled );
   EnableRulesControls( hwnd );
   return( TRUE );
}

/* This function enables or disables the controls on the rules
 * dialog.
 */
void FASTCALL EnableRulesControls( HWND hwnd )
{
   BOOL fEnabled;
   BOOL fAnyRules = TRUE;
   int count, index;
   HWND hwndList;



   hwndList = GetDlgItem( hwnd, IDD_RULES );
   index = ListBox_GetCurSel( hwndList );
   fEnabled = !IsDlgButtonChecked( hwnd, IDD_DISABLERULES );
   count = 0;
   if( fEnabled )
   {
      count = ListBox_GetCount( hwndList );
      if( count > 0 )
      {
         ListBox_GetText( hwndList, 0, lpTmpBuf );
         if( lstrcmpi(lpTmpBuf, GS(IDS_STR947)) == 0 )
            fAnyRules = FALSE;
      }
   }
   EnableControl( hwnd, IDD_LIST, fEnabled );
   EnableControl( hwnd, IDD_RULES, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_FIRST, ( fEnabled && fAnyRules && index > 0) );
   EnableControl( hwnd, IDD_UP, ( fEnabled && fAnyRules && index > 0) );
   EnableControl( hwnd, IDD_DOWN, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_LAST, ( fEnabled && fAnyRules && index < count) );
   EnableControl( hwnd, IDD_NEW, fEnabled );
   EnableControl( hwnd, IDD_EDIT, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_REMOVE, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_APPLY, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_MAILFORM, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_MAILFORMLIST, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_LIST2, ( fEnabled && fAnyRules ) );
   EnableControl( hwnd, IDD_NEWFOLDER, ( fEnabled && fAnyRules ) );


}

/* This function fills the tooltip control with a list of all
 * strings in the listbox.
 */
void FASTCALL FillTooltipFromListbox( HWND hwndList )
{
   int count;
   RECT rc;
   int top;
   int c;

   /* Start from first visible item in listbox.
    */
   count = ListBox_GetCount( hwndList );
   top = ListBox_GetTopIndex( hwndList );
   GetClientRect( hwndList, &rc );

   /* Delete any existing tooltip.
    */
   if( NULL != hwndTooltip )
      DestroyWindow( hwndTooltip );
   hwndTooltip = CreateWindow( TOOLTIPS_CLASS, "", 0, 0, 0, 0, 0, GetParent( hwndList ), 0, hRscLib, 0L );
   SendMessage( hwndTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 0L );
   for( c = top; c < count; ++c )
      {
      int cxSize;

      /* Get the width of this string.
       */
      ListBox_GetText( hwndList, c, lpTmpBuf );
      cxSize = GetStringExtent( hwndList, lpTmpBuf );
      if( cxSize > rc.right )
         {
         TOOLINFO ti;

         /* Create a tooltip for this item.
          */
         ti.cbSize = sizeof(TOOLINFO);
         ti.uFlags = 0;
         ti.lpszText = LPSTR_TEXTCALLBACK;
         ti.hwnd = GetParent( hwndList );
         ti.uId = c;
         ti.hinst = hRscLib;
         ListBox_GetItemRect( hwndList, c, &ti.rect );
         if( IsRectEmpty( &ti.rect ) )
            break;
         SendMessage( hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)(LPSTR)&ti );
         }
      }
}

/* This function lists the rules for the specified folder.
 */
void FASTCALL CmdRulesDlg_ListRules( HWND hwnd, LPVOID pFolder, BOOL fMoveGadgets )
{
   RULEHEADER FAR * lprh;
   HWND hwndList;
   int cxLongest;
   int count;

   /* Scan the rule base, looking for all rules that
    * match the specified folder.
    */
   hwndList = GetDlgItem( hwnd, IDD_RULES );
   ListBox_ResetContent( hwndList );
   cxLongest = 0;
   for( lprh = lprhFirst; lprh; lprh = lprh->lprhNext )
      {
      RULEITEM FAR * lpri;
      int index;

      if( lprh->pFolder == pFolder )
         for( lpri = lprh->lpriFirst; lpri; lpri = lpri->lpriNext )
            {
            int cxSize;

            CreateDescription( lpri, lpTmpBuf );
            cxSize = GetStringExtent( hwndList, lpTmpBuf );
            if( cxSize > cxLongest )
               {
               ListBox_SetHorizontalExtent( hwndList, cxSize );
               cxLongest = cxSize;
               }
            index = ListBox_InsertString( hwndList, -1, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpri );
            }
      }

   /* If any rules found, enable the first one.
    */
   if( ( count = ListBox_GetCount( hwndList ) ) > 0 )

      ListBox_SetCurSel( hwndList, 0 );

   else
      {
      ListBox_InsertString( hwndList, -1, GS(IDS_STR947) );
      ListBox_SetCurSel( hwndList, -1 );
      }

   EnableControl( hwnd, IDD_RULES, count > 0 );
   EnableControl( hwnd, IDD_EDIT, count > 0 );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
   EnableControl( hwnd, IDD_APPLY, count > 0 );

   /* Set the Up/Down buttons.
    */
   if( fMoveGadgets )
      {
      EnableControl( hwnd, IDD_PAD1, count > 0 );
      
      PicButton_SetBitmap( GetDlgItem( hwnd, IDD_FIRST ), hInst, IDB_BUTTONFIRST );
      PicButton_SetBitmap( GetDlgItem( hwnd, IDD_UP ), hInst, IDB_BUTTONUP );
      PicButton_SetBitmap( GetDlgItem( hwnd, IDD_DOWN ), hInst, IDB_BUTTONDOWN );
      PicButton_SetBitmap( GetDlgItem( hwnd, IDD_LAST ), hInst, IDB_BUTTONLAST );

      EnableControl( hwnd, IDD_FIRST, FALSE );
      EnableControl( hwnd, IDD_UP, FALSE );
      EnableControl( hwnd, IDD_DOWN, count > 1 );
      EnableControl( hwnd, IDD_LAST, count > 1 );
      }

}

/* This function returns the width of a string that is about to be added to
 * the listbox.
 */
int FASTCALL GetStringExtent( HWND hWndListBox, char * lpString )
{
   HDC hDCListBox;
   HFONT hFontOld;
   HFONT hFontNew;
   TEXTMETRIC tm;
   SIZE size;
 
   hDCListBox = GetDC( hWndListBox );
   hFontNew = (HFONT)SendMessage( hWndListBox, WM_GETFONT, 0, 0L );
   hFontOld = SelectObject( hDCListBox, hFontNew );
   GetTextMetrics( hDCListBox, &tm );
   GetTextExtentPoint( hDCListBox, lpString, strlen(lpString), &size );
   size.cx += tm.tmAveCharWidth;
   SelectObject( hDCListBox, hFontOld );
   ReleaseDC( hWndListBox, hDCListBox );
   return( size.cx );
}

/* This function receives all mouse messages relating to the subclassed
 * tooltip control.
 */
LRESULT CALLBACK ListBoxWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
   #ifdef WIN32
      case WM_MOUSEWHEEL:
   #endif
      case WM_VSCROLL:
         CallWindowProc( lpfnDefWindowProc, hwnd, message, wParam, lParam );
         FillTooltipFromListbox( hwnd );
         return( 0L );
         
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP: {
         MSG msg;

         msg.lParam = lParam;
         msg.wParam = wParam;
         msg.message = message;
         msg.hwnd = hwnd;
         SendMessage( hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
         break;
         }
      }
   return( CallWindowProc( lpfnDefWindowProc, hwnd, message, wParam, lParam ) );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL CmdRulesDlg_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TTN_NEEDTEXT: {
         TOOLTIPTEXT FAR * lpttt;
         HWND hwndList;
         UINT cbLen;

         lpttt = (TOOLTIPTEXT FAR *)lpNmHdr;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         cbLen = ListBox_GetTextLen( hwndList, lpttt->hdr.idFrom );
         if( NULL != lpTTString )
            FreeMemory( &lpTTString );
         if( fNewMemory( &lpTTString, cbLen + 1 ) )
            {
            ListBox_GetText( hwndList, lpttt->hdr.idFrom, lpTTString );
            lpttt->lpszText = lpTTString;
            lpttt->hinst = NULL;
            return( TRUE );
            }
         return( FALSE );
         }

      case TVN_SELCHANGED: {
         LPNM_TREEVIEW lpnmtv;
         TV_ITEM tv;

         /* Display the rules for the selected
          * folder.
          */
         lpnmtv = (LPNM_TREEVIEW)lpNmHdr;
         tv.hItem = lpnmtv->itemNew.hItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
         CmdRulesDlg_ListRules( hwnd, (LPVOID)tv.lParam, TRUE );
         FillTooltipFromListbox( GetDlgItem( hwnd, IDD_RULES ) );
         break;
         }

      case TVN_GETDISPINFO: {
         TV_DISPINFO FAR * lptvi;

         lptvi = (TV_DISPINFO FAR *)lpNmHdr;
         if( Amdb_IsCategoryPtr( (PCAT)lptvi->item.lParam ) )
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELCATEGORY;
               lptvi->item.iSelectedImage = IBML_SELCATEGORY;
               }
            else
               {
               lptvi->item.iImage = IBML_CATEGORY;
               lptvi->item.iSelectedImage = IBML_CATEGORY;
               }
            }
         else
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELFOLDER;
               lptvi->item.iSelectedImage = IBML_SELFOLDER;
               }
            else
               {
               lptvi->item.iImage = IBML_FOLDER;
               lptvi->item.iSelectedImage = IBML_FOLDER;
               }
            }
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL CmdRulesDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_DISABLERULES:
         EnableRulesControls( hwnd );
         break;

      case IDD_FIRST:
         {
            HWND hwndList;
            int index, i;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            index = ListBox_GetCurSel( hwndList );
            SendMessage(hwndList, WM_SETREDRAW, 0, 0L);
            for (i=0;i<index;i++)
               FORWARD_WM_COMMAND(hwnd, IDD_UP, hwndList, 0, SendMessage);
            SendMessage(hwndList, WM_SETREDRAW, 1, 0L);
            ListBox_SetSel(hwndList, TRUE, 0);
            UpdateWindow(hwndList);
            break;
         }
      case IDD_UP: {
         RULEITEM FAR * lpriPrev;
         RULEITEM FAR * lpriSel;
         HWND hwndList;
         int index;

         /* Get the selected item and make sure it isn't at
          * the end of the list.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpriSel = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
         if( index > 0 )
            {
            int count;

            /* Get the item with which we swap.
             */
            lpriPrev = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index - 1 );

            /* Now exchange pointers.
             */
            if( lpriPrev->lpriPrev )
               lpriPrev->lpriPrev->lpriNext = lpriSel;
            else
               lpriSel->lprh->lpriFirst = lpriSel;
            lpriSel->lpriPrev = lpriPrev->lpriPrev;
            if( lpriSel->lpriNext )
               lpriSel->lpriNext->lpriPrev = lpriPrev;
            else
               lpriSel->lprh->lpriLast = lpriPrev;
            lpriPrev->lpriNext = lpriSel->lpriNext;
            lpriSel->lpriNext = lpriPrev;
            lpriPrev->lpriPrev = lpriSel;

            /* Remove the items from the listbox.
             */
            ListBox_DeleteString( hwndList, index );
            ListBox_DeleteString( hwndList, index - 1 );
            --index;

            /* Add in the second item so it becomes first
             */
            CreateDescription( lpriSel, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriSel );
            ++index;

            /* Add in the first item so it becomes last
             */
            CreateDescription( lpriPrev, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriPrev );

            /* Select the item we moved.
             */
            ListBox_SetCurSel( hwndList, --index );
            gfRulesChanged = TRUE;

            /* Fix the buttons.
             */
            count = ListBox_GetCount( hwndList );
            EnableControl( hwnd, IDD_FIRST, index > 0 );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            EnableControl( hwnd, IDD_LAST, index < count - 1 );
            }
         break;
         }

      case IDD_LAST:
         {
            HWND hwndList;
            int index, i, count;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            count = ListBox_GetCount( hwndList );
            index = count - ListBox_GetCurSel( hwndList ) ;
            SendMessage(hwndList, WM_SETREDRAW, 0, 0L);
            for (i=0;i<=index;i++)
               FORWARD_WM_COMMAND(hwnd, IDD_DOWN, hwndList, 0, SendMessage);
            SendMessage(hwndList, WM_SETREDRAW, 1, 0L);
            ListBox_SetSel(hwndList, TRUE, count);
            UpdateWindow(hwndList);
            break;
         }
      case IDD_DOWN: {
         RULEITEM FAR * lpriNext;
         RULEITEM FAR * lpriSel;
         HWND hwndList;
         int index;

         /* Get the selected item and make sure it isn't at
          * the end of the list.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpriSel = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
         if( index + 1 < ListBox_GetCount( hwndList ) )
            {
            int count;

            /* Get the item with which we swap.
             */
            lpriNext = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index + 1 );

            /* Now exchange pointers.
             */
            if( lpriNext->lpriNext )
               lpriNext->lpriNext->lpriPrev = lpriSel;
            else
               lpriSel->lprh->lpriLast = lpriSel;
            lpriSel->lpriNext = lpriNext->lpriNext;
            if( lpriSel->lpriPrev )
               lpriSel->lpriPrev->lpriNext = lpriNext;
            else
               lpriSel->lprh->lpriFirst = lpriNext;
            lpriNext->lpriPrev = lpriSel->lpriPrev;
            lpriNext->lpriNext = lpriSel;
            lpriSel->lpriPrev = lpriNext;

            /* Remove the items from the listbox.
             */
            ListBox_DeleteString( hwndList, index );
            ListBox_DeleteString( hwndList, index );

            /* Add in the second item so it becomes first
             */
            CreateDescription( lpriNext, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriNext );
            ++index;

            /* Add in the first item so it becomes last
             */
            CreateDescription( lpriSel, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriSel );

            /* Select the item we moved.
             */
            ListBox_SetCurSel( hwndList, index );
            gfRulesChanged = TRUE;

            /* Fix the buttons.
             */
            count = ListBox_GetCount( hwndList );
            EnableControl( hwnd, IDD_FIRST, index > 0 );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            EnableControl( hwnd, IDD_LAST, index < count - 1 );
            }
         break;
         }

      case IDD_NEW: {
         HWND hwndTreeCtl;
         TV_ITEM tv;

         /* Get the selected folder.
          */
         hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
         tv.hItem = TreeView_GetSelection( hwndTreeCtl );;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

         /* Create a rule for that folder.
          */
         gfRulesChanged = RuleCommand_New( hwnd, hwnd, (LPVOID)tv.lParam );
         break;
         }

      case IDD_APPLY: {
         HWND hwndTreeCtl;
         TV_ITEM tv;
         HCURSOR hOldCursor;

         /* Get the selected folder.
          */
         hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
         tv.hItem = TreeView_GetSelection( hwndTreeCtl );;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

         hOldCursor = SetCursor( hWaitCursor );
         EnumerateFolders( (LPVOID)tv.lParam, EnumApplyRules, 0L );
//       EnableControl( hwnd, IDD_APPLY, FALSE );
         SetCursor( hOldCursor );
         SendAllWindows( WM_APPCOLOURCHANGE, (WPARAM)(WIN_MESSAGE|WIN_THREAD|WIN_INFOBAR|WIN_INBASK), 0L );
         break;
         }

      case IDD_RULES:
         if( codeNotify == LBN_SELCHANGE )
            {
            int index;
            int count;

            index = ListBox_GetCurSel( hwndCtl );
            count = ListBox_GetCount( hwndCtl );
            EnableControl( hwnd, IDD_FIRST, index > 0 );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            EnableControl( hwnd, IDD_LAST, index < count - 1 );
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_EDIT:
         gfRulesChanged = RuleCommand_Edit( hwnd, GetDlgItem( hwnd, IDD_RULES ) );
         break;

      case IDD_REMOVE: {
         HWND hwndList;
         int count;
         int index;

         /* First remove the rule.
          */
         gfRulesChanged = RuleCommand_Remove( hwnd, hwnd );

         /* Fix the buttons.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         if( !IsWindowEnabled( hwndList ) )
         {
            count = 0;
            SetFocus( GetDlgItem( hwnd, IDD_NEW ) );
         }
         else
            count = ListBox_GetCount( hwndList );
         EnableControl( hwnd, IDD_FIRST, index > 0 );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, count && index < count - 1 );
         EnableControl( hwnd, IDD_LAST, count && index < count - 1 );
         break;
         }

      case IDOK:
         fRulesEnabled = !IsDlgButtonChecked( hwnd, IDD_DISABLERULES );
         Amuser_WritePPInt( szSettings, "RulesEnabled", fRulesEnabled );
 
         /* Now destroys temp list (used for cancelling changes)
          * YH 17/04/96
          */
         SaveRules();
         DestroyTempList();
         HandleDialogClose( hwnd, id );
         break;

      case IDCANCEL: {
         BOOL fCanClose;

         /* Now destroys the main rules list and saves the temp list
          * created when the dialog was opened so that any changes made
          * would be undone. YH 17/04/96
          */
         if( gfRulesChanged )
            {
            fCanClose = fMessageBox( hwnd, 0, GS(IDS_ASK_RULE_CANCEL), MB_YESNO|MB_ICONINFORMATION ) == IDYES;
            if( fCanClose )
               {
               MakeTempListPermanent();
               SaveRules();
               }
            }
         else
            fCanClose = TRUE;
         if( fCanClose )
            {
            DestroyTempList();
            HandleDialogClose( hwnd, id );
            }
         break;
         }
      }
}

/* This function edits the selected rule.
 * Now returns BOOL such that TRUE means rules have changed YH 17/04/96
 */
BOOL FASTCALL RuleCommand_Edit( HWND hwnd, HWND hwndList )
{
   RULEITEM FAR *lpri;
   BOOL fChanged;
   int index;

   /* Get the selected item.
    */
   index = ListBox_GetCurSel( hwndList );
   ASSERT( LB_ERR != index );

   /* Get the handle of the selected RULEITEM and pass
    * it to the rule editor.
    */
   lpri = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );

   /* The following was added by YH on 16/04/96 to fix a bug
    * where the list item being edited was not being re-displayed
    * reflect the change.
    */
   fChanged = Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_EDITRULE), CmdEditRuleDlg, (LPARAM)&lpri->ru );

   /* If the user clicks on O.K.
    */
   if( fChanged )
      {
      int i;
      int j;
      BOOL fDupe = FALSE;

      RULEITEM FAR * lpri2;

      /* Update the list item because of changes
       */ 
      i = ListBox_GetCount( hwndList );
      j = 0;
      while( j < i && !fDupe )
      {
      lpri2 = (RULEITEM FAR *)ListBox_GetItemData( hwndList, j );
      if( ( lpri2 == lpri ) && ( j != index ) )
         fDupe = TRUE;
      j++;
      }
      if( !fDupe )
      {
         ListBox_DeleteString( hwndList, index );
         CreateDescription( lpri, lpTmpBuf );
         ListBox_InsertString( hwndList, index, lpTmpBuf );
         /* Added following to fix GPF when editing item for the second time
          * YH 17/04/96
          */
         ListBox_SetItemData( hwndList, index, (LPARAM)lpri );
         /* Next line added YH 18/04/96.
          */
         ListBox_SetCurSel( hwndList, index );
      }
      else
         ListBox_SetCurSel( hwndList, j );
      }
//       ListBox_SetCurSel( hwndList, 0 );
   return( fChanged );
}

/* This function removes the selected rule.
 *
 * Now returns BOOL such that TRUE means rules have changed YH 17/04/96
 */
BOOL FASTCALL RuleCommand_Remove( HWND hwndOwner, HWND hwnd )
{
   RULEITEM FAR * lpri;
   HWND hwndList;
   int index;
   BOOL fChanged;

   /* Get the selected item.
    */
   hwndList = GetDlgItem( hwnd, IDD_RULES );
   ASSERT( NULL != hwndList );
   index = ListBox_GetCurSel( hwndList );

   /* Get the handle of the selected RULEITEM.
    */
   lpri = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
   fChanged = ( fMessageBox( hwndOwner, 0, GS(IDS_STR704), MB_YESNO|MB_ICONINFORMATION ) == IDYES );
   if( fChanged )
      {
      /* First delete the rule.
       */
      DeleteRule( lpri );

      /* Remove from the listbox.
       */
      if( ListBox_DeleteString( hwndList, index ) == index )
         --index;
      if( index == -1 )
         ListBox_InsertString( hwndList, -1, GS(IDS_STR947) );
      ListBox_SetCurSel( hwndList, index );

      /* Enable/disable controls.
       */
      EnableControl( hwnd, IDD_PAD1, index != -1 );
      EnableControl( hwnd, IDD_RULES, index != -1 );
      EnableControl( hwnd, IDD_EDIT, index != -1 );
      EnableControl( hwnd, IDD_REMOVE, index != -1 );
      EnableControl( hwnd, IDD_APPLY, index != -1 );
      }
   return( fChanged );
}

/* This function creates a new rule for the specified folder.
 *
 * Now returns BOOL such that TRUE means rules have changed YH 17/04/96
 */
BOOL FASTCALL RuleCommand_New( HWND hwndOwner, HWND hwnd, LPVOID pFolder )
{
   RULEITEM FAR * lpri;
   BOOL fChanged;
   RULE fi;

   /* Initialise a default RULE structure.
    */
   memset( &fi, 0, sizeof( RULE ) );
   fi.wFlags |= FILF_PRIORITY;
   fi.pData = pFolder;
   fi.pMoveData = Amdb_GetFirstCategory();
   fChanged = Adm_Dialog( hInst, hwndOwner, MAKEINTRESOURCE(IDDLG_EDITRULE), CmdEditRuleDlg, (LPARAM)(LPSTR)&fi );
   if( fChanged )
      if( NULL != ( lpri = (RULEITEM FAR *)CreateRule( &fi ) ) )
         {
         HWND hwndList;
         int index;
         int count;
         int i;
         int j;
         BOOL fDupe = FALSE;
         RULEITEM FAR * lpri2;

         /* Insert this rule into the list.
          */
         hwndList = GetDlgItem( hwnd, IDD_RULES );
         ASSERT( NULL != hwndList );
         if( !IsWindowEnabled( hwndList ) )
            ListBox_DeleteString( hwndList, 0 );

         i = ListBox_GetCount( hwndList );
         for( j = 0; j < i; j++ )
         {
            lpri2 = (RULEITEM FAR *)ListBox_GetItemData( hwndList, j );
            if( lpri2 == lpri )
               fDupe = TRUE;
         }
         if( !fDupe )
         {
         CreateDescription( lpri, lpTmpBuf );
         index = ListBox_AddString( hwndList, lpTmpBuf );
         ListBox_SetItemData( hwndList, index, (LPARAM)lpri );
         ListBox_SetCurSel( hwndList, index );

         /* Enable/disable buttons
          */
         EnableControl( hwnd, IDD_PAD1, TRUE );
         EnableControl( hwnd, IDD_RULES, TRUE );
         EnableControl( hwnd, IDD_EDIT, TRUE );
         EnableControl( hwnd, IDD_REMOVE, TRUE );
         EnableControl( hwnd, IDD_APPLY, TRUE );

         /* Set the up/down buttons.
          */
         count = ListBox_GetCount( hwndList );
         EnableControl( hwnd, IDD_FIRST, index > 0 );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, count && index < count - 1 );
         EnableControl( hwnd, IDD_LAST, count && index < count - 1 );
         }
         }

   return( fChanged );
}

/* This function dispatches messages for the Rules page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK FolderPropRules( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, FolderPropRules_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, FolderPropRules_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, FolderPropRules_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FolderPropRules_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPVOID pData;
   HWND hwndList;
   int count;

   /* Dereference and save the handle of the category, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   /* Fill the Rules list with the list of rules for this
    * folder.
    */
   CmdRulesDlg_ListRules( hwnd, pData, FALSE );

   /* Create a temporary copy of list that will be used if user cancels
    * changes made
    * YH 17/04/96
    */
   CreateTempList();
// ShowWindow( GetDlgItem( hwnd, IDD_APPLY), SW_HIDE );

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
   count = ListBox_GetCount( hwndList );
   EnableControl( hwnd, IDD_PAD1, count > 0 );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_FIRST ), hInst, IDB_BUTTONFIRST );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_UP ), hInst, IDB_BUTTONUP );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_DOWN ), hInst, IDB_BUTTONDOWN );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_LAST ), hInst, IDB_BUTTONLAST );
   EnableControl( hwnd, IDD_FIRST, FALSE );
   EnableControl( hwnd, IDD_UP, FALSE );
   EnableControl( hwnd, IDD_DOWN, count > 1 );
   EnableControl( hwnd, IDD_LAST, count > 1 );

   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL FolderPropRules_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_NEW: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( RuleCommand_New( GetParent( hwnd ), hwnd, pFolder ) )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_RULES: {
         HWND hwndList;
         int index, count;

         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         count = ListBox_GetCount( hwndList );
         EnableControl( hwnd, IDD_FIRST, index > 0 );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, index < count - 1 );
         EnableControl( hwnd, IDD_LAST, index < count - 1 );


         if( codeNotify != LBN_DBLCLK )
            break;
         }

      case IDD_EDIT:
         if( RuleCommand_Edit( GetParent( hwnd ), GetDlgItem( hwnd, IDD_RULES ) ) )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_REMOVE: {
         HWND hwndList;
         int index, count;

         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         if( RuleCommand_Remove( GetParent( hwnd ), hwnd ) )
            PropSheet_Changed( GetParent( hwnd ), hwnd );

         index = ListBox_GetCurSel( hwndList );
         count = ListBox_GetCount( hwndList );
         EnableControl( hwnd, IDD_FIRST, index > 0 );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, index < count - 1 );
         EnableControl( hwnd, IDD_LAST, index < count - 1 );
         if( index == -1 )
         {
            EnableControl( hwnd, IDD_DOWN, FALSE );
            SetFocus( GetDlgItem( hwnd, IDD_NEW ) );
         }



         break;
         }
      case IDD_FIRST:
         {
/*          HWND hwndList;
            int index, i;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            index = ListBox_GetCurSel( hwndList );
            for (i=0;i<index;i++)
               SendMessage(hwndList, IDD_LAST, 0, 0);
*/
            HWND hwndList;
            int index, i;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            index = ListBox_GetCurSel( hwndList );
            SendMessage(hwndList, WM_SETREDRAW, 0, 0L);
            for (i=0;i<index;i++)
               FORWARD_WM_COMMAND(hwnd, IDD_UP, hwndList, 0, SendMessage);
            SendMessage(hwndList, WM_SETREDRAW, 1, 0L);
            ListBox_SetSel(hwndList, TRUE, 0);
            UpdateWindow(hwndList);
            break;

         }
      case IDD_LAST:
         {
            HWND hwndList;
            int index, i, count;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            count = ListBox_GetCount( hwndList );
            index = count - ListBox_GetCurSel( hwndList ) ;
            SendMessage(hwndList, WM_SETREDRAW, 0, 0L);
            for (i=0;i<=index;i++)
               FORWARD_WM_COMMAND(hwnd, IDD_DOWN, hwndList, 0, SendMessage);
            SendMessage(hwndList, WM_SETREDRAW, 1, 0L);
            ListBox_SetSel(hwndList, TRUE, count);
            UpdateWindow(hwndList);

            break;
         }
/*
      case IDD_FIRST:
         {
            RULEITEM FAR * lpriPrev;
            RULEITEM FAR * lpriNext;
            RULEITEM FAR * lpriSel;
            RULEITEM FAR * lpriFirst;

            HWND hwndList;
            int index;

            // Get the selected item and make sure it isn't at
            // the end of the list.
            //
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
            index = ListBox_GetCurSel( hwndList );
            ASSERT( LB_ERR != index );
            lpriSel = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
            if( index > 0 )
               {
               int count;

               // Get the item with which we swap.
                
               lpriPrev = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index - 1 );
               lpriNext = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index + 1 );
               lpriFirst = (RULEITEM FAR *)ListBox_GetItemData( hwndList, 0 );

               // Now exchange pointers.
                
               if( lpriPrev )
                  lpriPrev->lpriNext = lpriNext;

               if( lpriNext )
                  lpriNext->lpriPrev = lpriPrev;


               lpriPrev->lpriNext = lpriSel->lpriNext;
               lpriSel->lpriNext = lpriPrev;
               lpriPrev->lpriPrev = lpriSel;

               // Remove the items from the listbox.
                
               ListBox_DeleteString( hwndList, index );
               --index;

               // Add in the first item so it becomes first
                
               CreateDescription( lpriSel, lpTmpBuf );
               index = ListBox_InsertString( hwndList, 0, lpTmpBuf );
               ListBox_SetItemData( hwndList, index, (LPARAM)lpriSel );
               ++index;

               // Select the item we moved.
                
               ListBox_SetCurSel( hwndList, 0 );
               gfRulesChanged = TRUE;

               // Fix the buttons.
                
               count = ListBox_GetCount( hwndList );
               EnableControl( hwnd, IDD_UP, index > 0 );
               EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            }
            break;
         }       
*/
      case IDD_UP: {
         RULEITEM FAR * lpriPrev;
         RULEITEM FAR * lpriSel;
         HWND hwndList;
         int index;

         /* Get the selected item and make sure it isn't at
          * the end of the list.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpriSel = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
         if( index > 0 )
            {
            int count;

            /* Get the item with which we swap.
             */
            lpriPrev = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index - 1 );

            /* Now exchange pointers.
             */
            if( lpriPrev->lpriPrev )
               lpriPrev->lpriPrev->lpriNext = lpriSel;
            else
               lpriSel->lprh->lpriFirst = lpriSel;
            lpriSel->lpriPrev = lpriPrev->lpriPrev;
            if( lpriSel->lpriNext )
               lpriSel->lpriNext->lpriPrev = lpriPrev;
            else
               lpriSel->lprh->lpriLast = lpriPrev;
            lpriPrev->lpriNext = lpriSel->lpriNext;
            lpriSel->lpriNext = lpriPrev;
            lpriPrev->lpriPrev = lpriSel;

            /* Remove the items from the listbox.
             */
            ListBox_DeleteString( hwndList, index );
            ListBox_DeleteString( hwndList, index - 1 );
            --index;

            /* Add in the second item so it becomes first
             */
            CreateDescription( lpriSel, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriSel );
            ++index;

            /* Add in the first item so it becomes last
             */
            CreateDescription( lpriPrev, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriPrev );

            /* Select the item we moved.
             */
            ListBox_SetCurSel( hwndList, --index );
            gfRulesChanged = TRUE;

            /* Fix the buttons.
             */
            count = ListBox_GetCount( hwndList );
            EnableControl( hwnd, IDD_FIRST, index > 0 );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            EnableControl( hwnd, IDD_LAST, index < count - 1 );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }

      case IDD_DOWN: {
         RULEITEM FAR * lpriNext;
         RULEITEM FAR * lpriSel;
         HWND hwndList;
         int index;

         /* Get the selected item and make sure it isn't at
          * the end of the list.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_RULES ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpriSel = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index );
         if( index + 1 < ListBox_GetCount( hwndList ) )
            {
            int count;

            /* Get the item with which we swap.
             */
            lpriNext = (RULEITEM FAR *)ListBox_GetItemData( hwndList, index + 1 );

            /* Now exchange pointers.
             */
            if( lpriNext->lpriNext )
               lpriNext->lpriNext->lpriPrev = lpriSel;
            else
               lpriSel->lprh->lpriLast = lpriSel;
            lpriSel->lpriNext = lpriNext->lpriNext;
            if( lpriSel->lpriPrev )
               lpriSel->lpriPrev->lpriNext = lpriNext;
            else
               lpriSel->lprh->lpriFirst = lpriNext;
            lpriNext->lpriPrev = lpriSel->lpriPrev;
            lpriNext->lpriNext = lpriSel;
            lpriSel->lpriPrev = lpriNext;

            /* Remove the items from the listbox.
             */
            ListBox_DeleteString( hwndList, index );
            ListBox_DeleteString( hwndList, index );

            /* Add in the second item so it becomes first
             */
            CreateDescription( lpriNext, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriNext );
            ++index;

            /* Add in the first item so it becomes last
             */
            CreateDescription( lpriSel, lpTmpBuf );
            index = ListBox_InsertString( hwndList, index, lpTmpBuf );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpriSel );

            /* Select the item we moved.
             */
            ListBox_SetCurSel( hwndList, index );
            gfRulesChanged = TRUE;

            /* Fix the buttons.
             */
            count = ListBox_GetCount( hwndList );
            EnableControl( hwnd, IDD_FIRST, index > 0 );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            EnableControl( hwnd, IDD_LAST, index < count - 1 );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }


      case IDD_APPLY: {
         LPVOID pFolder;
         HCURSOR hOldCursor;

         hOldCursor = SetCursor( hWaitCursor );
         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         EnumerateFolders( pFolder, EnumApplyRules, 0L );
//       EnableControl( hwnd, IDD_APPLY, FALSE );
         SetCursor( hOldCursor );
         SendAllWindows( WM_APPCOLOURCHANGE, (WPARAM)(WIN_MESSAGE|WIN_THREAD|WIN_INFOBAR|WIN_INBASK), 0L );
         break;
         }
      }
}

/* This function is called from EnumerateFolders for the
 * folders to be rethreaded.
 */
BOOL FASTCALL EnumApplyRules( PTL ptl, LPARAM lParam )
{
   PTH pthNext = NULL;
   PTH pth;

   Amdb_LockTopic( ptl );
   pth = Amdb_GetFirstMsg( ptl, VM_VIEWCHRON );
   if( NULL != pth )
      pthNext = Amdb_GetNextMsg( pth, VM_VIEWCHRON );
   while( NULL != pth )
      {
      MatchRuleOnMessage( pth );
      pth = pthNext;
      if( NULL != pth )
         pthNext = Amdb_GetNextMsg( pth, VM_VIEWCHRON );
      }
   Amdb_UnlockTopic( ptl );
   Amdb_DiscardTopic( ptl );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL FolderPropRules_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_RULES );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = IPP_RULES;
         break;

      case PSN_APPLY:
         /* Save changes to rules.
          */
         SaveRules();

         /* Make sure pending changes are only those made after apply
          * YH 17/04/96
          */
         DestroyTempList();
         CreateTempList();

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );

      case PSN_RESET:
         /* Cancel pending changes by saving the temporary list created
          * since previous apply init_dialog, whichever came last
          * YH 17/04/96
          */
         MakeTempListPermanent();
         SaveRules();
         break;
      }
   return( FALSE );
}

/* Handles the Rules Editor dialog.
 */
BOOL EXPORT CALLBACK CmdEditRuleDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CmdEditRuleDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Rules dialog.
 */
LRESULT FASTCALL CmdEditRuleDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CmdEditRuleDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CmdEditRuleDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEDITRULE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CmdEditRuleDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   RULE FAR * lpRule;
   BOOL fDelete;
   BOOL fCopy;
   HWND hwndList;
   CMDREC msi;
   HCMD hCmd;

   /* Dereference and store the pointer to the RULE structure.
    */
   lpRule = (RULE FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill the list of folders combo box.
    */
   if( 0 == FillListWithTopics( hwnd, IDD_LIST2, FTYPE_TOPICS|FTYPE_ALL ) )
      {
      EnableControl( hwnd, IDD_MOVETOFOLDER, FALSE );
      EnableControl( hwnd, IDD_COPY, FALSE );
      EnableControl( hwnd, IDD_LIST2, FALSE );
      }

   /* Fill list of mail forms, if any.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_MAILFORMLIST ) );
   hCmd = NULL;
   while( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) )
      if( msi.wCategory == CAT_FORMS )
         ComboBox_AddString( hwndList, msi.lpszString );
   if( 0 == ComboBox_GetCount( hwndList ) )
      EnableWindow( hwndList, FALSE );

   /* Set the text.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_TOEDIT ), lpRule->szToText );
   Edit_SetText( GetDlgItem( hwnd, IDD_FROMEDIT ), lpRule->szFromText );
   Edit_SetText( GetDlgItem( hwnd, IDD_SUBJECTEDIT ), lpRule->szSubjectText );
   Edit_SetText( GetDlgItem( hwnd, IDD_BODYEDIT ), lpRule->szBodyText );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_TOEDIT ), sizeof(lpRule->szToText) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROMEDIT ), sizeof(lpRule->szFromText) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SUBJECTEDIT ), sizeof(lpRule->szSubjectText) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_BODYEDIT ), sizeof(lpRule->szBodyText) - 1 );

   /* Set the CC option.
    */
   if( lpRule->wFlags & FILF_USECC )
      CheckDlgButton( hwnd, IDD_USECC, TRUE );

   /* Set logic options
    */
   if( lpRule->wFlags & FILF_NOT )
      CheckDlgButton( hwnd, IDD_NOT, TRUE );
   else if( lpRule->wFlags & FILF_OR )
      CheckDlgButton( hwnd, IDD_OR, TRUE );
   else
      CheckDlgButton( hwnd, IDD_AND, TRUE );

   /* Disable Use CC button if To field blank.
    */
   EnableControl( hwnd, IDD_USECC, *lpRule->szToText != '\0' );

   /* Set the match action area.
    */
   if( lpRule->wFlags & FILF_DELETE )
      CheckDlgButton( hwnd, IDD_DELETE, TRUE );
   if( lpRule->wFlags & FILF_GETARTICLE )
      CheckDlgButton( hwnd, IDD_GETARTICLE, TRUE );
   if( lpRule->wFlags & FILF_PRIORITY )
      CheckDlgButton( hwnd, IDD_PRIORITY, TRUE );
   if( lpRule->wFlags & FILF_READ )
      CheckDlgButton( hwnd, IDD_READ, TRUE );
   if( lpRule->wFlags & FILF_IGNORE )
      CheckDlgButton( hwnd, IDD_IGNORE, TRUE );
   if( lpRule->wFlags & FILF_MAILFORM )
      CheckDlgButton( hwnd, IDD_MAILFORM, TRUE );
   if( lpRule->wFlags & FILF_WATCH )
      CheckDlgButton( hwnd, IDD_WATCH, TRUE );
   if( lpRule->wFlags & FILF_READLOCK )
      CheckDlgButton( hwnd, IDD_READLOCK, TRUE );
// if( lpRule->wFlags & FILF_WITHDRAWN )
//    CheckDlgButton( hwnd, IDD_WITHDRAWN, TRUE );
   if( lpRule->wFlags & FILF_REGEXP )           // !!SM!!
      CheckDlgButton( hwnd, IDD_REGEXP, TRUE ); 
   if( lpRule->wFlags & FILF_KEEP )
      CheckDlgButton( hwnd, IDD_KEEP, TRUE );
   if( lpRule->wFlags & FILF_MARK )
      CheckDlgButton( hwnd, IDD_MARK, TRUE );

   if( lpRule->wFlags & FILF_MOVE )
      {
      HWND hwndCombo;
      int index;

      CheckDlgButton( hwnd, IDD_MOVETOFOLDER, TRUE );
      hwndCombo = GetDlgItem( hwnd, IDD_LIST2 );
      index = RealComboBox_FindItemData( hwndCombo, -1, (LPARAM)lpRule->pMoveData );
      ComboBox_SetCurSel( hwndCombo, index );
      }
   if( lpRule->wFlags & FILF_COPY )
      {
      HWND hwndCombo;
      int index;

      CheckDlgButton( hwnd, IDD_COPY, TRUE );
      hwndCombo = GetDlgItem( hwnd, IDD_LIST2 );
      index = RealComboBox_FindItemData( hwndCombo, -1, (LPARAM)lpRule->pMoveData );
      ComboBox_SetCurSel( hwndCombo, index );
      }
   if( lpRule->wFlags & FILF_MAILFORM )
      {
      HWND hwndCombo;
      int index;

      hwndCombo = GetDlgItem( hwnd, IDD_MAILFORMLIST );
      index = ComboBox_FindString( hwndCombo, -1, lpRule->szMailForm );
      ComboBox_SetCurSel( hwndCombo, index );
      }

   /* Set the New button
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_NEWFOLDER ), hInst, IDB_DOWNARROW );

   /* Disable other options if Delete selected.
    */
   fDelete = lpRule->wFlags & FILF_DELETE;
   fCopy = lpRule->wFlags & FILF_COPY;
   EnableControl( hwnd, IDD_PRIORITY, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_READ, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_IGNORE, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_MOVETOFOLDER, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_GETARTICLE, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_MAILFORM, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_MAILFORMLIST, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_LIST2, !fDelete );
   EnableControl( hwnd, IDD_NEWFOLDER, !fDelete );
   EnableControl( hwnd, IDD_COPY, !fDelete );
   EnableControl( hwnd, IDD_MARK, !fDelete && !fCopy );
// EnableControl( hwnd, IDD_WITHDRAWN, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_READLOCK, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_WATCH, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_KEEP, !fDelete && !fCopy );
   EnableControl( hwnd, IDD_DELETE, !fCopy );
   if( fCopyRule || fCopy )
   {
      EnableControl( hwnd, IDD_PAD8, FALSE );
      EnableControl( hwnd, IDD_PAD9, TRUE );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD8 ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD9 ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_COPY ), SW_SHOW );
   }
   else
   {

      EnableControl( hwnd, IDD_PAD8, TRUE );
      EnableControl( hwnd, IDD_PAD9, FALSE );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD8 ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD9 ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_COPY ), SW_HIDE );
      
/*    ShowWindow( GetDlgItem( hwnd, IDD_PAD9 ), SW_SHOW );
      EnableControl( hwnd, IDD_PAD8, FALSE );
      EnableControl( hwnd, IDD_PAD9, FALSE );
*/
   }

   return( TRUE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL CmdEditRuleDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_MOVETOFOLDER: {

         CheckDlgButton( hwnd, IDD_COPY, FALSE );

         break;
                        }
      case IDD_COPY: {
         BOOL fCopyChecked;

         fCopyChecked = IsDlgButtonChecked( hwnd, IDD_COPY );
         CheckDlgButton( hwnd, IDD_DELETE, FALSE );
         CheckDlgButton( hwnd, IDD_PRIORITY, FALSE );
         CheckDlgButton( hwnd, IDD_IGNORE, FALSE );
         CheckDlgButton( hwnd, IDD_READ, FALSE );
         CheckDlgButton( hwnd, IDD_GETARTICLE, FALSE );
         CheckDlgButton( hwnd, IDD_MARK, FALSE );
         CheckDlgButton( hwnd, IDD_KEEP, FALSE );
         CheckDlgButton( hwnd, IDD_READLOCK, FALSE );
         CheckDlgButton( hwnd, IDD_WATCH, FALSE );
         CheckDlgButton( hwnd, IDD_MOVETOFOLDER, FALSE );
         CheckDlgButton( hwnd, IDD_MAILFORM, FALSE );
         CheckDlgButton( hwnd, IDD_MAILFORMLIST, FALSE );
         EnableControl( hwnd, IDD_PRIORITY, !fCopyChecked );
         EnableControl( hwnd, IDD_READ, !fCopyChecked );
         EnableControl( hwnd, IDD_IGNORE, !fCopyChecked );
         EnableControl( hwnd, IDD_MOVETOFOLDER, !fCopyChecked );
         EnableControl( hwnd, IDD_GETARTICLE, !fCopyChecked );
         EnableControl( hwnd, IDD_MAILFORMLIST, !fCopyChecked );
         EnableControl( hwnd, IDD_MAILFORM, !fCopyChecked );
         EnableControl( hwnd, IDD_MARK, !fCopyChecked );
         EnableControl( hwnd, IDD_READLOCK, !fCopyChecked );
         EnableControl( hwnd, IDD_WATCH, !fCopyChecked );
         EnableControl( hwnd, IDD_KEEP, !fCopyChecked );
         EnableControl( hwnd, IDD_DELETE, !fCopyChecked );
         break;
         }

      case IDD_NEWFOLDER: {
         HMENU hPopupMenu;
         RECT rc;

         GetWindowRect( hwndCtl, &rc );
         hPopupMenu = GetSubMenu( hPopupMenus, 21 );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, rc.left, rc.bottom, 0, hwnd, NULL );
         break;
         }

      case IDD_TOEDIT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDD_USECC, Edit_GetTextLength( GetDlgItem( hwnd, IDD_TOEDIT ) ) > 0 );
         break;

      case IDD_DELETE: {
         BOOL fDelete;

         fDelete = IsDlgButtonChecked( hwnd, IDD_DELETE );
         CheckDlgButton( hwnd, IDD_PRIORITY, FALSE );
         EnableControl( hwnd, IDD_PRIORITY, !fDelete );
         EnableControl( hwnd, IDD_READ, !fDelete );
         EnableControl( hwnd, IDD_IGNORE, !fDelete );
         EnableControl( hwnd, IDD_MOVETOFOLDER, !fDelete );
         EnableControl( hwnd, IDD_GETARTICLE, !fDelete );
         EnableControl( hwnd, IDD_NEWFOLDER, !fDelete );
         EnableControl( hwnd, IDD_MAILFORMLIST, !fDelete );
         EnableControl( hwnd, IDD_LIST2, !fDelete );
         EnableControl( hwnd, IDD_MAILFORM, !fDelete );
         EnableControl( hwnd, IDD_COPY, !fDelete );
         EnableControl( hwnd, IDD_MARK, !fDelete );
//       EnableControl( hwnd, IDD_WITHDRAWN, !fDelete );
         EnableControl( hwnd, IDD_READLOCK, !fDelete );
         EnableControl( hwnd, IDD_WATCH, !fDelete );
         EnableControl( hwnd, IDD_KEEP, !fDelete );
//       EnableControl( hwnd, IDD_PAD8, !fDelete );
//       EnableControl( hwnd, IDD_PAD9, !fDelete );
   
         if( fCopyRule )
         {
            EnableControl( hwnd, IDD_PAD8, FALSE );
            EnableControl( hwnd, IDD_PAD9, !fDelete );
            ShowWindow( GetDlgItem( hwnd, IDD_PAD8 ), SW_HIDE );
            ShowWindow( GetDlgItem( hwnd, IDD_PAD9 ), SW_SHOW );
            ShowWindow( GetDlgItem( hwnd, IDD_COPY ), SW_SHOW );
         }
         else
         {
            EnableControl( hwnd, IDD_PAD8, !fDelete );
            EnableControl( hwnd, IDD_PAD9, FALSE );
            ShowWindow( GetDlgItem( hwnd, IDD_PAD8 ), SW_SHOW );
            ShowWindow( GetDlgItem( hwnd, IDD_PAD9 ), SW_HIDE );
            ShowWindow( GetDlgItem( hwnd, IDD_COPY ), SW_HIDE );
         }
         break;
         }

      case IDD_MAILFORMLIST:
         if( codeNotify == CBN_SELCHANGE )
            CheckDlgButton( hwnd, IDD_MAILFORM, TRUE );
         break;

      case IDD_LIST2:
         if( codeNotify == CBN_SELCHANGE )
            if( !IsDlgButtonChecked( hwnd, IDD_COPY ) )
               CheckDlgButton( hwnd, IDD_MOVETOFOLDER, TRUE );
         break;

      case IDM_NEWLOCALTOPIC: {
         int index;
         HWND hwndList;

         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST2 ) );
         CreateNewLocalTopic( hwnd );
         ComboBox_ResetContent(hwndList);
         
         /* Fill the list of folders combo box.
          */
         if( 0 == FillListWithTopics( hwnd, IDD_LIST2, FTYPE_TOPICS|FTYPE_ALL ) )
         {
         EnableControl( hwnd, IDD_MOVETOFOLDER, FALSE );
         EnableControl( hwnd, IDD_COPY, FALSE );
         EnableControl( hwnd, IDD_LIST2, FALSE );
         }

         /* Select the newly created topic 
          */
         if( ptlLocalTopicName != NULL )
            if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)ptlLocalTopicName ) ) )
                  {
                     ComboBox_SetCurSel( hwndList, index );
                     if( !IsDlgButtonChecked( hwnd, IDD_COPY ) )
                        CheckDlgButton( hwnd, IDD_MOVETOFOLDER, TRUE );
                  }
         break;
         }

      case IDM_NEWMAILFOLDER: {
         int index;
         HWND hwndList;

         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST2 ) );
         CreateNewMailFolder( hwnd );
         ComboBox_ResetContent(hwndList);

         /* Fill the list of folders combo box.
          */
         if( 0 == FillListWithTopics( hwnd, IDD_LIST2, FTYPE_TOPICS|FTYPE_ALL ) )
         {
         EnableControl( hwnd, IDD_MOVETOFOLDER, FALSE );
         EnableControl( hwnd, IDD_COPY, FALSE );
         EnableControl( hwnd, IDD_LIST2, FALSE );
         }

         /* Select the newly created topic 
          */
         if ( ptlNewMailBox != NULL )
            if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM) ptlNewMailBox ) ) )
            {
            ComboBox_SetCurSel( hwndList, index );
            if( !IsDlgButtonChecked( hwnd, IDD_COPY ) )
               CheckDlgButton( hwnd, IDD_MOVETOFOLDER, TRUE );
            }

         break;
         }

      case IDOK: {
         RULE FAR * lpRule;
         char sz[100];

         if( !IsDlgButtonChecked( hwnd, IDD_MOVETOFOLDER ) &&
             !IsDlgButtonChecked( hwnd, IDD_MAILFORM ) &&
              !IsDlgButtonChecked( hwnd, IDD_GETARTICLE ) &&
               !IsDlgButtonChecked( hwnd, IDD_PRIORITY ) && 
                !IsDlgButtonChecked( hwnd, IDD_READ ) &&
                !IsDlgButtonChecked( hwnd, IDD_IGNORE ) &&
                   !IsDlgButtonChecked( hwnd, IDD_WATCH ) &&
                !IsDlgButtonChecked( hwnd, IDD_KEEP ) &&
                !IsDlgButtonChecked( hwnd, IDD_COPY ) &&
                !IsDlgButtonChecked( hwnd, IDD_MARK ) &&
                !IsDlgButtonChecked( hwnd, IDD_READLOCK ) &&
//              !IsDlgButtonChecked( hwnd, IDD_WITHDRAWN ) &&
                 !IsDlgButtonChecked( hwnd, IDD_DELETE ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR1247), MB_OK|MB_ICONEXCLAMATION );
               break;
               }


         /* Get the pointer to the RULE structure.
          */
         lpRule = (RULE FAR *)GetWindowLong( hwnd, DWL_USER );

         lpRule->wFlags = 0;

         /* Fill the structure from the dialog.
          */
         if( IsDlgButtonChecked( hwnd, IDD_MOVETOFOLDER ) )
            {
            HWND hwndList;
            LPVOID pData;
            int index;

            lpRule->wFlags |= FILF_MOVE;
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST2 ) );
            index = ComboBox_GetCurSel( hwndList );
            if( CB_ERR == index )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR728), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            pData = (LPVOID)ComboBox_GetItemData( hwndList, index );
            if( NULL == pData || !Amdb_IsTopicPtr( pData ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR728), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            lpRule->pMoveData = pData;
            }

         if( IsDlgButtonChecked( hwnd, IDD_COPY ) )
            {
            HWND hwndList;
            LPVOID pData;
            int index;

            lpRule->wFlags |= FILF_COPY;
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST2 ) );
            index = ComboBox_GetCurSel( hwndList );
            if( CB_ERR == index )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR728), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            pData = (LPVOID)ComboBox_GetItemData( hwndList, index );
            if( NULL == pData || !Amdb_IsTopicPtr( pData ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR728), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            lpRule->pMoveData = pData;
            }

            /* Get the mail form, if one is chosen.
          */
         if( IsDlgButtonChecked( hwnd, IDD_MAILFORM ) )
            {
            HWND hwndList;
            int index;

            lpRule->wFlags |= FILF_MAILFORM;
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_MAILFORMLIST ) );
            index = ComboBox_GetCurSel( hwndList );
            if( CB_ERR == index )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR728), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            ComboBox_GetLBText( hwndList, index, lpRule->szMailForm );
            }

         /* Get the To search text. Must NOT be blank, but this should
          * have been taken care of earlier.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_TOEDIT ), sz, sizeof(sz) );
         StripLeadingTrailingSpaces( sz );
         strcpy( lpRule->szToText, sz );

         /* Get the From search text. Must NOT be blank, but this should
          * have been taken care of earlier.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_FROMEDIT ), sz, sizeof(sz) );
         StripLeadingTrailingSpaces( sz );
         strcpy( lpRule->szFromText, sz );

         /* Get the Subject search text. Must NOT be blank, but this should
          * have been taken care of earlier.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_SUBJECTEDIT ), sz, sizeof(sz) );
         StripLeadingTrailingSpaces( sz );
         strcpy( lpRule->szSubjectText, sz );

         /* Get the From search text. Must NOT be blank, but this should
          * have been taken care of earlier.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_BODYEDIT ), sz, sizeof(sz) );
         StripLeadingTrailingSpaces( sz );
         strcpy( lpRule->szBodyText, sz );



         /* Set the match actions.
          */
         if( IsDlgButtonChecked( hwnd, IDD_GETARTICLE ) )
            lpRule->wFlags |= FILF_GETARTICLE;
         if( IsDlgButtonChecked( hwnd, IDD_PRIORITY ) )
            lpRule->wFlags |= FILF_PRIORITY;
         if( IsDlgButtonChecked( hwnd, IDD_READ ) )
            lpRule->wFlags |= FILF_READ;
         if( IsDlgButtonChecked( hwnd, IDD_IGNORE ) )
            lpRule->wFlags |= FILF_IGNORE;
         if( IsDlgButtonChecked( hwnd, IDD_KEEP ) )
            lpRule->wFlags |= FILF_KEEP;
//       if( IsDlgButtonChecked( hwnd, IDD_WITHDRAWN ) )
//          lpRule->wFlags |= FILF_WITHDRAWN;
         if( IsDlgButtonChecked( hwnd, IDD_REGEXP ) ) // !!SM!!
            lpRule->wFlags |= FILF_REGEXP;   
         if( IsDlgButtonChecked( hwnd, IDD_MARK ) )
            lpRule->wFlags |= FILF_MARK;
         if( IsDlgButtonChecked( hwnd, IDD_WATCH ) )
            lpRule->wFlags |= FILF_WATCH;
         if( IsDlgButtonChecked( hwnd, IDD_READLOCK ) )
            lpRule->wFlags |= FILF_READLOCK;
         if( IsDlgButtonChecked( hwnd, IDD_DELETE ) )
            lpRule->wFlags = FILF_DELETE;

         /* Use CC?
          */
         if( IsDlgButtonChecked( hwnd, IDD_USECC ) )
            lpRule->wFlags |= FILF_USECC;

         /* Get logic options.
          */
         if( IsDlgButtonChecked( hwnd, IDD_OR ) )
            lpRule->wFlags |= FILF_OR;
         else if( IsDlgButtonChecked( hwnd, IDD_NOT ) )
            lpRule->wFlags |= FILF_NOT;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function creates a short description for the specified rule.
 */
void FASTCALL CreateDescription( RULEITEM FAR * lpri, char * pszDescription )
{
   BOOL fText;

   fText = FALSE;
   if( *lpri->ru.szToText || *lpri->ru.szFromText || *lpri->ru.szSubjectText || *lpri->ru.szBodyText )
      {
      strcpy( pszDescription, "Match " );
      if( *lpri->ru.szToText )
         {
         strcat( pszDescription, "\"" );
         strcat( pszDescription, lpri->ru.szToText );
         strcat( pszDescription, "\"" );
         fText = TRUE;
         }
      if( *lpri->ru.szFromText )
         {
         if( fText )
            strcat( pszDescription, ", " );
         strcat( pszDescription, "\"" );
         strcat( pszDescription, lpri->ru.szFromText );
         strcat( pszDescription, "\"" );
         fText = TRUE;
         }
      if( *lpri->ru.szSubjectText )
         {
         if( fText )
            strcat( pszDescription, ", " );
         strcat( pszDescription, "\"" );
         strcat( pszDescription, lpri->ru.szSubjectText );
         strcat( pszDescription, "\"" );
         fText = TRUE;
         }
      if( *lpri->ru.szBodyText )
         {
         if( fText )
            strcat( pszDescription, ", " );
         strcat( pszDescription, "\"" );
         strcat( pszDescription, lpri->ru.szBodyText );
         strcat( pszDescription, "\"" );
         }
      strcat( pszDescription, " and " );
      }
   else
      strcpy( pszDescription, "Always " );
   fText = FALSE;
   if( lpri->ru.wFlags & FILF_DELETE )
      {
      strcat( pszDescription, "delete" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_PRIORITY )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark priority" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_KEEP )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark keep" );
      fText = TRUE;
      }
/* if( lpri->ru.wFlags & FILF_WITHDRAWN ) !!SM!!
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "withdraw" );
      fText = TRUE;
      }*/
   if( lpri->ru.wFlags & FILF_WATCH )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "watch" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_READLOCK )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark read locked" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_MARK )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark bookmarked" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_COPY )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "copy to " );
      WriteShortFolderPathname( pszDescription + strlen(pszDescription), lpri->ru.pMoveData );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_READ )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark read" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_IGNORE )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mark ignore" );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_MOVE )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "move to " );
      WriteShortFolderPathname( pszDescription + strlen(pszDescription), lpri->ru.pMoveData );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_MAILFORM )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "mail form " );
      strcat( pszDescription, lpri->ru.szMailForm );
      fText = TRUE;
      }
   if( lpri->ru.wFlags & FILF_GETARTICLE )
      {
      if( fText )
         strcat( pszDescription, ", " );
      strcat( pszDescription, "retrieve article" );
      }
}

/* This function fills the specified control with a list of
 * all folders.
 */
int FASTCALL FillListWithFolders( HWND hwnd, int id )
{
   HWND hwndList;
   PCAT pcatLast;
   PCL pclLast;
   PCAT pcat;
   PCL pcl;

   pclLast = NULL;
   pcatLast = NULL;

   hwndList = GetDlgItem( hwnd, id );
   pcat = Amdb_GetFirstCategory();
   do {
      if( NULL == pcat )
         continue;
      pcl = Amdb_GetFirstFolder( pcat );
      do {
         int index;
         if( NULL == pcl )
            continue;
         if( NULL != pcat && pcat != pcatLast )
            {
            index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetCategoryName( pcat ) );
            ComboBox_SetItemData( hwndList, index, (LPARAM)pcat );
            pcatLast = pcat;
            }
         if( NULL != pcl && pcl != pclLast )
            {
            index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetFolderName( pcl ) );
            ComboBox_SetItemData( hwndList, index, (LPARAM)pcl );
            pclLast = pcl;
            }
         if( NULL != pcl )
            pcl = Amdb_GetNextFolder( pcl );
         }
      while( pcl );
      if( NULL != pcat )
         pcat = Amdb_GetNextCategory( pcat );
      }
   while( pcat );

   
   /* for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         {
         int index;

         index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetFolderName( pcl ) );
         ComboBox_SetItemData( hwndList, index, (LPARAM)pcl );
         }
*/ return( ComboBox_GetCount( hwndList ) );
}

/* This function fills the specified control with a list of
 * all folders.
 */
int FASTCALL FillListWithMailboxes( HWND hwnd, int id )
{
   HWND hwndList;
   PCAT pcatLast;
   PCL pclLast;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   pclLast = NULL;
   pcatLast = NULL;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   pcat = Amdb_GetFirstCategory();
   do {
      if( NULL == pcat )
         continue;
      pcl = Amdb_GetFirstFolder( pcat );
      do {
         if( NULL == pcl )
            continue;
         ptl = Amdb_GetFirstTopic( pcl );
         do {
            if( ptl && Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
               {
               int index;

               if( NULL != pcat && pcat != pcatLast )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetCategoryName( pcat ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)pcat );
                  pcatLast = pcat;
                  }
               if( NULL != pcl && pcl != pclLast )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetFolderName( pcl ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)pcl );
                  pclLast = pcl;
                  }
               if( NULL != ptl )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetTopicName( ptl ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)ptl );
                  }
               }
            if( NULL != ptl )
               ptl = Amdb_GetNextTopic( ptl );
            }
         while( ptl );
         if( NULL != pcl )
            pcl = Amdb_GetNextFolder( pcl );
         }
      while( pcl );
      if( NULL != pcat )
         pcat = Amdb_GetNextCategory( pcat );
      }
   while( pcat );

return( ComboBox_GetCount( hwndList ) );
}

/* This function fills the specified control with a list of all
 * topics of the specified type.
 */
int FASTCALL FillListWithTopics( HWND hwnd, int id, WORD wType )
{
   HWND hwndList;
   PCAT pcatLast;
   PCL pclLast;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   pclLast = NULL;
   pcatLast = NULL;
   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   pcat = Amdb_GetFirstCategory();
   do {
      if( NULL == pcat )
         continue;
      pcl = Amdb_GetFirstFolder( pcat );
      do {
         if( NULL == pcl )
            continue;
         ptl = Amdb_GetFirstTopic( pcl );
         do {
            if( FTYPE_ALL == (wType & FTYPE_TYPEMASK) || ( ptl && Amdb_GetTopicType( ptl ) == (wType & FTYPE_TYPEMASK) ) )
               {
               int index;

               if( NULL != pcat && pcat != pcatLast && ((FTYPE_CATEGORIES|FTYPE_CONFERENCES|FTYPE_TOPICS) & ( wType & FTYPE_GROUPMASK )) )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetCategoryName( pcat ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)pcat );
                  pcatLast = pcat;
                  }
               if( NULL != pcl && pcl != pclLast && ((FTYPE_CONFERENCES|FTYPE_TOPICS) & ( wType & FTYPE_GROUPMASK )) && NULL != Amdb_GetFirstTopic( pcl ) )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetFolderName( pcl ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)pcl );
                  pclLast = pcl;
                  }
               if( NULL != ptl && ( FTYPE_TOPICS & ( wType & FTYPE_GROUPMASK ) ) )
                  {
                  index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetTopicName( ptl ) );
                  ComboBox_SetItemData( hwndList, index, (LPARAM)ptl );
                  }
               }
            if( NULL != ptl )
               ptl = Amdb_GetNextTopic( ptl );
            }
         while( ptl );
         if( NULL != pcl )
            pcl = Amdb_GetNextFolder( pcl );
         }
      while( pcl );
      if( NULL != pcat )
         pcat = Amdb_GetNextCategory( pcat );
      }
   while( pcat );
   return( ComboBox_GetCount( hwndList ) );
}

/* Called when either the property sheet or main rule dialog boxes
 * are opened. The reason is so that a temporary copy of all the
 * rules are kept in case the user wants to cancel all the changes
 * (new, removed or edited rules) since the dialog was opened.
 * YH 17/04/96
 */
void FASTCALL CreateTempList( void )
{
   RULEHEADER FAR * lprh;
   RULEHEADER FAR * lprhNew;
   RULEITEM FAR * lpri;
   RULEITEM FAR * lpriNew;

   /* Copy all the rule headers from the permanent list
    */
   for( lprh = lprhFirst; lprh; lprh = lprh->lprhNext )
      {
      INITIALISE_PTR(lprhNew);
      if( fNewMemory( &lprhNew, sizeof( RULEHEADER ) ) )
         {
         if( NULL == lprhTempFirst )
            lprhTempFirst = lprhNew;
         else
            lprhTempLast->lprhNext = lprhNew;
         lprhNew->lprhNext = NULL;
         lprhNew->lprhPrev = lprhTempLast;
         lprhTempLast = lprhNew;
         lprhNew->lpriFirst = NULL;
         lprhNew->lpriLast = NULL;
         lprhNew->pFolder = lprh->pFolder;

         /* Copy all the rule items for each rule header
          */
         for( lpri = lprh->lpriFirst; lpri; lpri = lpri->lpriNext )
            {
            INITIALISE_PTR(lpriNew);
            if( fNewMemory( &lpriNew, sizeof( RULEITEM ) ) )
               {
               /* Link this rule structure to the end of the
                * linked list of rules.
                */
               if( NULL == lprhNew->lpriFirst )
                  lprhNew->lpriFirst = lpriNew;
               else
                  lprhNew->lpriLast->lpriNext = lpriNew;
               lpriNew->lpriPrev = lprhNew->lpriLast;
               lpriNew->lpriNext = NULL;
               lpriNew->lprh = lprhNew;
               lprhNew->lpriLast = lpriNew;

               /* Fill in the fields from the pRule structure.
                */
               lpriNew->ru = lpri->ru;
               }
            }
         }
      }
}

/* Destroys the temporary rules lists. Called when either the main
 * or property sheet rule dialogs are closed.
 * YH 17/04/96
 */
void FASTCALL DestroyTempList( void )
{
   RULEHEADER FAR * lprh;
   RULEITEM FAR * lpri;
   RULEHEADER FAR * lprhNext;
   RULEITEM FAR * lpriNext;

   for( lprh = lprhTempFirst; lprh; )
      {
      for( lpri = lprh->lpriFirst; lpri; )
         {
            lpriNext = lpri->lpriNext;
            FreeMemory( &lpri );
            lpri = lpriNext;
         }
      lprhNext = lprh->lprhNext;
      FreeMemory( &lprh );
      lprh = lprhNext;
      }

   lprhTempFirst = NULL;
   lprhTempLast = NULL;
}

/* This makes the temporary rules lists permament, and the permanent
 * lists temporary. Called in the event of a user cancel
 * YH 17/04/96
 */
void FASTCALL MakeTempListPermanent( void )
{
   RULEHEADER FAR * lprh;

   lprh = lprhFirst;
   lprhFirst = lprhTempFirst;
   lprhTempFirst = lprh;

   lprh = lprhLast;
   lprhLast = lprhTempLast;
   lprhTempLast = lprh;
}

/* Called by the property sheet when it closes, so that temporary rules
 * lists can be deleted.
 * YH 17/04/96
 */
void FASTCALL PropertySheetClosingRules( void )
{
   DestroyTempList();
}

/* Handles the Rule Dialog closing (not the property sheet)
 * YH 17/04/96
 */
void FASTCALL HandleDialogClose( HWND hwnd, int id )
{
   HWND hwndTreeCtl;
   HIMAGELIST himl;

   /* Delete tooltip window.
    */
   DestroyWindow( hwndTooltip );

   /* Free image list associated with the tree control
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
   himl = TreeView_GetImageList( hwndTreeCtl, 0 );
   Amctl_ImageList_Destroy( himl );
   if( NULL != lpTTString )
      FreeMemory( &lpTTString );
   EndDialog( hwnd, id == IDOK );
   gfRulesChanged = FALSE;
}

/* Special function to match two e-mail addresses. Case is ignored.
 * The main improvement here is that if the rule match specifies an
 * address with no qualifier (ie. spalmer in spalmer@compulink.co.uk)
 * then this code will match both:
 *
 *   spalmer
 *   spalmer@compulink.co.uk
 *
 * The rule may also be @domain, so if the rule is @ftech.co.uk, then
 * all these will match:
 *
 *   sp@ftech.co.uk
 *   postmaster@ftech.co.uk
 */
int FASTCALL AddressRuleMatch( LPSTR lpRuleStr, LPSTR lpFullAddress )
{
   LPSTR lpRuleStrStart;

   lpRuleStrStart = lpRuleStr;
   do {
      while( isspace( *lpFullAddress ) )
         ++lpFullAddress;
      if( *lpRuleStr == '@' )
         {
         /* If rule is @<domain>, scan for the @ portion
          * of the address and begin matching from there.
          */
         while( *lpFullAddress && *lpFullAddress != '@' )
            ++lpFullAddress;
         }
      while( *lpRuleStr != '\0' && *lpFullAddress != '\0' && *lpFullAddress != ' ' )
         {
         if( *lpFullAddress != '?' )
            if( !IsEq( *lpRuleStr, *lpFullAddress, FALSE ) )
               break;
         ++lpRuleStr;
         ++lpFullAddress;
         }
      if( ( *lpFullAddress == ' ' || *lpFullAddress == '\0' ) && *lpRuleStr == '\0' )
         return( 0 );
      if( *lpFullAddress == '@' && *lpRuleStr == '\0' )
         return( 0 );
      lpRuleStr = lpRuleStrStart;
      while( *lpFullAddress && *lpFullAddress != ' ' )
         ++lpFullAddress;
      }
   while( *lpFullAddress && *lpFullAddress == ' ' );
   return( -1 );
}
