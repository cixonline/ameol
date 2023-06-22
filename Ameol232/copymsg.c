/* COPYMSG.C - Copies a message on CIX
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
#include "amlib.h"
#include <memory.h>
#include <string.h>
#include "shared.h"
#include "cix.h"
#include "cixip.h"
#include "cookie.h"
#include "rules.h"

/* The intrinsic version of this function does not
 * properly handle huge pointers!
 */
#pragma function(_fmemcpy)

static BOOL fDefDlgEx = FALSE;         /* DefDlg recursion flag trap */

void FASTCALL CopyLine( char *, char * );

/* This function installs the predefined copy commands.
 */
void FASTCALL InstallCopyCommands( void )
{
   LPSTR lpsz;

   INITIALISE_PTR(lpsz);

   /* Get the list of copy command entries and
    * install them into the command table.
    */
   if( fNewMemory( &lpsz, 8000 ) )
      {
      UINT c;

      Amuser_GetPPString( "Copy", NULL, "", lpsz, 8000 );
      for( c = 0; lpsz[ c ]; ++c )
         {
         char szTopic[ 128 ];

         Amuser_GetPPString( "Copy", &lpsz[ c ], "", szTopic, sizeof(szTopic) );
         /*AddCopyToCommandTable( &lpsz[ c ], szTopic );*/
         c += strlen( &lpsz[ c ] );
         }
      FreeMemory( &lpsz );
      }
}

/* This function copies a range of messages.
 */
BOOL FASTCALL DoCopyMsgRange( HWND hwnd, PTH FAR * lppth, int count, PTL ptlDst, BOOL fMove, BOOL fPrompt, BOOL fRepost )
{
   switch( Amdb_GetTopicType( ptlDst ) )
      {
      case FTYPE_MAIL:
      case FTYPE_LOCAL:
         return( DoCopyLocalRange( hwnd, lppth, count, ptlDst, fMove, fPrompt ) );

      default: {
         register int c;

         /* Copy one message at a time.
          */
         for( c = 0; c < count; ++c )
            if( !DoCopyMsg( hwnd, lppth[ c ], ptlDst, fMove, fPrompt, fRepost ) )
               break;
         return( c == count );
         }
      }
   return( FALSE );
}

/* This function copies a range of messages to another topic by physically
 * moving the messages. Unlike single message copy, it attemps to preserve
 * thread structure.
 */
