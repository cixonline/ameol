/* SEARCH.C - Handles the Search command
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

#define  THIS_FILE   __FILE__

#define  USEREGEXP

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>
#include <ctype.h>
#include "amlib.h"
#include "toolbar.h"
#include "editmap.h"
#include "ameol2.h"
#include "rules.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  MAX_FIND_STRINGS        10
#define  MAX_WORDLEN             63

#define  SRW_CURRENTTOFIRST      0     /* Search direction == backward */
#define  SRW_CURRENTTOLAST       1     /* Search direction == forward */
#define  SRW_ENTIREFOLDER        2     /* Search direction == forward */

static BOOL fDefDlgEx = FALSE;                  /* DefDlg recursion flag trap */

extern BOOL fWordWrap;
static BOOL fOrigStrip;

typedef struct tagSEARCHITEM {
   char szTitle[ 60 ];                          /* Title of this search item */
   char szFindStr[ 100 ];                       /* CURRENT search strings */
   char szAuthor[ LEN_AUTHORLIST+1 ];           /* List of authors to match */
   char szFolderPath[ 100 ];                    /* Folder pathname */
   int iCommand;                                /* Command associated with this ID */
   BOOL fCase;                                  /* TRUE if case sensitive search */
   BOOL fWordOnly;                              /* TRUE if we only match full words */
   BOOL fMark;                                  /* TRUE if we search and mark all matches */
   BOOL fRegExp;                                /* TRUE if we use regular expressions */
   BOOL nSearchWhere;                           /* Direction: from current, from start, up or down */
   BOOL fSearchHdr;                             /* TRUE if we've got to search the header */
   BOOL fSearchBody;                            /* TRUE if we've got to search the body */
   WORD wFromDate;                              /* Range: packed date from */
   WORD wToDate;                                /* Range: packed date to */
   BOOL fUnreadOnly;                         /* TRUE if we only search unread messages */
} SEARCHITEM;

static char * pszFindStr[ MAX_FIND_STRINGS ];   /* Pointers to search strings */
static int cFindStr = 0;                        /* Number of defined search strings */
static LPSTR lpszSrchAuthor = NULL;             /* Compacted list of authors for faster scan */
static int wHdrSize;                            /* Size of header when scanning mail or usenet message */
static BOOL fCancelFind;                        /* TRUE if Close button clicked during scan */
static BOOL fFindStringsLoaded = FALSE;         /* Set TRUE if we've loaded the find strings */
static BOOL fSearching = TRUE;                  /* Set TRUE if we're actually searching */
static int nFindMode;                           /* How we search for messages */
static SEARCHITEM FAR siCur;                    /* Current search item */
static int idNextCommandID;                     /* Next available search command ID */

static CURMSG cm;                               /* Current message */
static PTL ptlFindEnd;                          /* Folder in which scan is limited */
static HPSTR hpszText;                          /* Text of current message */
static HPSTR hpszRealText;                      /* Start of text of current message */
static int offset;                              /* Offset in current message */
static BOOL fPthFirst;                          /* TRUE if this is the start of the scan */
static BOOL fSavedSearchesChanged = TRUE;       /* TRUE if the saved searches db changed */
static UINT cMatches;                           /* Number of matches found */
static UINT cMarked;                            /* Number of messages marked */

LPSTR GetReadableText(PTL ptl, LPSTR pSource); // !!SM!! 2043

BOOL FASTCALL DoSlowFind( HWND );
BOOL FASTCALL SlowPatternMatch( HWND );

void FASTCALL LoadSavedSearch( char *, SEARCHITEM * );

BOOL EXPORT CALLBACK FindProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL FindProc_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL FindProc_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL FindProc_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK FindOne( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL FindOne_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL FindOne_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL FindOne_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT FAR PASCAL SaveSearchDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL SaveSearchDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SaveSearchDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SaveSearchDlg_OnCommand( HWND, int, HWND, UINT );

#define  OP_AND         0
#define  OP_OR          1
#define  OP_STRING      2
#define  OP_NOT         3

typedef struct tagNODE {
   int wType;                                   /* Node type */
   struct tagNODE FAR * lpLeftNode;             /* Pointer to left node */
   struct tagNODE FAR * lpRightNode;            /* Pointer to right node */
} NODE, FAR * LPNODE;

static BOOL fNodePushed = FALSE;                /* TRUE if a node stacked */
static LPNODE lpPushedNode;                     /* Stacked node */
static LPNODE lpNodeTree = NULL;                /* Regular expression tree */

void FASTCALL DrawFolderInCombo( HWND, const DRAWITEMSTRUCT FAR *, BOOL );

LPNODE FASTCALL BuildRegExpTree( void );
LPNODE FASTCALL CreateNode( int, LPSTR );
LPNODE FASTCALL GetOperand( LPSTR FAR * );
LPNODE FASTCALL RegExpLevel0( LPSTR FAR * );
LPNODE FASTCALL RegExpLevel1( LPSTR FAR * );
void FASTCALL DeleteRegExpTree( LPNODE );
void FASTCALL PushNode( LPNODE );
BOOL FASTCALL ExecuteRegExpTree( LPNODE, int *, int *, HPSTR, BOOL, BOOL );

void FASTCALL BeginFind( HWND );
void FASTCALL EndFind( void );
BOOL FASTCALL DoFind( HWND );
LPVOID FASTCALL NextFindTopic( void );
void FASTCALL LoadFindStrings( void );
void FASTCALL SaveFindStrings( void );
void FASTCALL UpdateDirectionFlags( HWND );
BOOL FASTCALL ReadFindDialog( HWND );
void FASTCALL FillFindDialog( HWND, SEARCHITEM * );


/* This function creates a command table of all saved search
 * strings. The keyboard mappings will be set later when we install
 * the keyboard configuration.
 */
void FASTCALL InstallSavedSearchCommands( void )
{
   /* Initialise siCur.
    */
   siCur.fRegExp = TRUE;
   siCur.fCase = FALSE;
   siCur.fWordOnly = FALSE;
   siCur.fMark = FALSE;
   siCur.fSearchHdr = TRUE;
   siCur.fSearchBody = TRUE;
   siCur.wFromDate = 0;
   siCur.wToDate = 0;
   siCur.szFindStr[ 0 ] = '\0';
   siCur.szAuthor[ 0 ] = '\0';
   siCur.fUnreadOnly = FALSE;
}

/* This function handles the Find command.
 */
void FASTCALL GlobalFind( HWND hwnd )
{
   fOrigStrip = fStripHeaders;
   if( hSearchDlg )
      BringWindowToTop( hSearchDlg );
   else if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_FIND ), (DLGPROC)FindProc, 0L ) )
      {
      UpdateWindow( hwnd );
      BeginFind( hwnd );
      }
}

/* Description of function goes here
 */
BOOL EXPORT CALLBACK FindOne( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, FindOne_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the FindOne
 * dialog.
 */
LRESULT FASTCALL FindOne_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FindOne_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FindOne_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFINDONE );
         break;

      case WM_CLOSE:
         fCancelFind = TRUE;
         fSearching = FALSE;
         EnableWindow( hwndFrame, TRUE );
         EndFind();
         DestroyWindow( hwnd );
         SetFocus( hwndActive );
         hSearchDlg = NULL;
         return( 0L );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FindOne_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_HIDE );
   PostDlgCommand( hwnd, IDOK, 0L );
   fCancelFind = FALSE;
   return( TRUE );
}

int fSeachNo;

/* This function handles the WM_COMMAND message.
 */
