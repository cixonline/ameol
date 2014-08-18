/* AMCTRLS.H - Ameol common controls header file
 *
 * This is a standard 16-bit/32-bit compatible common controls
 * header file for both Microsoft and Borland compilers.
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

#ifndef _AMCTRLS_H
#define _AMCTRLS_H

/* Define API decoration for direct importing of DLL references.
 */
#ifndef WINCOMMCTRLAPI
#if !defined(_COMCTL32_) && defined(_WIN32)
#define WINCOMMCTRLAPI DECLSPEC_IMPORT
#else
#define WINCOMMCTRLAPI
#endif
#endif

/* For compilers that don't support nameless unions
 * Added by Pete - 13/7/95
 */
#ifndef DUMMYUNIONNAME
#ifdef NONAMELESSUNION
#define DUMMYUNIONNAME   u
#define DUMMYUNIONNAME2  u2
#define DUMMYUNIONNAME3  u3
#else
#define DUMMYUNIONNAME
#define DUMMYUNIONNAME2
#define DUMMYUNIONNAME3
#endif
#endif

/* Added by Pete - 13/7/95
 */
#if defined(__BORLANDC__)
#pragma option -a-      /* Borland C++: Assume byte packing throughout */
#else
#pragma pack(1)         /* Microsoft C++: Assume byte packing throughout */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Include special definitions for HUGE pointers.
 */
#ifndef _PDEF_DEFS

#ifndef WIN32
#define  HUGEPTR  __huge
#else
#define  HUGEPTR
#endif

#ifndef HPSTR
#define  HPSTR char HUGEPTR *
#endif
#define  HPDWORD DWORD HUGEPTR *
#define  HPVOID void HUGEPTR *

#define  _PDEF_DEFS

#endif

//====== COMMON INIT API ===================================================

WINCOMMCTRLAPI int WINAPI Amctl_Init( void );
WINCOMMCTRLAPI DWORD WINAPI Amctl_GetAmctrlsVersion( void );
WINCOMMCTRLAPI void WINAPI Amctl_SetControlWheelScrollLines( int );
WINCOMMCTRLAPI int WINAPI Amctl_GetControlWheelScrollLines();

//====== COMMON CONTROL STYLES ================================================

#define ACTL_CCS_TOP                 0x00000001L
#define ACTL_CCS_NOMOVEY             0x00000002L
#define ACTL_CCS_BOTTOM              0x00000003L
#define ACTL_CCS_NORESIZE            0x00000004L
#define ACTL_CCS_NOPARENTALIGN       0x00000008L
#define ACTL_CCS_NOHILITE            0x00000010L
#define ACTL_CCS_ADJUSTABLE          0x00000020L
#define ACTL_CCS_NODIVIDER           0x00000040L

//====== Initialise common controls ===========================================

#define ODT_HEADER              100
#define ODT_TAB                 101
#define ODT_LISTVIEW            102

//====== Ranges for control message IDs =======================================

#define TV_FIRST                0x1100      /* TreeView messages */
#define HDM_FIRST               0x1200      /* Header messages */
#define WCM_FIRST               0x1300      /* Well messages */
#define PICBM_FIRST             0x1400      /* Picture button messages */
#define SPM_FIRST               0x1500      /* Splitter messages */
#define PICCTL_FIRST            0x1600      /* Picture control messages */

/* Callback stuff.
 */
#define LPSTR_TEXTCALLBACK     ((LPSTR)-1L)

#define I_IMAGECALLBACK         (-1)
#define I_NOIMAGE               (-2)

/* Enable or disable controls animation.
 */
WINCOMMCTRLAPI BOOL WINAPI Amctl_EnableControlsAnimation( BOOL );

//====== NOTIFICATION API ==================================================

/* Notifications and the notify structure are all Win32
 * stuff. Need to define it for Win16 API.
 */
#ifndef WIN32
typedef struct tagNMHDR {
   HWND hwndFrom;
   UINT idFrom;
   UINT code;
} NMHDR, FAR * LPNMHDR;

#ifdef WIN32
#define WM_NOTIFY                       0x004E
#else
#define WM_NOTIFY                       WM_USER+0x560
#endif
#endif

WINCOMMCTRLAPI LRESULT WINAPI Amctl_SendNotify( HWND, HWND, int, NMHDR FAR * );

#define HANDLE_WM_NOTIFY(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (int)(wParam), (NMHDR FAR*)(lParam))
#define FORWARD_WM_NOTIFY(hwnd, idFrom, pnmhdr, fn) \
    (void)(fn)((hwnd), WM_NOTIFY, (WPARAM)(int)(id), (LPARAM)(NMHDR FAR*)(pnmhdr))

/* Generic WM_NOTIFY notification codes
 */
#define NM_OUTOFMEMORY          (NM_FIRST-1)
#define NM_CLICK                (NM_FIRST-2)
#define NM_DBLCLK               (NM_FIRST-3)
#define NM_RETURN               (NM_FIRST-4)
#define NM_RCLICK               (NM_FIRST-5)
#define NM_RDBLCLK              (NM_FIRST-6)
#define NM_SETFOCUS             (NM_FIRST-7)
#define NM_KILLFOCUS            (NM_FIRST-8)
#define NM_STARTWAIT            (NM_FIRST-9)
#define NM_ENDWAIT              (NM_FIRST-10)
#define NM_BTNCLK               (NM_FIRST-10)

/* Generic to all controls
 */
#define NM_FIRST                (0U-  0U)       
#define NM_LAST                 (0U- 99U)

/* Header control.
 */
#define HDN_FIRST               (0U-300U)
#define HDN_LAST                (0U-399U)

/* Treeview control.
 */
#define TVN_FIRST               (0U-400U)
#define TVN_LAST                (0U-499U)

/* Tooltips control.
 */
#define TTN_FIRST               (0U-520U)
#define TTN_LAST                (0U-549U)

/* Tab control.
 */
#define TCN_FIRST               (0U-550U)
#define TCN_LAST                (0U-580U)

/* Toolbar control.
 */
#define TBN_FIRST               (0U-700U)
#define TBN_LAST                (0U-720U)

/* Up-down control.
 */
#define UDN_FIRST               (0U-721)
#define UDN_LAST                (0U-740)

/* Well control.
 */
#define WCN_FIRST               (0U-741U)
#define WCN_LAST                (0U-780U)

/* Hotkey control.
 */
#define HCN_FIRST               (0U-781U)
#define HCN_LAST                (0U-800U)

/* Splitter control.
 */
#define SPN_FIRST               (0U-801U)
#define SPN_LAST                (0U-820U)

/* Big Edit control.
 */
#define BEMN_FIRST              (0U-821U)
#define BEMN_LAST               (0U-830U)

//====== PROPERTY SHEET APIS ==================================================

#ifndef NOPRSHEET

#define WC_PROPSHEET           "amctlx_propsheet"
#define MAXPROPPAGES            100

struct _PSP;
typedef struct _PSP FAR* HAMPROPSHEETPAGE;

typedef struct _AMPROPSHEETPAGE FAR *LPAMPROPSHEETPAGE;
typedef UINT (CALLBACK FAR * LPFNAMPSPCALLBACK)(HWND hwnd, UINT uMsg, LPAMPROPSHEETPAGE ppsp);

#define PSP_DEFAULT             0x0000
#define PSP_DLGINDIRECT         0x0001
#define PSP_USEHICON            0x0002
#define PSP_USEICONID           0x0004
#define PSP_USETITLE            0x0008

#define PSP_HASHELP             0x0020
#define PSP_USEREFPARENT        0x0040
#define PSP_USECALLBACK         0x0080

#define PSPCB_RELEASE           1
#define PSPCB_CREATE            2


typedef struct _AMPROPSHEETPAGE {
        DWORD           dwSize;
        DWORD           dwFlags;
        HINSTANCE       hInstance;
        union {
            LPCSTR          pszTemplate;
#ifdef _WIN32
            LPCDLGTEMPLATE  pResource;
#else
            const VOID FAR *pResource;
#endif
        } DUMMYUNIONNAME;
        union {
            HICON       hIcon;
            LPCSTR      pszIcon;
        } DUMMYUNIONNAME2;
        LPCSTR          pszTitle;
        DLGPROC         pfnDlgProc;
        LPARAM          lParam;
        LPFNAMPSPCALLBACK pfnCallback;
        UINT FAR * pcRefParent;
} AMPROPSHEETPAGE, FAR *LPAMPROPSHEETPAGE;
typedef const AMPROPSHEETPAGE FAR *LPCAMPROPSHEETPAGE;

#define PSH_DEFAULT             0x0000
#define PSH_PROPTITLE           0x0001
#define PSH_USEHICON            0x0002
#define PSH_USEICONID           0x0004
#define PSH_PROPSHEETPAGE       0x0008
#define PSH_MULTILINETABS       0x0010
#define PSH_WIZARD              0x0020
#define PSH_USEPSTARTPAGE       0x0040
#define PSH_NOAPPLYNOW          0x0080
#define PSH_USECALLBACK         0x0100
#define PSH_HASHELP             0x0200
#define PSH_MODELESS            0x0400

typedef int (CALLBACK *PFNAMPROPSHEETCALLBACK)(HWND, UINT, LPARAM);

typedef struct _AMPROPSHEETHEADER {
        DWORD           dwSize;
        DWORD           dwFlags;
        HWND            hwndParent;
        HINSTANCE       hInstance;
        union {
            HICON       hIcon;
            LPCSTR      pszIcon;
        }DUMMYUNIONNAME;
        LPCSTR          pszCaption;


        UINT            nPages;
        union {
            UINT        nStartPage;
            LPCSTR      pStartPage;
        }DUMMYUNIONNAME2;
        union {
            LPCAMPROPSHEETPAGE ppsp;
            HAMPROPSHEETPAGE FAR *phpage;
        }DUMMYUNIONNAME3;
        PFNAMPROPSHEETCALLBACK pfnCallback;
} AMPROPSHEETHEADER, FAR *LPAMPROPSHEETHEADER;
typedef const AMPROPSHEETHEADER FAR *LPCAMPROPSHEETHEADER;

#define PSCB_INITIALIZED  1

WINCOMMCTRLAPI HAMPROPSHEETPAGE WINAPI Amctl_CreatePropertySheetPage(LPCAMPROPSHEETPAGE);
WINCOMMCTRLAPI BOOL           WINAPI Amctl_DestroyPropertySheetPage(HAMPROPSHEETPAGE);
WINCOMMCTRLAPI int            WINAPI Amctl_PropertySheet(LPCAMPROPSHEETHEADER);

typedef BOOL (CALLBACK FAR * LPFNADDAMPROPSHEETPAGE)(HAMPROPSHEETPAGE, LPARAM);
typedef BOOL (CALLBACK FAR * LPFNADDAMPROPSHEETPAGES)(LPVOID, LPFNADDAMPROPSHEETPAGE, LPARAM);

#define PSN_FIRST               (0U-200U)
#define PSN_LAST                (0U-299U)


