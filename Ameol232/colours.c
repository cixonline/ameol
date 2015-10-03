/* COLOURS.C - Implements the Colour Settings dialog
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
#include <stdio.h>
#include "ameol2.h"

#define  THIS_FILE   __FILE__

COLORREF crThreadWnd;
COLORREF crHighlight;
COLORREF crHighlightText;
COLORREF crNormalMsg;
COLORREF crMarkedMsg;
COLORREF crIgnoredMsg;
COLORREF crPriorityMsg;
COLORREF crInfoBar;
COLORREF crLoginAuthor;
COLORREF crMsgWnd;
COLORREF crMsgText;
COLORREF crEditWnd;
COLORREF crEditText;
COLORREF crUnReadMsg;
COLORREF crIgnoredTopic;
COLORREF crResignedTopic;
COLORREF crModerated;
COLORREF crMail;
COLORREF crNews;
COLORREF crKeepAtTop;
COLORREF crOutBaskWnd;
COLORREF crOutBaskText;
COLORREF crInBaskWnd;
COLORREF crInBaskText;
COLORREF crQuotedText;
COLORREF crHotlinks;
COLORREF crResumeWnd;
COLORREF crResumeText;
COLORREF crSchedulerWnd;
COLORREF crSchedulerText;
COLORREF crCIXListWnd;
COLORREF crCIXListText;
COLORREF crGrpListWnd;
COLORREF crGrpListText;
COLORREF crUserListWnd;
COLORREF crUserListText;
COLORREF crAddrBookWnd;
COLORREF crAddrBookText;
COLORREF crTermWnd;
COLORREF crTermText;

BOOL FASTCALL Customise_Colour_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Customise_Colour_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Customise_Colour_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Customise_Colour_SetColour( HWND );

#define  HIREF             0xFF000000
#define  SYSCOLOUR(x)      (HIREF|(x))
#define  APPCOLOUR(a,b,c)  (RGB((a),(b),(c)))

struct tagCOLOURARRAY {
   char * szItemName;
   COLORREF * pcrColourRef;
   char * npSectionName;
   COLORREF crDefaultColour;
   DWORD wColType;
   COLORREF crColourCopy;
} FAR aColourArray[] =
   {
   {  "Thread Window",           &crThreadWnd,     "ThreadPane",        SYSCOLOUR( COLOR_WINDOW ),       WIN_THREAD },
   {  "Highlight Bar",           &crHighlight,     "Highlight",         SYSCOLOUR( COLOR_HIGHLIGHT ),    WIN_THREAD },
   {  "Highlight Text",          &crHighlightText, "HighlightText",     SYSCOLOUR( COLOR_HIGHLIGHTTEXT ),   WIN_THREAD },
   {  "Normal Message",          &crNormalMsg,     "Normal",            SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_THREAD },
   {  "Marked Message",          &crMarkedMsg,     "Marked",            APPCOLOUR( 0, 0, 255 ),          WIN_THREAD },
   {  "Ignored Message",            &crIgnoredMsg,    "Ignored",           SYSCOLOUR( COLOR_BTNFACE ),         WIN_THREAD },
   {  "Priority Message",           &crPriorityMsg,      "Priority",          APPCOLOUR( 255, 0, 0 ),          WIN_THREAD },
   {  "Message Info Bar Text",      &crInfoBar,       "InfoBar",           SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INFOBAR },
   {  "Login Author",               &crLoginAuthor,      "LoginAuthor",       APPCOLOUR( 255, 0, 0 ),          WIN_THREAD },
   {  "Message Window",          &crMsgWnd,        "MessagePane",       SYSCOLOUR( COLOR_WINDOW ),       WIN_MESSAGE },
   {  "Message Text",               &crMsgText,       "Message",           SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_MESSAGE },
   {  "Quoted Text",             &crQuotedText,    "QuotedText",        APPCOLOUR( 0, 0, 255 ),          WIN_MESSAGE },
   {  "Hot links",               &crHotlinks,      "Hotlinks",          APPCOLOUR( 255, 0, 0 ),          WIN_MESSAGE },
   {  "Edit Window",             &crEditWnd,       "EditPane",          SYSCOLOUR( COLOR_WINDOW ),       WIN_EDITS },
   {  "Edit Text",               &crEditText,      "Edit",              SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_EDITS },
   {  "CIX Session Terminal",       &crTermWnd,       "CIXTermWnd",        SYSCOLOUR( COLOR_WINDOW ),       WIN_TERMINAL },
   {  "CIX Session Terminal Text",  &crTermText,      "CIXTermText",       SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_TERMINAL },
   {  "Out Basket",              &crOutBaskWnd,    "OutBasket",         SYSCOLOUR( COLOR_WINDOW ),       WIN_OUTBASK },
   {  "Out Basket Text",            &crOutBaskText,      "OutBasketText",     SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_OUTBASK },
   {  "In Basket",               &crInBaskWnd,     "InBasket",          SYSCOLOUR( COLOR_WINDOW ),       WIN_INBASK },
   {  "In Basket Text",          &crInBaskText,    "InBasketText",         SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INBASK },
   {  "Topic with Unread Messages", &crUnReadMsg,     "Unread",            APPCOLOUR( 255, 0, 0 ),          WIN_INBASK },
   {  "Ignored Topic",           &crIgnoredTopic,  "IgnoredTopic",         SYSCOLOUR( COLOR_BTNFACE ),         WIN_INBASK },
   {  "Resigned Topic",          &crResignedTopic, "ResignedTopic",     SYSCOLOUR( COLOR_BTNSHADOW ),    WIN_INBASK },
   {  "Moderated Forum",            &crModerated,     "ModeratedFolder",      SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INBASK },
   {  "Keep At Top",             &crKeepAtTop,     "KeepAtTop",         SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INBASK },
   {  "Resume Window",           &crResumeWnd,     "Resume",            SYSCOLOUR( COLOR_WINDOW ),       WIN_RESUMES },
   {  "Resume Text",             &crResumeText,    "ResumeText",        SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_RESUMES },
   {  "Scheduler Window",           &crSchedulerWnd,  "Scheduler",         SYSCOLOUR( COLOR_WINDOW ),       WIN_SCHEDULE },
   {  "Scheduler Text",          &crSchedulerText, "SchedulerText",     SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_SCHEDULE },
   {  "Address Book Window",        &crAddrBookWnd,      "AddressBook",       SYSCOLOUR( COLOR_WINDOW ),       WIN_ADDRBOOK },
   {  "Address Book Text",       &crAddrBookText,  "AddressBookText",      SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_ADDRBOOK },
   {  "Forum List Window",       &crCIXListWnd,    "ConferenceList",    SYSCOLOUR( COLOR_WINDOW ),       WIN_CIXLIST },
   {  "Forum List Text",            &crCIXListText,      "ConferenceListText",   SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_CIXLIST },
   {  "Usenet Group List Window",      &crGrpListWnd,    "UsenetList",        SYSCOLOUR( COLOR_WINDOW ),       WIN_GRPLIST },
   {  "Usenet Group List Text",     &crGrpListText,      "UsenetListText",    SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_GRPLIST },
   {  "User List Window",           &crUserListWnd,      "UserList",          SYSCOLOUR( COLOR_WINDOW ),       WIN_USERLIST },
   {  "User List Text",          &crUserListText,  "UserListText",         SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_USERLIST },
   {  "Mail Folder and Mailboxes",  &crMail,       "MainMailFolder",    SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INBASK },
   {  "News Folder and Newsgroups", &crNews,       "MailNewsFolder",    SYSCOLOUR( COLOR_WINDOWTEXT ),      WIN_INBASK }
   };
#define  MAX_ITEMS      (sizeof(aColourArray)/sizeof(aColourArray[0]))

static COLORREF FAR crDefArray[] = {
   RGB( 255,128,128 ),
   RGB( 255,0,0 ),
   RGB( 128,64,64 ),
   RGB( 128,0,0 ),
   RGB( 64,0,0 ),
   RGB( 0,0,0 ),
   RGB( 255,255,128 ),
   RGB( 255,255,0 ),
   RGB( 255,128,64 ),
   RGB( 255,128,0 ),
   RGB( 128,64,0 ),
   RGB( 128,128,0 ), 
   RGB( 128,255,128 ),
   RGB( 128,255,0 ),
   RGB( 0,255,0 ),
   RGB( 0,128,0 ),
   RGB( 0,64,0 ),
   RGB( 128,128,64 ),
   RGB( 0,255,128 ),
   RGB( 0,255,64 ),
   RGB( 0,128,128 ),
   RGB( 0,128,64 ),
   RGB( 0,64,64 ),
   RGB( 128,128,128 ),
   RGB( 128,255,255 ),
   RGB( 0,255,255 ),
   RGB( 0,64,128 ),
   RGB( 0,0,255 ),   
   RGB( 0,0,128 ),
   RGB( 64,128,128 ),
   RGB( 0,128,255 ),
   RGB( 0,128,192 ),
   RGB( 128,128,255 ),
   RGB( 0,0,160 ),
   RGB( 0,0,64 ),
   RGB( 192,192,192 ),
   RGB( 255,128,192 ),
   RGB( 128,128,192 ),
   RGB( 128,0,64 ),
   RGB( 128,0,128 ),
   RGB( 64,0,64 ),
   RGB( 64,0,64 ),
   RGB( 255,128,255 ),
   RGB( 255,0,255 ), 
   RGB( 255,0,128 ),
   RGB( 128,0,255 ),
   RGB( 64,0,128 ),
   RGB( 255,255,255 )
   };
#define  MAX_COLOURS    (sizeof(crDefArray)/sizeof(crDefArray[0]))

void FASTCALL WritePPColour( char *, COLORREF );
COLORREF FASTCALL GetPPColour( char *, COLORREF );

/* Initialises the colour settings
 */