void FASTCALL FindOne_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   TOPICINFO ti; /*!!SM!!*/

   switch( id )
      {
      case IDD_MARK:
         Amdb_MarkMsg( cm.pth, TRUE );
         OpenMsgViewerWindow( Amdb_TopicFromMsg( cm.pth ), cm.pth, FALSE );
         break;

      case IDD_NEXTMSG:
         fSeachNo = 0;
         if( hpszRealText )
            {
            Amdb_FreeMsgTextBuffer( hpszRealText );
            hpszText = NULL;
            hpszRealText = NULL;
            }

         if( cm.ptl != NULL )   /*!!SM!!*/
         {
            Amdb_GetTopicInfo( cm.ptl, &ti ); /*!!SM!!*/
            nFindMode = ti.vd.nViewMode; /*!!SM!!*/
         }

         cm.pth = siCur.nSearchWhere ? Amdb_GetNextMsg( cm.pth, nFindMode ) : Amdb_GetPreviousMsg( cm.pth, nFindMode );
         if( cm.pth == NULL )
            if( NextFindTopic() == NULL )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR345), MB_OK|MB_ICONEXCLAMATION );
               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
               }

      case IDOK:
         EnableControl( hwnd, IDOK, FALSE );
         EnableControl( hwnd, IDD_NEXTMSG, FALSE );
         EnableControl( hwnd, IDD_MARK, FALSE );
         if( !DoFind( hwnd ) )
            fMessageBox( hwnd, 0, GS(IDS_STR345), MB_OK|MB_ICONEXCLAMATION );
         else if( !siCur.fMark )
            {
            int idDef;

            EnableControl( hwnd, IDOK, *siCur.szFindStr );
            EnableControl( hwnd, IDD_NEXTMSG, TRUE );
            EnableControl( hwnd, IDD_MARK, cm.pth != NULL );
            idDef = ( *siCur.szFindStr ? IDOK : IDD_NEXTMSG );
            ChangeDefaultButton( hwnd, idDef );
            SetFocus( GetDlgItem( hwnd, idDef ) );
            break;
            }

      case IDCANCEL:
         if( fSearching )
            fCancelFind = TRUE;
         else
            PostMessage( hwnd, WM_CLOSE, 0, 0L );
         break;
      }
}

/* This function checks whether the message, topic or folder
 * specified by pcur is one currently being scanned by Find
 * and, if so, switches off Find.
 */
void FASTCALL SyncSearchPtrs( LPVOID pSomething )
{
   BOOL fKillSearch;

   fKillSearch = FALSE;
   if( (PTH)pSomething == cm.pth )
      fKillSearch = TRUE;
   if( (PTL)pSomething == cm.ptl )
      fKillSearch = TRUE;
   if( (PCL)pSomething == cm.pcl )
      fKillSearch = TRUE;
   if( (PCAT)pSomething == cm.pcat )
      fKillSearch = TRUE;
   if( fKillSearch )
      PostMessage( hSearchDlg, WM_CLOSE, 0, 0L );
}

/* Initiates for Find. If cm.pcl is NULL, we're scanning the entire messagebase. If
 * cm.pcl is non-NULL but cm.ptl is NULL, we're scanning an entire folder. Otherwise
 * we're just scanning one topic and cm.pcl/pclFindEnd can be ignored. The end of the scan
 * is when cm.ptl == ptlFindEnd and we've finished scanning ptlFindEnd.
 */
void FASTCALL BeginFind( HWND hwnd )
{
   LPVOID pData;
   TOPICINFO ti; /*!!SM!!*/

   /* Create a compact representation of the
    * author names.
    */
   if( *siCur.szAuthor )
      {
      if( lpszSrchAuthor )
         FreeMemory( &lpszSrchAuthor );
      lpszSrchAuthor = MakeCompactString( siCur.szAuthor );
      }

   /* Parse the folder pathname.
    */
   cm.pcl = NULL;
   cm.ptl = NULL;
   ParseFolderPathname( siCur.szFolderPath, &pData, FALSE, 0 );
   if( !pData )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR949), (LPSTR)siCur.szFolderPath );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      return;
      }

   if( Amdb_IsCategoryPtr( pData ) )
      cm.pcat = (PCAT)pData;
   else if( Amdb_IsFolderPtr( pData ) )
      {
      cm.pcl = (PCL)pData;
      cm.pcat = Amdb_CategoryFromFolder( cm.pcl );
      }
   else
      {
      cm.ptl = (PTL)pData;
      cm.pcl = Amdb_FolderFromTopic( cm.ptl );
      cm.pcat = Amdb_CategoryFromFolder( cm.pcl );
      }
   cm.pth = NULL;

   /* Set the handles of the start and end of the
    * search range.
    */
   if( !cm.pcl )
      {
      LPVOID pclFindEnd;

//    cm.pcat = Amdb_GetFirstCategory();
      cm.pcl = Amdb_GetFirstFolder( cm.pcat );
      cm.ptl = Amdb_GetFirstTopic( cm.pcl );
      pclFindEnd = siCur.nSearchWhere ? Amdb_GetLastFolder( cm.pcat ) : Amdb_GetFirstFolder( cm.pcat );
      ptlFindEnd = siCur.nSearchWhere ? Amdb_GetLastTopic( pclFindEnd ) : Amdb_GetFirstTopic( pclFindEnd );
      fPthFirst = FALSE;
      }
   else if( !cm.ptl )
      {
      cm.ptl = Amdb_GetFirstTopic( cm.pcl );
      ptlFindEnd = siCur.nSearchWhere ? Amdb_GetLastTopic( cm.pcl ) : Amdb_GetFirstTopic( cm.pcl );
      fPthFirst = FALSE;
      }
   else
      {
      ptlFindEnd = cm.ptl;
      fPthFirst = siCur.nSearchWhere != SRW_ENTIREFOLDER;
      }
   if( cm.ptl == NULL )
      {
      do {
         cm.pcl = siCur.nSearchWhere ? Amdb_GetNextFolder( cm.pcl ) : Amdb_GetPreviousFolder( cm.pcl );
         if( cm.pcl == NULL )
            return;
         cm.ptl = siCur.nSearchWhere ? Amdb_GetFirstTopic( cm.pcl ) : Amdb_GetLastTopic( cm.pcl );
         }
      while( cm.ptl == NULL );
      }
   Amdb_LockTopic( cm.ptl );

   Amdb_GetTopicInfo( cm.ptl, &ti );

   cm.pth = NULL;
   hpszText = NULL;
// if( ( nFindMode = vdDef.nViewMode ) != VM_VIEWCHRON )
//    nFindMode = VM_VIEWREFEX;

   nFindMode = ti.vd.nViewMode; // /*!!SM!!*/

   /* Build the regular expression tree.
    */
   if( '\0' == *siCur.szFindStr )
      lpNodeTree = NULL;
   else {
      if( siCur.fRegExp ) 
         lpNodeTree = BuildRegExpTree();
      else
         lpNodeTree = CreateNode( OP_STRING, siCur.szFindStr );
      if( NULL == lpNodeTree )
         {
         OutOfMemoryError( hwnd, FALSE, FALSE );
         return;
         }
      }

   /* Display the find dialog box.
    */
   if( siCur.fMark )
      hSearchDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_FINDMARK ), (DLGPROC)FindOne, 0L );
   else
      hSearchDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_FINDONE ), (DLGPROC)FindOne, 0L );
}

/* This function cleans up at the end of a search.
 */
void FASTCALL EndFind( void )
{
   if( lpNodeTree )
      DeleteRegExpTree( lpNodeTree );
   if( hpszRealText )
   {
      Amdb_FreeMsgTextBuffer( hpszRealText );
      hpszText = NULL;
      hpszRealText = NULL;
      fSeachNo = 0;
   }
   if( lpszSrchAuthor )
      {
      FreeMemory( &lpszSrchAuthor );
      lpszSrchAuthor = NULL;
      }
   if( NULL != cm.ptl )
      {
      Amdb_UnlockTopic( cm.ptl );
      cm.ptl = NULL;
      }

}

/* Scans for the search text.
 */
BOOL FASTCALL DoFind( HWND hwnd )
{
   return( DoSlowFind( hwnd ) );
}

/* Scans for the search text using the slow method.
 */
