/* PRINTMSG.C - Code that prints one or more messages in a topic
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
#include "palrc.h"
#include "resource.h"
#include "print.h"
#include "amlib.h"
#include <string.h>
#include "ameol2.h"
#include "rules.h"
#include "editmap.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static DWORD cbToPrint;                   /* Total number of messages to print */
static DWORD cPrinted;                    /* Count of messages written so far */
static DWORD cLastPrinted;                /* Last count of messages printed so far */
static int wMode;                         /* Print mode */
static AM_DATE dtFrom;                    /* Range: date from */
static AM_DATE dtTo;                      /* Range: date to */
static WORD wFromDate;                    /* Range: packed date from */
static WORD wToDate;                      /* Range: packed date to */
static DWORD dwFrom;                      /* Range: message from */
static DWORD dwTo;                        /* Range: message to */
static int cCopies;                       /* Number of copies to print */
static char szAuthor[ 40 ];               /* Author's name */
static LPVOID pData;                      /* Folder from which to print */
static BOOL fThrowPage = FALSE;           /* TRUE if we start a new page for each message */

LPSTR GetReadableText(PTL ptl, LPSTR pSource); // !!SM!! 2039

BOOL EXPORT CALLBACK PrintDlgHook( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintDlgHook_OnInitDialog( HWND, HWND, LPARAM );
BOOL FASTCALL PrintDlgHook_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK PrintMsgDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintMsgDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintMsgDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintMsgDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL UpdatePrinterName( HWND );
BOOL FASTCALL PrintDatabase( HPRINT, PCAT );
BOOL FASTCALL PrintFolder( HPRINT, PCL );
BOOL FASTCALL PrintTopic( HPRINT, PTL );
BOOL FASTCALL PrintOneMsg( HPRINT, PTH );

/* This function handles the Print dialog box.
 */
BOOL FASTCALL PrintMessage( HWND hwnd, BOOL fNonInteractive )
{
   HPRINT hPr;
   BOOL fOk;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return( FALSE );

   /* Show and get the print message dialog box.
    */
   if( fNonInteractive )
      {
      CURMSG curmsg;

      /* Non interactive mode, default to 1 copy of
       * the current message (if topic window open) or
       * all messages in the folder (if In Basket open).
       */
      Ameol2_GetCurrentMsg( &curmsg );
      if( NULL == ( pData = curmsg.pFolder ) )
         return( FALSE );
      wMode = IDD_ALL;
      cCopies = 1;
      wFromDate = 0;
      wToDate = 0;
      if( hwndTopic )
         {
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( hwndTopic );
         if( lpWindInfo->lpMessage )
            wMode = IDD_CURRENT;
         }
      }
   else if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTMSG), PrintMsgDlg, 0L ) )
      return( FALSE );

   /* The main print code starts here.
    */
   ASSERT( cCopies > 0 && cCopies < 100 );
   if( NULL != ( hPr = BeginPrint( hwnd, "Messages", &lfMsgFont ) ) )
   {
      register int cpy;

      fOk = TRUE;
      for( cpy = 0; cpy < cCopies; ++cpy )
      {
         ResetPageNumber( hPr );
         if( BeginPrintPage( hPr ) )
         {
            cPrinted = cLastPrinted = 0;
            switch( wMode )
            {
               case IDD_CURRENTTHREAD: {
                  LPWINDINFO lpWindInfo;
                  PTH pth;
                  PCL pcl;
         
                  /* Print the current thread from the current topic. If this option
                   * is choosen, pData is ignored.
                   */
                  ASSERT( NULL != hwndTopic );
                  lpWindInfo = GetBlock( hwndTopic );
                  pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
                  if( NULL != ( pth = lpWindInfo->lpMessage ) )
                     {
                     pth = Amdb_GetRootMsg( pth, TRUE );
                     while( pth && !fMainAbort )
                        {
                        if( !( fOk = PrintOneMsg( hPr, pth ) ) )
                           break;
                        ++cPrinted;
                        pth = Amdb_GetNextMsg( pth, VM_VIEWREFEX );
                        if( pth )
                           {
                           MSGINFO msginfo;
         
                           Amdb_GetMsgInfo( pth, &msginfo );
                           if( msginfo.wLevel == 0 )
                              pth = NULL;
                           }
                        }
                     }
                  break;
                  }
         
               case IDD_CURRENT: {
                  LPINT lpi;
                  PTH pth;
         
                  /* Print the current message from the current topic. If this option
                   * is choosen, pData is ignored.
                   * Now prints selected range of messages.
                   */
                  cbToPrint = 1;
                  ASSERT( NULL != hwndTopic );
                  if( NULL != ( lpi = GetSelectedItems( hwndTopic ) ) )
                     {
                     register int c;
                     HWND hwndList;
         
                     hwndList = GetDlgItem( hwndTopic, IDD_LIST );
                     for( c = 0; lpi[ c ] != LB_ERR; ++c )
                        {
                        pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
                        fOk = PrintOneMsg( hPr, pth );
                        if( fOk )
                           ++cPrinted;
                        if( fThrowPage && lpi[ c + 1] != LB_ERR )
                        {
                           EndPrintPage( hPr );
                           BeginPrintPage( hPr );
                        }
                        }
                     FreeMemory( &lpi );
                     }
                  break;
                  }
         
               case IDD_RANGE:
                  /* Print a range of messages from the selected folder. If this option is
                   * choosen, pData must be a topic.
                   */
                  ASSERT( Amdb_IsTopicPtr( pData ) );
                  fOk = PrintTopic( hPr, (PTL)pData );
                  break;
         
               case IDD_ALL:
                  /* Print all messages from the selected folder. If this option is choosen,
                   * pData can be a database, folder or topic.
                   */
                  if( Amdb_IsTopicPtr( pData ) )
                     {
                     TOPICINFO topicinfo;
         
                     Amdb_GetTopicInfo( (PTL)pData, &topicinfo );
                     cbToPrint = topicinfo.cMsgs;
                     fOk = PrintTopic( hPr, (PTL)pData );
                     }
                  else if( Amdb_IsFolderPtr( pData ) )
                     {
                     PTL ptl;
         
                     /* Count how many messages we're printing.
                      */
                     cbToPrint = 0;
                     for( ptl = Amdb_GetFirstTopic( (PCL)pData ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                        {
                        TOPICINFO topicinfo;
            
                        Amdb_GetTopicInfo( ptl, &topicinfo );
                        cbToPrint += topicinfo.cMsgs;
                        }
                     fOk = PrintFolder( hPr, (PCL)pData );
                     }
                  else
                     {
                     PCL pcl;
                     PTL ptl;
         
                     /* Count how many messages we're printing.
                      */
                     cbToPrint = 0;
                     for( pcl = Amdb_GetFirstFolder( (PCAT)pData ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                        for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                           {
                           TOPICINFO topicinfo;
         
                           Amdb_GetTopicInfo( ptl, &topicinfo );
                           cbToPrint += topicinfo.cMsgs;
                           }
                     fOk = PrintDatabase( hPr, (PCAT)pData );
                     }
                  break;
               case IDD_SELECTED: {

                  LPWINDINFO lpWindInfo;
                  HPSTR lpText;
                  BEC_SELECTION bsel;
                  HWND hwndMsg;
                  HEDIT hedit;
                  LPSTR lp2;
                  BOOL fSelected = FALSE;
#ifndef USEBIGEDIT                  
                  SELRANGE len;
#endif USEBIGEDIT                

                  /* If printing selection, set lpText to the
                   * selection.
                   */
                  ASSERT( NULL != hwndTopic );

                  lpWindInfo = GetBlock( hwndQuote );
                  if( lpWindInfo->lpMessage && ( NULL != ( lpText = Amdb_GetMsgText( lpWindInfo->lpMessage ) ) ) )
                  {

                  /* Get source and target edit windows.
                   */
                  hwndMsg = GetDlgItem( hwndTopic, IDD_MESSAGE );
                  hedit = Editmap_Open( hwndMsg );

                  /* Get the message we are going to quote.
                   */

                  Editmap_GetSelection( hedit, hwndMsg, &bsel );
#ifdef USEBIGEDIT                
                  if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_NEWS ) )
                     lp2 = GetReadableText(Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText ); // !!SM!! 2039
                  else
                     lp2 = GetTextBody( Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText );

                  if( fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
                  {
                     LPSTR lpszPreText;

                     lpszPreText = GetShortHeaderText( lpWindInfo->lpMessage );
                     if( NULL != lpszPreText && *lpszPreText )
                     {
                        if( strlen( lpszPreText ) > bsel.lo )
                        {
                           if( bsel.lo != bsel.hi )
                           {
                              fSelected = TRUE;
                              bsel.lo = strlen( lpszPreText );
                              bsel.hi = strlen( lpszPreText );
                           }
                        }
                              
                        else
                        {
                           bsel.lo -= strlen( lpszPreText );
                           bsel.hi -= strlen( lpszPreText );
                        }
                     }
                  }

                  if( bsel.lo != bsel.hi )
                     {
                     SELRANGE lenSel;

                     lenSel =  bsel.hi - bsel.lo;
                     if( bsel.lo )
                        _fmemcpy( lp2, lp2 + bsel.lo, (UINT)lenSel );
                     lp2[ lenSel ] = '\0';
                     }
                  else
                     fSelected = TRUE;
                  if( fOk && !fSelected )
                     fOk = PrintPage( hPr, lp2 );
                  Amdb_FreeMsgTextBuffer( lpText );
#else USEBIGEDIT                 
                  INITIALISE_PTR( lp2 );

                  if(bsel.hi > bsel.lo)
                     len = (bsel.hi - bsel.lo) + 1;
                  else
                     len = (bsel.lo - bsel.hi) + 1;

                  if( fNewMemory( &lp2, len + 1 ) )
                  {
//                   Editmap_GetText( hedit, hwndMsg, len, lp2 );
                     Editmap_GetSel( hedit, hwndMsg, len, lp2 );
                     fOk = PrintPage( hPr, lp2 );
                     FreeMemory( &lp2 );
                  }

#endif USEBIGEDIT                
                  }
                  break;
                  }
            }
            EndPrintPage( hPr );
         }
      }

      /* Finish printing and report on the
       * result to the user.
       */
      EndPrint( hPr );
      if( !fOk )
         fMessageBox( hwnd, 0, GS(IDS_STR96), MB_OK|MB_ICONEXCLAMATION );
   }
   return( TRUE );
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintMsgDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintMsgDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintMsgDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintMsgDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintMsgDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTMSG );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintMsgDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HWND hwndSpin;
   CURMSG curmsg;
   int index;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHORNAME ), sizeof( szAuthor ) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO1 ), 10 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROM1 ), 10 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO2 ), 20 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROM2 ), 20 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );

   /* Set number of copies.
    */
   SetDlgItemInt( hwnd, IDD_COPIES, 1, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_COPIES ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 99, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( 1, 0 ) );

   /* Display current printer name.
    */
   UpdatePrinterName( hwnd );

   /* Fill the list of folders combo box.
    */
   FillListWithTopics( hwnd, IDD_LIST, FTYPE_ALL|FTYPE_TOPICS );

   /* Select the default folder.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Ameol2_GetCurrentMsg( &curmsg );
   if( NULL != curmsg.pFolder )
      if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)curmsg.pFolder ) ) )
         {
         ComboBox_SetCurSel( hwndList, index );
         EnableScrExportOptions( hwnd, curmsg.pFolder );
         }

   /* Default to Current Message if topic window open,
    * All otherwise.
    */
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;
      HPSTR lpText;

      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo->lpMessage && ( NULL != ( lpText = Amdb_GetMsgText( lpWindInfo->lpMessage ) ) ) )
         {
         BEC_SELECTION bsel;
         HWND hwndMsg;
         HEDIT hedit;
         LPSTR lp2;

         /* Get source and target edit windows.
          */
         hwndMsg = GetDlgItem( hwndTopic, IDD_MESSAGE );
         hedit = Editmap_Open( hwndMsg );

         /* Get the message we are going to quote.
          */
         if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_NEWS ) )
            lp2 = GetReadableText(Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText ); // !!SM!! 2039
         else
            lp2 = GetTextBody( Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText );

         Editmap_GetSelection( hedit, hwndMsg, &bsel );

         if( bsel.lo != bsel.hi )
            CheckDlgButton( hwnd, IDD_SELECTED, TRUE );
         else
         {
            hwndList = GetDlgItem( hwndTopic, IDD_LIST );
            if( ListBox_GetSelCount( hwndList ) > 1 )
            {
               SetDlgItemText( hwnd, IDD_CURRENT, GS(IDS_STR858) );
               ShowWindow( GetDlgItem( hwnd, IDD_THROWPAGE ), SW_SHOW );
            }
         CheckDlgButton( hwnd, IDD_CURRENT, TRUE );
         }

         return( TRUE );
         }
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintMsgDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SETUP:
         PrintSetup( hwnd );
         UpdatePrinterName( hwnd );
         break;

      case IDD_FROM1:
      case IDD_TO1:
         if( codeNotify == EN_SETFOCUS )
            CheckRadioButton( hwnd, IDD_ALL, IDD_RANGE, IDD_RANGE );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPVOID pData;
            int index;

            /* Get the selected item.
             */
            index = ComboBox_GetCurSel( hwndCtl );
            pData = (LPVOID)ComboBox_GetItemData( hwndCtl, index );
            ASSERT( pData != NULL );
            EnableScrExportOptions( hwnd, pData );
            }
         break;

      case IDOK: {
         HWND hwndCombo;
         int index;

         /* Get number of copies.
          */
         if( !GetDlgInt( hwnd, IDD_COPIES, &cCopies, 1, 99 ) )
            break;

         /* Get the Print folder.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_LIST ) );
         index = ComboBox_GetCurSel( hwndCombo );
         pData = (LPVOID)ComboBox_GetItemData( hwndCombo, index );

         /* Get the author name.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_AUTHORNAME ) );
         ComboBox_GetText( hwndCombo, szAuthor, 40 );

         /* If message range selected, get it and validate the
          * ranges.
          */
         if( IsDlgButtonChecked( hwnd, IDD_RANGE ) )
            {
            if( !GetDlgLongInt( hwnd, IDD_FROM1, &dwFrom, 1, 99999999 ) )
               break;
            if( !GetDlgLongInt( hwnd, IDD_TO1, &dwTo, 1, 99999999 ) )
               break;
            if( dwFrom > dwTo )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR91), MB_OK|MB_ICONEXCLAMATION );
               SetFocus( GetDlgItem( hwnd, IDD_FROM1 ) );
               Edit_SetSel( GetDlgItem( hwnd, IDD_FROM1 ), 0, 32767 );
               break;
               }
            wMode = IDD_RANGE;
            }

         /* Handle other ranges.
          */
         else if( IsDlgButtonChecked( hwnd, IDD_ALL ) )
            wMode = IDD_ALL;
         else if( IsDlgButtonChecked( hwnd, IDD_CURRENTTHREAD ) )
            wMode = IDD_CURRENTTHREAD;
         else if( IsDlgButtonChecked( hwnd, IDD_SELECTED ) )
            wMode = IDD_SELECTED;
         else
            wMode = IDD_CURRENT;

         fThrowPage = IsDlgButtonChecked( hwnd, IDD_THROWPAGE );

         /* If date range selected, get the date range and validate
          * each date individually.
          */
         wFromDate = wToDate = 0;
         dtFrom.iDay = 1;
         dtFrom.iMonth = 1;
         dtFrom.iYear = 1970;
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FROM2 ) ) || Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO2 ) ) )
            {
            if( !BreakDate( hwnd, IDD_FROM2, &dtFrom ) )
               break;
            Amdate_GetCurrentDate( &dtTo );
            if( !BreakDate( hwnd, IDD_TO2, &dtTo ) )
               break;
            wFromDate = Amdate_PackDate( &dtFrom );
            wToDate = Amdate_PackDate( &dtTo );
            if( wFromDate > wToDate )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR92), MB_OK|MB_ICONEXCLAMATION );
               SetFocus( GetDlgItem( hwnd, IDD_FROM2 ) );
               Edit_SetSel( GetDlgItem( hwnd, IDD_FROM2 ), 0, 32767 );
               break;
               }
            }
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/*int gdwEnumFlags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;

BOOL FASTCALL FillPrinterList( HWND hwnd )
{
   DWORD  dwBytesNeeded;     
   DWORD  dwPrtRet1, dwPrtRet2;     
   DWORD  dwMaxPrt;     
   LPTSTR lpName = NULL;
   LPPRINTER_INFO_1 pPrtInfo1 = NULL;     
   LPPRINTER_INFO_2 pPrtInfo2 = NULL;          
   BOOL   bReturn = TRUE;          
   DWORD j;

   ComboBox_ResetContent( GetDlgItem(hwnd, IDD_PRINTERNAME) );
   SetWindowRedraw( GetDlgItem(hwnd, IDD_PRINTERNAME) , FALSE );

   //     
   // get byte count needed for buffer, alloc buffer, the enum the printers     
   //     
   
   EnumPrinters (gdwEnumFlags,   // types of printer objects to enumerate                             
      lpName,         // name of printer object                                            
      1,              // specifies type of printer info structure                          
      NULL,           // use NULL to get buffer size                                       
      0,              // size, in bytes, of array                                          
      &dwBytesNeeded, // pointer to variable with no. of bytes copied (or required)                                              
      &dwPrtRet1);    // pointer to variable with no. of printer info. structures copied                             
   //     
   // (simple error checking, if these work assume rest will too)     
   //          

   // Allocate enough room for the returned data     
   if (!(pPrtInfo1 = (LPPRINTER_INFO_1) LocalAlloc (LPTR, dwBytesNeeded)))     
   {         
//    ErrMsgBox (GetStringRes(IDS_ENUMPRTLALLOCFAIL), GetStringRes2(ERR_MOD_NAME));                  
      return FALSE;  // Bail     
   }          

   if (!EnumPrinters(gdwEnumFlags,      // types of printer objects to enumerate                              
      lpName,            // name of printer object                                             
      1,                 // specifies type of printer info structure                           
      (LPBYTE)pPrtInfo1, // pointer to buffer to receive printer info structures                                                         
      dwBytesNeeded,     // size, in bytes, of array                                           
      &dwBytesNeeded,    // pointer to variable with no. of bytes copied (or required)                                               
      &dwPrtRet1))       // pointer to variable with no. of printer info. structures copied                          
   {         
//    TCHAR  tcBuffer[256];                     // Create and display our error message         
//    wsprintf (tcBuffer, "%s, 1, GetLastError: %d", GetStringRes2(ERR_MOD_NAME), GetLastError());         
//    ErrMsgBox (GetStringRes(IDS_ENUMPRT1FAIL), tcBuffer);                   // Free the buffer we allocated         
      LocalFree(pPrtInfo1);          
      return FALSE;  // Bail     
   }          
   //     
   // If we don't get any printers from the Level == 1 call, there is     
   //  no point in continuing... report it, free memory, and return.     
   //     
   if (dwPrtRet1 == 0) 
   {
        MessageBox (hwndFrame, (LPCTSTR) "EnumPrinters (Level == 1) returned 0 printers", "", MB_OK);
      // Free the buffer we allocated         
      LocalFree(pPrtInfo1);          
      return FALSE;  // Bail     
   }               
   //     
   // Call EnumPrinters again, this time with Level == 2.     
   //     
   // get byte count needed for buffer, alloc buffer, the enum the printers     
   //     
   EnumPrinters (gdwEnumFlags,   // types of printer objects to enumerate                          
      lpName,                   // name of printer object                                         
      2,                // specifies type of printer info structure                       
      NULL,             // use NULL to get buffer size                                    
      0,                // size, in bytes, of array                                       
      &dwBytesNeeded,   // pointer to variable with no. of bytes copied (or required)                                           
      &dwPrtRet2);      // pointer to variable with no. of printer info. structures copied 
   // Allocate enough room for the returned data     
   if (!(pPrtInfo2 = (LPPRINTER_INFO_2) LocalAlloc (LPTR, dwBytesNeeded)))
   {
//    ErrMsgBox (GetStringRes(IDS_ENUMPRTLALLOCFAIL), GetStringRes2(ERR_MOD_NAME));     
   } 
   else 
   {    
      if (!EnumPrinters (gdwEnumFlags,      // types of printer objects to enumerate        
         lpName,            // name of printer object                       
         2,                 // specifies type of printer info structure     (
         (LPBYTE)pPrtInfo2, // pointer to buffer to receive printer info structures                                   
         dwBytesNeeded,     // size, in bytes, of array                     
         &dwBytesNeeded,    // pointer to variable with no. of bytes copied (or required)                         
         &dwPrtRet2))       // pointer to variable with no. of printer info. structures copied                       
         dwPrtRet2 = 0;     
   }          
   //     
   //  Calling EnumPrinters with Level == 2 frequently returns 0 printers.     
   //  If so display only the PRINTER_INFO_1 structures we got before.     
   //          
   if (dwPrtRet2 == 0)     
   {         
      dwMaxPrt = dwPrtRet1;         
      LocalFree (pPrtInfo2);         
      pPrtInfo2 = NULL;     
   } else {         
      dwMaxPrt = dwPrtRet1 > dwPrtRet2 ? dwPrtRet2 : dwPrtRet1;     
   }               

   for (j = 0; j < dwMaxPrt; j++) 
   { 
      ComboBox_AddString( GetDlgItem(hwnd, IDD_PRINTERNAME) , (pPrtInfo1 + j)->pName );
   }
   SetWindowRedraw( GetDlgItem(hwnd, IDD_PRINTERNAME) , TRUE );
   ComboBox_SetCurSel( GetDlgItem(hwnd, IDD_PRINTERNAME) , 0 );
   ComboBox_SetExtendedUI( GetDlgItem(hwnd, IDD_PRINTERNAME) , TRUE );

   if (pPrtInfo2)         
      LocalFree (pPrtInfo2);  
   if (pPrtInfo1)         
      LocalFree (pPrtInfo1);          
   return TRUE; 
}*/

