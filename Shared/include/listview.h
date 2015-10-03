/* LISTVIEW.H - Listview control
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

#define LISTVIEW_CLASS "amxctl_listviewcls"

typedef struct LVIEW_ITEM {
   HICON hIcon;
   LPSTR lpszText;
   DWORD dwData;
} LVIEW_ITEM;

#define  LVM_ADDICON          WM_USER+100
#define  LVM_RESETCONTENT     WM_USER+101
#define  LVM_GETCOUNT         WM_USER+102
#define  LVM_GETSELCOUNT      WM_USER+103
#define  LVM_GETSEL           WM_USER+104
#define  LVM_GETICONITEM      WM_USER+105
#define  LVM_SETSEL           WM_USER+106
#define  LVM_GETCURSEL        WM_USER+107
#define  LVM_SETTEXT          WM_USER+108

#define  LVS_HORZ             0x0000
#define  LVS_VERT             0x0001