BOOL FASTCALL DoSlowFind( HWND hwnd )
{
   BOOL fFirstMatch = TRUE;
   HCURSOR hOldCursor;
   TOPICINFO ti; /*!!SM!!*/

   hOldCursor = SetCursor( hWaitCursor );
   EnableWindow( hwndFrame, FALSE );
   ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_SHOW );
   fSearching = TRUE;
   cMatches = 0;
   cMarked = 0;
   if( fCancelFind )
      goto Done;

   do {
      /* If no message handle, then get one.
       */
      if( cm.ptl != NULL ) 
      {
         Amdb_GetTopicInfo( cm.ptl, &ti ); /*!!SM!!*/
         nFindMode = ti.vd.nViewMode; /*!!SM!!*/
      }
      if( cm.pth == NULL ) 
         {
         if( fPthFirst && hwndTopic )
            {
            CURMSG unr;
   
            Ameol2_GetCurrentMsg( &unr );

            if( unr.ptl == cm.ptl )
               cm.pth = unr.pth;
            else
               cm.pth = siCur.nSearchWhere ? Amdb_GetFirstMsg( cm.ptl, nFindMode ) : Amdb_GetLastMsg( cm.ptl, nFindMode );
            }
         else
            cm.pth = siCur.nSearchWhere ? Amdb_GetFirstMsg( cm.ptl, nFindMode ) : Amdb_GetLastMsg( cm.ptl, nFindMode );
         fPthFirst = FALSE;
         hpszText = NULL;
         }

      /* Tell the user where we're searching.
       */
      wsprintf( lpTmpBuf, GS(IDS_STR950), Amdb_GetFolderName( cm.pcl ), Amdb_GetTopicName( cm.ptl ) );
      SetDlgItemText( hwnd, IDD_PAD1, lpTmpBuf );
      while( cm.pth != NULL )
         {
         if( fCancelFind )
            goto Done;
         TaskYield();
         if( fCancelFind )
            goto Done;
         if( SlowPatternMatch( hwnd ) )
            goto Done;
         cm.pth = siCur.nSearchWhere ? Amdb_GetNextMsg( cm.pth, nFindMode ) : Amdb_GetPreviousMsg( cm.pth, nFindMode );
         fFirstMatch = FALSE;
         }
      }
   while( NextFindTopic() );
Done:
   ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_HIDE );
   EnableWindow( hwndFrame, TRUE );
   SetCursor( hOldCursor );
   if( cMarked )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR358), cMarked );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }
   fSearching = FALSE;
   return( cMatches );
}


#define ORGSEARCH

/* This function locates the current search pattern in the
 * specified message.
 */
BOOL FASTCALL SlowPatternMatch( HWND hwnd )
{
   MSGINFO msginfo;
   int iMatchStrLen = 0;
   int iMatchStrPos = 0;
// int cchMaxText;
   BOOL fMatch;
   BOOL fDone;
   WORD wOldHSize;
   HEDIT hedit;
   HWND hwndEdit;
   int lSkip;

   /* If we're searching for text, then set hpszText to the text and offset to
    * the offset of the text in which the search begins.
    */
   if( lpNodeTree && hpszText == NULL && ( siCur.fSearchBody || siCur.fSearchHdr ) )
   {
      if( hpszText = hpszRealText = Amdb_GetMsgText( cm.pth ) )
      {
         WORD wType;
         int index2;

         if( fStripHeaders != fOrigStrip )
         {
            fStripHeaders = fOrigStrip;
            SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         }

         /* Skip the first (non-displayed) line.
          */
         while( *hpszText && *hpszText != '\r' )
            ++hpszText;
         if( *hpszText == '\r' )
            ++hpszText;
         if( *hpszText == '\n' )
            ++hpszText;

         /* Locate the point at which we begin searching.
          */
         offset = 0;
         wHdrSize = 0;
         wType = Amdb_GetTopicType( Amdb_TopicFromMsg( cm.pth ) );
         if( wType == FTYPE_NEWS || wType == FTYPE_MAIL )
         {
            if( ( index2 = HStrMatch( (LPSTR)hpszText, "\r\n\r\n", FALSE, FALSE ) ) != -1 )
            {
               if( siCur.fSearchHdr && !siCur.fSearchBody )
                  *(hpszText + index2) = '\0';
               else if( !siCur.fSearchHdr && siCur.fSearchBody )
               {
//#ifndef ORGSEARCH
                  HPSTR lpText;
                  PTL ptl;

                  ptl = Amdb_TopicFromMsg( cm.pth );
                  lpText = hpszText;
                  hpszText = GetReadableText(ptl, lpText);
                  offset = 0;

/*#else ORGSEARCH
                  offset = index2 + 4;
#endif ORGSEARCH*/

               }
               if( fStripHeaders )
               {
/*                LPSTR lpszPreText;

                  lpszPreText = GetShortHeaderText( cm.pth );
                  wsprintf( hpszText, "%s%s",  lpszPreText, hpszText);*/
                  wOldHSize = wHdrSize = index2 + 4;
               }
            }
         }
      }
   }

   /* Allow ESC to cancel a search.
    */
   if( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
      return( TRUE );

   /* Start of the matching algorithm.
    */
   if( ( siCur.fUnreadOnly && !Amdb_IsRead( cm.pth ) ) || !siCur.fUnreadOnly )
   {
      Amdb_GetMsgInfo( cm.pth, &msginfo );
      if( Amdb_GetTopicType( cm.ptl ) == FTYPE_MAIL )
         if( Amdb_GetOutboxMsgBit( cm.pth ) )
         {
            char szMyAddress[ LEN_MAILADDR + 1 ];
            char szMyAddress2[ LEN_MAILADDR + 1 ];

            if( Amdb_GetMailHeaderField( cm.pth, "From", szMyAddress, LEN_TOCCLIST, FALSE ) )
            {
               ParseFromField( szMyAddress, szMyAddress2, lpTmpBuf );
               strncpy( msginfo.szAuthor, szMyAddress2, sizeof(msginfo.szAuthor) );
            }
         }
   // fMatch = TRUE;
      fMatch = ( siCur.fSearchBody || siCur.fSearchHdr );
      iMatchStrPos = -1;
      if( lpNodeTree && hpszText )
         fMatch = fMatch && ExecuteRegExpTree( lpNodeTree, &iMatchStrPos, &iMatchStrLen, hpszText + offset, siCur.fCase, siCur.fWordOnly );
      if( *siCur.szAuthor )
      {
         LPSTR lpszAuthor;
         BOOL fAuthor = FALSE;

         for( lpszAuthor = lpszSrchAuthor; !fAuthor && *lpszAuthor; lpszAuthor += lstrlen( lpszAuthor ) + 1 )
            fAuthor = lstrcmpi( lpszAuthor, msginfo.szAuthor ) == 0;
         fMatch = fMatch && fAuthor;
      }
      if( siCur.wFromDate > 0 )
         fMatch = fMatch && ( msginfo.wDate >= siCur.wFromDate ) & ( msginfo.wDate <= siCur.wToDate );
   }
   else
      fMatch = FALSE;

   /* We've got a match, so take action!
    */
   fDone = FALSE;
   if( fMatch )
   {
      ++cMatches;
      if( siCur.fMark )
      {
         HWND hwndTopic2;
   
         Amdb_MarkMsg( cm.pth, TRUE );
         hwndTopic2 = Amdb_GetTopicSel( Amdb_TopicFromMsg( cm.pth ) );
         if( hwndTopic2 )
            SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)cm.pth );
         ++cMarked;
      }
      else
      {
         SetCurrentMsgEx( &cm, TRUE, FALSE, FALSE );
         if( iMatchStrPos < wHdrSize && fStripHeaders && iMatchStrPos != -1 && siCur.fSearchHdr )
         {
            fStripHeaders = !fStripHeaders;
            SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
//          Amuser_WritePPInt( szSettings, "ShowHeaders", fStripHeaders );
            wHdrSize = 0;
         }
         if( iMatchStrPos != -1 )
         {
            BEC_SELECTION bsel;
            PTL ptl;
            int lFlags;

            /* Highlight a search result.
             */
            hwndEdit = GetDlgItem( hwndTopic, IDD_MESSAGE );
            hedit = Editmap_Open( hwndEdit );
            bsel.lo = iMatchStrPos /*+ ( offset - wHdrSize )*/;
            bsel.hi = iMatchStrPos /*+ ( offset - wHdrSize )*/ + iMatchStrLen;

            lSkip = 0;

            ptl = Amdb_TopicFromMsg( cm.pth );
            if( NULL != ptl && fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( ptl ) == FTYPE_MAIL || Amdb_GetTopicType( ptl ) == FTYPE_NEWS ) )
            {
               LPSTR lpszPreText;
#ifdef USEBIGEDIT
               int lSkip;
#endif USEBIGEDIT
   
               lpszPreText = GetShortHeaderText( cm.pth );

               if( NULL != lpszPreText && *lpszPreText )
               {
                  bsel.lo += strlen( lpszPreText );
                  bsel.hi += strlen( lpszPreText );
                  lSkip = strlen( lpszPreText );
               }
               
#ifdef USEBIGEDIT
               if ( fMsgStyles ) /*!!SM!!*/
               {
                  lSkip = 0;

                  if(strstr(lpszPreText, "*Subject:*") ) lSkip += 2;
                  if(strstr(lpszPreText, "*From:*") ) lSkip += 2;
                  if(strstr(lpszPreText, "*To:*") ) lSkip += 2;
                  if(strstr(lpszPreText, "*CC:*") ) lSkip += 2;
                  if(strstr(lpszPreText, "*Date:*") ) lSkip += 2;
                  bsel.lo -= lSkip;
                  bsel.hi -= lSkip;
               }
#endif USEBIGEDIT
            }

            //Editmap_SetSel( hedit, hwndEdit, &bsel );
            //MakeEditSelectionVisible( hwnd, hwndEdit );
#ifndef USEBIGEDIT
            //Editmap_SetSel( hedit, hwndEdit, &bsel );
#endif USEBIGEDIT

            lFlags = 0;
            if ( siCur.fCase ) 
               lFlags = lFlags | SCFIND_MATCHCASE;
            if ( siCur.fWordOnly ) 
               lFlags = lFlags | SCFIND_WHOLEWORD;
            if ( siCur.fRegExp ) 
               lFlags = lFlags | SCFIND_REGEXP ;

            Editmap_Searchfor(hedit, hwndEdit, (LPSTR)lpNodeTree + sizeof(NODE), fSeachNo++, lSkip, lFlags );
            MakeEditSelectionVisible( hwnd, hwndEdit );
         }
         SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR951) );
         SetFocus( hwnd );
         offset += iMatchStrPos + 1;
         fDone = TRUE;
         }
   }

   if( !fDone && hpszRealText )
   {
      fSeachNo = 0;
      Amdb_FreeMsgTextBuffer( hpszRealText );
      hpszText = NULL;
      hpszRealText = NULL;
   }
   return( fDone );
}

