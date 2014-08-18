// webapi.h
#ifndef WEBAPI_H
#define WEBAPI_H

#include <windows.h>
#include <windowsx.h>

LRESULT EXPORT CALLBACK Am2DownloadScratch(LPSTR pUsername, LPSTR pPassword, LPSTR pLocalFile);
LRESULT EXPORT CALLBACK Am2GetFDir(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, LPSTR pLocalFile);
LRESULT EXPORT CALLBACK Am2GetFileList(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, LPSTR pLocalFile);
LRESULT EXPORT CALLBACK Am2Resign(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV);
LRESULT EXPORT CALLBACK Am2GetMessage(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pFilename, int pMsgNum);
LRESULT EXPORT CALLBACK Am2Download(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pFilename, LPSTR pOutFile);
LRESULT EXPORT CALLBACK Am2Withdraw(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, int pMsgNum);
LRESULT EXPORT CALLBACK Am2Restore(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, int pMsgNum);
LRESULT EXPORT CALLBACK Am2Post(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pBody, int pCommentTo);
LRESULT EXPORT CALLBACK Am2Join(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV);
LRESULT EXPORT CALLBACK Am2GetConferenceList(LPSTR pUsername, LPSTR pPassword, LPSTR pLocalFile);
LRESULT EXPORT CALLBACK Am2AbortConnection(void);

typedef BOOL(FAR PASCAL * AM2JOIN)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2POST)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, int);
typedef BOOL(FAR PASCAL * AM2WITHDRAW)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, int);
typedef BOOL(FAR PASCAL * AM2RESTORE)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, int);
typedef BOOL(FAR PASCAL * AM2DOWNLOAD)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2GETMESSAGE)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, int);
typedef BOOL(FAR PASCAL * AM2RESIGN)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2GETFILELIST)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2GETFDIR)(HWND, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2DOWNLOADSCRATCH)(HWND, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2GETCONFERENCELIST)(HWND, LPSTR, LPSTR, LPSTR, LPSTR);
typedef BOOL(FAR PASCAL * AM2ABORTCONNECTION)(void);

/*
Pascal Functions

FUNCTION Am2Join(pWnd: HWND; pUsername, pPassword, pConference, pTopicCSV: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2Post(pWnd: HWND; pUsername, pPassword, pConference, pTopicCSV, pBody: LPSTR; pCommentTo: Integer): Boolean; STDCALL; EXPORT;
FUNCTION Am2Withdraw(pWnd: HWND; pUsername, pPassword, pConference, pTopic: LPSTR; pMsgNum: Integer): Boolean; STDCALL; EXPORT;
FUNCTION Am2Download(pWnd: HWND; pUsername, pPassword, pConference, pTopicCSV, pFilename, pOutFile: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2GetMessage(pWnd: HWND; pUsername, pPassword, pConference, pTopicCSV, pFilename: LPSTR; pMsgNum: Integer): Boolean; STDCALL; EXPORT;
FUNCTION Am2Resign(pWnd: HWND; pUsername, pPassword, pConference, pTopicCSV: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2GetFileList(pWnd: HWND; pUsername, pPassword, pConference, pTopic, pLocalFile: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2GetFDir(pWnd: HWND; pUsername, pPassword, pConference, pTopic, pLocalFile: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2GetConferenceList(pWnd: HWND; pUsername, pPassword, pLocalFile: LPSTR): Boolean; STDCALL;
FUNCTION Am2DownloadScratch(pWnd: HWND; pUsername, pPassword, pLocalFile: LPSTR): Boolean; STDCALL; EXPORT;
FUNCTION Am2AbortConnection: Boolean; STDCALL; EXPORT;
*/

#endif WEBAPI_H