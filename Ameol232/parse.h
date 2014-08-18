/* PARSE.H - Scratchpad/message parser definitions
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

#ifndef _INC_PARSE

#define  MIME_ET_QUOTEDPRINTABLE    1
#define  MIME_ET_7BIT               2

/* Parse flags.
 */
#define  PARSEFLG_ARCHIVE                 0x0001
#define  PARSEFLG_IGNORECR                0x0002
#define  PARSEFLG_MARKREAD                0x0004
#define  PARSEFLG_IMPORT                     0x0008
#define  PARSEFLG_NORULES                 0x0010
#define  PARSEFLG_REPLACECR                  0x0020
#define PARSEFLG_KEEPAFTERIMPORT          0x0040

#define  LEN_TOCCLIST                     2047

/* This is a BIG structure! (nearly 5k) so don't try and
 * allocate it on the stack.
 */
typedef struct tagHEADER {
   PTL ptl;                                  /* Topic in which header is being constructed */
   struct {
      unsigned fMime:1;                      /* TRUE if we're doing Mime decoding */
      unsigned fXref:1;                      /* TRUE if article is cross-referenced */
      unsigned fPriority:1;                  /* TRUE if message is priority */
      unsigned fPossibleRefScanning:1;       /* TRUE if we do subject field scanning */
      unsigned fInHeader:1;                  /* TRUE if we're parsing a mail or news header */
      unsigned fCheckForInReplyTo:1;         /* TRUE if we check for In-Reply-To */
      unsigned fCheckForXOLRField:1;         /* TRUE if we check for In-Reply-To */
      unsigned fOutboxMsg:1;                 /* TRUE if message was sent by user */
      unsigned fNotification:1;              /* TRUE if we send a notification back */
      unsigned fCoverNote:1;                 /* TRUE if this is a binmail cover note */
      };
   UINT nEncodingType;                       /* Encoding type */
   char szAuthor[ LEN_INTERNETNAME + 1 ];    /* The author of the message */
   char szFrom[ LEN_INTERNETNAME + 1   ];    /* The FROM field in the message */
   char szFullname[ LEN_INTERNETNAME + 1 ];  /* The full name of the author of the message */
   char szTo[ LEN_TOCCLIST + 1 ];            /* To whom this message is directed */
   char szCC[ LEN_TOCCLIST + 1 ];            /* To whom this message is CC'd */
   char szTitle[ LEN_TITLE + 1 ];            /* The title of the message, if applicable */
   char szMessageId[ LEN_MESSAGEID + 1 ];    /* Message identification code */
   char szType[ LEN_RFCTYPE+1 ];             /* Current header type being parsed */
   char szReference[ LEN_MESSAGEID + 1 ];    /* Message-ID of reply */
   AM_DATE date;                             /* Date of the message */
   AM_TIME time;                             /* Time of the message */
   DWORD dwComment;                          /* Number of message to which this is a comment */
   DWORD dwMsg;                              /* Number of this message */
   UINT cLines;                              /* Number of lines in Usenet message */
} HEADER;

void FASTCALL InitialiseHeader( HEADER * );
BOOL FASTCALL ParseSocketLine( char *, HEADER * );
char * FASTCALL ParseMailDate( char *, HEADER * );
char * FASTCALL ParseMailSubject( char *, HEADER * );
char * FASTCALL ParseMessageId( char *, HEADER *, BOOL );
char * FASTCALL ParseMailAuthor( char *, HEADER * );
char * FASTCALL ParseReplyNumber( char *, HEADER * );
char * FASTCALL ParseReference( char *, HEADER * );
char * FASTCALL ParseToField( char *, char *, int );
char * FASTCALL ParseCrossReference( char *, HEADER * );
char * FASTCALL ParseXPriority( char *, HEADER * );
char * FASTCALL ParsePriority( char *, HEADER * );
char * FASTCALL ParseCIXDate( char *, char * );
char * FASTCALL ParseCIXTime( char *, AM_DATE FAR *, AM_TIME FAR * );
char * FASTCALL ParseExactCIXTime( char *, AM_DATE FAR *, AM_TIME FAR * );
char * FASTCALL ParseContentTransferEncoding( char *, HEADER * );
char * FASTCALL ParseDispositionNotification( char *, HEADER * );
char * FASTCALL ParseXOlr( char *, HEADER * );
BOOL FASTCALL ApplyEncodingType( char *, HEADER * );
BOOL FASTCALL CheckForInReplyTo( HEADER *, char * );
WORD FASTCALL FixupYear( WORD );
BOOL FASTCALL MustRecoverScratchpad( void );

#define _INC_PARSE
#endif
