/* CIX.H - CIX API definitions and functions
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

/* Include CIX out-basket objects
 */
#include "cixob.h"

#define  CAT_CIX_MENU      32

/* CIX.C */
BOOL FASTCALL LoadCIX( void );
void FAR PASCAL UnloadCIX( int );
int FASTCALL GetCIXSystemParameter( char *, char *, int );
BOOL FASTCALL InitialiseCIXInterface( HWND );
BOOL FASTCALL IsValidCIXPassword( char * );
BOOL FASTCALL IsValidCIXNickname( char * );
BOOL FASTCALL CheckValidCixFilename( HWND, char * );
LRESULT EXPORT CALLBACK ObProc_UpdateNote( UINT, HNDFILE, LPVOID, LPSTR );
void FASTCALL UpdateConfNotes( LPVOID, BOOL );

/* JOINCONF.C */
PTL FASTCALL CmdJoinConference( HWND, char * );
LRESULT EXPORT CALLBACK ObProc_JoinConference( UINT, HNDFILE, LPVOID, LPVOID );
BOOL EXPORT CALLBACK JoinConferenceDlg( HWND, UINT, WPARAM, LPARAM );

/* RESUMES.C */
BOOL FASTCALL OpenResumesWindow( char * );
void FASTCALL CmdShowResume( HWND, LPSTR, BOOL );
void FASTCALL CreateNewResume( char *, HPSTR );
void FASTCALL ShowResumesListWindow( void );
LRESULT EXPORT CALLBACK ObProc_GetResume( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_PutResume( UINT, HNDFILE, LPVOID, LPSTR );
int FASTCALL GetResumeUserName( HWND, char *, int );
void FASTCALL GetResumeFilename( char *, char * );
BOOL FASTCALL IsResumeAvailable( char * );

/* EDITRSM.C */
void FASTCALL CmdEditResume( HWND );

/* SAY.C */
void FASTCALL CmdSay( HWND );
LRESULT EXPORT CALLBACK ObProc_SayMessage( UINT, HNDFILE, LPVOID, LPVOID );
BOOL FASTCALL CreateSayWindow( HWND, SAYOBJECT FAR *, LPOB );

/* COMMENT.C */
void FASTCALL CmdComment( HWND );
LRESULT EXPORT CALLBACK ObProc_CommentMessage( UINT, HNDFILE, LPVOID, LPVOID );
BOOL FASTCALL CreateCommentWindow( HWND, COMMENTOBJECT FAR *, LPOB );

/* USERLIST.C */
BOOL FASTCALL CreateUserListWindow( HWND );
void FASTCALL ReadUsersList( HWND );
void FASTCALL UpdateUsersList( void );
LRESULT EXPORT CALLBACK ObProc_GetUserList( UINT, HNDFILE, LPVOID, LPVOID );

/* CIXLIST.C */
BOOL FASTCALL CreateCIXListWindow( HWND );
void FASTCALL ReadCIXList( HWND );
void FASTCALL UpdateCIXList( void );
LRESULT EXPORT CALLBACK ObProc_GetCIXList( UINT, HNDFILE, LPVOID, LPVOID );

/* WITHDRAW.C */
LRESULT EXPORT CALLBACK ObProc_Withdraw( UINT, HNDFILE, LPVOID, LPVOID );
LRESULT EXPORT CALLBACK ObProc_Restore( UINT, HNDFILE, LPVOID, LPVOID );

/* FDL.C */
BOOL EXPORT CALLBACK Download( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK ObProc_Download( UINT, HNDFILE, LPVOID, LPVOID );
void FASTCALL CmdCIXFileDownload( HWND );
BOOL FASTCALL CmdDownload( HWND );

/* FAXMAIL.C */
LRESULT EXPORT CALLBACK ObProc_FaxMailMessage( UINT, HNDFILE, LPVOID, LPVOID );
void FASTCALL CmdFaxMailMessage( HWND );

/* GETMESS.C */
LRESULT EXPORT CALLBACK ObProc_GetCIXMessage( UINT, HNDFILE, LPVOID, LPSTR );

/* RESIGN.C */
LRESULT EXPORT CALLBACK ObProc_CIXResign( UINT, HNDFILE, LPVOID, LPSTR );
BOOL FASTCALL CmdResign( HWND, LPVOID, BOOL, BOOL );

/* BUILDSCR.C */
LRESULT EXPORT CALLBACK ObProc_Inline( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_Include( UINT, HNDFILE, LPVOID, LPSTR );

/* FILELIST.C */
BOOL FASTCALL CreateFilelistWindow( HWND, PTL );
void FASTCALL CmdFileListFind( HWND, PTL );
void FASTCALL UpdateFileLists( LPVOID, BOOL );
LRESULT EXPORT CALLBACK ObProc_Filelist( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_Fdir( UINT, HNDFILE, LPVOID, LPSTR );

/* PARLIST.C */
BOOL FASTCALL CreateParlistWindow( HWND, PCL );
LPSTR FASTCALL GetParticipants( HWND, BOOL );
void FASTCALL UpdatePartLists( LPVOID, BOOL );
LRESULT EXPORT CALLBACK ObProc_GetParList( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_AddPar( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_RemPar( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_CoMod( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_ExMod( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_BanPar( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_UnBanPar( UINT, HNDFILE, LPVOID, LPSTR );
BOOL FASTCALL ParlistWndResetContent( HWND );
BOOL FASTCALL UpdateParList( HWND, PCL, BOOL *, BOOL );


/* NEWMES.C */
BOOL FASTCALL GenerateScript( DWORD );
void CDECL FmtWriteToScriptFile( HNDFILE, BOOL, LPSTR, ... );
void FASTCALL WriteToScriptFile( HNDFILE, BOOL, LPSTR );
LRESULT EXPORT CALLBACK ObProc_ExecCIXScript( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_ExecVerScript( UINT, HNDFILE, LPVOID, LPSTR );


BOOL FASTCALL CreateMailScript( void );

/* CIXCONN.C */
int FASTCALL Exec_CIX( void );
int FASTCALL Exec_CIX2( void );

/* FUL.C */
void FASTCALL CmdFileUpload( HWND );
LRESULT EXPORT CALLBACK ObProc_FileUpload( UINT, HNDFILE, LPVOID, LPSTR );

/* COPYMSG.C */
BOOL FASTCALL DoCopyMsgRange( HWND, PTH FAR *, int, PTL, BOOL, BOOL, BOOL );
BOOL FASTCALL DoCopyLocalRange( HWND, PTH FAR *, int, PTL, BOOL, BOOL );
BOOL FASTCALL DoCopyMsg( HWND, PTH, PTL, BOOL, BOOL, BOOL );
LRESULT EXPORT CALLBACK ObProc_CopyMsg( UINT, HNDFILE, LPVOID, LPSTR );

/* MAILDIR.C */
void FASTCALL CmdMailUpload( HWND );
void FASTCALL CmdMailDownload( HWND, char * );
void FASTCALL CmdMailDirectory( HWND );
void FASTCALL CmdMailErase( HWND, char * );
void FASTCALL CmdMailRename( HWND, char * );
void FASTCALL CmdMailExport( HWND, char * );
BOOL FASTCALL ExtractPathAndBasename( OPENFILENAME *, char * );
DWORD FASTCALL GetFileSizeFromMailDirectory( char * );
LRESULT EXPORT CALLBACK ObProc_MailDirectory( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_MailErase( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_MailRename( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_MailDownload( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_MailExport( UINT, HNDFILE, LPVOID, LPSTR );
LRESULT EXPORT CALLBACK ObProc_MailUpload( UINT, HNDFILE, LPVOID, LPSTR );

/* BINMAIL.C */
BOOL FASTCALL CmdBinmail( HWND, LPSTR );
BOOL FASTCALL VerifyFilenamesExist( HWND, LPSTR );
LRESULT EXPORT CALLBACK ObProc_Binmail( UINT, HNDFILE, LPVOID, LPSTR );