#define PSN_SETACTIVE           (PSN_FIRST-0)
#define PSN_KILLACTIVE          (PSN_FIRST-1)
#define PSN_APPLY               (PSN_FIRST-2)
#define PSN_RESET               (PSN_FIRST-3)
#define PSN_HELP                (PSN_FIRST-5)
#define PSN_WIZBACK             (PSN_FIRST-6)
#define PSN_WIZNEXT             (PSN_FIRST-7)
#define PSN_WIZFINISH           (PSN_FIRST-8)
#define PSN_QUERYCANCEL         (PSN_FIRST-9)


#define PSNRET_NOERROR              0
#define PSNRET_INVALID              1
#define PSNRET_INVALID_NOCHANGEPAGE 2


#define PSM_SETCURSEL           (WM_USER + 101)
#define PropSheet_SetCurSel(hDlg, hpage, index) \
        SendMessage(hDlg, PSM_SETCURSEL, (WPARAM)index, (LPARAM)hpage)


#define PSM_REMOVEPAGE          (WM_USER + 102)
#define PropSheet_RemovePage(hDlg, index, hpage) \
        SendMessage(hDlg, PSM_REMOVEPAGE, index, (LPARAM)hpage)


#define PSM_ADDPAGE             (WM_USER + 103)
#define PropSheet_AddPage(hDlg, hpage) \
        SendMessage(hDlg, PSM_ADDPAGE, 0, (LPARAM)hpage)


#define PSM_CHANGED             (WM_USER + 104)
#define PropSheet_Changed(hDlg, hwnd) \
        SendMessage(hDlg, PSM_CHANGED, (WPARAM)hwnd, 0L)


#define PSM_RESTARTWINDOWS      (WM_USER + 105)
#define PropSheet_RestartWindows(hDlg) \
        SendMessage(hDlg, PSM_RESTARTWINDOWS, 0, 0L)


#define PSM_REBOOTSYSTEM        (WM_USER + 106)
#define PropSheet_RebootSystem(hDlg) \
        SendMessage(hDlg, PSM_REBOOTSYSTEM, 0, 0L)


#define PSM_CANCELTOCLOSE       (WM_USER + 107)
#define PropSheet_CancelToClose(hDlg) \
        SendMessage(hDlg, PSM_CANCELTOCLOSE, 0, 0L)


#define PSM_QUERYSIBLINGS       (WM_USER + 108)
#define PropSheet_QuerySiblings(hDlg, wParam, lParam) \
        SendMessage(hDlg, PSM_QUERYSIBLINGS, wParam, lParam)


#define PSM_UNCHANGED           (WM_USER + 109)
#define PropSheet_UnChanged(hDlg, hwnd) \
        SendMessage(hDlg, PSM_UNCHANGED, (WPARAM)hwnd, 0L)


#define PSM_APPLY               (WM_USER + 110)
#define PropSheet_Apply(hDlg) \
        SendMessage(hDlg, PSM_APPLY, 0, 0L)


#define PSM_SETTITLE           (WM_USER + 111)

#define PropSheet_SetTitle(hDlg, wStyle, lpszText)\
        SendMessage(hDlg, PSM_SETTITLE, wStyle, (LPARAM)(LPCSTR)lpszText)


#define PSM_SETWIZBUTTONS       (WM_USER + 112)
#define PropSheet_SetWizButtons(hDlg, dwFlags) \
        PostMessage(hDlg, PSM_SETWIZBUTTONS, 0, (LPARAM)dwFlags)



#define PSWIZB_BACK             0x00000001
#define PSWIZB_NEXT             0x00000002
#define PSWIZB_FINISH           0x00000004
#define PSWIZB_DISABLEDFINISH   0x00000008


#define PSM_PRESSBUTTON         (WM_USER + 113)
#define PropSheet_PressButton(hDlg, iButton) \
        SendMessage(hDlg, PSM_PRESSBUTTON, (WPARAM)iButton, 0)


#define PSBTN_BACK              0
#define PSBTN_NEXT              1
#define PSBTN_FINISH            2
#define PSBTN_OK                3
#define PSBTN_APPLYNOW          4
#define PSBTN_CANCEL            5
#define PSBTN_HELP              6
#define PSBTN_MAX               6



#define PSM_SETCURSELID         (WM_USER + 114)
#define PropSheet_SetCurSelByID(hDlg, id) \
        SendMessage(hDlg, PSM_SETCURSELID, 0, (LPARAM)id)


#define PSM_SETFINISHTEXT      (WM_USER + 115)

#define PropSheet_SetFinishText(hDlg, lpszText) \
        SendMessage(hDlg, PSM_SETFINISHTEXT, 0, (LPARAM)lpszText)


#define PSM_GETTABCONTROL       (WM_USER + 116)
#define PropSheet_GetTabControl(hDlg) \
        (HWND)SendMessage(hDlg, PSM_GETTABCONTROL, 0, 0)

#define PSM_ISDIALOGMESSAGE     (WM_USER + 117)
#define PropSheet_IsDialogMessage(hDlg, pMsg) \
        (BOOL)SendMessage(hDlg, PSM_ISDIALOGMESSAGE, 0, (LPARAM)pMsg)

#define ID_PSRESTARTWINDOWS     0x2
#define ID_PSREBOOTSYSTEM       (ID_PSRESTARTWINDOWS | 0x1)


#define WIZ_CXDLG               276
#define WIZ_CYDLG               140

#define WIZ_CXBMP               80

#define WIZ_BODYX               92
#define WIZ_BODYCX              184

#define PROP_SM_CXDLG           212
#define PROP_SM_CYDLG           188

#define PROP_MED_CXDLG          227
#define PROP_MED_CYDLG          215

#define PROP_LG_CXDLG           252
#define PROP_LG_CYDLG           218

#endif

//====== SPLITTER APIS ===========================================================

#define  WC_SPLITTER                "amxctl_splitter"

/* Messages.
 */
#define  SPM_ADJUST                 (SPM_FIRST+0)

/* Styles.
 */
#define  SPS_HORZ                   0x0100
#define  SPS_VERT                   0x0000

typedef struct _NM_SPLITTER {
   NMHDR hdr;
   int nPos;
} NM_SPLITTER, FAR *LPNM_SPLITTER;

/* Notifications.
 */
#define  SPN_SPLITDBLCLK            ((WORD)SPN_FIRST-0)
#define  SPN_SPLITSET               ((WORD)SPN_FIRST-1)

/* Functions.
 */
WINCOMMCTRLAPI HWND WINAPI Amctl_CreateSplitterWindow( HWND, WORD, BOOL );

//====== PICBUTTON APIS ===========================================================

#ifndef NOPICBUTTON

#define  WC_PICBUTTON               "amxctl_picbutton"

/* Messages.
 */
#define  PICBM_SETBITMAP            (PICBM_FIRST+0)
#define  PICBM_SETTEXT              (PICBM_FIRST+1)

/* Styles.
 */
#define  PBS_TOGGLE                 0x0010

/* Message macros.
 */
#define PicButton_SetBitmap(hwnd, h, rsc) \
    (int)SendMessage((hwnd), PICBM_SETBITMAP, (WPARAM)(h), (LPARAM)MAKEINTRESOURCE((rsc)))

#define PicButton_SetBitmapHandle(hwnd, hnd) \
    (int)SendMessage((hwnd), PICBM_SETBITMAP, 0, (LPARAM)(LPSTR)(hnd))

#define PicButton_SetText(hwnd, t) \
    (int)SendMessage((hwnd), PICBM_SETTEXT, 0, (LPARAM)(LPSTR)(t))

#endif

//====== PICCTRL APIS ===========================================================

#ifndef NOPICCTRL

#define  WC_PICCTRL                 "amxctl_picctrl"

typedef struct tagPICCTRLSTRUCT {
   HBITMAP hBitmap;
   HPALETTE hPalette;
} PICCTRLSTRUCT, FAR * LPPICCTRLSTRUCT;

/* Function that retrieves a DIB
 */
BOOL WINAPI Amctl_LoadDIBBitmap( HINSTANCE, LPCSTR, LPPICCTRLSTRUCT );

/* Messages.
 */
#define  PICCTL_SETBITMAP           (PICCTL_FIRST+0)
#define  PICCTL_SETTEXT             (PICCTL_FIRST+1)

/* Styles.
 */
#define  PCTL_STRETCH               0x0001
#define  PCTL_MAINTAINAR            0x0002
#define  PCTL_LEFT                  0x0004
#define  PCTL_RIGHT                 0x0008
#define  PCTL_TOP                   0x0010
#define  PCTL_BOTTOM                0x0020
#define  PCTL_CENTER                0x0000

/* Message macros.
 */
#define PicCtrl_SetBitmap(hwnd, h, rsc) \
    (int)SendMessage((hwnd), PICCTL_SETBITMAP, (WPARAM)(h), (LPARAM)MAKEINTRESOURCE((rsc)))

#define PicCtrl_SetBitmapHandle(hwnd, lppc) \
    (int)SendMessage((hwnd), PICCTL_SETBITMAP, 0, (LPARAM)(LPSTR)(lppc))

#define PicCtrl_SetText(hwnd, t) \
    (int)SendMessage((hwnd), PICCTL_SETTEXT, 0, (LPARAM)(LPSTR)(t))

#endif

//====== WELL APIS ===========================================================

#ifndef NOWELL

#define  WC_WELL                 "amxctl_well"

/* Messages.
 */
#define  WCM_SETITEMCOUNT        (WCM_FIRST+0)
#define  WCM_SETSELECTED         (WCM_FIRST+1)
#define  WCM_SETCOLOURARRAY      (WCM_FIRST+2)
#define  WCM_GETITEMCOUNT        (WCM_FIRST+3)
#define  WCM_GETSELECTED         (WCM_FIRST+4)
#define  WCM_GETSELECTEDCOLOUR   (WCM_FIRST+5)
#define  WCM_SETSELECTEDCOLOUR   (WCM_FIRST+6)
#define  WCM_GETCOLOURARRAY      (WCM_FIRST+7)

/* Notifications.
 */
#define  WCN_SELCHANGING         (WCN_FIRST-0)
#define  WCN_SELCHANGED          (WCN_FIRST-1)

#endif

//====== IMAGE APIS ===========================================================

#ifndef NOIMAGEAPIS

#define CLR_NONE                0xFFFFFFFFL
#define CLR_DEFAULT             0xFF000000L

#define  IMAGE_BITMAP   0
#define  IMAGE_ICON     1
#define  IMAGE_CURSOR   2

struct _IMAGELIST;
typedef struct _IMAGELIST FAR * HIMAGELIST;

#define ILC_MASK                0x0001
#define ILC_COLOR               0x00FE
#define ILC_COLORDDB            0x00FE
#define ILC_COLOR4              0x0004
#define ILC_COLOR8              0x0008
#define ILC_COLOR16             0x0010
#define ILC_COLOR24             0x0018
#define ILC_COLOR32             0x0020
#define ILC_PALETTE             0x0800