BOOL FASTCALL DoCopyLocalRange( HWND hwnd, PTH FAR * lppth, int count, PTL ptlDst, BOOL fMove, BOOL fPrompt )
{
   DWORD FAR * nMappedTo;
   DWORD FAR * nMapped;
   register int c;
   int cbMapped;
   BOOL fSuccess;
   BOOL fOldAutoCollapse;
   LPWINDINFO lpWindInfo;
   HWND hwndGauge;
   HWND hDlg;
   PTH pthNext;

   /* Initialise variables.
    */
   INITIALISE_PTR(nMapped);
   INITIALISE_PTR(nMappedTo);
   cbMapped = 0;
   hDlg = NULL;
   hwndGauge = NULL;
   fSuccess = TRUE;
   pthNext = NULL;

   /* Get topic window info record.
    */
   lpWindInfo = GetBlock( hwnd );

   /* Create arrays to hold the old and new message numbers.
    */
   c = count;
   if( fNewMemory( &nMapped, c * sizeof(DWORD) ) )
      {
      if( fNewMemory( &nMappedTo, c * sizeof(DWORD) ) )
         {
         HCURSOR hOldCursor;

         /* More than 50 messages? Create a progress dialog.
          */
         if( c > 50 )
            {
            EnableWindow( hwndFrame, FALSE );
            hDlg = Adm_Billboard( hRscLib, hwnd,
                                 fMove ? MAKEINTRESOURCE(IDDLG_MOVINGMSGS) : MAKEINTRESOURCE(IDDLG_COPYINGMSGS) );
            hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
            SetDlgItemText( hDlg, IDD_STATUS, Amdb_GetTopicName( ptlDst ) );

            /* Set the progress bar gauge range.
             */
            SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, c ) );
            SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
            }

         /* Prevent the thread panes from being redrawn.
          */
         SendAllWindows( WM_LOCKUPDATES, WIN_THREAD, (LPARAM)TRUE );
         hOldCursor = SetCursor( hWaitCursor );

         /* Loop for each message, get it's details and the body and
          * use that to reconstruct the message in the destination
          * topic.
          */
         Amdb_LockTopic( ptlDst );
         for( c = 0; fSuccess && lppth[ c ] != NULL; ++c )
            {
            HPSTR lpszText;

            if( lpszText = Amdb_GetMsgText( lppth[ c ] ) )
               {
               LPSTR lpszRealText;
               MSGINFO msginfo;
               MSGINFO msgnewinfo;
               AM_DATE date;
               AM_TIME time;
               PTH pthNew;
               DWORD dwComment;
               DWORD cbText;
               register int i;
               PTL ptl;
               int r;

               /* Get message info for reconstruction.
                */
               Amdb_GetMsgInfo( lppth[ c ], &msginfo );
               Amdate_UnpackDate( msginfo.wDate, &date );
               Amdate_UnpackTime( msginfo.wTime, &time );
               cbText = Amdb_GetMsgTextLength( lppth[ c ] );
               lpszRealText = lpszText;

               /* If this is a comment, find the number that the
                * messgage to which this is a comment was allocated
                * in the destination topic.
                *
                * If the source topic is mail, so get the message ID of
                * the message being copied and locate it in the
                * destination folder.
                */
               dwComment = 0;
               ptl = Amdb_TopicFromMsg( lppth[ c ] );
               if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
                  {
                  BOOL fPossibleRefScanning = FALSE;
                  char szBuf[ 100 ];

                  Amdb_GetMailHeaderFieldInText( lpszText, "In-Reply-To", szBuf, sizeof(szBuf), FALSE );
                  if( szBuf[0] != '\x0' )
                     {
                     StripLeadingTrailingChars( szBuf, '<' );
                     StripLeadingTrailingChars( szBuf, '>' );
                     dwComment = Amdb_MessageIdToComment( ptlDst, szBuf );
                     }
                  if( dwComment == 0L )
                  {
                     char szSubject[ 80 ];

                     if( _strnicmp( msginfo.szTitle, "Re:", 3 ) == 0 )
                        fPossibleRefScanning = TRUE;
                     else if( _strnicmp( msginfo.szTitle, "Re[", 3 ) == 0 && isdigit( msginfo.szTitle[3] ) )
                        fPossibleRefScanning = TRUE;
                     Amdb_GetCookie( ptl, SUBJECT_IGNORE_TEXT, szSubject, "", 80 );
                     if( *szSubject )
                        fPossibleRefScanning = TRUE;
                     if( fPossibleRefScanning && fThreadMailBySubject )
                        dwComment = Amdb_LookupSubjectReference( ptlDst, msginfo.szTitle, szSubject );
                  }
               }
               else {
                  /* Adjust the comment field.
                   */
                  if( msginfo.dwComment )
                     for( i = 0; i < cbMapped; ++i )
                        if( nMapped[ i ] == msginfo.dwComment )
                           {
                           dwComment = nMappedTo[ i ];
                           break;
                           }

                  /* Replace any >>> CIX header with one that references the
                   * new local topic.
                   */
                  if( FTYPE_CIX == Amdb_GetTopicType( ptl ) ||
                      FTYPE_LOCAL == Amdb_GetTopicType( ptl ) )
                     {
                     TOPICINFO topicinfo;
                     LPSTR lpszBody;

                     Amdb_GetTopicInfo( ptlDst, &topicinfo );
                     lpszBody = AdvanceLine( lpszText );
                     wsprintf( lpTmpBuf, szCompactHdrTmpl,
                              Amdb_GetFolderName( Amdb_FolderFromTopic( ptlDst ) ),
                              Amdb_GetTopicName( ptlDst ),
                              topicinfo.dwMaxMsg + 1,
                              msginfo.szAuthor,
                              CountSize( lpszBody ),
                              date.iDay,
                              (LPSTR)pszCixMonths[ date.iMonth - 1 ],
                              date.iYear % 100,
                              time.iHour,
                              time.iMinute );
                     if( dwComment )
                        wsprintf( lpTmpBuf + strlen( lpTmpBuf ), " c%lu*", dwComment );
                     strcat( lpTmpBuf, "\r\n" );
                     lpszText = ConcatenateStrings( lpTmpBuf, lpszBody );
                     cbText = hstrlen( lpszText );
                     }
                  }

               /* Create the message!
                */
               r = Amdb_CreateMsg( ptlDst,
                          &pthNew,
                          (DWORD)-1,
                          dwComment,
                          msginfo.szTitle,
                          msginfo.szAuthor,
                          msginfo.szReply,
                          &date,
                          &time,
                          lpszText,
                          cbText, 0L );

               /* Couldn't copy? Why?
                */
               if( NULL == pthNew )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR754), msginfo.dwMsg, Amdb_GetTopicName( ptlDst ) );
                  fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  fSuccess = FALSE;
                  }
               else
                  {
                  /* Move any attachments
                   */
                  if( msginfo.dwFlags & MF_ATTACHMENTS )
                     {
                     ATTACHMENT attch;
                     PAH pah;

                     pah = NULL;
                     while( pah = Amdb_GetMsgAttachment( lppth[ c ], pah, &attch ) )
                        Amdb_CreateAttachment( pthNew, attch.szPath );
                     }

                  /* Delete the original.
                   */
                  if( fMove )
                     {
                     /* Determine the message we'll display after the move has
                      * completed.
                      */
                     if( NULL != lpWindInfo && ( NULL == pthNext || pthNext == lppth[ c ] ) )
                        {
                        pthNext = Amdb_GetNextMsg( lppth[ c ], lpWindInfo->vd.nViewMode );
                        if( NULL == pthNext )
                           pthNext = Amdb_GetPreviousMsg( lppth[ c ], lpWindInfo->vd.nViewMode );
                        }
                     Amdb_DeleteMsg( lppth[ c ], FALSE );
                     }

                  /* Mark the message read as it was copied, and set the
                   * MF_OUTBOXMSG bit if it was set this way. Also set other
                   * flags as required.
                   */
                  if( r == AMDBERR_NOERROR )
                  {

                  if( msginfo.dwFlags & MF_READ )
                     Amdb_MarkMsgRead( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_OUTBOXMSG )
                     Amdb_SetOutboxMsgBit( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_HDRONLY )
                     Amdb_MarkHdrOnly( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_KEEP )
                     Amdb_MarkMsgKeep( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_READLOCK )
                     {
                     Amdb_MarkMsgRead( pthNew, FALSE );
                     Amdb_MarkMsgReadLock( pthNew, TRUE );
                     }
                  if( msginfo.dwFlags & MF_PRIORITY )
                     Amdb_MarkMsgPriority( pthNew, TRUE, FALSE );
                  if( msginfo.dwFlags & MF_DELETED )
                     Amdb_MarkMsgDelete( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_MARKED )
                     Amdb_MarkMsg( pthNew, TRUE );
                  if( msginfo.dwFlags & MF_IGNORE )
                     Amdb_MarkMsgIgnore( pthNew, TRUE, FALSE );
   
                  /* Save the mapped details.
                   */
                  Amdb_GetMsgInfo( pthNew, &msgnewinfo );
                  nMapped[ cbMapped ] = msginfo.dwMsg;
                  nMappedTo[ cbMapped ] = msgnewinfo.dwMsg;
                  }
                  ++cbMapped;
                  }
               Amdb_FreeMsgTextBuffer( lpszRealText );
               }
            else
               {
               OutOfMemoryError( hwnd, FALSE, FALSE );
               fSuccess = FALSE;
               break;
               }
            if( hwndGauge != NULL )
               SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
            }

         /* Update the in-basket message count.
          */
         SendMessage( hwndInBasket, WM_UPDATE_UNREAD, 0, (LPARAM)ptlDst );
         SetCursor( hOldCursor );

         /* Delete any progress window.
          */
         if( hwndGauge != NULL )
            {
            EnableWindow( hwndFrame, TRUE );
            DestroyWindow( hDlg );
            }
         Amdb_UnlockTopic( ptlDst );
         FreeMemory( &nMappedTo );

         /* Prevent the thread panes from being redrawn.
          */
         fOldAutoCollapse = fAutoCollapse;
         fAutoCollapse = FALSE;
         SendAllWindows( WM_LOCKUPDATES, WIN_THREAD, (LPARAM)FALSE );
         fAutoCollapse = fOldAutoCollapse;

         /* Show the message after the drop.
          */
         if( fMove && NULL != lpWindInfo )
            ShowMsg( hwndTopic, pthNext, TRUE, TRUE, FALSE, NULL );
         }
      FreeMemory( &nMapped );
      }
   ShowUnreadMessageCount();
   return( fSuccess );
}

