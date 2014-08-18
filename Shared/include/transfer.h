/* TRANSFER.H - File transfer definitions
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

#ifndef _TRANSFER_H_

#ifndef _MAX_PATH
#include <stdlib.h>
#endif

/* Transfer status codes.
 */
#define FTST_BEGINRECEIVE     0
#define FTST_BEGINTRANSMIT    1
#define FTST_COMPLETED        2
#define FTST_UPDATE           3
#define FTST_FAILED           4
#define FTST_QUERYEXISTING    5
#define FTST_YIELD            6
#define FTST_UPLOAD           7
#define FTST_DOWNLOAD         8

/* Transfer callback structure.
 */
typedef struct tagFILETRANSFERINFO {
   int wStatus;                        /* Status code */
   HWND hwndOwner;                     /* Owner window */
   char szFilename[ _MAX_PATH ];       /* Actual download filename */
   DWORD dwTotalSize;                  /* Total size of file (-1 = unknown) */
   DWORD dwSizeSoFar;                  /* Size of file transferred so far */
   DWORD dwBytesRead;                  /* Bytes transferred this session */
   DWORD dwTimeRemaining;              /* Time remaining */
   DWORD dwTimeSoFar;                  /* Time elapsed */
   int cps;                            /* Computed CPS */
   int cErrors;                        /* Errors so far */
   int wPercentage;                    /* Computed % completed */
   char szLastMessage[ 100 ];          /* Last message */
} FILETRANSFERINFO, FAR * LPFILETRANSFERINFO;

typedef int (WINAPI CALLBACK * LPFFILETRANSFERSTATUS)(LPFILETRANSFERINFO);

/* File download options
 */
#define  ZDF_DEFAULT          0
#define  ZDF_OVERWRITE        1
#define  ZDF_RENAME           2
#define  ZDF_PROMPT           3

/* AFTP file handle.
 */
#ifdef WIN32
#define  DECLARE_HANDLE32     DECLARE_HANDLE
#endif

DECLARE_HANDLE32(HXFER);

#define _TRANSFER_H_
#endif