WINCOMMCTRLAPI HIMAGELIST  WINAPI Amctl_ImageList_Create(int cx, int cy, UINT flags, int cInitial, int cGrow);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_Destroy(HIMAGELIST himl);
WINCOMMCTRLAPI int         WINAPI Amctl_ImageList_GetImageCount(HIMAGELIST himl);
WINCOMMCTRLAPI int         WINAPI Amctl_ImageList_Add(HIMAGELIST himl, HBITMAP hbmImage, HBITMAP hbmMask);
WINCOMMCTRLAPI int         WINAPI Amctl_ImageList_ReplaceIcon(HIMAGELIST himl, int i, HICON hicon);
WINCOMMCTRLAPI COLORREF    WINAPI Amctl_ImageList_SetBkColor(HIMAGELIST himl, COLORREF clrBk);
WINCOMMCTRLAPI COLORREF    WINAPI Amctl_ImageList_GetBkColor(HIMAGELIST himl);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_SetOverlayImage(HIMAGELIST himl, int iImage, int iOverlay);

#define     Amctl_ImageList_AddIcon(himl, hicon) Amctl_ImageList_ReplaceIcon(himl, -1, hicon)

#define ILD_NORMAL              0x0000
#define ILD_TRANSPARENT         0x0001
#define ILD_MASK                0x0010
#define ILD_IMAGE               0x0020
#define ILD_BLEND               0x000E
#define ILD_BLEND25             0x0002
#define ILD_BLEND50             0x0004
#define ILD_OVERLAYMASK         0x0F00
#define INDEXTOOVERLAYMASK(i)   ((i) << 8)

#define ILD_SELECTED            ILD_BLEND50
#define ILD_FOCUS               ILD_BLEND25
#define CLR_HILIGHT             CLR_DEFAULT

WINCOMMCTRLAPI BOOL WINAPI Amctl_ImageList_Draw(HIMAGELIST himl, int i, HDC hdcDst, int x, int y, UINT fStyle);

WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_Replace(HIMAGELIST himl, int i, HBITMAP hbmImage, HBITMAP hbmMask);
WINCOMMCTRLAPI int         WINAPI Amctl_ImageList_AddMasked(HIMAGELIST himl, HBITMAP hbmImage, COLORREF crMask);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_DrawEx(HIMAGELIST himl, int i, HDC hdcDst, int x, int y, int dx, int dy, COLORREF rgbBk, COLORREF rgbFg, UINT fStyle);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_Remove(HIMAGELIST himl, int i);
WINCOMMCTRLAPI HICON       WINAPI Amctl_ImageList_GetIcon(HIMAGELIST himl, int i, UINT flags);
WINCOMMCTRLAPI HIMAGELIST  WINAPI Amctl_ImageList_LoadImage(HINSTANCE hi, LPCSTR lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags);

WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_BeginDrag(HIMAGELIST himlTrack, int iTrack, int dxHotspot, int dyHotspot);
WINCOMMCTRLAPI void        WINAPI Amctl_ImageList_EndDrag();
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_DragEnter(HWND hwndLock, int x, int y);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_DragLeave(HWND hwndLock);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_DragMove(int x, int y);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_SetDragCursorImage(HIMAGELIST himlDrag, int iDrag, int dxHotspot, int dyHotspot);

WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_DragShowNolock(BOOL fShow);
WINCOMMCTRLAPI HIMAGELIST  WINAPI Amctl_ImageList_GetDragImage(POINT FAR* ppt,POINT FAR* pptHotspot);

#define     Amctl_ImageList_RemoveAll(himl) Amctl_ImageList_Remove(himl, -1)
#define     Amctl_ImageList_ExtractIcon(hi, himl, i) Amctl_ImageList_GetIcon(himl, i, 0)
#define     Amctl_ImageList_LoadBitmap(hi, lpbmp, cx, cGrow, crMask) Amctl_ImageList_LoadImage(hi, lpbmp, cx, cGrow, crMask, IMAGE_BITMAP, 0)

#ifdef __IStream_INTERFACE_DEFINED__
WINCOMMCTRLAPI HIMAGELIST WINAPI Amctl_ImageList_Read(LPSTREAM pstm);
WINCOMMCTRLAPI BOOL       WINAPI Amctl_ImageList_Write(HIMAGELIST himl, LPSTREAM pstm);
#endif

typedef struct _IMAGEINFO
{
    HBITMAP hbmImage;
    HBITMAP hbmMask;
    int     Unused1;
    int     Unused2;
    RECT    rcImage;
} IMAGEINFO;

WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_GetIconSize(HIMAGELIST himl, int FAR *cx, int FAR *cy);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_SetIconSize(HIMAGELIST himl, int cx, int cy);
WINCOMMCTRLAPI BOOL        WINAPI Amctl_ImageList_GetImageInfo(HIMAGELIST himl, int i, IMAGEINFO FAR* pImageInfo);
WINCOMMCTRLAPI HIMAGELIST  WINAPI Amctl_ImageList_Merge(HIMAGELIST himl1, int i1, HIMAGELIST himl2, int i2, int dx, int dy);

#endif

//====== TOPICLIST CONTROL =======================================================

#ifndef NOTOPICLST

#define WC_TOPICLST             "amxctl_TopicList"

#define TLS_OWNERDRAWFIXED      0x00000001
#define TLS_BUTTONS             0x00000002
#define TLS_HASSTRINGS          0x00000004
#define TLS_OWNERDRAWVARIABLE   0x00000008
#define TLS_SORT                0x00000010
#define TLS_VLIST               0x00000020
#define TLS_EXTENDEDSEL         0x00000040
#define TLS_MULTIPLESEL         0x00000080

#endif

//====== HEADER CONTROL =======================================================

#ifndef NOHEADER

#define WC_HEADER               "amxctl_HeaderCtl"

#define HDS_HORZ                0x00000000
#define HDS_BUTTONS             0x00000002
#define HDS_HIDDEN              0x00000008

typedef struct _HD_ITEM
{
    UINT    mask;
    int     cxy;
    LPSTR  pszText;
    HBITMAP hbm;
    int     cchTextMax;
    int     fmt;
    LPARAM  lParam;
} HD_ITEM;

#define HDI_WIDTH               0x0001
#define HDI_HEIGHT              HDI_WIDTH
#define HDI_TEXT                0x0002
#define HDI_FORMAT              0x0004
#define HDI_LPARAM              0x0008
#define HDI_BITMAP              0x0010

#define HDF_LEFT                0
#define HDF_RIGHT               1
#define HDF_CENTER              2
#define HDF_JUSTIFYMASK         0x0003

#define HDF_OWNERDRAW           0x8000
#define HDF_STRING              0x4000
#define HDF_BITMAP              0x2000
#define HDF_HIDDEN              0x1000


#define HDM_GETITEMCOUNT        (HDM_FIRST + 0)
#define Header_GetItemCount(hwndHD) \
    (int)SendMessage((hwndHD), HDM_GETITEMCOUNT, 0, 0L)


#define HDM_INSERTITEM         (HDM_FIRST + 1)

#define Header_InsertItem(hwndHD, i, phdi) \
    (int)SendMessage((hwndHD), HDM_INSERTITEM, (WPARAM)(int)(i), (LPARAM)(const HD_ITEM FAR*)(phdi))


#define HDM_DELETEITEM          (HDM_FIRST + 2)
#define Header_DeleteItem(hwndHD, i) \
    (BOOL)SendMessage((hwndHD), HDM_DELETEITEM, (WPARAM)(int)(i), 0L)


#define HDM_GETITEM            (HDM_FIRST + 3)

#define Header_GetItem(hwndHD, i, phdi) \
    (BOOL)SendMessage((hwndHD), HDM_GETITEM, (WPARAM)(int)(i), (LPARAM)(HD_ITEM FAR*)(phdi))


#define HDM_SETITEM            (HDM_FIRST + 4)

#define Header_SetItem(hwndHD, i, phdi) \
    (BOOL)SendMessage((hwndHD), HDM_SETITEM, (WPARAM)(int)(i), (LPARAM)(const HD_ITEM FAR*)(phdi))


typedef struct _HD_LAYOUT
{
    RECT FAR* prc;
    WINDOWPOS FAR* pwpos;
} HD_LAYOUT;


#define HDM_LAYOUT              (HDM_FIRST + 5)
#define Header_Layout(hwndHD, playout) \
    (BOOL)SendMessage((hwndHD), HDM_LAYOUT, 0, (LPARAM)(HD_LAYOUT FAR*)(playout))


#define HHT_NOWHERE             0x0001
#define HHT_ONHEADER            0x0002
#define HHT_ONDIVIDER           0x0004
#define HHT_ONDIVOPEN           0x0008
#define HHT_ABOVE               0x0100
#define HHT_BELOW               0x0200
#define HHT_TORIGHT             0x0400
#define HHT_TOLEFT              0x0800

typedef struct _HD_HITTESTINFO
{
    POINT pt;
    UINT flags;
    int iItem;
} HD_HITTESTINFO;
#define HDM_HITTEST             (HDM_FIRST + 6)

#define HDM_SETEXTENT           (HDM_FIRST + 7)

#define Header_SetExtent(hwndHD, i) \
    (int)SendMessage((hwndHD), HDM_SETEXTENT, (WPARAM)(int)(i), 0L)

#define HDM_SHOWITEM            (HDM_FIRST + 8)

#define Header_ShowItem(hwndHD, i, fShow) \
    (int)SendMessage((hwndHD), HDM_SHOWITEM, (WPARAM)(int)(i), (LPARAM)(fShow))

#define HDM_SETITEMWIDTH        (HDM_FIRST + 9 )

#define Header_SetItemWidth(hwndHD, i, w) \
    (int)SendMessage((hwndHD), HDM_SETITEMWIDTH, (WPARAM)(int)(i), (LPARAM)(w))

#define HDN_ITEMCHANGING       (HDN_FIRST-0)
#define HDN_ITEMCHANGED        (HDN_FIRST-1)
#define HDN_ITEMCLICK          (HDN_FIRST-2)
#define HDN_ITEMDBLCLICK       (HDN_FIRST-3)
#define HDN_DIVIDERDBLCLICK    (HDN_FIRST-5)
#define HDN_BEGINTRACK         (HDN_FIRST-6)
#define HDN_ENDTRACK           (HDN_FIRST-7)
#define HDN_TRACK              (HDN_FIRST-8)

typedef struct _HD_NOTIFY
{
    NMHDR   hdr;
    int     iItem;
    int     iButton;
    int     cxyUpdate;
    HD_ITEM FAR* pitem;
} HD_NOTIFY;

#endif


//====== TOOLBAR CONTROL ======================================================

#ifndef NOTOOLBAR

#define TOOLBARCLASSNAME        "amxctl_ToolbarWindow"

typedef struct _TBBUTTON {
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    DWORD dwData;
    int iString;
} TBBUTTON, NEAR* PTBBUTTON, FAR* LPTBBUTTON;
typedef const TBBUTTON FAR* LPCTBBUTTON;

