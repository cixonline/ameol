/* TELNET.H - Telnet specific definitions
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

#define  SE          240
#define  NOP         241
#define  DM          242
#define  BREAK       243
#define  IP          244
#define  AO          245
#define  AYT         246
#define  EC          247
#define  EL          248
#define  GOAHEAD     249
#define  SB          250
#define  WILLTEL     251
#define  WONTTEL     252
#define  DOTEL       253
#define  DONTTEL     254
#define  IAC         255

#define  BINARY      0
#define  ECHO        1
#define  SGA         3
#define  TERMTYPE    24
#define  NAWS        31

#define  SEND        1