/* This function performs the actual copy process of the specified message from the
 * source topic to the destination topic.
 */
BOOL FASTCALL DoCopyMsg( HWND hwnd, PTH pth, PTL ptlDst, BOOL fMove, BOOL fPrompt, BOOL fRepost )
{
   MSGINFO msginfo;
   TOPICINFO topicinfo;
   HPSTR lpszText;
   AM_DATE date;
   AM_TIME time;
   PTL ptlSrc;
   PCL pclSrc;
   PCL pclDst;
   int nSrcTopicType;
   int nDstTopicType;
   BOOL fOk;

   /* Get the source topic.
    */
   ptlSrc = Amdb_TopicFromMsg( pth );

   /* Compute the destination conferences.
    */
   pclSrc = Amdb_FolderFromTopic( ptlSrc );
   pclDst = Amdb_FolderFromTopic( ptlDst );

   /* Get information about the message and destination.
    */
   Amdb_GetMsgInfo( pth, &msginfo );
   Amdb_GetTopicInfo( ptlDst, &topicinfo );
   fOk = TRUE;

   /* Get folder types.
    */
   nDstTopicType = Amdb_GetTopicType( ptlDst );
   nSrcTopicType = Amdb_GetTopicType( ptlSrc );

   /* Special case for copying to local topics. Create a new message
    * locally with the same contents as the message being copied.
    */
   if( nDstTopicType == FTYPE_LOCAL )
      {
      Amdate_UnpackDate( msginfo.wDate, &date );
      Amdate_UnpackTime( msginfo.wTime, &time );
      if( fPrompt )
         {
         wsprintf( lpTmpBuf, fMove ? GS(IDS_STR615) : GS(IDS_STR47), Amdb_GetFolderName( pclDst ), Amdb_GetTopicName( ptlDst ) );
         fOk = fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES;
         }
      if( fOk )
         {
         lpszText = Amdb_GetMsgText( pth );
         if( lpszText )
            {
            DWORD cbText;
            PTH pth2;

            /* Get the message being copied and append it to the
             * header.
             */
            cbText = Amdb_GetMsgTextLength( pth );
            if( Amdb_CreateMsg( ptlDst, &pth2, (DWORD)-1, 0, msginfo.szTitle, msginfo.szAuthor, "", &date, &time, lpszText, cbText, 0L ) == AMDBERR_NOERROR )
               {
               /* Mark the message read as it was copied, and set the
                * MF_OUTBOXMSG bit if it was set this way. Also set other
                * flags as required.
                */
               if( msginfo.dwFlags & MF_READ )
                  Amdb_MarkMsgRead( pth2, TRUE );
               if( msginfo.dwFlags & MF_OUTBOXMSG )
                  Amdb_SetOutboxMsgBit( pth2, TRUE );
               if( msginfo.dwFlags & MF_HDRONLY )
                  Amdb_MarkHdrOnly( pth2, TRUE );
               if( msginfo.dwFlags & MF_KEEP )
                  Amdb_MarkMsgKeep( pth2, TRUE );
               if( msginfo.dwFlags & MF_READLOCK )
                  {
                  Amdb_MarkMsgRead( pth2, FALSE );
                  Amdb_MarkMsgReadLock( pth2, TRUE );
                  }
               if( msginfo.dwFlags & MF_PRIORITY )
                  Amdb_MarkMsgPriority( pth2, TRUE, FALSE );
               if( msginfo.dwFlags & MF_DELETED )
                  Amdb_MarkMsgDelete( pth2, TRUE );
               if( msginfo.dwFlags & MF_MARKED )
                  Amdb_MarkMsg( pth2, TRUE );
               if( msginfo.dwFlags & MF_IGNORE )
                  Amdb_MarkMsgIgnore( pth2, TRUE, FALSE );
               fOk = TRUE;
               }
            else
               {
               fMessageBox( hwnd, 0, GS(IDS_STR187), MB_OK|MB_ICONEXCLAMATION );
               fOk = FALSE;
               }
            Amdb_FreeMsgTextBuffer( lpszText );

            /* If all okay and we're moving, then delete the
             * message at its source.
             */
            if( fMove && fOk )
               Amdb_DeleteMsg( pth, TRUE );
            }
         else
            {
            fMessageBox( hwnd, 0, GS(IDS_STR187), MB_OK|MB_ICONEXCLAMATION );
            fOk = FALSE;
            }
         }
      return( fOk );
      }

   /* Disallow if destination topic is read-only and we are not a moderator
    */
   if( ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pclDst, szCIXNickname ) )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR199), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* For every other case, copy by creating a copy message
    */
   Amdb_GetTopicInfo( ptlSrc, &topicinfo );
   Amdate_UnpackDate( msginfo.wDate, &date );
   Amdate_UnpackTime( msginfo.wTime, &time );
   if( fPrompt )
      {
      wsprintf( lpTmpBuf, fMove ? GS(IDS_STR615) : GS(IDS_STR47), Amdb_GetFolderName( pclDst ), Amdb_GetTopicName( ptlDst ) );
      fOk = fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES;
      }

   /* If okay to copy, do the copy.
    */
   if( fOk )
      if( nSrcTopicType != FTYPE_CIX || nDstTopicType != FTYPE_CIX || ( ( msginfo.dwFlags & MF_WITHDRAWN ) && !fRepost ) )
         {
         /* We're copying (or moving) a message betweens services
          * so handle by recreating the message for the destination
          * service.
          */
         lpszText = Amdb_GetMsgText( pth );
         if( lpszText )
            {
            LPSTR lpszBody;
            DWORD dwLen;

            /* Skip the first line of the message, which will be
             * the message number.
             */
            lpszBody = strstr( lpszText, "\r\n" );

            /* The subject field for copying from CIX messages is
             * the standard CIX copied header.
             */
            if( nSrcTopicType != FTYPE_CIX )
               strcpy( lpTmpBuf, msginfo.szTitle );
            else
               {
               strcpy( lpTmpBuf, "**COPIED FROM: " );
               CopyLine( lpTmpBuf, lpszText );
               }
            if( NULL == lpszBody )
               lpszBody = lpszText;
            dwLen = Amdb_GetMsgTextLength( pth );
            if( nDstTopicType == FTYPE_MAIL )
               {
               /* Post as a mail message. Handle by forwarding for
                * simplicity but use mail rather than forward.
                */
               CmdMailMessage( hwnd, lpszBody, NULL );
               }
            else if( nDstTopicType == FTYPE_NEWS )
               {
               /* Post to a Usenet newsgroup. Handle by reposting the
                * message as an article.
                */
//             CmdPostArticle( hwnd, lpszBody );
               ARTICLEOBJECT ao;
               char sz[ 128 ];
               char szReplyTo[ LEN_INTERNETNAME+1 ];
            
               Amob_New( OBTYPE_ARTICLEMESSAGE, &ao );
               Amob_CreateRefString( &ao.recReplyTo, szMailReplyAddress );
               Amob_CreateRefString( &ao.recNewsgroup, (LPSTR)Amdb_GetTopicName( ptlDst ) );
               WriteFolderPathname( sz, ptlDst );
               Amob_CreateRefString( &ao.recFolder, sz );
               Amdb_GetCookie( ptlDst, NEWS_REPLY_TO_COOKIE, szReplyTo, szMailReplyAddress, sizeof(szReplyTo) );
               Amob_CreateRefString( &ao.recReplyTo, szReplyTo );
               Amob_CreateRefString( &ao.recText, lpszBody );
               Amob_CreateRefString( &ao.recSubject, lpTmpBuf );
               Amob_Commit( NULL, OBTYPE_ARTICLEMESSAGE, &ao );
               Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );

               }
            else
               {
               SAYOBJECT so;

               /* For all other instances, posting a message to CIX from
                * any source simply involves creating a SAY message.
                */
               Amob_New( OBTYPE_SAYMESSAGE, &so );
               Amob_CreateRefString( &so.recConfName, (LPSTR)Amdb_GetFolderName( pclDst ) );
               Amob_CreateRefString( &so.recTopicName, (LPSTR)Amdb_GetTopicName( ptlDst ) );
               Amob_CreateRefString( &so.recText, lpszBody );
               Amob_CreateRefString( &so.recSubject, lpTmpBuf );
               Amob_Commit( NULL, OBTYPE_SAYMESSAGE, &so );
               Amob_Delete( OBTYPE_SAYMESSAGE, &so );
               }
            Amdb_FreeMsgTextBuffer( lpszText );
            }
         else
            {
            fMessageBox( hwnd, 0, GS(IDS_STR187), MB_OK|MB_ICONEXCLAMATION );
            fOk = FALSE;
            }
         }
      else if( fRepost )
            {
            lpszText = Amdb_GetMsgText( pth );
            if( lpszText )
            {
            LPSTR lpszBody;
            DWORD dwLen;
            SAYOBJECT so;
            char szDate[ 40 ];

            Amdate_UnpackDate( msginfo.wDate, &date );
            Amdate_UnpackTime( msginfo.wTime, &time );
            Amdate_FormatShortDate( &date, szDate );



            /* Skip the first line of the message, which will be
             * the message number.
             */
            lpszBody = strstr( lpszText, "\r\n" );

//          CopyLine( lpTmpBuf, lpszText );
            if( NULL == lpszBody )
               lpszBody = lpszText;
            dwLen = Amdb_GetMsgTextLength( pth );
            wsprintf( lpTmpBuf, "**REPOST: cix:%s/%s:%lu, %s, %s %2.2d:%2.2d", Amdb_GetFolderName( pclSrc ), Amdb_GetTopicName( ptlSrc ), msginfo.dwMsg, msginfo.szAuthor, szDate, time.iHour, time.iMinute );


               /* For all other instances, posting a message to CIX from
                * any source simply involves creating a SAY message.
                */
               Amob_New( OBTYPE_SAYMESSAGE, &so );
               Amob_CreateRefString( &so.recConfName, (LPSTR)Amdb_GetFolderName( pclDst ) );
               Amob_CreateRefString( &so.recTopicName, (LPSTR)Amdb_GetTopicName( ptlDst ) );
               Amob_CreateRefString( &so.recText, lpszBody );
               Amob_CreateRefString( &so.recSubject, lpTmpBuf );
               Amob_Commit( NULL, OBTYPE_SAYMESSAGE, &so );
               Amob_Delete( OBTYPE_SAYMESSAGE, &so );
            }
            }
      else
         {
         COPYMSGOBJECT cob;

         /* For CIX to CIX copying, use the CIX system to perform the
          * actual copy process.
          */
         Amob_New( OBTYPE_COPYMSG, &cob );
         cob.dwMsg = msginfo.dwMsg;
         Amob_CreateRefString( &cob.recConfName, (LPSTR)Amdb_GetFolderName( pclDst ) );
         Amob_CreateRefString( &cob.recTopicName, (LPSTR)Amdb_GetTopicName( ptlDst ) );
         Amob_CreateRefString( &cob.recCurConfName, (LPSTR)Amdb_GetFolderName( pclSrc ) );
         Amob_CreateRefString( &cob.recCurTopicName, (LPSTR)Amdb_GetTopicName( ptlSrc ) );
         if( !Amob_Find( OBTYPE_COPYMSG, &cob ) )
            Amob_Commit( NULL, OBTYPE_COPYMSG, &cob );
         Amob_Delete( OBTYPE_COPYMSG, &cob );
         }

      /* We're moving a message on CIX so withdraw the original if we're a moderator
       * of the source conference or we were the author of the message being moved.
       */
      if( fOk && fMove && nDstTopicType == FTYPE_CIX &&
          (_strcmpi( szCIXNickname, msginfo.szAuthor ) == 0 ||
          Amdb_IsModerator( pclSrc, szCIXNickname )) )
         {
         WITHDRAWOBJECT wo;

         Amob_New( OBTYPE_WITHDRAW, &wo );
         wo.dwMsg = msginfo.dwMsg;
         wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pclSrc ), Amdb_GetTopicName( ptlSrc ) );
         Amob_CreateRefString( &wo.recTopicName, lpTmpBuf );
         if( !Amob_Find( OBTYPE_WITHDRAW, &wo ) )
            Amob_Commit( NULL, OBTYPE_WITHDRAW, &wo );
         Amob_Delete( OBTYPE_WITHDRAW, &wo );
         Amdb_MarkMsgWithdrawn( pth, TRUE );
         }
   return( fOk );
}

