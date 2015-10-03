/* ASSERT.C - My own assertion function
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

#include "warnings.h"
#include <windows.h>
#include "amlib.h"

/* Our replacement assertion macro. Unlike Window's default assertion macro, we
 * provide the option to break back into the debugger so we can trace where the
 * problem occurs.
 */
BOOL __cdecl AssertMsg( void * pExp, void * pFilename, unsigned nLine )
{
   char sz[ 200 ]; /* BUGBUG: This really isn't enough! */

   wsprintf( sz, "%s in %s line %u\n\nBreak to debugger ?", (LPSTR)pExp, (LPSTR)pFilename, nLine );
   return( IDYES == MessageBox( NULL, sz, "Assertion Error", MB_YESNO|MB_ICONEXCLAMATION ) );
}
