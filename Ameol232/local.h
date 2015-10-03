/* LOCAL.H - Local folder out-basket objects
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

#include "localob.h"

/* LOCAL.C */
BOOL FASTCALL LoadLocal( void );
void FAR PASCAL UnloadLocal( int );
BOOL FASTCALL InitialiseLocalInterface( HWND );
void FASTCALL CreateNewLocalTopic( HWND );

/* LOCALSAY.C */
void FASTCALL CmdLocalSay( HWND );
LRESULT EXPORT CALLBACK ObProc_LocalSayMessage( UINT, HNDFILE, LPVOID, LPVOID );
BOOL FASTCALL CreateLocalSayWindow( HWND, LOCALSAYOBJECT FAR * );

/* LOCALCMT.C */
void FASTCALL CmdLocalCmt( HWND );
LRESULT EXPORT CALLBACK ObProc_LocalCmtMessage( UINT, HNDFILE, LPVOID, LPVOID );
BOOL FASTCALL CreateLocalCmtWindow( HWND, LOCALCMTOBJECT FAR * );