/* Returns the handle of the next topic in which to scan. If the current topic is
 * actually the last topic, then it returns NULL. Otherwise it unlocks the current
 * topic, advances to the next (crossing folder boundaries if necessary) and
 * locks the new topic.
 */
LPVOID FASTCALL NextFindTopic( void )
{
   if( cm.ptl == ptlFindEnd )
      return( NULL );
   Amdb_UnlockTopic( cm.ptl );
   Amdb_DiscardTopic( cm.ptl );
   cm.ptl = siCur.nSearchWhere ? Amdb_GetNextTopic( cm.ptl ) : Amdb_GetPreviousTopic( cm.ptl );

   if( cm.ptl == NULL )
      {
      do {
         cm.pcl = siCur.nSearchWhere ? Amdb_GetNextFolder( cm.pcl ) : Amdb_GetPreviousFolder( cm.pcl );
         if( cm.pcl == NULL )
            return( NULL );
         cm.ptl = siCur.nSearchWhere ? Amdb_GetFirstTopic( cm.pcl ) : Amdb_GetLastTopic( cm.pcl );
         }
      while( cm.ptl == NULL );
      }
   Amdb_LockTopic( cm.ptl );
   return( cm.ptl );
}

/* This function fills the SEARCHITEM record from the specified saved
 * search structure.
 */
void FASTCALL LoadSavedSearch( char * pszSavedSearch, SEARCHITEM * psi )
{
   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( lpPathBuf, "\\search.ini" );
    GetPrivateProfileString( pszSavedSearch, "String", psi->szFindStr, psi->szFindStr, 100, lpPathBuf );
   GetPrivateProfileString( pszSavedSearch, "Author", psi->szAuthor, psi->szAuthor, LEN_AUTHORLIST+1, lpPathBuf );
   GetPrivateProfileString( pszSavedSearch, "Path", psi->szFolderPath, psi->szFolderPath, 100, lpPathBuf );
   psi->fCase = GetPrivateProfileInt( pszSavedSearch, "CaseMatch", psi->fCase, lpPathBuf );
   psi->fWordOnly = GetPrivateProfileInt( pszSavedSearch, "WordMatch", psi->fWordOnly, lpPathBuf );
   psi->fMark = GetPrivateProfileInt( pszSavedSearch, "Mark", psi->fMark, lpPathBuf );
   psi->fRegExp = GetPrivateProfileInt( pszSavedSearch, "RegExp", psi->fRegExp, lpPathBuf );
   psi->nSearchWhere = GetPrivateProfileInt( pszSavedSearch, "SearchScope", psi->nSearchWhere, lpPathBuf );
   psi->fSearchHdr = GetPrivateProfileInt( pszSavedSearch, "SearchHeader", psi->fSearchHdr, lpPathBuf );
   psi->fSearchBody = GetPrivateProfileInt( pszSavedSearch, "SearchBody", psi->fSearchBody, lpPathBuf );
   psi->wFromDate = GetPrivateProfileInt( pszSavedSearch, "DateFrom", psi->wFromDate, lpPathBuf );
   psi->wToDate = GetPrivateProfileInt( pszSavedSearch, "DateTo", psi->wToDate, lpPathBuf );
   psi->fUnreadOnly = GetPrivateProfileInt( pszSavedSearch, "UnreadOnly", psi->fUnreadOnly, lpPathBuf );
}

/* Description of function goes here
 */
BOOL EXPORT CALLBACK FindProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = FindProc_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the FindProc
 * dialog.
 */
