/* ABOUT.C - About dialog
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

#include "main.h"
#include "resource.h"
#include "palrc.h"
#include <string.h>
#include "ssce.h"
#include "common.bld"

#define  THIS_FILE   __FILE__

extern char szDebug[];

extern BOOL fLogSMTP;
extern BOOL fLogNNTP;
extern BOOL fLogPOP3;

static BOOL fDefDlgEx = FALSE;

static HFONT hDlgFont6;    /* 6-point dialog font */
static HFONT hDlgFont8;    /* 8-point dialog font */

BOOL EXPORT CALLBACK CmdAboutDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL CmdAboutDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdAboutDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CmdAboutDlg_OnInitDialog( HWND, HWND, LPARAM );
S16 (WINAPI * fSSCE_Version)(S16 *, S16 *);
static WORD wSSCEVersion;                       /* Version number of SSCE library */
static char szSpellVer[ 20 ] = "(not found)";
static char szSpellDictDate[ 20 ] = "(not found)";
static char szUserModule[] = "USER32";

/* This function displays the About dialog.
 */
void FASTCALL CmdAbout( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_ABOUT), CmdAboutDlg, 0L );
}

/* Handles the About dialog.
 */
BOOL EXPORT CALLBACK CmdAboutDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CmdAboutDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL CmdAboutDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CmdAboutDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CmdAboutDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsABOUT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL CmdAboutDlg_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
   switch (id)
   {
      case IDCANCEL:
         {
         /* Delete the two fonts we created at the start.*/
            
            DeleteFont(hDlgFont6);
            DeleteFont(hDlgFont8);
            EndDialog(hwnd, 0);
            break;
         }
      case IDOK:
         if (fUseInternet)
         {
            fLogSMTP = IsDlgButtonChecked(hwnd, IDC_CHKLOGSMTP);
            fLogPOP3 = IsDlgButtonChecked(hwnd, IDC_CHKLOGPOP3);
            fLogNNTP = IsDlgButtonChecked(hwnd, IDC_CHKLOGNNTP);
            
            Amuser_WritePPInt( szDebug, "LogSMTP", fLogSMTP );
            Amuser_WritePPInt( szDebug, "LogNNTP", fLogNNTP );
            Amuser_WritePPInt( szDebug, "LogPOP3", fLogPOP3 );
         }

         /* Delete the two fonts we created at the start.*/
         DeleteFont(hDlgFont6);
         DeleteFont(hDlgFont8);
         EndDialog(hwnd, 0);
         break;
   }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CmdAboutDlg_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
   char szBuf[400];
   char szTmpl[40];
   int idBmp;
   HNDFILE fh;
   AM_DATE date;
   AM_TIME time;
   WORD uDate;
   WORD uTime;

   /* Set the About... title on the dialog caption.
   */
   GetWindowText(hwnd, szTmpl, sizeof(szTmpl));
   wsprintf(szBuf, szTmpl, (LPSTR)amv.szFileDescription);
   SetWindowText(hwnd, szBuf);
   
   /* Get user and site name.
   */
   memset(szBuf, '\0', sizeof(szBuf));
   About_GetUserName(szBuf, sizeof(szBuf));
   SetDlgItemText(hwnd, IDC_USERNAME, szBuf);
   memset(szBuf, '\0', sizeof(szBuf));
   About_GetSiteName(szBuf, sizeof(szBuf));
   SetDlgItemText(hwnd, IDC_SITENAME, szBuf);
   
   /* Fill in information from the resources.
   */
   SetDlgItemText(hwnd, IDC_APPTITLE, amv.szFileDescription);
   SetDlgItemText(hwnd, IDC_COPYRIGHT, amv.szLegalCopyright);
   wsprintf(szBuf, GS(IDS_VERSION), amv.nMaxima, amv.nMinima, amv.nBuild);
   
   if (IS_BETA)
      wsprintf(szBuf, "%s - BETA", szBuf);
   
   SetDlgItemText(hwnd, IDC_VERSION, szBuf);
   
   /* Retrieve the disclaimer string. Because it's longer than 255 characters,
   * it has to be retrieved in two parts.
   */
   strcpy(szBuf, GS(IDS_DISCLAIMER1));
   strcat(szBuf, GS(IDS_DISCLAIMER2));
   SetDlgItemText(hwnd, IDC_DISCLAIMER, szBuf);
   
   /* Get the spell DLL version number and dict date */
   
   if (!hSpellLib)
      hSpellLib = LoadLibrary(szSSCELib);
   if (NULL != hSpellLib)
   {
      S16 maxSSCEVer;
      S16 minSSCEVer;
      
      /* Start with some version numbering stuff.
      */
      (FARPROC)fSSCE_Version = GetProcAddress(hSpellLib, "SSCE_Version");
      
      /* Get the SSCE library version.
      */
      if (NULL != fSSCE_Version)
      {
         fSSCE_Version(&maxSSCEVer, &minSSCEVer);
         wsprintf(szSpellVer, "%d.%d", maxSSCEVer, minSSCEVer);
      }
   }
   
   BEGIN_PATH_BUF;
   wsprintf(lpPathBuf, "%s%sSSCEBRM.CLX", (LPSTR)pszAmeolDir, (LPSTR)"DICT\\");
   if ((fh = Amfile_Open(lpPathBuf, AOF_READ)) != HNDFILE_ERROR)
   {
      Amfile_GetFileTime(fh, &uDate, &uTime);
      Amdate_UnpackDate(uDate, &date);
      Amdate_UnpackTime(uTime, &time);
      wsprintf(lpTmpBuf, "%s", Amdate_FormatShortDate(&date, NULL));
      wsprintf(szSpellDictDate, lpTmpBuf);
      Amfile_Close(fh);
   }
   END_PATH_BUF;
   
   wsprintf(szBuf, GS(IDS_COPYRIGHT2), szSpellVer, szSpellDictDate);
   SetDlgItemText(hwnd, IDC_COPYRIGHT2, szBuf);
   
   /* Create two new fonts specific to this dialog. The 6pt font
   * is used for the copyright warning message.
   */
   hDlgFont8 = CreateFont(8, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
      FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
      VARIABLE_PITCH | FF_SWISS, "Helv");
   hDlgFont6 = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
      FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
      VARIABLE_PITCH | FF_SWISS, "Arial");
   
   /* Display the titles in bold.
   */
   SetWindowFont(GetDlgItem(hwnd, IDC_APPTITLE), hSys10Font, FALSE);
   SetWindowFont(GetDlgItem(hwnd, IDC_COPYRIGHT), hSys8Font, FALSE);
   
   /* Set the picture control.
   */
   idBmp = fOldLogo ? IDB_ABOUT_OLD  : IDB_ABOUT;
   if (!*szAppLogo)
      PicCtrl_SetBitmap(GetDlgItem(hwnd, IDD_LOGO), hRscLib, idBmp);
   else
   {
      PICCTRLSTRUCT pcs;
      
      pcs.hBitmap = Amuser_LoadBitmapFromDisk(szAppLogo, &pcs.hPalette);
      if (NULL != pcs.hBitmap)
         PicCtrl_SetBitmapHandle(GetDlgItem(hwnd, IDD_LOGO), &pcs);
      else
         PicCtrl_SetBitmap(GetDlgItem(hwnd, IDD_LOGO), hRscLib, idBmp);
   }

   if (fUseInternet)
   {
      CheckDlgButton(hwnd, IDC_CHKLOGSMTP, fLogSMTP);
      CheckDlgButton(hwnd, IDC_CHKLOGPOP3, fLogPOP3);
      CheckDlgButton(hwnd, IDC_CHKLOGNNTP, fLogNNTP);
   }
   else
   {
      ShowWindow(GetDlgItem( hwnd, IDC_CHKLOGSMTP), FALSE);
      ShowWindow(GetDlgItem( hwnd, IDC_CHKLOGPOP3), FALSE);
      ShowWindow(GetDlgItem( hwnd, IDC_CHKLOGNNTP), FALSE);
   }
   
   /* Set some text on the dialog to a different font from normal.
   */
   SetWindowFont(GetDlgItem(hwnd, IDC_DISCLAIMER), hDlgFont6, FALSE);
   SetWindowFont(GetDlgItem(hwnd, IDC_USERNAME), hDlgFont8, FALSE);
   SetWindowFont(GetDlgItem(hwnd, IDC_SITENAME), hDlgFont8, FALSE);
   SetWindowFont(GetDlgItem(hwnd, IDC_COPYRIGHT2), hDlgFont8, FALSE);
   return (TRUE);
}

/* This function returns the user name.
 */
void FASTCALL About_GetUserName( char * pUserName, int cbBuf )
{
   if( !Amuser_GetLMString( szInfo, "UserName", pUserName, pUserName, cbBuf ) )
      LoadString( GetModuleHandle( szUserModule ), 514, pUserName, cbBuf );
}

/* This function returns the site name.
 */
void FASTCALL About_GetSiteName( char * pSiteName, int cbBuf )
{
   if( !Amuser_GetLMString( szInfo, "SiteName", pSiteName, pSiteName, cbBuf ) )
      LoadString( GetModuleHandle( szUserModule ), 515, pSiteName, cbBuf );
}