typedef struct _TBDRAWBUTTON {
    RECT rc;
    TBBUTTON tb;
} TBDRAWBUTTON, NEAR* PTBDRAWBUTTON, FAR* LPTBDRAWBUTTON;
typedef const TBDRAWBUTTON FAR* LPCTBDRAWBUTTON;

typedef struct _COLORMAP {
    COLORREF from;
    COLORREF to;
} COLORMAP, FAR* LPCOLORMAP;

WINCOMMCTRLAPI HWND WINAPI Amctl_CreateToolbar(HWND hwnd, DWORD ws, UINT wID, int nBitmaps,
                        HINSTANCE hBMInst, UINT wBMID, LPCTBBUTTON lpButtons,
                        int iNumButtons, int dxButton, int dyButton,
                        int dxBitmap, int dyBitmap, UINT uStructSize);

WINCOMMCTRLAPI HBITMAP WINAPI Amctl_CreateMappedBitmap(HINSTANCE hInstance, int idBitmap,
                                  UINT wFlags, LPCOLORMAP lpColorMap,
                                  int iNumMaps);

#define CMB_MASKED              0x02

#define TBSTATE_CHECKED         0x01
#define TBSTATE_PRESSED         0x02
#define TBSTATE_ENABLED         0x04
#define TBSTATE_HIDDEN          0x08
#define TBSTATE_INDETERMINATE   0x10
#define TBSTATE_WRAP            0x20

#define TBSTYLE_BUTTON          0x00
#define TBSTYLE_SEP             0x01
#define TBSTYLE_CHECK           0x02
#define TBSTYLE_GROUP           0x04
#define TBSTYLE_CALLBACK        0x08
#define TBSTYLE_CHECKGROUP      (TBSTYLE_GROUP | TBSTYLE_CHECK)

#define TBSTYLE_TOOLTIPS        0x0100
#define TBSTYLE_WRAPABLE        0x0200
#define TBSTYLE_ALTDRAG         0x0400
#define TBSTYLE_VERT            0x0800
#define TBSTYLE_FLAT            0x1000
#define TBSTYLE_FIXEDWIDTH      0x2000
#define TBSTYLE_VISSEP        0x4000
#define TBSTYLE_NEWBUTTONS    0x8000

#define TB_ENABLEBUTTON         (WM_USER + 1)
#define TB_CHECKBUTTON          (WM_USER + 2)
#define TB_PRESSBUTTON          (WM_USER + 3)
#define TB_HIDEBUTTON           (WM_USER + 4)
#define TB_INDETERMINATE        (WM_USER + 5)
#define TB_ISBUTTONENABLED      (WM_USER + 9)
#define TB_ISBUTTONCHECKED      (WM_USER + 10)
#define TB_ISBUTTONPRESSED      (WM_USER + 11)
#define TB_ISBUTTONHIDDEN       (WM_USER + 12)
#define TB_ISBUTTONINDETERMINATE (WM_USER + 13)
#define TB_SETSTATE             (WM_USER + 17)
#define TB_GETSTATE             (WM_USER + 18)
#define TB_ADDBITMAP            (WM_USER + 19)

typedef struct tagTBADDBITMAP {
        HINSTANCE       hInst;
        UINT            nID;
} TBADDBITMAP, FAR *LPTBADDBITMAP;

#define HINST_COMMCTRL          ((HINSTANCE)-1)
#define IDB_STD_SMALL_COLOR     0
#define IDB_STD_LARGE_COLOR     1
#define IDB_VIEW_SMALL_COLOR    4
#define IDB_VIEW_LARGE_COLOR    5

// icon indexes for standard bitmap

#define STD_CUT                 0
#define STD_COPY                1
#define STD_PASTE               2
#define STD_UNDO                3
#define STD_REDOW               4
#define STD_DELETE              5
#define STD_FILENEW             6
#define STD_FILEOPEN            7
#define STD_FILESAVE            8
#define STD_PRINTPRE            9
#define STD_PROPERTIES          10
#define STD_HELP                11
#define STD_FIND                12
#define STD_REPLACE             13
#define STD_PRINT               14

// icon indexes for standard view bitmap

#define VIEW_LARGEICONS         0
#define VIEW_SMALLICONS         1
#define VIEW_LIST               2
#define VIEW_DETAILS            3
#define VIEW_SORTNAME           4
#define VIEW_SORTSIZE           5
#define VIEW_SORTDATE           6
#define VIEW_SORTTYPE           7
#define VIEW_PARENTFOLDER       8
#define VIEW_NETCONNECT         9
#define VIEW_NETDISCONNECT      10
#define VIEW_NEWFOLDER          11


#define TB_ADDBUTTONS           (WM_USER + 20)
#define TB_INSERTBUTTON         (WM_USER + 21)
#define TB_DELETEBUTTON         (WM_USER + 22)
#define TB_GETBUTTON            (WM_USER + 23)
#define TB_BUTTONCOUNT          (WM_USER + 24)
#define TB_COMMANDTOINDEX       (WM_USER + 25)

#ifdef _WIN32

typedef struct tagTBSAVEPARAMS {
    HKEY hkr;
    LPCSTR pszSubKey;
    LPCSTR pszValueName;
} TBSAVEPARAMS;

#endif

#define TB_CUSTOMIZE            (WM_USER + 27)
#define TB_ADDSTRING           (WM_USER + 28)
#define TB_GETITEMRECT          (WM_USER + 29)
#define TB_BUTTONSTRUCTSIZE     (WM_USER + 30)
#define TB_SETBUTTONSIZE        (WM_USER + 31)
#define TB_SETBITMAPSIZE        (WM_USER + 32)
#define TB_AUTOSIZE             (WM_USER + 33)
#define TB_SETBUTTONTYPE        (WM_USER + 34)
#define TB_GETTOOLTIPS          (WM_USER + 35)
#define TB_SETTOOLTIPS          (WM_USER + 36)
#define TB_SETPARENT            (WM_USER + 37)
#define TB_SETROWS              (WM_USER + 39)
#define TB_GETROWS              (WM_USER + 40)
#define TB_SETCMDID             (WM_USER + 42)
#define TB_CHANGEBITMAP         (WM_USER + 43)
#define TB_GETBITMAP            (WM_USER + 44)
#define TB_GETBUTTONTEXT       (WM_USER + 45)
#define TB_DRAWBUTTON           (WM_USER + 49)
#define TB_EDITMODE             (WM_USER + 50)
#define TB_HITTEST              (WM_USER + 51)
#define TB_GETBITMAPSIZE        (WM_USER + 52)

#define TBHT_LEFTEND            (-1)
#define TBHT_RIGHTEND           (-2)
#define TBHT_NOWHERE            (-3)

#define TBBF_LARGE              0x0001

#define TB_GETBITMAPFLAGS       (WM_USER + 41)

typedef struct tagTBCALLBACK {
   NMHDR hdr;
   TBBUTTON tb;
   UINT flag;
   HBITMAP hBmp;
   LPSTR pStr;
   char szStr[ 80 ];
} TBCALLBACK, FAR *LPTBCALLBACK;

#define TBCBF_WANTBITMAP        0x00
#define TBCBF_WANTSTRING        0x01

#define TBN_GETBUTTONINFO      (TBN_FIRST-0)
#define TBN_BEGINDRAG           (TBN_FIRST-1)
#define TBN_ENDDRAG             (TBN_FIRST-2)
#define TBN_BEGINADJUST         (TBN_FIRST-3)
#define TBN_ENDADJUST           (TBN_FIRST-4)
#define TBN_RESET               (TBN_FIRST-5)
#define TBN_QUERYINSERT         (TBN_FIRST-6)
#define TBN_QUERYDELETE         (TBN_FIRST-7)
#define TBN_TOOLBARCHANGE       (TBN_FIRST-8)
#define TBN_CUSTHELP            (TBN_FIRST-9)
#define TBN_BUTTONSTATECHANGED  (TBN_FIRST-10)
#define TBN_NEEDRECT            (TBN_FIRST-11)
#define TBN_CALLBACK            (TBN_FIRST-12)

typedef struct tagTBRECT {
    NMHDR   hdr;
    RECT    rc;
} TBRECT, FAR *LPTBRECT;

typedef struct tagTBNOTIFY {
    NMHDR   hdr;
    int     iItem;
    TBBUTTON tbButton;
    int     cchText;
    LPSTR   pszText;
} TBNOTIFY, FAR *LPTBNOTIFY;

#endif


//====== TOOLTIPS CONTROL =====================================================

#ifndef NOTOOLTIPS

#define TOOLTIPS_CLASS          "amxctl_tooltips_class"

typedef struct tagTOOLINFO {
    UINT cbSize;
    UINT uFlags;
    HWND hwnd;
    UINT uId;
    RECT rect;
    HINSTANCE hinst;
    LPSTR lpszText;
} TOOLINFO, NEAR *PTOOLINFO, FAR *LPTOOLINFO;

#define TTS_ALWAYSTIP           0x01
#define TTS_NOPREFIX            0x02

#define TTF_IDISHWND            0x01
#define TTF_CENTERTIP           0x02
#define TTF_COVERHWND           0x04

#define TTDT_AUTOMATIC          0
#define TTDT_RESHOW             1
#define TTDT_AUTOPOP            2
#define TTDT_INITIAL            3

#define TTM_ACTIVATE             (WM_USER + 1)
#define TTM_SETDELAYTIME         (WM_USER + 3)
#define TTM_ADDTOOL            (WM_USER + 4)
#define TTM_DELTOOL            (WM_USER + 5)
#define TTM_NEWTOOLRECT        (WM_USER + 6)
#define TTM_RELAYEVENT          (WM_USER + 7)
#define TTM_GETTOOLINFO        (WM_USER + 8)
#define TTM_SETTOOLINFO        (WM_USER + 9)
#define TTM_HITTEST            (WM_USER +10)
#define TTM_GETTEXT            (WM_USER +11)
#define TTM_UPDATETIPTEXT      (WM_USER +12)
#define TTM_GETTOOLCOUNT        (WM_USER +13)
#define TTM_ENUMTOOLS          (WM_USER +14)
#define TTM_GETCURRENTTOOL     (WM_USER + 15)
#define TTM_SETMARGIN          (WM_USER + 16)
#define TTM_GETMARGIN          (WM_USER + 17)
#define TTM_SETMAXTIPWIDTH     (WM_USER + 18)
#define TTM_GETMAXTIPWIDTH     (WM_USER + 19)
#define TTM_POP                (WM_USER + 20)
#define TTM_GETDELAYTIME       (WM_USER + 21)

typedef struct _TT_HITTESTINFO {
    HWND hwnd;
    POINT pt;
    TOOLINFO ti;
} TTHITTESTINFO, FAR * LPHITTESTINFO;

#define TTN_NEEDTEXT           (TTN_FIRST - 0)
#define TTN_SHOW                (TTN_FIRST - 1)
#define TTN_POP                 (TTN_FIRST - 2)

