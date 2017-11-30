/* STARTWIZ.C - Handles the startup wizard
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
#include "palrc.h"
#include "resource.h"
#include "string.h"
#include "amlib.h"
#include "command.h"
#include <shellapi.h>
#include "admin.h"
#include "lbf.h"

#define   THIS_FILE __FILE__

extern BOOL fIsAmeol2Scratchpad;

HWND hwndStartWiz = NULL;                              /* Startup wizard screen 1 window handle */
HWND hwndStartWizGuide = NULL;                         /* Startup wizard guide screen window handle */
HWND hwndStartWizProf = NULL;                          /* Startup wizard profile screen window handle */
static HFONT hTitleFont;                               /* Font used for title in Login dialog */


BOOL EXPORT CALLBACK StartWiz( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL StartWiz_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL StartWiz_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CmdStartWizGuide( HWND );
BOOL EXPORT CALLBACK StartWizGuide( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL StartWizGuide_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL StartWizGuide_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CmdStartWizProf( HWND );
BOOL EXPORT CALLBACK StartWizProf( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL StartWizProf_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL StartWizProf_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SetControls( HWND, int );
void FASTCALL SetContent( HWND, int );
void ReadCategories( HWND );

int iTabpage = 0;
int iTotaltabs = 6;
char FAR szProf1[ LEN_TEMPBUF ];
char FAR szProf2[ LEN_TEMPBUF ];
char FAR szProf3[ LEN_TEMPBUF ];
char FAR szProf4[ LEN_TEMPBUF ];
char FAR szProf5[ LEN_TEMPBUF ];
BOOL fSkipNextTime = FALSE;
HWND hwndList;

/* This function displays the Startup wizard.
 */
BOOL FASTCALL CmdStartWiz( HWND hwnd )
{
     if( NULL != hwndStartWiz )
          BringWindowToTop( hwndStartWiz );
     else
          hwndStartWiz = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_STARTWIZONE), (DLGPROC)StartWiz, 0L );
     return( TRUE );
}

/* This function handles the dialog box messages passed to the EventLog
 * dialog.
 */
BOOL EXPORT CALLBACK StartWiz( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_DLGMSG( hwnd, WM_INITDIALOG, StartWiz_OnInitDialog );
          HANDLE_DLGMSG( hwnd, WM_COMMAND, StartWiz_OnCommand );

          case WM_ADMHELP: {
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;
               }

          case WM_CLOSE: {
               DestroyWindow( hwnd );
               hwndStartWiz = NULL;
               hwndStartWizGuide = NULL;
               hwndStartWizProf = NULL;
               break;
               }
          }
     return( FALSE );
}


BOOL FASTCALL StartWiz_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{

     hTitleFont = CreateFont( 28, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                                             FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                                             CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                                             VARIABLE_PITCH | FF_SWISS, "Times New Roman" );


     PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_STARTWIZ );
     SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );

//   SetFocus( hwndStartWiz );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL StartWiz_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_HELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;

          case IDD_PROFILE:
               CmdStartWizProf( hwnd );
               break;

          case IDD_GUIDE:
               CmdStartWizGuide( hwnd );
               break;

          case IDD_USEAMEOL:
          case IDCANCEL:
          case IDOK: {
               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
               }
          }
}

/* This function displays the Startup wizard.
 */
BOOL FASTCALL CmdStartWizGuide( HWND hwnd )
{
     if( NULL != hwndStartWizGuide )
          BringWindowToTop( hwndStartWizGuide );
     else
          hwndStartWizGuide = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_STARTWIZGUIDE), (DLGPROC)StartWizGuide, 0L );
     return( TRUE );
}

/* This function handles the dialog box messages passed to the EventLog
 * dialog.
 */
BOOL EXPORT CALLBACK StartWizGuide( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_DLGMSG( hwnd, WM_INITDIALOG, StartWizGuide_OnInitDialog );
          HANDLE_DLGMSG( hwnd, WM_COMMAND, StartWizGuide_OnCommand );

          case WM_ADMHELP: {
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;
               }

          case WM_CLOSE: {
               DestroyWindow( hwnd );
               hwndStartWizGuide = NULL;
               break;
               }
          }
     return( FALSE );
}


BOOL FASTCALL StartWizGuide_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_STARTWIZ );
     iTabpage = 0;
     SetControls( hwnd, 0 );
     SetContent( hwnd, 0 );
     SetFocus( hwndStartWizGuide );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL StartWizGuide_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_HELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;

          case IDD_NEXT: {
               iTabpage++;
               SetControls( hwnd, iTabpage );
               SetContent( hwnd, iTabpage );
               break;
             }

          case IDD_BACK: {
               if( iTabpage > 0 )
                    iTabpage--;
               SetControls( hwnd, iTabpage );
               SetContent( hwnd, iTabpage );
               break;
             }

          case IDCANCEL:
          case IDOK: {
             PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
                       }
          }
}
/* This function displays the Startup wizard.
 */
