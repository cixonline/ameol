/* WEBAPI.C - Web API functions
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
#include "resource.h"
#include <memory.h>
#include <string.h>
#include "cix.h"
#include "amlib.h"
#include "parse.h"
#include "webapi.h"

extern char szLoginUsername[ LEN_LOGINNAME ];      /* Login name entered at startup */

LRESULT EXPORT CALLBACK Am2Join(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV)
{
   AM2JOIN lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2JOIN)GetProcAddress(hWebLib, "Am2Join")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopicCSV);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}


LRESULT EXPORT CALLBACK Am2Post(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pBody, int pCommentTo)
{
   AM2POST lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2POST)GetProcAddress(hWebLib, "Am2Post")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopicCSV, pBody, pCommentTo);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2Withdraw(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, int pMsgNum)
{
   AM2WITHDRAW lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2WITHDRAW)GetProcAddress(hWebLib, "Am2Withdraw")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopic, pMsgNum);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2Restore(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, int pMsgNum)
{
   AM2RESTORE lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2RESTORE)GetProcAddress(hWebLib, "Am2Restore")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopic, pMsgNum);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2Download(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pFilename, LPSTR pOutFile)
{
   AM2DOWNLOAD lpVerProc;

   if (NULL !=(lpVerProc = (AM2DOWNLOAD)GetProcAddress(hWebLib, "Am2Download")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopicCSV, pFilename, pOutFile);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2GetMessage(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV, LPSTR pFilename, int pMsgNum)
{
   AM2GETMESSAGE lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2GETMESSAGE)GetProcAddress(hWebLib, "Am2GetMessage")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopicCSV, pFilename, pMsgNum);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2Resign(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopicCSV)
{
   AM2RESIGN lpVerProc;
   
   if (NULL !=(lpVerProc = (AM2RESIGN)GetProcAddress(hWebLib, "Am2Resign")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopicCSV);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2GetFileList(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, LPSTR pLocalFile)
{
   AM2GETFILELIST lpVerProc;

   if (NULL !=(lpVerProc = (AM2GETFILELIST)GetProcAddress(hWebLib, "Am2GetFileList")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopic, pLocalFile);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2GetFDir(LPSTR pUsername, LPSTR pPassword, LPSTR pConference, LPSTR pTopic, LPSTR pLocalFile)
{
   AM2GETFDIR lpVerProc;

   if (NULL !=(lpVerProc = (AM2GETFDIR)GetProcAddress(hWebLib, "Am2GetFDir")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pConference, pTopic, pLocalFile);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2DownloadScratch(LPSTR pUsername, LPSTR pPassword, LPSTR pLocalFile)
{
   AM2DOWNLOADSCRATCH lpVerProc;

   if (NULL !=(lpVerProc = (AM2DOWNLOADSCRATCH)GetProcAddress(hWebLib, "Am2DownloadScratch")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pLocalFile);
      if( Amfile_QueryFile( pLocalFile ) )
         CIXImportAndArchive( hwndFrame, pLocalFile, PARSEFLG_ARCHIVE );
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2GetConferenceList(LPSTR pUsername, LPSTR pPassword, LPSTR pLocalFile)
{
   AM2GETCONFERENCELIST lpVerProc;

   if (NULL !=(lpVerProc = (AM2GETCONFERENCELIST)GetProcAddress(hWebLib, "Am2GetConferenceList")))
   {
      lpVerProc(hwndBlink, (LPSTR)&szLoginUsername, pUsername, pPassword, pLocalFile);
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}

LRESULT EXPORT CALLBACK Am2AbortConnection(void)
{
   AM2ABORTCONNECTION lpVerProc;

   if (NULL !=(lpVerProc = (AM2ABORTCONNECTION)GetProcAddress(hWebLib, "Am2AbortConnection")))
   {
      lpVerProc();
      return (POF_PROCESSCOMPLETED);
   } 
   else
      return (POF_HELDBYSCRIPT);
}