typedef struct tagTOOLTIPTEXT {
    NMHDR hdr;
    LPSTR lpszText;
    char szText[80];
    HINSTANCE hinst;
    UINT uFlags;
} TOOLTIPTEXT, FAR *LPTOOLTIPTEXT;

#endif


//====== STATUS BAR CONTROL ===================================================

#ifndef NOSTATUSBAR

#define SBARS_SIZEGRIP          0x0100


WINCOMMCTRLAPI void WINAPI Amctl_DrawlStatusText(HDC hDC, LPRECT lprc, LPCSTR pszText, UINT uFlags);
WINCOMMCTRLAPI HWND WINAPI Amctl_CreateStatusbar(LONG style, LPCSTR lpszText, HWND hwndParent, UINT wID);

#define STATUSCLASSNAME         "amxctl_statusbar"

#define SB_SETTEXT             (WM_USER+1)
#define SB_GETTEXT             (WM_USER+2)
#define SB_GETTEXTLENGTH       (WM_USER+3)

#define SB_SETPARTS             (WM_USER+4)
#define SB_GETPARTRECT          (WM_USER+5)
#define SB_GETPARTS             (WM_USER+6)
#define SB_GETBORDERS           (WM_USER+7)
#define SB_SETMINHEIGHT         (WM_USER+8)
#define SB_SIMPLE               (WM_USER+9)
#define SB_GETRECT              (WM_USER+10)

#define SB_BEGINSTATUSGAUGE     (WM_USER+14)
#define SB_STEPSTATUSGAUGE      (WM_USER+15)
#define SB_ENDSTATUSGAUGE       (WM_USER+16)

#define SBT_OWNERDRAW            0x1000
#define SBT_NOBORDERS            0x0100
#define SBT_POPOUT               0x0200
#define HBT_SPRING               0x0400

#endif

//====== DRAG LIST CONTROL ====================================================

#ifndef NODRAGLIST

typedef struct tagDRAGLISTINFO {
    UINT uNotification;
    HWND hWnd;
    POINT ptCursor;
} DRAGLISTINFO, FAR *LPDRAGLISTINFO;

#define DL_BEGINDRAG            (WM_USER+133)
#define DL_DRAGGING             (WM_USER+134)
#define DL_DROPPED              (WM_USER+135)
#define DL_CANCELDRAG           (WM_USER+136)

#define DL_CURSORSET            0
#define DL_STOPCURSOR           1
#define DL_COPYCURSOR           2
#define DL_MOVECURSOR           3

#define DRAGLISTMSGSTRING       "Amctl_DragListMsg"

WINCOMMCTRLAPI BOOL WINAPI Amctl_MakeDragList(HWND hLB);
WINCOMMCTRLAPI void WINAPI Amctl_DrawInsert(HWND handParent, HWND hLB, int nItem);
WINCOMMCTRLAPI int WINAPI Amctl_LBItemFromPt(HWND hLB, POINT pt, BOOL bAutoScroll);

#endif

//====== UPDOWN CONTROL =======================================================

#ifndef NOUPDOWN

#define UPDOWN_CLASS            "amxctl_updown"

typedef struct _UDACCEL {
    UINT nSec;
    UINT nInc;
} UDACCEL, FAR *LPUDACCEL;

#define UD_MAXVAL               0x7fff
#define UD_MINVAL               (-UD_MAXVAL)


#define UDS_WRAP                0x0001
#define UDS_SETBUDDYINT         0x0002
#define UDS_ALIGNRIGHT          0x0004
#define UDS_ALIGNLEFT           0x0008
#define UDS_AUTOBUDDY           0x0010
#define UDS_ARROWKEYS           0x0020
#define UDS_HORZ                0x0040
#define UDS_NOTHOUSANDS         0x0080


#define UDM_SETRANGE            (WM_USER+101)
#define UDM_GETRANGE            (WM_USER+102)
#define UDM_SETPOS              (WM_USER+103)
#define UDM_GETPOS              (WM_USER+104)
#define UDM_SETBUDDY            (WM_USER+105)
#define UDM_GETBUDDY            (WM_USER+106)
#define UDM_SETACCEL            (WM_USER+107)
#define UDM_GETACCEL            (WM_USER+108)
#define UDM_SETBASE             (WM_USER+109)
#define UDM_GETBASE             (WM_USER+110)


WINCOMMCTRLAPI HWND WINAPI Amctl_CreateUpDownControl(DWORD dwStyle, int x, int y, int cx, int cy,
                                HWND hParent, int nID, HINSTANCE hInst,
                                HWND hBuddy,
                                int nUpper, int nLower, int nPos);

typedef struct _NM_UPDOWN
{
    NMHDR hdr;
    int iPos;
    int iDelta;
} NM_UPDOWN, FAR *LPNM_UPDOWN;

#define UDN_DELTAPOS (UDN_FIRST - 1)

#endif

//====== PROGRESS CONTROL =====================================================

#ifndef NOPROGRESS

#define PROGRESS_CLASS          "amxctl_progress"

#define PBS_SMOOTH              0x0001

#define PBM_SETRANGE            (WM_USER+1)
#define PBM_SETPOS              (WM_USER+2)
#define PBM_DELTAPOS            (WM_USER+3)
#define PBM_SETSTEP             (WM_USER+4)
#define PBM_STEPIT              (WM_USER+5)
#define PBM_SETRANGE32          (WM_USER+6)  // lParam = high, wParam = low

typedef struct
{
   int iLow;
   int iHigh;
} PBRANGE, *PPBRANGE;

#define PBM_GETRANGE            (WM_USER+7)  // wParam = return (TRUE ? low : high). lParam = PPBRANGE or NULL
#define PBM_GETPOS              (WM_USER+8)

#endif

//====== HOTKEY CONTROL =======================================================

#ifndef NOHOTKEY

#define HOTKEYF_SHIFT           0x01
#define HOTKEYF_CONTROL         0x02
#define HOTKEYF_ALT             0x04
#define HOTKEYF_EXT             0x08

#define HKCOMB_NONE             0x0001
#define HKCOMB_S                0x0002
#define HKCOMB_C                0x0004
#define HKCOMB_A                0x0008
#define HKCOMB_SC               0x0010
#define HKCOMB_SA               0x0020
#define HKCOMB_CA               0x0040
#define HKCOMB_SCA              0x0080

#define HCN_SELCHANGED          (HCN_FIRST-0)

#define HKM_SETHOTKEY           (WM_USER+1)
#define HKM_GETHOTKEY           (WM_USER+2)
#define HKM_SETRULES            (WM_USER+3)

#define HOTKEY_CLASS            "amxctl_hotkey"

WINCOMMCTRLAPI void WINAPI Amctl_NewGetKeyNameText( WORD, LPSTR, UINT );

#endif

//====== TREEVIEW CONTROL =====================================================

#ifndef NOTREEVIEW

#define WC_TREEVIEW             "amxctl_SysTreeView"

#define TVS_HASBUTTONS          0x0001
#define TVS_HASLINES            0x0002
#define TVS_LINESATROOT         0x0004
#define TVS_EDITLABELS          0x0008
#define TVS_DISABLEDRAGDROP     0x0010
#define TVS_SHOWSELALWAYS       0x0020
#define TVS_OWNERDRAW           0x0040
#define TVS_WANTRETURN          0x0080
#define TVS_TOOLTIPS            0x0100

typedef DWORD HTREEITEM;

#define TVIF_TEXT               0x0001
#define TVIF_IMAGE              0x0002
#define TVIF_PARAM              0x0004
#define TVIF_STATE              0x0008
#define TVIF_HANDLE             0x0010
#define TVIF_SELECTEDIMAGE      0x0020
#define TVIF_CHILDREN           0x0040
#define TVIF_RECT               0x0080

#define TVIS_FOCUSED            0x0001
#define TVIS_SELECTED           0x0002
#define TVIS_CUT                0x0004
#define TVIS_DROPHILITED        0x0008
#define TVIS_BOLD               0x0010
#define TVIS_EXPANDED           0x0020
#define TVIS_EXPANDEDONCE       0x0040

#define TVIS_OVERLAYMASK        0x0F00
#define TVIS_STATEIMAGEMASK     0xF000
#define TVIS_USERMASK           0xF000

#define I_CHILDRENCALLBACK  (-1)

typedef struct _TV_ITEM {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPSTR     pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
    RECT      rcItem;
} TV_ITEM, FAR *LPTV_ITEM;

#define TVI_ROOT                ((HTREEITEM)0x0FFFF000)
#define TVI_FIRST               ((HTREEITEM)0x0FFFF001)
#define TVI_LAST                ((HTREEITEM)0x0FFFF002)
#define TVI_SORT                ((HTREEITEM)0x0FFFF003)

typedef struct _TV_INSERTSTRUCT {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
    TV_ITEM item;
} TV_INSERTSTRUCT, FAR *LPTV_INSERTSTRUCT;

#define TVM_INSERTITEM         (TV_FIRST + 0)

#define TreeView_InsertItem(hwnd, lpis) \
    (HTREEITEM)SendMessage((hwnd), TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)(lpis))

typedef struct _TV_DRAWITEM {
    LPSTR     pszText;
    TV_ITEM   tv;
} TV_DRAWITEM, FAR * LPTV_DRAWITEM;

#define TVM_DELETEITEM          (TV_FIRST + 1)
#define TreeView_DeleteItem(hwnd, hitem) \
    (BOOL)SendMessage((hwnd), TVM_DELETEITEM, 0, (LPARAM)(HTREEITEM)(hitem))


#define TreeView_DeleteAllItems(hwnd) \
    (BOOL)SendMessage((hwnd), TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT)


#define TVM_EXPAND              (TV_FIRST + 2)
#define TreeView_Expand(hwnd, hitem, code) \
    (BOOL)SendMessage((hwnd), TVM_EXPAND, (WPARAM)code, (LPARAM)(HTREEITEM)(hitem))


#define TVE_COLLAPSE            0x0001
#define TVE_EXPAND              0x0002
#define TVE_TOGGLE              0x0003
#define TVE_COLLAPSERESET       0x8000


#define TVM_GETITEMRECT         (TV_FIRST + 4)
#define TreeView_GetItemRect(hwnd, hitem, prc, code) \
    (*(HTREEITEM FAR *)prc = (hitem), (BOOL)SendMessage((hwnd), TVM_GETITEMRECT, (WPARAM)(code), (LPARAM)(RECT FAR*)(prc)))


#define TVM_GETCOUNT            (TV_FIRST + 5)
#define TreeView_GetCount(hwnd) \
    (UINT)SendMessage((hwnd), TVM_GETCOUNT, 0, 0)


#define TVM_GETINDENT           (TV_FIRST + 6)
#define TreeView_GetIndent(hwnd) \
    (UINT)SendMessage((hwnd), TVM_GETINDENT, 0, 0)


#define TVM_SETINDENT           (TV_FIRST + 7)
#define TreeView_SetIndent(hwnd, indent) \
    (BOOL)SendMessage((hwnd), TVM_SETINDENT, (WPARAM)indent, 0)