BOOL FASTCALL CmdStartWizProf( HWND hwnd )
{
     if( NULL != hwndStartWizProf )
          BringWindowToTop( hwndStartWizProf );
     else
          hwndStartWizProf = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_STARTWIZPROF), (DLGPROC)StartWizProf, 0L );
     return( TRUE );
}

/* This function handles the dialog box messages passed to the EventLog
 * dialog.
 */
BOOL EXPORT CALLBACK StartWizProf( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_DLGMSG( hwnd, WM_INITDIALOG, StartWizProf_OnInitDialog );
          HANDLE_DLGMSG( hwnd, WM_COMMAND, StartWizProf_OnCommand );

          case WM_ADMHELP: {
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;
               }

          case WM_CLOSE: {
               DestroyWindow( hwnd );
               hwndStartWizProf = NULL;
               break;
               }
          }
     return( FALSE );
}


BOOL FASTCALL StartWizProf_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_STARTWIZ );
     VERIFY( hwndList = GetDlgItem( hwnd, IDD_PROFLIST ) );
     SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Choosing Profiles" );
     SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
     SetWindowText( GetDlgItem( hwnd, IDD_PROFTEXT ), fUseCIX? "CIX hosts a variety of interesting forums. To get you started we have selected some forums and grouped them into Profile categories.\r\n\r\nYou can choose up to five categories from the list shown here, then when you first connect you will be joined to the forums that match those categories. Joining forums does not cost anything, but your next connection will take slightly longer while the new messages are downloaded.\r\n\r\nPlease choose the categories that you are interested in from this list (maximum of five) and click OK to continue. If you want to change your profile you can come back to this page and choose again." : "As you have not set Ameol up to access CIX Forums, Profiles categories are not available." );
     ReadCategories( hwnd );
     SetFocus( hwndStartWizProf );

     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL StartWizProf_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_HELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSTARTWIZ );
               break;

          case IDOK: {
     
               int count, index, items, length, retCode;

               if(  ListBox_GetSelCount( hwndList ) > 5 )
               {
                    fMessageBox( GetFocus(), 0, "Please choose no more than five categories.", MB_OK|MB_ICONINFORMATION );
                    break;
               }
               items = 1;
               length = 0;
               szProf1[ 0 ] = '\0';
               szProf2[ 0 ] = '\0';
               szProf3[ 0 ] = '\0';
               szProf4[ 0 ] = '\0';
               szProf5[ 0 ] = '\0';
               count = ListBox_GetCount( hwndList );
               for( index = 0; index < count; ++index )
               {
                    if( ListBox_GetSel( hwndList, index ) )
                         {
                         ListBox_GetText( hwndList, index, lpTmpBuf );
                         if( items == 1 )
                              strcat( szProf1, lpTmpBuf );
                         else if( items == 2 )
                              strcat( szProf2, lpTmpBuf );
                         else if( items == 3 )
                              strcat( szProf3, lpTmpBuf );
                         else if( items == 4 )
                              strcat( szProf4, lpTmpBuf );
                         else if( items == 5 )
                              strcat( szProf5, lpTmpBuf );
                         items++;
                         }
               }
               retCode = IDOK;
               if( szProf1[ 0 ] )
                    wsprintf( lpTmpBuf, "You have chosen these categories:\r\n\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\nClick OK to continue or Cancel to cancel.", szProf1, szProf2, szProf3, szProf4, szProf5 );
               else
                    wsprintf( lpTmpBuf, "You have chosen no categories" );
               retCode = fMessageBox( GetFocus(), 0, lpTmpBuf, szProf1[ 0 ] ? MB_OKCANCEL|MB_ICONINFORMATION : MB_OK|MB_ICONINFORMATION );
               if( retCode == IDCANCEL )
               {
                    szProf1[ 0 ] = '\0';
                    szProf2[ 0 ] = '\0';
                    szProf3[ 0 ] = '\0';
                    szProf4[ 0 ] = '\0';
                    szProf5[ 0 ] = '\0';
                    break;
               }

               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
             }

          case IDCANCEL:
               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
          }
}
void FASTCALL SetControls( HWND hwnd, int iTabpage )
{
     switch( iTabpage )
     {
     case 0: {
               EnableControl( hwnd, IDD_BACK, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_NEXT ), "&Next >" );
               }
               break;
     case 1:
     case 2:
     case 3:
     case 4:
               {
               EnableControl( hwnd, IDD_BACK, TRUE );
               SetWindowText( GetDlgItem( hwnd, IDD_NEXT ), "&Next >" );
               }
               break;

     case 5: {
               EnableControl( hwnd, IDD_BACK, TRUE );
               SetWindowText( GetDlgItem( hwnd, IDD_NEXT ), "&Finish" );
               }
               break;
     case 6:
               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               break;
     case 7:
               break;

     }

}

