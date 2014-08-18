/* SHARED.H - Shared CIX and CIX Internet outbasket definitions
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

#ifndef _H_SHARED

#include "shrdob.h"
#include "parse.h"

typedef struct tagPRINTLISTWND {
   HWND hwnd;
   int idDlg;
   HPSTR lpLines;
   HPDWORD lpIndexTable;
   DWORD cLines;
} PRINTLISTWND;

/* FWMAIL.C */
LRESULT EXPORT CALLBACK ObProc_ForwardMail( UINT, HNDFILE, LPVOID, LPVOID );
void FASTCALL CmdForwardMailMessage( HWND, PTH );

/* MAIL.C */
LRESULT EXPORT CALLBACK ObProc_MailMessage( UINT, HNDFILE, LPVOID, LPVOID );
LRESULT EXPORT CALLBACK ObProc_CIXMailMessage( UINT, HNDFILE, LPVOID, LPSTR );
void FASTCALL InstallMailForms( void );
void FASTCALL ComposeMailForm( char *, BOOL, HPSTR, HEADER FAR * );
HWND FASTCALL CreateMailWindow( HWND, MAILOBJECT FAR *, LPOB );
void FASTCALL CommonPickerCode( HWND, int, int, int, BOOL, BOOL );
void FASTCALL CmdMailMessage( HWND, LPSTR, LPSTR );
void FASTCALL CmdMailReply( HWND, BOOL, BOOL );
void FASTCALL CommonSigChangeCode( HWND, char *, BOOL, BOOL );
void FASTCALL CommonTopicSigChangeCode( HWND, char * );
void FASTCALL CommonSetSignature( HWND, PTL );
void FASTCALL FillEncodingScheme( HWND, int, UINT, BOOL, BOOL );
BOOL FASTCALL GetReplyMailHeaderField( PTH, char *, char *, int );
UINT FASTCALL GetEncodingScheme( HWND, int );
BOOL FASTCALL IsCixAddress( char * );
void FASTCALL CreateLocalCopyOfAttachmentMail( MAILOBJECT FAR *, LPSTR, int, char * );
void FASTCALL CreateLocalCopyOfForwardAttachmentMail( FORWARDMAILOBJECT FAR *, LPSTR, int, char * );
LPSTR FASTCALL ExtractMailAddress( LPSTR, LPSTR, int );
LRESULT EXPORT CALLBACK ObProc_ClearCIXMail( UINT, HNDFILE, LPVOID, LPSTR );
void FASTCALL CreateReplyAddress( RECPTR *, PTL );
void FASTCALL CreateMailMailAddress( RECPTR *, PTL );

/* ARTICLE.C */
void FASTCALL CmdPostArticle( HWND, LPSTR );
void FASTCALL CmdFollowUpArticle( HWND, BOOL );
void FASTCALL ShowHideEncodingField( HWND );
LRESULT EXPORT CALLBACK ObProc_ArticleMessage( UINT, HNDFILE, LPVOID, LPVOID );
BOOL FASTCALL CreateArticleWindow( HWND, ARTICLEOBJECT FAR *, LPOB );

/* GRPLIST.C */
BOOL FASTCALL CreateGroupListWindow( HWND );
void FASTCALL RefreshNewsgroupWindow( void );
void FASTCALL ReadGroupsDat( HWND, char * );
BOOL FASTCALL Subscribe( HWND, LPSTR );
void FASTCALL DisplayNewsgroupStatus();
DWORD FASTCALL GetNewNewsgroups( void );
LRESULT EXPORT CALLBACK ObProc_GetNewNewsgroups( UINT, HNDFILE, LPVOID, LPVOID );
LRESULT EXPORT CALLBACK ObProc_GetNewsgroups( UINT, HNDFILE, LPVOID, LPVOID );
void FASTCALL EnsureNewsFolder( void );
void FASTCALL CommonListCommands( HWND, int, HWND, UINT );

/* CIXLIST */
void FASTCALL CmdPrintGeneralList( HWND, PRINTLISTWND * );

#define _H_SHARED
#endif