LRESULT FASTCALL FindProc_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FindProc_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FindProc_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFIND );
         break;

      case WM_USER+0x200: {
         HWND hwndList;
         SEARCHITEM si;
         int index;

         /* Get the handle of the record selected.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         if( index != CB_ERR )
            {
            char szSavedSearch[ 60 ];

            ComboBox_GetLBText( hwndList, index, szSavedSearch );
            LoadSavedSearch( szSavedSearch, &si );
            FillFindDialog( hwnd, &si );
            EnableControl( hwnd, IDD_DELETE, TRUE );
            siCur = si;
            }
         return( 0L );
         }

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FindProc_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;
   HWND hwndList;
   PTL ptlThis = NULL;
   PCL pclThis = NULL;
   int nSearchWhere;
   LPVOID pData;
   LPSTR lp;
   register int c;
   char szDateSep[ 4 ];

   /* Fill the list of saved searches.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( lpPathBuf, "\\search.ini" );
   GetPrivateProfileString( NULL, NULL, "", lpTmpBuf, LEN_TEMPBUF, lpPathBuf );
   for( lp = lpTmpBuf; *lp; lp += strlen( lp ) + 1 )
      ComboBox_AddString( hwndList, lp );
   EnableControl( hwnd, IDD_DELETE, FALSE );
   fSavedSearchesChanged = FALSE;

   /* Fill the list of find strings combo box.
    */
   if( !fFindStringsLoaded )
      LoadFindStrings();
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   ComboBox_LimitText( hwndEdit, 99 );
   for( c = 0; c < cFindStr; ++c )
      ComboBox_InsertString( hwndEdit, -1, pszFindStr[ c ] );

   /* Fill the list of folders combo box.
    */
   FillListWithTopics( hwnd, IDD_FOLDER, FTYPE_ALL|FTYPE_TOPICS );

   /* Figure out which folder we select.
    */
   nSearchWhere = SRW_ENTIREFOLDER;
   pData = Amdb_GetFirstCategory();
   if( NULL == pData )
      {
      EndDialog( hwnd, FALSE );
      return( TRUE );
      }
   if( hwndActive == hwndInBasket )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;

         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         pData = (LPVOID)tv.lParam;
         }
      }
   else if( hwndActive == hwndTopic && ( IsWindowVisible( GetDlgItem( hwndTopic, IDD_FOLDERLIST ) ) ) )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;
      hwndTreeCtl = GetDlgItem( hwndTopic, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( hwndTopic );

         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         pData = (LPVOID)tv.lParam;
         nSearchWhere = SRW_CURRENTTOFIRST;
         if( NULL == Amdb_GetPreviousMsg( lpWindInfo->lpMessage, vdDef.nViewMode ) )
            nSearchWhere = SRW_CURRENTTOLAST;
         }
      }
   else if( hwndActive == hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      pData = lpWindInfo->lpTopic;
      if( lpWindInfo->lpMessage )
         {
         nSearchWhere = SRW_CURRENTTOFIRST;
         if( NULL == Amdb_GetPreviousMsg( lpWindInfo->lpMessage, vdDef.nViewMode ) )
            nSearchWhere = SRW_CURRENTTOLAST;
         }
      }


   /* Fill the dialog from the current search item record.
    */
   siCur.nSearchWhere = nSearchWhere;
   if( CB_ERR == ( c = RealComboBox_FindItemData( GetDlgItem( hwnd, IDD_FOLDER ), -1, (LPARAM)pData ) ) )
         pData = (LPVOID)Amdb_GetFirstRealFolder( Amdb_GetFirstCategory() );
   WriteFolderPathname( siCur.szFolderPath, pData );
   FillFindDialog( hwnd, &siCur );

   GetProfileString( "intl", "sDate", ",", szDateSep, sizeof( szDateSep ) );
   wsprintf( lpTmpBuf, "Date format: dd%smm%syy", szDateSep, szDateSep );
   SetDlgItemText( hwnd, IDC_DATEFORMAT, lpTmpBuf );

   /* Set the OK button depending on what is enabled.
    */
   EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 ||
                              Edit_GetTextLength( GetDlgItem( hwnd, IDD_AUTHORLIST ) ) > 0 ||
                              Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO2 ) ) > 0 ||
                              Edit_GetTextLength( GetDlgItem( hwnd, IDD_FROM2 ) ) > 0 );
   return( TRUE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL FolderCombo_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmp;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_DATABASE) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmp );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight + 2 );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL FolderCombo_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   DrawFolderInCombo( hwnd, lpDrawItem, TRUE );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL FolderListCombo_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   DrawFolderInCombo( hwnd, lpDrawItem, FALSE );
}

/* This function draws an entry in a folder combo box with
 * optional indent.
 */