void FASTCALL InitColourSettings( void )
{
   register int c;

   for( c = 0; c < MAX_ITEMS; ++c )
      {
      COLORREF crDef;

      if( ( crDef = aColourArray[ c ].crDefaultColour ) & HIREF )
         crDef = GetSysColor( (int)( crDef & ~HIREF ) );
      *aColourArray[ c ].pcrColourRef = GetPPColour( aColourArray[ c ].npSectionName, crDef );
      }
}

/* This function writes a colour setting to the configuration
 * file.
 */
void FASTCALL WritePPColour( char * pszEntry, COLORREF cr )
{
   char sz[ 20 ];

   wsprintf( sz, "%d %d %d", GetRValue( cr ), GetGValue( cr ), GetBValue( cr ) );
   Amuser_WritePPString( szColours, pszEntry, sz );
}

/* This function reads a colour setting from the configuration
 * file.
 */
COLORREF FASTCALL GetPPColour( char * pszEntry, COLORREF defcr )
{
   char sz[ 20 ];
   int red, green, blue;

   if( !Amuser_GetPPString( szColours, pszEntry, "", sz, sizeof( sz ) ) )
      return( defcr );
   sscanf( sz, "%d %d %d", &red, &green, &blue );
   return( RGB( red, green, blue ) );
}