#define TVM_GETIMAGELIST        (TV_FIRST + 8)
#define TreeView_GetImageList(hwnd, iImage) \
    (HIMAGELIST)SendMessage((hwnd), TVM_GETIMAGELIST, iImage, 0)


#define TVSIL_NORMAL            0
#define TVSIL_STATE             2


#define TVM_SETIMAGELIST        (TV_FIRST + 9)
#define TreeView_SetImageList(hwnd, himl, iImage) \
    (HIMAGELIST)SendMessage((hwnd), TVM_SETIMAGELIST, iImage, (LPARAM)(HIMAGELIST)(himl))


#define TVM_GETNEXTITEM         (TV_FIRST + 10)
#define TreeView_GetNextItem(hwnd, hitem, code) \
    (HTREEITEM)SendMessage((hwnd), TVM_GETNEXTITEM, (WPARAM)code, (LPARAM)(HTREEITEM)(hitem))


#define TVGN_ROOT               0x0000
#define TVGN_NEXT               0x0001
#define TVGN_PREVIOUS           0x0002
#define TVGN_PARENT             0x0003
#define TVGN_CHILD              0x0004
#define TVGN_FIRSTVISIBLE       0x0005
#define TVGN_NEXTVISIBLE        0x0006
#define TVGN_PREVIOUSVISIBLE    0x0007
#define TVGN_DROPHILITE         0x0008
#define TVGN_CARET              0x0009

#define TreeView_GetChild(hwnd, hitem)          TreeView_GetNextItem(hwnd, hitem, TVGN_CHILD)
#define TreeView_GetNextSibling(hwnd, hitem)    TreeView_GetNextItem(hwnd, hitem, TVGN_NEXT)
#define TreeView_GetPrevSibling(hwnd, hitem)    TreeView_GetNextItem(hwnd, hitem, TVGN_PREVIOUS)
#define TreeView_GetParent(hwnd, hitem)         TreeView_GetNextItem(hwnd, hitem, TVGN_PARENT)
#define TreeView_GetFirstVisible(hwnd)          TreeView_GetNextItem(hwnd, NULL,  TVGN_FIRSTVISIBLE)
#define TreeView_GetNextVisible(hwnd, hitem)    TreeView_GetNextItem(hwnd, hitem, TVGN_NEXTVISIBLE)
#define TreeView_GetPrevVisible(hwnd, hitem)    TreeView_GetNextItem(hwnd, hitem, TVGN_PREVIOUSVISIBLE)
#define TreeView_GetSelection(hwnd)             TreeView_GetNextItem(hwnd, NULL,  TVGN_CARET)
#define TreeView_GetDropHilight(hwnd)           TreeView_GetNextItem(hwnd, NULL,  TVGN_DROPHILITE)
#define TreeView_GetRoot(hwnd)                  TreeView_GetNextItem(hwnd, NULL,  TVGN_ROOT)


#define TVM_SELECTITEM          (TV_FIRST + 11)
#define TreeView_Select(hwnd, hitem, code) \
    (HTREEITEM)SendMessage((hwnd), TVM_SELECTITEM, (WPARAM)code, (LPARAM)(HTREEITEM)(hitem))


#define TreeView_SelectItem(hwnd, hitem)            TreeView_Select(hwnd, hitem, TVGN_CARET)
#define TreeView_SelectDropTarget(hwnd, hitem)      TreeView_Select(hwnd, hitem, TVGN_DROPHILITE)
#define TreeView_SelectSetFirstVisible(hwnd, hitem) TreeView_Select(hwnd, hitem, TVGN_FIRSTVISIBLE)


#define TVM_GETITEM            (TV_FIRST + 12)

#define TreeView_GetItem(hwnd, pitem) \
    (BOOL)SendMessage((hwnd), TVM_GETITEM, 0, (LPARAM)(TV_ITEM FAR*)(pitem))


#define TVM_SETITEM            (TV_FIRST + 13)

#define TreeView_SetItem(hwnd, pitem) \
    (BOOL)SendMessage((hwnd), TVM_SETITEM, 0, (LPARAM)(const TV_ITEM FAR*)(pitem))


#define TVM_EDITLABEL          (TV_FIRST + 14)

#define TreeView_EditLabel(hwnd, hitem) \
    (HWND)SendMessage((hwnd), TVM_EDITLABEL, 0, (LPARAM)(HTREEITEM)(hitem))


#define TVM_GETEDITCONTROL      (TV_FIRST + 15)
#define TreeView_GetEditControl(hwnd) \
    (HWND)SendMessage((hwnd), TVM_GETEDITCONTROL, 0, 0)


#define TVM_GETVISIBLECOUNT     (TV_FIRST + 16)
#define TreeView_GetVisibleCount(hwnd) \
    (UINT)SendMessage((hwnd), TVM_GETVISIBLECOUNT, 0, 0)


#define TVM_HITTEST             (TV_FIRST + 17)
#define TreeView_HitTest(hwnd, lpht) \
    (HTREEITEM)SendMessage((hwnd), TVM_HITTEST, 0, (LPARAM)(LPTV_HITTESTINFO)(lpht))


typedef struct _TV_HITTESTINFO {
    POINT       pt;
    UINT        flags;
    HTREEITEM   hItem;
} TV_HITTESTINFO, FAR *LPTV_HITTESTINFO;

#define TVHT_NOWHERE            0x0001
#define TVHT_ONITEMICON         0x0002
#define TVHT_ONITEMLABEL        0x0004
#define TVHT_ONITEM             (TVHT_ONITEMICON | TVHT_ONITEMLABEL | TVHT_ONITEMSTATEICON)
#define TVHT_ONITEMINDENT       0x0008
#define TVHT_ONITEMBUTTON       0x0010
#define TVHT_ONITEMRIGHT        0x0020
#define TVHT_ONITEMSTATEICON    0x0040

#define TVHT_ABOVE              0x0100
#define TVHT_BELOW              0x0200
#define TVHT_TORIGHT            0x0400
#define TVHT_TOLEFT             0x0800


#define TVM_CREATEDRAGIMAGE     (TV_FIRST + 18)
#define TreeView_CreateDragImage(hwnd, hitem) \
    (HIMAGELIST)SendMessage((hwnd), TVM_CREATEDRAGIMAGE, 0, (LPARAM)(HTREEITEM)(hitem))


#define TVM_SORTCHILDREN        (TV_FIRST + 19)
#define TreeView_SortChildren(hwnd, hitem, recurse) \
    (BOOL)SendMessage((hwnd), TVM_SORTCHILDREN, (WPARAM)recurse, (LPARAM)(HTREEITEM)(hitem))


#define TVM_ENSUREVISIBLE       (TV_FIRST + 20)
#define TreeView_EnsureVisible(hwnd, hitem) \
    (BOOL)SendMessage((hwnd), TVM_ENSUREVISIBLE, 0, (LPARAM)(HTREEITEM)(hitem))


#define TVM_SORTCHILDRENCB      (TV_FIRST + 21)
#define TreeView_SortChildrenCB(hwnd, psort, recurse) \
    (BOOL)SendMessage((hwnd), TVM_SORTCHILDRENCB, (WPARAM)recurse, \
    (LPARAM)(LPTV_SORTCB)(psort))


#define TVM_ENDEDITLABELNOW     (TV_FIRST + 22)
#define TreeView_EndEditLabelNow(hwnd, fCancel) \
    (BOOL)SendMessage((hwnd), TVM_ENDEDITLABELNOW, (WPARAM)fCancel, 0)


#define TVM_GETISEARCHSTRING   (TV_FIRST + 23)

#define TreeView_GetISearchString(hwndTV, lpsz) \
        (BOOL)SendMessage((hwndTV), TVM_GETISEARCHSTRING, 0, (LPARAM)(LPCSTR)lpsz)

#define TVM_FINDSTRING           (TV_FIRST + 24)

typedef struct _TV_SEARCH
{
        HTREEITEM       hItem;
        LPSTR           pszText;
        WORD            wFlags;
} TV_SEARCH, FAR * LPTV_SEARCH;

#define TreeView_FindString(hwndTV, lpsz) \
        (HTREEITEM)SendMessage((hwndTV), TVM_FINDSTRING, 0, (LPARAM)(LPCSTR)lpsz)

#define TVM_SETBACKCOLOR         (TV_FIRST + 25 )
#define TVM_SETTEXTCOLOR         (TV_FIRST + 26 )

#define TreeView_SetTextColor(hwndTV, cr) \
        SendMessage((hwndTV), TVM_SETTEXTCOLOR, 0, (LPARAM)(cr))
#define TreeView_SetBackColor(hwndTV, cr) \
        SendMessage((hwndTV), TVM_SETBACKCOLOR, 0, (LPARAM)(cr))