void FASTCALL DrawFolderInCombo( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem, BOOL fIndent )
{
   HBRUSH hbr;
   HFONT hFont;
   HBITMAP hbmp;
   LPCSTR lpsz;
   LPVOID pData;
   int offset;
   int width;
   COLORREF tmpT, tmpB;
   COLORREF T, B;
   RECT rc;

   /* Get the folder handle.
    */
   if( lpDrawItem->itemID == -1 )
      return;
   pData = (LPVOID)ComboBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
   if( NULL == pData )
      return;
   rc = lpDrawItem->rcItem;

   /* Set fSelect if this item is selected in the edit field.
    */
   if( rc.left > 0 )
      width = 0;
   else
      width = 16;

   /* Set the colours we're going to use.
    */
   if( lpDrawItem->itemState & ODS_SELECTED ) {
      T = GetSysColor( COLOR_HIGHLIGHTTEXT );
      B = GetSysColor( COLOR_HIGHLIGHT );
      }
   else {
      T = GetSysColor( COLOR_WINDOWTEXT );
      B = GetSysColor( COLOR_WINDOW );
      }
   hbr = CreateSolidBrush( B );
   FillRect( lpDrawItem->hDC, &rc, hbr );
   tmpT = SetTextColor( lpDrawItem->hDC, T );
   tmpB = SetBkColor( lpDrawItem->hDC, B );

   /* Set the bitmap, offset and text for the
    * selected item.
    */
   offset = 0;
   if( Amdb_IsCategoryPtr( pData ) )
      {
      hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_DATABASESELECTED) );
      lpsz = Amdb_GetCategoryName( (PCAT)pData );
      }
   else if( Amdb_IsFolderPtr( pData ) )
      {
      hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_FOLDERSELECTED) );
      if( fIndent )
         offset = width;
      lpsz = Amdb_GetFolderName( (PCL)pData );
      }
   else
      {
      PCL pcl;

      switch( Amdb_GetTopicType( (PTL)pData ) )
         {
         default:             hbmp = NULL; break;
         case FTYPE_NEWS:     hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_NEWSFOLDER) ); break;
         case FTYPE_MAIL:     hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_MAILFOLDER) ); break;
         case FTYPE_UNTYPED:  hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_UNTYPEDFOLDER) ); break;
         case FTYPE_CIX:      hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(fOldLogo ? IDB_CIXFOLDER_OLD : IDB_CIXFOLDER) ); break;
         case FTYPE_LOCAL:    hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_LOCALTOPIC) ); break;
         }
      if( fIndent )
         offset = 2 * width;
      pcl = Amdb_FolderFromTopic( (PTL)pData );
      if( 0 == width )
         {
         wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( (PTL)pData ) );
         lpsz = lpTmpBuf;
         }
      else
         lpsz = Amdb_GetTopicName( (PTL)pData );
      }

   /* Draw the bitmap.
    */
   rc.left += offset;
   if( NULL != hbmp )
      {
      Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top + 1, 16, 15, hbmp, 0 );
      DeleteBitmap( hbmp );
      }

   /* Draw the text.
    */
   rc.left += 22;
   hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
   DrawText( lpDrawItem->hDC, lpsz, -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );
   SelectFont( lpDrawItem->hDC, hFont );

   /* Clean up afterwards.
    */
   SetTextColor( lpDrawItem->hDC, tmpT );
   SetBkColor( lpDrawItem->hDC, tmpB );
   DeleteBrush( hbr );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL FindProc_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FOLDER:
         if( codeNotify == CBN_SELCHANGE )
            UpdateDirectionFlags( hwnd );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELENDOK )
            PostMessage( hwnd, WM_USER+0x200, 0, 0L );
         break;

      case IDD_DELETE: {
         HWND hwndList;
         int index;
         char sz[ 80 ];

         /* Get the index of the selected record.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );

         /* First query whether or not the user wants to delete.
          */
         ComboBox_GetLBText( hwndList, index, sz );
         wsprintf( lpTmpBuf, GS(IDS_STR952), (LPSTR)sz );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
            break;

         /* Okay. Delete it!
          */
         Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
         strcat( lpPathBuf, "\\search.ini" );
         WritePrivateProfileString( sz, NULL, NULL, lpPathBuf );

         /* If the record was deleted, also delete the title from
          * the list box and disable the Delete button if all
          * titles are now deleted.
          */
         if( ComboBox_DeleteString( hwndList, index ) == index )
            --index;
         if( index >= 0 )
            {
            char szSavedSearch[ 60 ];
            SEARCHITEM si;

            /* Set the next search record as the active record
             * and fill the dialog.
             */
            ComboBox_SetCurSel( hwndList, index );
            ComboBox_GetLBText( hwndList, index, szSavedSearch );
            LoadSavedSearch( szSavedSearch, &si );
            FillFindDialog( hwnd, &si );
            fSavedSearchesChanged = TRUE;
            siCur = si;
            }
         else
            {
            EnableControl( hwnd, IDD_DELETE, FALSE );
            PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDOK ), (LPARAM)TRUE );
            }
         break;
         }

      case IDD_SAVE: {
         SEARCHITEM si;

         /* Save the current search settings.
          */
         if( !ReadFindDialog( hwnd ) )
            break;
         si = siCur;
         si.szTitle[0] = '\0';
         if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SAVESEARCH), SaveSearchDlg, (LPARAM)(LPSTR)&si.szTitle ) )
            {
            HWND hwndList;
            int index;

            /* Save this search.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
            strcat( lpPathBuf, "\\search.ini" );
            WritePrivateProfileString( si.szTitle, "String", si.szFindStr, lpPathBuf );
            WritePrivateProfileString( si.szTitle, "Author", si.szAuthor, lpPathBuf );
            WritePrivateProfileString( si.szTitle, "Path", si.szFolderPath, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "CaseMatch", si.fCase, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "WordMatch", si.fWordOnly, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "Mark", si.fMark, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "RegExp", si.fRegExp, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "SearchScope", si.nSearchWhere, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "SearchHeader", si.fSearchHdr, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "SearchBody", si.fSearchBody, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "DateFrom", si.wFromDate, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "DateTo", si.wToDate, lpPathBuf );
            WritePrivateProfileInt( si.szTitle, "UnreadOnly", si.fUnreadOnly, lpPathBuf );

            /* If this entry already exists, select it.
             */
            if( ( index = ComboBox_FindStringExact( hwndList, -1, si.szTitle ) ) != CB_ERR )
               {
               ComboBox_SetCurSel( hwndList, index );
               EnableControl( hwnd, IDD_DELETE, TRUE );
               }
            else
               {
               /* Add this record to the list.
                */
               index = ComboBox_AddString( hwndList, si.szTitle );
               ComboBox_SetCurSel( hwndList, index );
               EnableControl( hwnd, IDD_DELETE, TRUE );
               fSavedSearchesChanged = TRUE;
               }
            }
         break;
         }

      case IDD_REGEXP: {
         BOOL fWordMatch;
         char szFindStr[ 100 ];

         ComboBox_GetText( GetDlgItem( hwnd, IDD_EDIT ), szFindStr, 100 );
         if( !IsDlgButtonChecked( hwnd, IDD_REGEXP ) )
            fWordMatch = strchr( szFindStr, ' ' ) == NULL;
         else
            fWordMatch = TRUE;
         EnableControl( hwnd, IDD_WORDMATCH, fWordMatch );
         break;
         }

      case IDD_EDIT:
         if( codeNotify == CBN_EDITCHANGE )
            {
            char szFindStr[ 100 ];

            ComboBox_GetText( hwndCtl, szFindStr, 100 );
            EnableControl( hwnd, IDD_WORDMATCH, strchr( szFindStr, ' ' ) == NULL );
            goto E1;
            }
         else if( codeNotify == CBN_SELCHANGE )
            PostDlgCommand( hwnd, IDD_EDIT, CBN_EDITCHANGE );
         break;
         
      case IDD_AUTHORLIST:
      case IDD_FROM2:
         if( codeNotify == EN_CHANGE ) {
            HWND hwndEdit;
            HWND hwndAuthor;
            HWND hwndDate;

E1:         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            hwndAuthor = GetDlgItem( hwnd, IDD_AUTHORLIST );
            hwndDate = GetDlgItem( hwnd, IDD_FROM2 );
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( hwndEdit ) > 0 ||
               Edit_GetTextLength( hwndAuthor ) > 0 ||
               Edit_GetTextLength( hwndDate ) > 0 );
            }
         break;

      case IDOK: {
         register int c;

         /* Read the dialog settings.
          */
         if( !ReadFindDialog( hwnd ) )
            break;

         /* Save the find string.
          */
         if( *siCur.szFindStr )
            {
            for( c = 0; c < cFindStr; ++c )
               if( strcmp( siCur.szFindStr, pszFindStr[ c ] ) == 0 )
                  {
                  free( pszFindStr[ c ] );
                  for( ; c < cFindStr - 1; ++c )
                     pszFindStr[ c ] = pszFindStr[ c + 1 ];
                  --cFindStr;
                  pszFindStr[ c ] = NULL;
                  break;
                  }
            if( cFindStr == MAX_FIND_STRINGS )
               {
               --cFindStr;
               free( pszFindStr[ cFindStr ] );
               }
            ++cFindStr;
            for( c = cFindStr; c > 0; --c )
               pszFindStr[ c ] = pszFindStr[ c - 1 ];
            pszFindStr[ 0 ] = _strdup( siCur.szFindStr );
            SaveFindStrings();
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function fills out the siCur structure from the settings in
 * the Find dialog box.
 */
BOOL FASTCALL ReadFindDialog( HWND hwnd )
{
   HWND hwndList;
   HWND hwndEdit;
   HWND hwndDate;
   LPVOID pData;
   int index;

   /* Read the search string from the user input. If the search string
    * was selected from the list, delete it from its old position and put
    * it at the top.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   ComboBox_GetText( hwndEdit, siCur.szFindStr, sizeof( siCur.szFindStr ) );

   /* Get the other options
    */
   siCur.fCase = IsDlgButtonChecked( hwnd, IDD_CASE );
   siCur.fMark = IsDlgButtonChecked( hwnd, IDD_MARK );
   siCur.fWordOnly = IsDlgButtonChecked( hwnd, IDD_WORDMATCH );
   siCur.fSearchBody = IsDlgButtonChecked( hwnd, IDD_BODY );
   siCur.fSearchHdr = IsDlgButtonChecked( hwnd, IDD_HEADERS );
   siCur.fRegExp = IsDlgButtonChecked( hwnd, IDD_REGEXP );
   siCur.fUnreadOnly = IsDlgButtonChecked( hwnd, IDD_UNREADONLY );

   /* Get the search direction
    */
   hwndList = GetDlgItem( hwnd, IDD_DIRECTION );
   index = ComboBox_GetCurSel( hwndList );
   siCur.nSearchWhere = (int)ComboBox_GetItemData( hwndList, index );

   /* Get the search folder pathname.
    */
   hwndList = GetDlgItem( hwnd, IDD_FOLDER );
   if( ( index = ComboBox_GetCurSel( hwndList ) ) != CB_ERR )
      {
      pData = (LPVOID)ComboBox_GetItemData( hwndList, index );
      WriteFolderPathname( siCur.szFolderPath, pData );
      }

   /* Get the date range for the search.
    * if wFromDate and wToDate are both 0 after this, then the date
    * is ignored in the search.
    */
   siCur.wFromDate = siCur.wToDate = 0;
   hwndDate = GetDlgItem( hwnd, IDD_FROM2 );
   if( IsWindowEnabled( hwndDate ) )
      if( Edit_GetTextLength( hwndDate ) > 0 )
         {
         AM_DATE dtFrom;
         AM_DATE dtTo;

         dtFrom.iDay = 1;
         dtFrom.iMonth = 1;
         dtFrom.iYear = 1970;
         if( !BreakDate( hwnd, IDD_FROM2, &dtFrom ) )
            return( FALSE );
         Amdate_GetCurrentDate( &dtTo );
         if( !BreakDate( hwnd, IDD_TO2, &dtTo ) )
            return( FALSE );
         siCur.wFromDate = Amdate_PackDate( &dtFrom );
         siCur.wToDate = Amdate_PackDate( &dtTo );
         if( siCur.wFromDate > siCur.wToDate )
            {
            fMessageBox( hwnd, 0, GS( IDS_STR92 ), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_FROM2 );
            return( FALSE );
            }
         }

   /* Get the author to include in the search criteria.
    * If the author name is empty, then it is ignored in the search.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_AUTHORLIST );
   *siCur.szAuthor = '\0';
   if( IsWindowEnabled( hwndEdit ) )
      Edit_GetText( hwndEdit, siCur.szAuthor, sizeof( siCur.szAuthor ) );
   return( TRUE );
}

/* This function fills the dialog from the specified SEARCHITEM record.
 */
void FASTCALL FillFindDialog( HWND hwnd, SEARCHITEM * psi )
{
   HWND hwndEdit;
   HWND hwndList;
   int index;
   LPVOID pData;
   char chDateSep[ 2 ];

   /* Set the search text.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   index = ComboBox_RealFindStringExact( hwndEdit, psi->szFindStr );
   if( CB_ERR == index )
      ComboBox_SetText( hwndEdit, psi->szFindStr );
   else
      ComboBox_SetCurSel( hwndEdit, index );

   /* Initialise the author field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_AUTHORLIST );
   Edit_SetText( hwndEdit, psi->szAuthor );
   Edit_LimitText( hwndEdit, sizeof( psi->szAuthor ) - 1 );

   /* Fill the options.
    */
   CheckDlgButton( hwnd, IDD_MARK, psi->fMark );
   CheckDlgButton( hwnd, IDD_CASE, psi->fCase );
   CheckDlgButton( hwnd, IDD_UNREADONLY, psi->fUnreadOnly );
   CheckDlgButton( hwnd, IDD_WORDMATCH, psi->fWordOnly );
   CheckDlgButton( hwnd, IDD_HEADERS, psi->fSearchHdr );
   CheckDlgButton( hwnd, IDD_BODY, psi->fSearchBody );
#ifdef USEREGEXP
   CheckDlgButton( hwnd, IDD_REGEXP, psi->fRegExp );
#else USEREGEXP
   CheckDlgButton( hwnd, IDD_REGEXP, FALSE );
   EnableControl( hwnd, IDD_REGEXP, FALSE );
#endif USEREGEXP

   /* Set the date fields.
    */
   GetProfileString( "intl", "sDate", "/", chDateSep, sizeof( chDateSep ) );
   if( psi->wFromDate > 0 )
      {
      AM_DATE date;
      char sz[ 9 ];

      Amdate_UnpackDate( psi->wFromDate, &date );
      wsprintf( sz, "%02u%c%02u%c%02u", date.iDay, *chDateSep, date.iMonth, *chDateSep, date.iYear % 100 );
      Edit_SetText( GetDlgItem( hwnd, IDD_FROM2 ), sz );
      }
   if( psi->wToDate > 0 )
      {
      AM_DATE date;
      char sz[ 9 ];

      Amdate_UnpackDate( psi->wToDate, &date );
      wsprintf( sz, "%02u%c%02u%c%02u", date.iDay, *chDateSep, date.iMonth, *chDateSep, date.iYear % 100 );
      Edit_SetText( GetDlgItem( hwnd, IDD_TO2 ), sz );
      }
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO2 ), 20 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROM2 ), 20 );

   /* Disable Word Match if current pattern contains spaces.
    */
   if( !psi->fRegExp )
      EnableControl( hwnd, IDD_WORDMATCH, strchr( psi->szFindStr, ' ' ) == NULL );
   else
      EnableControl( hwnd, IDD_WORDMATCH, TRUE );

   /* Set the folder name.
    */
   hwndList = GetDlgItem( hwnd, IDD_FOLDER );
   ParseFolderPathname( psi->szFolderPath, &pData, FALSE, 0 );
   if( ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)pData ) ) != CB_ERR )
      ComboBox_SetCurSel( hwndList, index );

   /* Initialise the directions field.
    */
   UpdateDirectionFlags( hwnd );
   hwndList = GetDlgItem( hwnd, IDD_DIRECTION );
   for( index = ComboBox_GetCount( hwndList ) - 1; index >= 0; --index )
      if( (int)ComboBox_GetItemData( hwndList, index ) == psi->nSearchWhere )
         break;
   if( index == -1 )
      index = 0;
   ComboBox_SetCurSel( hwndList, index );
}