/* This function dispatches messages for the News page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Customise_Colour( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Customise_Colour_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Customise_Colour_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Customise_Colour_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Customise_Colour_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   COLORREF crWhite;
   register int c;
   HWND hwndList;
   int iSel;

   /* Make a copy of the colour settings in case we cancel
    * the changes.
    */
   for( c = 0; c < MAX_ITEMS; ++c )
      aColourArray[ c ].crColourCopy = *aColourArray[ c ].pcrColourRef;

   /* Fill the list box.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   iSel = -1;
   for( c = 0; c < MAX_ITEMS; ++c )
      {
      int index;

      index = ListBox_InsertString( hwndList, -1, aColourArray[ c ].szItemName );
      if( ( aColourArray[ c ].wColType & Ameol2_GetWindowType() ) && iSel == -1 )
         iSel = index;
      }
   if( iSel == -1 )
      iSel = 0;

   /* Set the focus.
    */
   ListBox_SetCurSel( hwndList, iSel );

   /* Set the main well.
    */
   SendMessage( GetDlgItem( hwnd, IDD_MAINWELL ), WCM_SETITEMCOUNT, 0, MAKELPARAM( 8, 6 ) );
   SendMessage( GetDlgItem( hwnd, IDD_MAINWELL ), WCM_SETCOLOURARRAY, MAX_COLOURS, (LPARAM)(LPSTR)&crDefArray );

   /* Set the custom well.
    */
   crWhite = RGB( 255, 255, 255 );
   SendMessage( GetDlgItem( hwnd, IDD_CUSTWELL ), WCM_SETITEMCOUNT, 0, MAKELPARAM( 1, 1 ) );
   SendMessage( GetDlgItem( hwnd, IDD_CUSTWELL ), WCM_SETCOLOURARRAY, 1, (LPARAM)(LPSTR)&crWhite );
   EnableControl( hwnd, IDD_CUSTWELL, FALSE );

   /* Set the well selection for the first listbox
    * item.
    */
   Customise_Colour_SetColour( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Customise_Colour_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            Customise_Colour_SetColour( hwnd );
         break;

      case IDD_RESET: {
         COLORREF crDef;
         HWND hwndList;
         int index;

         /* Get the index of the selected item.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );

         /* Reset the colour.
          */
         if( ( crDef = aColourArray[ index ].crDefaultColour ) & HIREF )
            crDef = GetSysColor( (int)( crDef & ~HIREF ) );
         aColourArray[ index ].crColourCopy = crDef;

         /* Update the UI.
          */
         Customise_Colour_SetColour( hwnd );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_RESETALL: {
         register int c;

         for( c = 0; c < MAX_ITEMS; ++c )
            {
            COLORREF crDef;

            if( ( crDef = aColourArray[ c ].crDefaultColour ) & HIREF )
               crDef = GetSysColor( (int)( crDef & ~HIREF ) );
            aColourArray[ c ].crColourCopy = crDef;
            }
         Customise_Colour_SetColour( hwnd );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_CUSTOM: {
         CHOOSECOLOR cc;
         HWND hwndWell;
         COLORREF aclrCust[ 16 ];
         register int i;

         /* Initialise the custom colour grid.
          */
         for( i = 0; i < 16; i++ )
            {
            char sz[ 15 ];

            wsprintf( sz, "Custom%u", i+1 );
            aclrCust[ i ] = GetPPColour( sz, RGB( 255, 255, 255 ) );
            }
         hwndWell = GetDlgItem( hwnd, IDD_CUSTWELL );

         /* Initialise the custom colour array.
          */
         cc.lStructSize = sizeof( CHOOSECOLOR );
         cc.hwndOwner = hwnd;
         cc.lpCustColors = aclrCust;
         cc.Flags = CC_RGBINIT;
         cc.rgbResult = SendMessage( hwndWell, WCM_GETSELECTEDCOLOUR, 0, 0L );
         if( ChooseColor( &cc ) )
            {
            HWND hwndList;
            int index;

            /* Set the colour for the selected list item.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            index = ListBox_GetCurSel( hwndList );
            aColourArray[ index ].crColourCopy = cc.rgbResult;

            /* Save the custom colour settings.
             */
            for( i = 0; i < 16; i++ )
               {
               char sz[ 15 ];

               wsprintf( sz, "Custom%u", i+1 );
               WritePPColour( sz, aclrCust[ i ] );
               }

            /* Update the custom well colour.
             */
            SendMessage( hwndWell, WCM_SETCOLOURARRAY, 1, (LPARAM)(LPSTR)&cc.rgbResult );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }
      }
}

/* Highlight the colour setting for the selected item.
 */
void FASTCALL Customise_Colour_SetColour( HWND hwnd )
{
   COLORREF crThis;
   HWND hwndWell;
   HWND hwndList;
   int index;

   /* Get the index of the selected item.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   index = ListBox_GetCurSel( hwndList );

   /* List selection has changed. Highlight the selected colour
    * in the well. If the colour doesn't appear in the well, set
    * it in the custom well.
    */
   crThis = aColourArray[ index ].crColourCopy;
   hwndWell = GetDlgItem( hwnd, IDD_MAINWELL );
   SendMessage( hwndWell, WCM_SETSELECTEDCOLOUR, 0, crThis );
   hwndWell = GetDlgItem( hwnd, IDD_CUSTWELL );
   SendMessage( hwndWell, WCM_SETCOLOURARRAY, 1, (LPARAM)(LPSTR)&crThis );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Customise_Colour_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCUSTOMISE_COLOURS );
         break;

      case PSN_SETACTIVE:
         nLastCustomiseDialogPage = CDU_COLOURS;
         break;

      case WCN_SELCHANGED: {
         HWND hwndList;
         HWND hwndWell;
         COLORREF crThis;
         int index;

         /* A colour selection has changed, so update the working copy.
          * Also update the preview panel.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         crThis = SendMessage( lpnmhdr->hwndFrom, WCM_GETSELECTEDCOLOUR, 0, 0L );
         aColourArray[ index ].crColourCopy = crThis;
         PropSheet_Changed( GetParent( hwnd ), hwnd );

         /* Update the custom colour area.
          */
         hwndWell = GetDlgItem( hwnd, IDD_CUSTWELL );
         SendMessage( hwndWell, WCM_SETCOLOURARRAY, 1, (LPARAM)(LPSTR)&crThis );
         break;
         }

      case PSN_APPLY: {
         DWORD wChangedColTypes;
         register int c;

         /* Loop for each item in the list and if any have changed
          * colour, set the type of the item in the wChangedColTypes
          * flag.
          */
         wChangedColTypes = 0;
         for( c = 0; c < MAX_ITEMS; ++c )
            if( aColourArray[ c ].crColourCopy != *aColourArray[ c ].pcrColourRef )
               {
               *aColourArray[ c ].pcrColourRef = aColourArray[ c ].crColourCopy;
               wChangedColTypes |= aColourArray[ c ].wColType;
               WritePPColour( aColourArray[ c ].npSectionName, *aColourArray[ c ].pcrColourRef );
               }

         /* If we've changed the attributes of a message viewer window,
          * send a WM_APPCOLOURCHANGE to all the windows.
          */
         if( wChangedColTypes & (WIN_MESSAGE|WIN_THREAD|WIN_INFOBAR|WIN_INBASK) )
            {
            if( NULL != hwndMsg )
               SendAllWindows( WM_APPCOLOURCHANGE, (WPARAM)wChangedColTypes, 0L );
            }

         /* If we've changed the attributes of the in basket
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_INBASK )
            {
            if( NULL != hwndInBasket && !IsIconic( hwndInBasket ) )
               SendMessage( hwndInBasket, WM_APPCOLOURCHANGE, WIN_INBASK, 0L );
            }

         /* If we've changed the attributes of the address book
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_ADDRBOOK )
            if( NULL != hwndAddrBook && !IsIconic( hwndAddrBook ) )
               SendMessage( hwndAddrBook, WM_APPCOLOURCHANGE, WIN_ADDRBOOK, 0L );

         /* If we've changed the attributes of a resume window, so
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_RESUMES )
            SendAllWindows( WM_APPCOLOURCHANGE, WIN_RESUMES, 0L );

         /* We've changed the attributes of the conf list window, so
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_CIXLIST )
            if( NULL != hwndCIXList && !IsIconic( hwndCIXList )  )
               SendMessage( hwndCIXList, WM_APPCOLOURCHANGE, WIN_CIXLIST, 0L );

         /* We've changed the attributes of the conf list window, so
          * send a WM_APPCOLOURCHANGE to the window.
          */

         if( wChangedColTypes & WIN_GRPLIST )
            if( NULL != hwndGrpList && !IsIconic( hwndGrpList )  )
               SendMessage( hwndGrpList, WM_APPCOLOURCHANGE, WIN_GRPLIST, 0L );

         /* We've changed the attributes of the conf list window, so
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_USERLIST )
            if( NULL != hwndUserList && !IsIconic( hwndUserList )  )
               SendMessage( hwndUserList, WM_APPCOLOURCHANGE, WIN_USERLIST, 0L );

         /* If we've changed the attributes of the scheduler window, so
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_SCHEDULE )
            {
            if( NULL != hwndScheduler && !IsIconic( hwndScheduler ) )
               SendMessage( hwndScheduler, WM_APPCOLOURCHANGE, WIN_SCHEDULE, 0L );
            }

         /* If we've changed the attributes of the out basket
          * send a WM_APPCOLOURCHANGE to the window.
          */
         if( wChangedColTypes & WIN_OUTBASK )
            {
            if( !IsIconic( hwndOutBasket ) )
               SendMessage( hwndOutBasket, WM_APPCOLOURCHANGE, WIN_OUTBASK, 0L );
            }

         /* If we've changed the attributes of a terminal window.
          */
         if( wChangedColTypes & WIN_TERMINAL )
            SendAllWindows( WM_APPCOLOURCHANGE, WIN_TERMINAL, 0L );

         /* If we've changed the attributes of an edit window
          * send a WM_APPCOLOURCHANGE to the windows.
          */
         if( wChangedColTypes & WIN_EDITS )
            SendAllWindows( WM_APPCOLOURCHANGE, WIN_EDITS, 0L );
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}