typedef int (CALLBACK *PFNTVCOMPARE)(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
typedef struct _TV_SORTCB
{
        HTREEITEM       hParent;
        PFNTVCOMPARE    lpfnCompare;
        LPARAM          lParam;
} TV_SORTCB, FAR *LPTV_SORTCB;


typedef struct _NM_TREEVIEW {
    NMHDR       hdr;
    UINT        action;
    TV_ITEM    itemOld;
    TV_ITEM    itemNew;
    POINT       ptDrag;
} NM_TREEVIEW, FAR *LPNM_TREEVIEW;

#define TVN_SELCHANGING        (TVN_FIRST-1)
#define TVN_SELCHANGED         (TVN_FIRST-2)

#define TVC_UNKNOWN             0x0000
#define TVC_BYMOUSE             0x0001
#define TVC_BYKEYBOARD          0x0002

#define TVN_GETDISPINFO        (TVN_FIRST-3)
#define TVN_SETDISPINFO        (TVN_FIRST-4)

#define TVIF_DI_SETITEM         0x1000

typedef struct _TV_DISPINFO {
    NMHDR hdr;
    TV_ITEM item;
} TV_DISPINFO;

#define TVN_ITEMEXPANDING      (TVN_FIRST-5)
#define TVN_ITEMEXPANDED       (TVN_FIRST-6)
#define TVN_BEGINDRAG          (TVN_FIRST-7)
#define TVN_BEGINRDRAG         (TVN_FIRST-8)
#define TVN_DELETEITEM         (TVN_FIRST-9)
#define TVN_BEGINLABELEDIT     (TVN_FIRST-10)
#define TVN_ENDLABELEDIT       (TVN_FIRST-11)
#define TVN_KEYDOWN             (TVN_FIRST-12)

typedef struct _TV_KEYDOWN {
    NMHDR hdr;
    WORD wVKey;
    UINT flags;
} TV_KEYDOWN;

#endif


//====== TAB CONTROL ==========================================================

#ifndef NOTABCONTROL

#define WC_TABCONTROL           "amxctl_SysTabControl"

#define TCS_FORCEICONLEFT       0x0010
#define TCS_FORCELABELLEFT      0x0020
#define TCS_HOTTRACK            0x0040
#define TCS_TABS                0x0000
#define TCS_BUTTONS             0x0100
#define TCS_SINGLELINE          0x0000
#define TCS_MULTILINE           0x0200
#define TCS_RIGHTJUSTIFY        0x0000
#define TCS_FIXEDWIDTH          0x0400
#define TCS_RAGGEDRIGHT         0x0800
#define TCS_FOCUSONBUTTONDOWN   0x1000
#define TCS_OWNERDRAWFIXED      0x2000
#define TCS_TOOLTIPS            0x4000
#define TCS_FOCUSNEVER          0x8000


#define TCM_FIRST               0x1300

#define TCM_GETBKCOLOR          (TCM_FIRST + 0)
#define TabCtrl_GetBkColor(hwnd)  \
    (COLORREF)SendMessage((hwnd), TCM_GETBKCOLOR, 0, 0L)


#define TCM_SETBKCOLOR          (TCM_FIRST + 1)
#define TabCtrl_SetBkColor(hwnd, clrBk) \
    (BOOL)SendMessage((hwnd), TCM_SETBKCOLOR, 0, (LPARAM)(COLORREF)(clrBk))


#define TCM_GETIMAGELIST        (TCM_FIRST + 2)
#define TabCtrl_GetImageList(hwnd) \
    (HIMAGELIST)SendMessage((hwnd), TCM_GETIMAGELIST, 0, 0L)


#define TCM_SETIMAGELIST        (TCM_FIRST + 3)
#define TabCtrl_SetImageList(hwnd, himl) \
    (HIMAGELIST)SendMessage((hwnd), TCM_SETIMAGELIST, 0, (LPARAM)(UINT)(HIMAGELIST)(himl))


#define TCM_GETITEMCOUNT        (TCM_FIRST + 4)
#define TabCtrl_GetItemCount(hwnd) \
    (int)SendMessage((hwnd), TCM_GETITEMCOUNT, 0, 0L)



#define TCIF_TEXT               0x0001
#define TCIF_IMAGE              0x0002
#define TCIF_PARAM              0x0008


typedef struct _TC_ITEMHEADER
{
    UINT mask;
    UINT lpReserved1;
    UINT lpReserved2;
    LPSTR pszText;
    int cchTextMax;
    int iImage;
} TC_ITEMHEADER;

typedef struct _TC_ITEM
{
    UINT mask;
    UINT lpReserved1;
    UINT lpReserved2;
    LPSTR pszText;
    int cchTextMax;
    int iImage;

    LPARAM lParam;
} TC_ITEM;

#define TCM_GETITEM            (TCM_FIRST + 5)

#define TabCtrl_GetItem(hwnd, iItem, pitem) \
    (BOOL)SendMessage((hwnd), TCM_GETITEM, (WPARAM)(int)iItem, (LPARAM)(TC_ITEM FAR*)(pitem))


#define TCM_SETITEM            (TCM_FIRST + 6)

#define TabCtrl_SetItem(hwnd, iItem, pitem) \
    (BOOL)SendMessage((hwnd), TCM_SETITEM, (WPARAM)(int)iItem, (LPARAM)(TC_ITEM FAR*)(pitem))


#define TCM_INSERTITEM         (TCM_FIRST + 7)

#define TabCtrl_InsertItem(hwnd, iItem, pitem)   \
    (int)SendMessage((hwnd), TCM_INSERTITEM, (WPARAM)(int)iItem, (LPARAM)(const TC_ITEM FAR*)(pitem))


#define TCM_DELETEITEM          (TCM_FIRST + 8)
#define TabCtrl_DeleteItem(hwnd, i) \
    (BOOL)SendMessage((hwnd), TCM_DELETEITEM, (WPARAM)(int)(i), 0L)


#define TCM_DELETEALLITEMS      (TCM_FIRST + 9)
#define TabCtrl_DeleteAllItems(hwnd) \
    (BOOL)SendMessage((hwnd), TCM_DELETEALLITEMS, 0, 0L)


#define TCM_GETITEMRECT         (TCM_FIRST + 10)
#define TabCtrl_GetItemRect(hwnd, i, prc) \
    (BOOL)SendMessage((hwnd), TCM_GETITEMRECT, (WPARAM)(int)(i), (LPARAM)(RECT FAR*)(prc))


#define TCM_GETCURSEL           (TCM_FIRST + 11)
#define TabCtrl_GetCurSel(hwnd) \
    (int)SendMessage((hwnd), TCM_GETCURSEL, 0, 0)


#define TCM_SETCURSEL           (TCM_FIRST + 12)
#define TabCtrl_SetCurSel(hwnd, i) \
    (int)SendMessage((hwnd), TCM_SETCURSEL, (WPARAM)i, 0)


#define TCHT_NOWHERE            0x0001
#define TCHT_ONITEMICON         0x0002
#define TCHT_ONITEMLABEL        0x0004
#define TCHT_ONITEM             (TCHT_ONITEMICON | TCHT_ONITEMLABEL)


typedef struct _TC_HITTESTINFO
{
    POINT pt;
    UINT flags;
} TC_HITTESTINFO, FAR * LPTC_HITTESTINFO;


#define TCM_HITTEST             (TCM_FIRST + 13)
#define TabCtrl_HitTest(hwndTC, pinfo) \
    (int)SendMessage((hwndTC), TCM_HITTEST, 0, (LPARAM)(TC_HITTESTINFO FAR*)(pinfo))


#define TCM_ENABLETAB           (TCM_FIRST + 15)
#define TabCtrl_EnableTab(hwndTC, cb, flag) \
    (int)SendMessage((hwndTC), TCM_ENABLETAB, (WPARAM)(cb), (LPARAM)(BOOL)(flag))


#define TCM_GETENABLE           (TCM_FIRST + 16)
#define TabCtrl_GetEnable(hwndTC, cb) \
    (int)SendMessage((hwndTC), TCM_GETENABLE, (WPARAM)(cb), 0L)


#define TCM_SETITEMEXTRA        (TCM_FIRST + 14)
#define TabCtrl_SetItemExtra(hwndTC, cb) \
    (BOOL)SendMessage((hwndTC), TCM_SETITEMEXTRA, (WPARAM)(cb), 0L)


#define TCM_ADJUSTRECT          (TCM_FIRST + 40)
#define TabCtrl_AdjustRect(hwnd, bLarger, prc) \
    (void)SendMessage(hwnd, TCM_ADJUSTRECT, (WPARAM)(BOOL)bLarger, (LPARAM)(RECT FAR *)prc)


#define TCM_SETITEMSIZE         (TCM_FIRST + 41)
#define TabCtrl_SetItemSize(hwnd, x, y) \
    (DWORD)SendMessage((hwnd), TCM_SETITEMSIZE, 0, MAKELPARAM(x,y))


#define TCM_REMOVEIMAGE         (TCM_FIRST + 42)
#define TabCtrl_RemoveImage(hwnd, i) \
        (void)SendMessage((hwnd), TCM_REMOVEIMAGE, i, 0L)


#define TCM_SETPADDING          (TCM_FIRST + 43)
#define TabCtrl_SetPadding(hwnd,  cx, cy) \
        (void)SendMessage((hwnd), TCM_SETPADDING, 0, MAKELPARAM(cx, cy))


#define TCM_GETROWCOUNT         (TCM_FIRST + 44)
#define TabCtrl_GetRowCount(hwnd) \
        (int)SendMessage((hwnd), TCM_GETROWCOUNT, 0, 0L)


#define TCM_GETTOOLTIPS         (TCM_FIRST + 45)
#define TabCtrl_GetToolTips(hwnd) \
        (HWND)SendMessage((hwnd), TCM_GETTOOLTIPS, 0, 0L)


#define TCM_SETTOOLTIPS         (TCM_FIRST + 46)
#define TabCtrl_SetToolTips(hwnd, hwndTT) \
        (void)SendMessage((hwnd), TCM_SETTOOLTIPS, (WPARAM)hwndTT, 0L)


#define TCM_GETCURFOCUS         (TCM_FIRST + 47)
#define TabCtrl_GetCurFocus(hwnd) \
    (int)SendMessage((hwnd), TCM_GETCURFOCUS, 0, 0)

#define TCM_SETCURFOCUS         (TCM_FIRST + 48)
#define TabCtrl_SetCurFocus(hwnd, i) \
    SendMessage((hwnd),TCM_SETCURFOCUS, i, 0)


#define TCN_KEYDOWN             (TCN_FIRST - 0)
typedef struct _TC_KEYDOWN
{
    NMHDR hdr;
    WORD wVKey;
    UINT flags;
} TC_KEYDOWN;

#define TCN_SELCHANGE           (TCN_FIRST - 1)
#define TCN_SELCHANGING         (TCN_FIRST - 2)

#endif

//====== BIGEDIT CONTROL ======================================================

#ifndef NOBIGEDIT

#define WC_BIGEDIT      "amxctl_editplus"

#define  BES_3DBORDER            0x0001
#define  BES_USEATTRIBUTES       0x0002

typedef DWORD SELRANGE;
typedef SELRANGE FAR * LPSELRANGE;

typedef struct _LPBEC_SCROLL {
   int cbVScroll;
   int cbHScroll;
} BEC_SCROLL, * PBEC_SCROLL, FAR * LPBEC_SCROLL;

typedef struct _BEC_SELECTION {
   SELRANGE lo;
   SELRANGE hi;
} BEC_SELECTION, * PBEC_SELECTION, FAR * LPBEC_SELECTION;

typedef struct _BEC_GETTEXT {
   HPSTR hpText;
   BEC_SELECTION bsel;
   SELRANGE cchMaxText;
} BEC_GETTEXT, * PBEC_GETTEXT, FAR * LPBEC_GETTEXT;

typedef struct tagBEMN_HOTSPOT {
   NMHDR hdr;
   BOOL fHotspot;
   char szText[ 500 ];
} BEMN_HOTSPOT, FAR *LPBEMN_HOTSPOT;

#define  BEMN_CLICK                 (BEMN_FIRST-0)

#define  BEM_SETSEL                 (WM_USER+0x100)
#define  BEM_GETSEL                 (WM_USER+0x101)
#define  BEM_REPLACESEL             (WM_USER+0x102)
#define  BEM_GETTEXTLENGTH          (WM_USER+0x103)
#define  BEM_GETTEXT                (WM_USER+0x104)
#define  BEM_SCROLL                 (WM_USER+0x105)
#define  BEM_GETFIRSTVISIBLELINE    (WM_USER+0x106)
#define  BEM_GETRECT                (WM_USER+0x107)
#define  BEM_LINEFROMCHAR           (WM_USER+0x108)
#define  BEM_SETTEXTCOLOUR          (WM_USER+0x109)
#define  BEM_SETBACKCOLOUR          (WM_USER+0x110)
#define  BEM_GETTEXTCOLOUR          (WM_USER+0x111)
#define  BEM_GETBACKCOLOUR          (WM_USER+0x112)
#define  BEM_GETQUOTECOLOUR         (WM_USER+0x113)
#define  BEM_SETQUOTECOLOUR         (WM_USER+0x114)
#define  BEM_COLOURQUOTES           (WM_USER+0x115)
#define  BEM_SETHOTLINKCOLOUR       (WM_USER+0x116)
#define  BEM_ENABLEHOTLINKS         (WM_USER+0x117)
#define  BEM_SETWORDWRAP            (WM_USER+0x118)
#define  BEM_GETWORDWRAP            (WM_USER+0x119)
#define BEM_RESUMEDISPLAY        (WM_USER+0x120)

/* Macros.
 */
#define HANDLE_BEM_SETSEL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (LPBEC_SELECTION)(lParam)),0L)
#define FORWARD_BEM_SETSEL(hwnd, flag, lpbc, fn) \
    (void)(fn)((hwnd), BEM_SETSEL, (WPARAM)(flag), (LPARAM)(lpbc))

#define HANDLE_BEM_GETSEL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (LPBEC_SELECTION)(lParam)),0L)
#define FORWARD_BEM_GETSEL(hwnd, lpbc, fn) \
    (void)(fn)((hwnd), BEM_GETSEL, 0, (LPARAM)(lpbc))

#define HANDLE_EM_GETSEL(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_EM_GETSEL(hwnd, fn) \
    (void)(fn)((hwnd), BEM_GETSEL, 0, 0L)

#define HANDLE_BEM_REPLACESEL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (LPSTR)(lParam)),0L)
#define FORWARD_BEM_REPLACESEL(hwnd, lp, fn) \
    (void)(fn)((hwnd), BEM_REPLACESEL, 0, (LPARAM)(lp))

#define HANDLE_BEM_GETTEXTLENGTH(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETTEXTLENGTH(hwnd, fn) \
    (fn)((hwnd), BEM_GETTEXTLENGTH, 0, 0L)

#define HANDLE_BEM_LINEFROMCHAR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (SELRANGE)(lParam))
#define FORWARD_BEM_LINEFROMCHAR(hwnd, off, fn) \
    (fn)((hwnd), BEM_LINEFROMCHAR, 0, (LPARAM)(off))

#define HANDLE_EM_LINEINDEX(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (UINT)(wParam))
#define FORWARD_BEM_LINEINDEX(hwnd, line, fn) \
    (fn)((hwnd), EM_LINEINDEX, (WPARAM)(line), 0L)

#define HANDLE_EM_GETFIRSTVISIBLELINE(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_EM_GETFIRSTVISIBLELINE(hwnd, fn) \
    (fn)((hwnd), EM_GETFIRSTVISIBLELINE, 0, 0L)

#define HANDLE_BEM_GETFIRSTVISIBLELINE(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETFIRSTVISIBLELINE(hwnd, fn) \
    (fn)((hwnd), BEM_GETFIRSTVISIBLELINE, 0, 0L)

#define HANDLE_BEM_GETTEXT(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (LPBEC_GETTEXT)(lParam))
#define FORWARD_BEM_GETTEXT(hwnd, lp, fn) \
    (fn)((hwnd), BEM_GETTEXT, 0, (LPBEC_GETTEXT)(lp))

#define HANDLE_BEM_GETRECT(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (LPRECT)(lParam)),0L)
#define FORWARD_BEM_GETRECT(hwnd, lp, fn) \
    (void)(fn)((hwnd), BEM_GETRECT, 0, (LPBEC_GETRECT)(lp))

#define HANDLE_BEM_SCROLL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (LPBEC_SCROLL)(lParam)),0L)
#define FORWARD_BEM_SCROLL(hwnd, lp, fn) \
    (void)(fn)((hwnd), BEM_SCROLL, 0, (LPBEC_SCROLL)(lp))

#define HANDLE_BEM_GETTEXTCOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETTEXTCOLOUR(hwnd, fn) \
    (void)(fn)((hwnd), BEM_GETTEXTCOLOUR, 0, 0L)

#define HANDLE_BEM_SETTEXTCOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (COLORREF)(lParam), (BOOL)(wParam))
#define FORWARD_BEM_SETTEXTCOLOUR(hwnd, fRefresh, cr, fn) \
    (void)(fn)((hwnd), BEM_SETTEXTCOLOUR, (WPARAM)(fRefresh), (COLORREF)(cr))

#define HANDLE_BEM_GETBACKCOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETBACKCOLOUR(hwnd, fn) \
    (void)(fn)((hwnd), BEM_GETBACKCOLOUR, 0, 0L)

#define HANDLE_BEM_SETBACKCOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (COLORREF)(lParam), (BOOL)(wParam))
#define FORWARD_BEM_SETBACKCOLOUR(hwnd, fRefresh, cr, fn) \
    (void)(fn)((hwnd), BEM_SETBACKCOLOUR, (WPARAM)(fRefresh), (LPARAM)(cr))

#define HANDLE_BEM_GETQUOTECOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETQUOTECOLOUR(hwnd, fn) \
    (void)(fn)((hwnd), BEM_GETQUOTECOLOUR, 0, 0L)

#define HANDLE_BEM_SETQUOTECOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (COLORREF)(lParam), (BOOL)(wParam))
#define FORWARD_BEM_SETQUOTECOLOUR(hwnd, fRefresh, cr, fn) \
    (void)(fn)((hwnd), BEM_SETQUOTECOLOUR, (WPARAM)(fRefresh), (LPARAM)(cr))

#define HANDLE_BEM_SETHOTLINKCOLOUR(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (COLORREF)(lParam), (BOOL)(wParam))
#define FORWARD_BEM_SETHOTLINKCOLOUR(hwnd, fRefresh, cr, fn) \
    (void)(fn)((hwnd), BEM_SETHOTLINKCOLOUR, (WPARAM)(fRefresh), (LPARAM)(cr))

#define HANDLE_BEM_COLOURQUOTES(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (BOOL)wParam, (LPSTR)(lParam))
#define FORWARD_BEM_COLOURQUOTES(hwnd, fQuote, lp, fn) \
    (void)(fn)((hwnd), BEM_COLOURQUOTES, (WPARAM)(fQuote), (LPARAM)(lp))

#define HANDLE_BEM_ENABLEHOTLINKS(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (BOOL)wParam )
#define FORWARD_BEM_ENABLEHOTLINKS(hwnd, fEnable, fn) \
    (void)(fn)((hwnd), BEM_ENABLEHOTLINKS, (WPARAM)(fEnable), (LPARAM)(0L))

#define HANDLE_BEM_SETWORDWRAP(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (BOOL)wParam )
#define FORWARD_BEM_SETWORDWRAP(hwnd, fEnable, fn) \
    (void)(fn)((hwnd), BEM_SETWORDWRAP, (WPARAM)(fEnable), (LPARAM)(0L))

#define HANDLE_BEM_GETWORDWRAP(hwnd, wParam, lParam, fn) \
    (fn)((hwnd))
#define FORWARD_BEM_GETWORDWRAP(hwnd, fn) \
    (void)(fn)((hwnd), BEM_GETWORDWRAP, 0, 0L)

#endif


//====== VIRTUAL LIST CONTROL ===================================================

#define VLB_OK            0
#define VLB_ERR           -1
#define VLB_ENDOFFILE     -1

#define VLBLBOXID      100

#define VLBS_USEDATAVALUES     0x8000L  
#define VLBS_3DFRAME           0x4000L
#define VLBS_NOTIFY            0x0001L
#define VLBS_NOREDRAW          0x0004L
#define VLBS_OWNERDRAWFIXED    0x0010L
#define VLBS_HASSTRINGS        0x0040L
#define VLBS_USETABSTOPS       0x0080L
#define VLBS_NOINTEGRALHEIGHT  0x0100L
#define VLBS_WANTKEYBOARDINPUT 0x0400L
#define VLBS_DISABLENOSCROLL   0x1000L

// Application->VLIST messages               
// Corresponding to LB_ messages
#define VLB_MSGMIN              (WM_USER+500)
#define VLB_RESETCONTENT        (WM_USER+500)
#define VLB_SETCURSEL           (WM_USER+501)
#define VLB_GETCURSEL           (WM_USER+502)
#define VLB_GETTEXT             (WM_USER+503)
#define VLB_GETTEXTLEN          (WM_USER+504)
#define VLB_GETCOUNT            (WM_USER+505)
#define VLB_SELECTSTRING        (WM_USER+506)
#define VLB_FINDSTRING          (WM_USER+507)
#define VLB_GETITEMRECT         (WM_USER+508)
#define VLB_GETITEMDATA         (WM_USER+509)
#define VLB_SETITEMDATA         (WM_USER+510)
#define VLB_SETITEMHEIGHT       (WM_USER+511)
#define VLB_GETITEMHEIGHT       (WM_USER+512)
#define VLB_FINDSTRINGEXACT     (WM_USER+513)
#define VLB_INITIALIZE          (WM_USER+514)
#define VLB_SETTABSTOPS         (WM_USER+515)
#define VLB_GETTOPINDEX         (WM_USER+516)
#define VLB_SETTOPINDEX         (WM_USER+517)
#define VLB_GETHORIZONTALEXTENT (WM_USER+518)
#define VLB_SETHORIZONTALEXTENT (WM_USER+519)

// Unique to VLIST
#define VLB_UPDATEPAGE          (WM_USER+520)
#define VLB_GETLINES            (WM_USER+521)
#define VLB_PAGEDOWN            (WM_USER+522)
#define VLB_PAGEUP              (WM_USER+523)
#define VLB_MSGMAX            (WM_USER+523)

// VLIST->Application messages  
// Conflicts with VLB_
#define VLBR_FINDSTRING         (WM_USER+600) 
#define VLBR_FINDSTRINGEXACT    (WM_USER+601) 
#define VLBR_SELECTSTRING       (WM_USER+602) 
#define VLBR_GETITEMDATA        (WM_USER+603)
#define VLBR_GETTEXT            (WM_USER+604)
#define VLBR_GETTEXTLEN         (WM_USER+605)

// Unique Messages
//
#define VLB_FIRST               (WM_USER+606)
#define VLB_PREV                (WM_USER+607)
#define VLB_NEXT                (WM_USER+608)
#define VLB_LAST                (WM_USER+609)
#define VLB_FINDITEM            (WM_USER+610)
#define VLB_RANGE               (WM_USER+611)
#define VLB_FINDPOS             (WM_USER+612)

// VLIST->Application Notifications
#define VLBN_FREEITEM            (WM_USER+700)
#define VLBN_FREEALL             (WM_USER+701)

#define IDS_VLBOXNAME         1

typedef struct _VLBStruct {
   int   nCtlID;
   int   nStatus;
   LONG  lData;
   LONG  lIndex;
   LPSTR lpTextPointer;
   LPSTR lpFindString;
} VLBSTRUCT;

typedef VLBSTRUCT FAR*  LPVLBSTRUCT;

#define VLIST_CLASSNAME "amxctl_biglist"

#ifdef __cplusplus
}
#endif

#if defined(__BORLANDC__)
#pragma option -a-      /* Borland C++: Assume byte packing throughout */
#else
#pragma pack()          /* Microsoft C++: Revert to default packing throughout */
#endif

#endif