void FASTCALL SetContent( HWND hwnd, int iTabpage )
{
     switch( iTabpage )
     {
     case 0: 
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Welcome to Ameol" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), "Welcome to Ameol.\r\n\r\nThis wizard will help you to become familiar with Ameol, Email and Forums.\r\n\r\nUse the scroll bar on the right to see all of the instructions.\r\n\r\nWhile the wizard is active you can use other parts of Ameol, then come back to the wizard at any time by selecting 'Show Startup Wizard' from the Help menu.\r\n\r\nTo go to the next page of the wizard click on the Next button shown below.\r\n\r\nIf you want to go back to an earlier page click the Back button." );
               }
          break;

     case 1:
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Reading Messages" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), "Ameol is an off-line reader. This means your Mail, News and Forum messages are downloaded and stored on your computer for viewing at any time that is convenient to you, and your replies are sent when you decide to connect.\r\n\r\nYou can move to the next unread message by pressing the Return key, and if the message is longer than a page you can scroll down it using the Page Down key.\r\n\r\nTo go back to the last message you read, press Backspace.\r\n\r\nTo read a different message area (called a 'Topic'), select that topic in the In Basket (select 'In Basket' from the 'View' menu) and press the Return key." );
               }
          break;
     case 2:
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Creating Messages" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), "It's easy to create new messages using Ameol.\r\n\r\nTo create a new Mail message select 'New Message' from the 'Mail' menu, enter the email address of the person you are sending the mail to, enter the Subject and then type the message text. When you have finished click the 'Save' button. As Ameol is an off-line reader your message is put into the Out Basket ready to be sent on your next connection.\r\n\r\nTo reply to a mail message choose 'Reply to Message' from the 'Mail' menu.\r\n\r\nYou can create new forum messages by selecting 'Say' from the 'Message' menu - this will create a new message in the topic you are reading.\r\n\r\nTo reply to a forum message (known as 'commenting'), select 'Comment' from the 'Message' menu - this will make your message a reply to the forum message you are reading.\r\n\r\nYou can make changes to your messages before sending them - choose 'Out Basket' from the 'View' menu, highlight your message and click the 'Edit' button." );
               }
          break;
     case 3:
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Finding New Forums" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), fUseCIX? "Another way to find forums is to use the CIX Forums List. Select 'Show All Forums' from the 'Forums' menu. If the list has not been downloaded yet click the 'Update' button and it will be collected when you next connect.\r\n\r\nWhen you have the list you can enter a word to search for in the box at the top of the window, and all forums which match that word will be displayed.\r\n\r\nSelect a forum that interests you and click the 'Join' button, you will be joined to the forum when you next connect to CIX." : "As you have not set Ameol up to access CIX Forums, the CIX Forums List is not available." );
               }
          break;
     case 4:
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Connecting to CIX" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), "Connecting to CIX is simple - just click the Connect toolbar button (the telephone near the centre of the toolbar) and Ameol will contact CIX, send any new messages you have written and saved in the Out Basket, and then download any new mail, forum and news messages waiting for you.\r\n\r\nOnce Ameol has disconnected from CIX any newly downloaded messages will be imported marked as unread.\r\n\r\nIf you have received any new email messages they will, by default, be placed in the 'Messages' mailbox in the 'Mail' folder. If you have received any new forum messages these will be placed within their respective folders and topics in the In Basket." );
               }
          break;
     case 5:
               {
               SetWindowText( GetDlgItem( hwnd, IDD_TITLETEXT ), "Help and Support" );
               SetWindowFont( GetDlgItem( hwnd, IDD_TITLETEXT ), hTitleFont, FALSE );
               SetWindowText( GetDlgItem( hwnd, IDD_BODYTEXT ), "If you ever get stuck in Ameol, there are a variety of ways to get help:\r\n\r\n1. The Help file - press the Help button on any unfamilar dialog and the Help file will explain the function of the dialog.\r\n\r\n2. The Tutorial - this is contained in the Tutorials folder in your In Basket.\r\n\r\n3. By contacting the Support department - use 'Submit Report' from the Help menu, mail support@cix.uk or call 03300 538 548." );
               }
          break;
     case 6:
          break;
     }
}

void ReadCategories( HWND hwnd )
{
     
     HNDFILE fh;
     char szDefCats[] = "cats.def";
     
     if( ( fh = Amfile_Open( szDefCats, AOF_READ ) ) != HNDFILE_ERROR )
          {
               LPLBF hlbf;

               if( hlbf = Amlbf_Open( fh, AOF_READ ) )
                    {
                    char sz[ 512 ];

                    /* Read each line from the default categories file and add it to
                     * the registry.
                     */
                    while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
                         {
                         if( strlen( sz ) > 0 )
                              ListBox_AddString( hwndList, sz );
                         }
                    Amlbf_Close( hlbf );
                    }
          }
}
