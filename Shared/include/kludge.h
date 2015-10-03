/* KLUDGE.H - Borland compiler kludges
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

#ifndef _AMCTRLS_KLUDGE_H
#define _AMCTRLS_KLUDGE_H

#if defined(NONAMELESSUNION)
#define U1(field)   u.field
#define U2(field)   u2.field
#define U3(field)   u3.field
#else
#define U1(field)   field
#define U2(field)   field
#define U3(field)   field
#endif

#if defined(__BORLANDC__)

#include <string.h>

#if defined(WIN32)
#define lstrcpyn(dest, src, count) strncpy(dest, src, count)
#else
#define lstrcpyn(dest, src, count) _fstrncpy(dest, src, count)
#endif
#endif

#endif
