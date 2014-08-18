/* VERSION.H - Version structure
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

typedef struct tagAMAPI_VERSION {
    HINSTANCE hInstance;                    /* Instance handle of application */
    UINT nBuild;                            /* Build number of application */
    UINT nRelease;                          /* Release number of application */
    UINT nMaxima;                           /* Maxima version number of application */
    UINT nMinima;                           /* Minima version number of application */
    struct {
        unsigned fPrivate:1;                /* Set TRUE if this is a private (internal) build */
        unsigned fDebug:1;                  /* Set TRUE if this is a debug build */
        unsigned fPatched:1;                /* Set TRUE if this is a patch build */
        unsigned fPrerelease:1;             /* Set TRUE if this is a pre-release build */
    };
    UINT nPackedBuildDate;                  /* Packed 16-bit build date */
    UINT nPackedBuildTime;                  /* Packed 16-bit build time */
    char szLegalCopyright[ 100 ];           /* Legal copyright string */
    char szFileDescription[ 40 ];           /* Description of this application */
} AMAPI_VERSION;

typedef struct tagAMAPI_VERSION FAR * LPAMAPI_VERSION;

BOOL CDECL GetPalantirVersion( LPAMAPI_VERSION );
