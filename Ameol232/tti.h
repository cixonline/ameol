/* TTI.H - Tooltips info structure
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

#ifndef _TTI_H

typedef struct tagTOOLTIPITEMS {
    UINT uID;
    UINT uMsg;
} TOOLTIPITEMS, FAR * LPTOOLTIPITEMS;

/* The tooltip functions.
 */
void FASTCALL AddTooltipsToWindow( HWND, int, LPTOOLTIPITEMS );
void FASTCALL RemoveTooltipsFromWindow( HWND );

#define _TTI_H
#endif