/* This function updates the printer name in the Printer section
 * of the dialog box.
 */
void FASTCALL UpdatePrinterName( HWND hwnd )
{
   char szPrtName[ 64 ];

   GetPrinterName( szPrtName, sizeof(szPrtName) );
   wsprintf( lpTmpBuf, GS(IDS_STR940), szPrtName );
   SetDlgItemText( hwnd, IDD_PRINTERNAME, lpTmpBuf );
   //FillPrinterList( hwnd );

}

/* This function exports all messages from all topics in the specified
 * folder.
 */
BOOL FASTCALL PrintDatabase( HPRINT hpr, PCAT pcat )
{
   PCL pcl;

   for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      {
      if( !PrintFolder( hpr, pcl ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function exports all messages from all topics in the specified
 * folder.
 */
BOOL FASTCALL PrintFolder( HPRINT hpr, PCL pcl )
{
   PTL ptl;

   for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      {
      if( !PrintTopic( hpr, ptl ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function exports all messages in the specified folder.
 */
BOOL FASTCALL PrintTopic( HPRINT hpr, PTL ptl )
{
   PTH pth;
   BOOL fOk = TRUE;

   /* Export this topic.
    */
   Amdb_LockTopic( ptl );
   pth = Amdb_GetFirstMsg( ptl, VM_VIEWCHRON );
   for( ; fOk && pth; pth = Amdb_GetNextMsg( pth, VM_VIEWCHRON ) )
      {
      MSGINFO msginfo;

      /* Get information about the specified message.
       */
      Amdb_GetMsgInfo( pth, &msginfo );

      /* Update progress gauge.
       */
      TaskYield();

      /* If an author field was defined, validate this author
       * against the specified author.
       */
      if( *szAuthor )
         if( _strcmpi( szAuthor, msginfo.szAuthor ) != 0 )
            continue;

      /* If a date range was defined, ensure that the date of
       * this message falls within that range.
       */
      if( wFromDate && wToDate )
         if( msginfo.wDate < wFromDate || msginfo.wDate > wToDate )
            continue;

      /* Handle the abort key.
       */
      if( fMainAbort )
         fOk = FALSE;
      else switch( wMode )
         {
         case IDD_ALL:
            fOk = PrintOneMsg( hpr, pth );
            if( fOk )
               ++cPrinted;
            break;

         case IDD_RANGE:
            if( msginfo.dwMsg >= dwFrom && msginfo.dwMsg <= dwTo )
               {
               fOk = PrintOneMsg( hpr, pth );
               if( fOk )
                  ++cPrinted;
               }
            break;
         }
      }
   Amdb_UnlockTopic( ptl );
   return( fOk );
}

/* This function prints one copy of the specified message.
 */
BOOL FASTCALL PrintOneMsg( HPRINT hpr, PTH pth )
{
   char szDate[ 40 ];
   char szTime[ 40 ];
   AM_DATE date;
   AM_TIME time;
   MSGINFO msginfo;
   PCL pcl;
   PTL ptl;
   HPSTR lpszMsg;
   HPSTR lpszMsg2;
   char sz[ 550 ];
   DWORD dwTopicType = 0;
   int doneDate = 0;  //!!SM!! 2.55

   /* Get the message text and fail if we can't get it.
    */
   if( !( lpszMsg = Amdb_GetMsgText( pth ) ) )
      return( FALSE );

   /* Get some information about the specified
    * message.
    */
   Amdb_GetMsgInfo( pth, &msginfo );

   /* Print the From line.
    */
   ptl = Amdb_TopicFromMsg( pth );
   pcl = Amdb_FolderFromTopic( ptl );
   dwTopicType = Amdb_GetTopicType( ptl);
   wsprintf( sz, GS(IDS_STR304), msginfo.dwMsg, Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
   if( !PrintLine( hpr, sz ) )
      {
      Amdb_FreeMsgTextBuffer( lpszMsg );
      return( FALSE );
      }


   if( dwTopicType == FTYPE_MAIL || dwTopicType == FTYPE_NEWS )
   {
      Amdb_GetMailHeaderField( pth, "From", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {              
         wsprintf( sz, "From:    %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }
      }
      Amdb_GetMailHeaderField( pth, "To", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {              
         wsprintf( sz, "To:      %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }
      }
      Amdb_GetMailHeaderField( pth, "CC", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {              
         wsprintf( sz, "CC:      %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }         
      }

      Amdb_GetMailHeaderField( pth, "Newsgroups", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {              
         wsprintf( sz, "Groups:  %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }         
      }
      
      Amdb_GetMailHeaderField( pth, "Organization", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {               
         wsprintf( sz, "Org:     %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }         
      }
      Amdb_GetMailHeaderField( pth, "Date", lpTmpBuf, 500, TRUE );
      if(*lpTmpBuf)
      {              
         wsprintf( sz, "Date:    %s", (LPSTR)lpTmpBuf);
         if( !PrintLine( hpr, sz ) )
            {
            Amdb_FreeMsgTextBuffer( lpszMsg );
            return( FALSE );
            }         
         doneDate = 1; //!!SM!! 2.55
      }
      
   }
   else
   {
      if( msginfo.dwFlags & MF_OUTBOXMSG )
         wsprintf( sz, GS(IDS_STR312), (LPSTR)msginfo.szAuthor );
      else
         wsprintf( sz, GS(IDS_STR308), (LPSTR)msginfo.szAuthor );


      if( !PrintLine( hpr, sz ) )
         {
         Amdb_FreeMsgTextBuffer( lpszMsg );
         return( FALSE );
         }
   }

   if ( !doneDate ) //!!SM!! 2.55
   {
      Amdate_UnpackDate( msginfo.wDate, &date );
      Amdate_UnpackTime( msginfo.wTime, &time );
      Amdate_FormatShortDate( &date, szDate );
      Amdate_FormatTime( &time, szTime );
      wsprintf( sz, GS(IDS_STR307), (LPSTR)szDate, (LPSTR)szTime );
      if( !PrintLine( hpr, sz ) )
      {
         Amdb_FreeMsgTextBuffer( lpszMsg );
         return( FALSE );
      }
   }

   if( ( msginfo.dwComment && ( dwTopicType != FTYPE_MAIL && dwTopicType != FTYPE_NEWS ) ) )
      wsprintf( sz, GS(IDS_STR305), msginfo.dwComment );
   else
      wsprintf( sz, GS(IDS_STR306), (LPSTR)msginfo.szTitle );
   if( !PrintLine( hpr, sz ) )
      {
      Amdb_FreeMsgTextBuffer( lpszMsg );
      return( FALSE );
      }

   if( !PrintLine( hpr, "" ) )
      {
      Amdb_FreeMsgTextBuffer( lpszMsg );
      return( FALSE );
      }

   if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_NEWS ) )
      lpszMsg2 = GetReadableText( Amdb_TopicFromMsg(pth), lpszMsg ); // !!SM!! 2039
   else
      lpszMsg2 = GetTextBody( Amdb_TopicFromMsg(pth), lpszMsg );
   if( !PrintPage( hpr, lpszMsg2 ) )
      {
      Amdb_FreeMsgTextBuffer( lpszMsg );
      return( FALSE );
      }
   Amdb_FreeMsgTextBuffer( lpszMsg );
   if( hpr->y + hpr->dy * 2 < hpr->y2 )
      {
      if( !PrintLine( hpr, "" ) )
         return( FALSE );
      if( !PrintLine( hpr, "" ) )
         return( FALSE );
      }
   return( TRUE );
}