/* This function copies one line from the source to the
 * destination.
 */
void FASTCALL CopyLine( char * pDest, char * pSrc )
{
   while( *pSrc && *pSrc != '\r' && *pSrc != '\n' )
      *pDest++ = *pSrc++;
   *pDest = '\0';
}

/* This is the copy message out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_CopyMsg( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   COPYMSGOBJECT FAR * lpcmo;

   lpcmo = (COPYMSGOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR236),
                   lpcmo->dwMsg,
                   DRF(lpcmo->recCurConfName),
                   DRF(lpcmo->recCurTopicName),
                   DRF(lpcmo->recConfName),
                   DRF(lpcmo->recTopicName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         lpcmo = (COPYMSGOBJECT FAR *)dwData;
         _fmemset( lpcmo, 0, sizeof( COPYMSGOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         COPYMSGOBJECT cmo;

         Amob_LoadDataObject( fh, &cmo, sizeof( COPYMSGOBJECT ) );
         if( fNewMemory( &lpcmo, sizeof( COPYMSGOBJECT ) ) )
            {
            *lpcmo = cmo;
            lpcmo->recTopicName.hpStr = NULL;
            lpcmo->recConfName.hpStr = NULL;
            lpcmo->recCurTopicName.hpStr = NULL;
            lpcmo->recCurConfName.hpStr = NULL;
            }
         return( (LRESULT)lpcmo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpcmo->recTopicName );
         Amob_SaveRefObject( &lpcmo->recConfName );
         Amob_SaveRefObject( &lpcmo->recCurTopicName );
         Amob_SaveRefObject( &lpcmo->recCurConfName );
         return( Amob_SaveDataObject( fh, lpcmo, sizeof( COPYMSGOBJECT ) ) );

      case OBEVENT_COPY: {
         COPYMSGOBJECT FAR * lpcmoNew;

         INITIALISE_PTR( lpcmoNew );
         if( fNewMemory( &lpcmoNew, sizeof( COPYMSGOBJECT ) ) )
            {
            INITIALISE_PTR( lpcmoNew->recTopicName.hpStr );
            INITIALISE_PTR( lpcmoNew->recConfName.hpStr );
            INITIALISE_PTR( lpcmoNew->recCurTopicName.hpStr );
            INITIALISE_PTR( lpcmoNew->recCurConfName.hpStr );
            lpcmoNew->dwMsg = lpcmo->dwMsg;
            Amob_CopyRefObject( &lpcmoNew->recTopicName, &lpcmo->recTopicName );
            Amob_CopyRefObject( &lpcmoNew->recConfName, &lpcmo->recConfName );
            Amob_CopyRefObject( &lpcmoNew->recCurTopicName, &lpcmo->recCurTopicName );
            Amob_CopyRefObject( &lpcmoNew->recCurConfName, &lpcmo->recCurConfName );
            }
         return( (LRESULT)lpcmoNew );
         }

      case OBEVENT_FIND: {
         COPYMSGOBJECT FAR * lpcmo1;
         COPYMSGOBJECT FAR * lpcmo2;

         lpcmo1 = (COPYMSGOBJECT FAR *)dwData;
         lpcmo2 = (COPYMSGOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpcmo1->recTopicName), DRF(lpcmo2->recTopicName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpcmo1->recConfName), DRF(lpcmo2->recConfName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpcmo1->recCurTopicName), DRF(lpcmo2->recCurTopicName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpcmo1->recCurConfName), DRF(lpcmo2->recCurConfName) ) != 0 )
            return( FALSE );
         return( lpcmo1->dwMsg == lpcmo2->dwMsg );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpcmo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpcmo->recConfName );
         Amob_FreeRefObject( &lpcmo->recTopicName );
         Amob_FreeRefObject( &lpcmo->recCurConfName );
         Amob_FreeRefObject( &lpcmo->recCurTopicName );
         return( TRUE );
      }
   return( 0L );
}
