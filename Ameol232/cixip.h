/* CIXIP.H - CIX Internet API definitions and functions
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

#include "cixipob.h"

/* CIXIP.C */
BOOL FASTCALL LoadCIXIP( void );
BOOL FASTCALL InitialiseCIXIPInterface( HWND );
void FAR PASCAL UnloadCIXIP( int );
int FASTCALL GetCIXIPSystemParameter( char *, char *, int );
LRESULT EXPORT CALLBACK ObProc_ClearIPMail( UINT, HNDFILE, LPVOID, LPSTR );

/* IPNEWS.C */
int FASTCALL Exec_GetNewsgroups( LPVOID );
int FASTCALL Exec_GetNewNewsgroups( LPVOID );
int FASTCALL Exec_GetNewArticles( LPVOID );
int FASTCALL Exec_GetTagged( LPVOID );
int FASTCALL Exec_GetArticle( LPVOID );
int FASTCALL Exec_ArticleMessage( LPVOID );
int FASTCALL Exec_CancelArticle( LPVOID );
int FASTCALL CountOpenNewsServers( void );
void FASTCALL DisconnectAllNewsServers( void );
BOOL FASTCALL IsNewsServerBusy( void );

/* RESIGN.C */
LRESULT EXPORT CALLBACK ObProc_UsenetResign( UINT, HNDFILE, LPVOID, LPSTR );

/* IPMAIL.C */
int FASTCALL Exec_ClearIPMail( LPVOID );
int FASTCALL Exec_GetNewMail( LPVOID );

/* IPSMTP.C */
int FASTCALL Exec_MailMessage( LPVOID );
int FASTCALL Exec_ForwardMail( LPVOID );

/* CANCEL.C */
LRESULT EXPORT CALLBACK ObProc_CancelArticle( UINT, HNDFILE, LPVOID, LPVOID );

/* NEWARTCL.C */
void FASTCALL FillListWithNewsgroups( HWND, int );
LRESULT EXPORT CALLBACK ObProc_GetNewArticles( UINT, HNDFILE, LPVOID, LPVOID );
BOOL EXPORT CALLBACK GetNewArticlesDlg( HWND, UINT, WPARAM, LPARAM );

/* NEWMAIL.C */
LRESULT EXPORT CALLBACK ObProc_GetNewMail( UINT, HNDFILE, LPVOID, LPVOID );

/* GETARTCL.C */
LRESULT EXPORT CALLBACK ObProc_GetArticle( UINT, HNDFILE, LPVOID, LPVOID );

/* NEWTAGGD.C */
LRESULT EXPORT CALLBACK ObProc_GetTagged( UINT, HNDFILE, LPVOID, LPVOID );
BOOL EXPORT CALLBACK GetTaggedDlg( HWND, UINT, WPARAM, LPARAM );