/* This function handles the case where the folder and/or topic is
 * changed in the Find dialog. It updates the possible search directions
 * as appropriate.
 */
void FASTCALL UpdateDirectionFlags( HWND hwnd )
{
   HWND hwndCombo;
   HWND hwndList;
   int iFirstSel;
   int index;

   /* Initialise
    */
   hwndList = GetDlgItem( hwnd, IDD_FOLDER );
   hwndCombo = GetDlgItem( hwnd, IDD_DIRECTION );
   ComboBox_ResetContent( hwndCombo );

   /* If we're in a message window...
    */
   iFirstSel = 0;
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      index = ComboBox_GetCurSel( hwndList );
      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo->lpTopic == (PTL)ComboBox_GetItemData( hwndList, index ) )
         if( lpWindInfo->lpMessage )
            {
            index = ComboBox_InsertString( hwndCombo, -1, GS(IDS_STR953) );
            ComboBox_SetItemData( hwndCombo, index, SRW_CURRENTTOFIRST );
            index = ComboBox_InsertString( hwndCombo, -1, GS(IDS_STR954) );
            ComboBox_SetItemData( hwndCombo, index, SRW_CURRENTTOLAST );
            if( NULL == Amdb_GetPreviousMsg( lpWindInfo->lpMessage, vdDef.nViewMode ) )
               iFirstSel = 1;
            }
      }
   index = ComboBox_InsertString( hwndCombo, -1, GS(IDS_STR955) );
   ComboBox_SetItemData( hwndCombo, index, SRW_ENTIREFOLDER );
   ComboBox_SetCurSel( hwndCombo, iFirstSel );
}

/* This function loads the find search string history
 * list from the configuration file.
 */
void FASTCALL LoadFindStrings( void )
{
   register int c;
   char sz[ 10 ];

   /* Load the strings from the [Find] section.
    */
   cFindStr = 0;
   for( c = 0; c < MAX_FIND_STRINGS; ++c )
      {
      wsprintf( sz, "Find%u", c + 1 );
      if( !Amuser_GetPPString( szFind, sz, "", siCur.szFindStr, sizeof( siCur.szFindStr ) ) )
         break;
      pszFindStr[ c ] = _strdup( siCur.szFindStr );
      ++cFindStr;
      }

   /* Pad out the unused history items to NULL.
    */
   while( c < MAX_FIND_STRINGS )
      pszFindStr[ c++ ] = NULL;

   /* Make the first string the active string.
    */
   if( cFindStr )
      strcpy( siCur.szFindStr, pszFindStr[ 0 ] );
   fFindStringsLoaded = TRUE;
}

/* This function saves the find search string history
 * list to the configuration file.
 */
void FASTCALL SaveFindStrings( void )
{
   register int c;
   char sz[ 10 ];

   /* Only do this if any strings are loaded.
    */
   if( fFindStringsLoaded )
      for( c = 0; c < MAX_FIND_STRINGS; ++c )
         {
         wsprintf( sz, "Find%u", c + 1 );
         if( !Amuser_WritePPString( szFind, sz, pszFindStr[ c ] ) )
            break;
         }
}

/* This function handles the Save Search dialog box.
 */
BOOL EXPORT CALLBACK SaveSearchDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = SaveSearchDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Save Search
 * dialog.
 */
