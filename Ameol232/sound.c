/* SOUND.C - Play an Ameol sound
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

#include "main.h"

void FASTCALL Am_PlaySound( LPSTR lpSoundName )
{
      char szSound[ _MAX_DIR ];
      char szPath[ _MAX_PATH ];

      szSound[ 0 ] = '\0';
      wsprintf( szPath, "AppEvents\\Schemes\\Apps\\Ameol2\\%s\\.Current", lpSoundName );
      ReadRegistryKey( HKEY_CURRENT_USER, szPath, "", "", szSound, sizeof( szSound ) );
      PlaySound( szSound, NULL, SND_FILENAME|SND_NODEFAULT|SND_SYNC );
}
