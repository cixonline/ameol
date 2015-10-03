/* RULES.H - Rules and rule matching
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

#ifndef _RULES_H

#include "parse.h"

/* Rule flags.
 */
#define  FILF_USECC        0x0001      /* Inspect the CC field */
#define  FILF_OR           0x0002      /* Must match any fields */
#define  FILF_NOT          0x0004      /* Must match none of the fields */
#define  FILF_WATCH        0x0008
#define  FILF_READLOCK     0x0010
#define  FILF_WITHDRAWN    0x0020      /* Hope this isn't being used !!SM!!*/
#define  FILF_REGEXP       0x0020      
#define  FILF_KEEP         0x0040
#define  FILF_MARK         0x0080
#define  FILF_DELETE       0x0100      /* Delete if match found */
#define  FILF_PRIORITY     0x0200      /* Mark priority if match found */
#define  FILF_READ         0x0400      /* Mark read if match found */
#define  FILF_MOVE         0x0800      /* Move to another folder */
#define  FILF_IGNORE       0x1000      /* Mark the message to be ignored */
#define  FILF_MAILFORM     0x2000      /* Send a mail form */
#define  FILF_GETARTICLE   0x4000      /* Retrieve full article */
#define FILF_COPY          0x8000
#define  FILF_ACTIONMASK   0xFF00      /* Action flags mask */

/* Rule matches
 * May now be combinations.
 */
#define  FLT_MATCH_NONE          0x0000
#define  FLT_MATCH_DELETE        0x0001
#define  FLT_MATCH_PRIORITY      0x0002
#define  FLT_MATCH_READ          0x0004
#define  FLT_MATCH_GETARTICLE    0x0008
#define  FLT_MATCH_MOVE          0x0010
#define  FLT_MATCH_IGNORE        0x0020
#define  FLT_MATCH_MAILFORM      0x0040
#define  FLT_MATCH_READLOCK      0x0080
#define  FLT_MATCH_WITHDRAWN     0x0100
#define  FLT_MATCH_KEEP          0x0200
#define  FLT_MATCH_MARK          0x0400
#define  FLT_MATCH_COPY          0x0800
#define  FLT_MATCH_WATCH         0x1000
#define  FLT_MATCH_REGEXP        0x2000


/* Rule target structure.
 */
typedef struct tagRULETARGET {
   WORD wResult;                       /* Result of match (FLT_xx) */
   LPVOID pMoveFolder;                 /* Move folder, if FLT_MATCH_MOVE */
} RULETARGET;

/* Rule structure.
 */
typedef struct tagRULE {
   char szToText[100];                 /* Text to match in To field */
   char szFromText[100];               /* Text to match in From field */
   char szSubjectText[100];            /* Text to match in Subject field */
   char szBodyText[100];               /* Text to match in Body field */
   char szMailForm[ 40 ];              /* Mail form to use */
   LPVOID pData;                       /* Folder in which rule applies */
   LPVOID pMoveData;                   /* Folder to which message is moved */
   WORD wFlags;                        /* Specifies which of the rule items are valid */
} RULE, FAR * HRULE;

void FASTCALL CmdRules( HWND );
void FASTCALL SaveRules( void );
void FASTCALL InitRules( void );
int FASTCALL MatchRule( PTL *, HPSTR, HEADER FAR *, BOOL );
BOOL FASTCALL MatchRuleOnMessage( PTH );
LPSTR FASTCALL ParseFolderPathname( LPSTR, LPVOID *, BOOL, WORD );
char * FASTCALL WriteFolderPathname( LPSTR, LPVOID );
HRULE FASTCALL CreateRule( RULE FAR * );
void FASTCALL DeleteAllApplicableRules( LPVOID );
BOOL EXPORT CALLBACK FolderPropRules( HWND, UINT, WPARAM, LPARAM );
int FASTCALL FillListWithFolders( HWND, int );
int FASTCALL FillListWithTopics( HWND, int, WORD );
void FASTCALL PropertySheetClosingRules( void );

#define _RULES_H
#endif