LRESULT FASTCALL SaveSearchDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, SaveSearchDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, SaveSearchDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSAVESEARCH );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SaveSearchDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Save the pointer to the store.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   Edit_LimitText( hwndEdit, 79 );
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SaveSearchDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT: {
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
         break;
         }

      case IDOK: {
         LPSTR lpszText;
         HWND hwndEdit;

         /* Save the search text.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         lpszText = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         Edit_GetText( hwndEdit, lpszText, 80 );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function deletes a regular expression tree.
 */
void FASTCALL DeleteRegExpTree( LPNODE lpNode )
{
   if( lpNode )
      {
      DeleteRegExpTree( lpNode->lpLeftNode );
      DeleteRegExpTree( lpNode->lpRightNode );
      FreeMemory( &lpNode );
      }
}

/* This function searches the regular expression tree.
 */
BOOL FASTCALL ExecuteRegExpTree( LPNODE lpNode, int * piMatchStrPos, int * piMatchStrLen,
                                 HPSTR hpText, BOOL fCase, BOOL fWordOnly )
{
   BOOL fMatchLeft;
   BOOL fMatchRight;
   int iMatchStrPos;

   if( NULL == lpNode )
      {
      fMessageBox( hFindDlg, 0, GS(IDS_STR814), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   switch( lpNode->wType )
      {
      case OP_AND:
         /* Match left subtree. If this fails, the entire expression is false, so
          * don't bother with the right subtree.
          */
         fMatchRight = FALSE;
         fMatchLeft = ExecuteRegExpTree( lpNode->lpLeftNode, piMatchStrPos, piMatchStrLen, hpText, fCase, fWordOnly );
         if( fMatchLeft )
            fMatchRight = ExecuteRegExpTree( lpNode->lpRightNode, piMatchStrPos, piMatchStrLen, hpText, fCase, fWordOnly );
         return( fMatchLeft && fMatchRight );

      case OP_OR:
         /* Match left subtree. If this succeeds, the entire expression is true, so
          * don't bother with the right subtree.
          */
         fMatchRight = FALSE;
         fMatchLeft = ExecuteRegExpTree( lpNode->lpLeftNode, piMatchStrPos, piMatchStrLen, hpText, fCase, fWordOnly );
         if( !fMatchLeft )
            fMatchRight = ExecuteRegExpTree( lpNode->lpRightNode, piMatchStrPos, piMatchStrLen, hpText, fCase, fWordOnly );
         return( fMatchLeft || fMatchRight );

      case OP_NOT:
         return( !ExecuteRegExpTree( lpNode->lpLeftNode, piMatchStrPos, piMatchStrLen, hpText, fCase, fWordOnly ) );

      case OP_STRING:
         iMatchStrPos = HStrMatch( hpText, (LPSTR)lpNode + sizeof(NODE), fCase, fWordOnly );
         if( -1 == *piMatchStrPos )
            {
            *piMatchStrPos = iMatchStrPos;
            *piMatchStrLen = lstrlen( (LPSTR)lpNode + sizeof(NODE) );
            }
         return( iMatchStrPos != -1 );
      }
   return( 0 );
}

/* This function builds a regular expression tree using the current search
 * pattern.
 */
LPNODE FASTCALL BuildRegExpTree( void )
{
   char * pszFindStr;

   pszFindStr = siCur.szFindStr;
   return( RegExpLevel0( &pszFindStr ) );
}

/* This function handles level 0 of the regular expression parser. At
 * this level, the lowest precedence operators are parsed.
 */
LPNODE FASTCALL RegExpLevel0( LPSTR FAR * lppText )
{
   LPNODE pNode1;

   pNode1 = RegExpLevel1( lppText );
   while( pNode1 && **lppText )
      {
      LPNODE pNode2;

      if( NULL == ( pNode2 = GetOperand( lppText ) ) )
         break;
      else if( pNode2->wType == OP_OR )
         {
         pNode2->lpLeftNode = pNode1;
         pNode2->lpRightNode = RegExpLevel1( lppText );
         pNode1 = pNode2;
         }
      else
         {
         /* Malformed expression!
          */
         fMessageBox( hFindDlg, 0, GS(IDS_STR814), MB_OK|MB_ICONEXCLAMATION );
         DeleteRegExpTree( pNode1 );
         pNode1 = NULL;
         break;
         }
      }
   return( pNode1 );
}

/* This function handles level 0 of the regular expression parser. At
 * this level, the highest precedence operators are parsed.
 */
LPNODE FASTCALL RegExpLevel1( LPSTR FAR * lppText )
{
   LPNODE pNode1;

   pNode1 = GetOperand( lppText );
   while( pNode1 && **lppText )
      {
      LPNODE pNode2;
      LPNODE pNode3;

      if( NULL == ( pNode2 = GetOperand( lppText ) ) )
         break;
      else if( pNode2->wType == OP_AND )
         {
         pNode2->lpLeftNode = pNode1;
         pNode2->lpRightNode = GetOperand( lppText );
         pNode1 = pNode2;
         }
      else if( pNode2->wType == OP_STRING )
         {
         if( NULL != ( pNode3 = CreateNode( OP_AND, NULL ) ) )
            {
            pNode3->lpLeftNode = pNode1;
            pNode3->lpRightNode = pNode2;
            }
         pNode1 = pNode3;
         }
      else
         {
         PushNode( pNode2 );
         break;
         }
      }
   return( pNode1 );
}

/* This function retrieves an operand.
 */
LPNODE FASTCALL GetOperand( LPSTR FAR * lppText )
{
   LPNODE lpNode;
   LPSTR lpText;
   register int c;
   char szWord[ MAX_WORDLEN+1 ];

   /* Node pushed? Return that instead.
    */
   if( fNodePushed )
      {
      fNodePushed = FALSE;
      return( lpPushedNode );
      }

   /* Skip initial spaces.
    */
   lpText = *lppText;
   while( isspace( *lpText ) )
      ++lpText;

   /* Parenthesis?
    */
   lpNode = NULL;
   if( *lpText == '(' )
      {
      ++lpText;
      lpNode = RegExpLevel0( &lpText );
      if( *lpText != ')' )
         {
         fMessageBox( GetFocus(), 0, GS(IDS_STR812), MB_OK|MB_ICONEXCLAMATION );
         return( NULL );
         }
      ++lpText;
      }
   else if( *lpText != ')' )
      {
      BOOL fInQuote;
      BOOL fQuoted;

      /* Parse the string.
       */
      fInQuote = fQuoted = FALSE;
      for( c = 0; *lpText; )
         {
         if( *lpText == '\"' )
            {
            fInQuote = !fInQuote;
            fQuoted = TRUE;
            }
         else if( ( *lpText == ')' || *lpText == ' ' ) && !fInQuote )
            break;
         else if( c < MAX_WORDLEN )
            szWord[ c++ ] = *lpText;
         ++lpText;
         }
      if( *lpText == '\0' && fInQuote )
         {
         fMessageBox( GetFocus(), 0, GS(IDS_STR813), MB_OK|MB_ICONEXCLAMATION );
         return( NULL );
         }
      szWord[ c ] = '\0';

      /* Skip trailing spaces.
       */
      while( isspace( *lpText ) )
         ++lpText;

      /* Test for operators.
       */
      if( !fQuoted && _strcmpi( szWord, "and" ) == 0 )
         lpNode = CreateNode( OP_AND, NULL );
      else if( !fQuoted && _strcmpi( szWord, "or" ) == 0 )
         lpNode = CreateNode( OP_OR, NULL );
      else if( !fQuoted && _strcmpi( szWord, "not" ) == 0 )
         {
         if( NULL != ( lpNode = CreateNode( OP_NOT, NULL ) ) )
            lpNode->lpLeftNode = GetOperand( &lpText );
         }
      else
         lpNode = CreateNode( OP_STRING, szWord );
      }

   /* Finished.
    */
   *lppText = lpText;
   return( lpNode );
}

/* This function creates a new node.
 */
LPNODE FASTCALL CreateNode( int wType, LPSTR lpText )
{
   LPNODE pNewNode;
   UINT cbNode;

   INITIALISE_PTR(pNewNode);
   cbNode = sizeof( NODE );
   if( lpText )
      cbNode += lstrlen( lpText ) + 1;
   if( fNewMemory( &pNewNode, cbNode ) )
      {
      pNewNode->wType = wType;
      pNewNode->lpLeftNode = NULL;
      pNewNode->lpRightNode = NULL;
      if( lpText )
         strcpy( (LPSTR)pNewNode + sizeof(NODE), lpText );
      }
   return( pNewNode );
}

/* This function pushes a node onto the node stack.
 */
void FASTCALL PushNode( LPNODE lpNode )
{
   ASSERT( FALSE == fNodePushed );
   lpPushedNode = lpNode;
   fNodePushed = TRUE;
}
