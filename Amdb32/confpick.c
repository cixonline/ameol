/* CONFPICK.C - Implements the conference/topic picker
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
#include <windowsx.h>
#include <assert.h>
#include "amcntxt.h"
#include <htmlhelp.h>
#include "amlib.h"
#include "amctrls.h"
#include "amuser.h"
#include "amdbi.h"
#include "amdbrc.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

/* Database folder view indexes
 */
#define  IBML_CATEGORY           0        /* Index of category icon */
#define  IBML_SELCATEGORY        1        /* Index of expanded category icon */
#define  IBML_FOLDER             2        /* Index of folder icon */
#define  IBML_SELFOLDER          3        /* Index of expanded folder icon */
#define  IBML_NEWS               4        /* Index of news folder icon */
#define  IBML_MAIL               5        /* Index of mail folder icon */
#define  IBML_UNTYPED            6        /* Index of untyped folder icon */
#define  IBML_CIX                7        /* Index of CIX topic icon */
#define  IBML_LOCAL              8        /* Index of local topic icon */
#define  IBML_MAX                9        /* Total number of icons */

BOOL EXPORT CALLBACK ConfPicker( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ConfPicker_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ConfPicker_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ConfPicker_OnNotify( HWND, int, LPNMHDR );
LRESULT FASTCALL ConfPicker_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL FillInBasket( HWND );
HTREEITEM FASTCALL InsertCategory( HWND, PCAT, BOOL );
HTREEITEM FASTCALL InsertFolder( HWND, HTREEITEM, PCL, BOOL, HTREEITEM, BOOL );
HTREEITEM FASTCALL InsertTopic( HWND, HTREEITEM, PTL, HTREEITEM, BOOL );
HTREEITEM FASTCALL Amdb_FindTopicItemInBranch( HWND, HTREEITEM, PTL );
HTREEITEM FASTCALL Amdb_FindFolderItemInBranch( HWND, HTREEITEM, PCL );
int FASTCALL GetTopicTypeImage( PTL );

extern const char NEAR szHelpFile[];

/* This function calls the conference picker dialog.
 */
BOOL WINAPI EXPORT Amdb_ConfPicker( HWND hwnd, LPPICKER lppk )
{
   return( Adm_Dialog( hLibInst, hwnd, MAKEINTRESOURCE(IDDLG_CONFPICKER), ConfPicker, (LPARAM)lppk ) );
}

/* This function handles the ConfPicker dialog.
 */
BOOL EXPORT CALLBACK ConfPicker( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ConfPicker_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ConfPicker
 * dialog.
 */
LRESULT FASTCALL ConfPicker_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ConfPicker_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ConfPicker_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, ConfPicker_OnNotify );

      case WM_ADMHELP: {
         LPPICKER lppk;

         lppk = (LPPICKER)GetWindowLong( hwnd, DWL_USER );
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, lppk->wHelpID );
         break;
         }

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ConfPicker_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPPICKER lppk;
   HWND hwndTreeCtl;
   HIMAGELIST himl;
   HTREEITEM hItem;
   WORD wFlags;
   int count;

   /* Dereference and save the picker structure.
    */
   lppk = (LPPICKER)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Create the image list.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
   himl = Amdb_CreateFolderImageList();
   if( himl )
      TreeView_SetImageList( hwndTreeCtl, himl, 0 );

   /* Fill list with in-basket contents.
    */
   wFlags = FTYPE_ALL;
   if( lppk->wType & CPP_TOPICS )
      wFlags |= FTYPE_TOPICS;
   if( lppk->wType & CPP_CONFERENCES )
      wFlags |= FTYPE_CONFERENCES;
   Amdb_FillFolderTree( hwndTreeCtl, wFlags, TRUE );
   if( NULL != lppk->ptl )
      hItem = Amdb_FindTopicItem( hwndTreeCtl, lppk->ptl );
   else if( NULL != lppk->pcl )
      hItem = Amdb_FindFolderItem( hwndTreeCtl, lppk->pcl );
   else
      hItem = 0L;
   if( 0L != hItem )
      TreeView_SelectItem( hwndTreeCtl, hItem );

   /* Disable control if listbox empty. Always disable
    * OK button as the category will be selected.
    */
   count = TreeView_GetCount( hwndTreeCtl );
   EnableControl( hwnd, IDD_LIST, count > 0 );
   EnableControl( hwnd, IDOK, 0L != hItem );

   /* Set the window caption.
    */
   SetWindowText( hwnd, lppk->szCaption );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ConfPicker_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         LPVOID pFolder;
         LPPICKER lppk;
         HWND hwndTreeCtl;
         TV_ITEM tv;

         /* Find the selected item, assuming that there is one
          * of course.
          */
         hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
         tv.hItem = TreeView_GetSelection( hwndTreeCtl );
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         pFolder = (LPVOID)tv.lParam;

         /* Set the return values.
          */
         lppk = (LPPICKER)GetWindowLong( hwnd, DWL_USER );
         if( Amdb_IsTopicPtr( pFolder ) && (lppk->wType & CPP_TOPICS) )
            {
            lppk->pcl = Amdb_FolderFromTopic( pFolder );
            lppk->ptl = pFolder;
            }
         else if( Amdb_IsFolderPtr( pFolder ) && (lppk->wType & CPP_CONFERENCES) )
            {
            lppk->pcl = pFolder;
            lppk->ptl = Amdb_GetFirstTopic( pFolder );
            }
         else
            break;
         }

      case IDCANCEL: {
         HWND hwndTreeCtl;
         HIMAGELIST himl;

         /* Free image list associated with the tree control
          */
         hwndTreeCtl = GetDlgItem( hwnd, IDD_LIST );
         himl = TreeView_GetImageList( hwndTreeCtl, 0 );
         Amctl_ImageList_Destroy( himl );
         EndDialog( hwnd, id == IDOK );
         break;
         }
      }
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL ConfPicker_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case NM_DBLCLK:
         PostDlgCommand( hwnd, IDOK, BN_CLICKED );
         return( TRUE );

      case TVN_SELCHANGED: {
         LPNM_TREEVIEW lpnmtv;
         TV_ITEM tv;

         /* Display the rules for the selected
          * folder.
          */
         lpnmtv = (LPNM_TREEVIEW)lpNmHdr;
         tv.hItem = lpnmtv->itemNew.hItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
         EnableControl( hwnd, IDOK, !Amdb_IsCategoryPtr( (PCAT)tv.lParam ) );
         break;
         }

      case TVN_GETDISPINFO: {
         TV_DISPINFO FAR * lptvi;

         lptvi = (TV_DISPINFO FAR *)lpNmHdr;
         if( Amdb_IsCategoryPtr( (PCAT)lptvi->item.lParam ) )
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELCATEGORY;
               lptvi->item.iSelectedImage = IBML_SELCATEGORY;
               }
            else
               {
               lptvi->item.iImage = IBML_CATEGORY;
               lptvi->item.iSelectedImage = IBML_CATEGORY;
               }
            }
         else
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELFOLDER;
               lptvi->item.iSelectedImage = IBML_SELFOLDER;
               }
            else
               {
               lptvi->item.iImage = IBML_FOLDER;
               lptvi->item.iSelectedImage = IBML_FOLDER;
               }
            }
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function fills the treeview control with a list of
 * all categories and folders installed.
 */
void WINAPI EXPORT Amdb_FillFolderTree( HWND hwndTreeCtl, WORD wType, BOOL fShowFolderType )
{
   HTREEITEM htreeConfItem;
   HTREEITEM htreeCatItem;
   HTREEITEM htreeTopicItem;
   HTREEITEM hSelItem;
   LPVOID pFolder;
   PCAT pcatLast;
   PCL pclLast;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   /* Initialise.
    */
   pclLast = NULL;
   pcatLast = NULL;
   pcl = NULL;
   ptl = NULL;
   htreeConfItem = 0L;
   htreeCatItem = 0L;
   ASSERT( IsWindow( hwndTreeCtl ) );
   SetWindowRedraw( hwndTreeCtl, FALSE );

   /* If treeview already exists, get selected item.
    */
   hSelItem = TreeView_GetSelection( hwndTreeCtl );
   if( 0L == hSelItem )
      pFolder = NULL;
   else
      {
      TV_ITEM tv;

      tv.mask = TVIF_PARAM;
      tv.hItem = hSelItem;
      TreeView_GetItem( hwndTreeCtl, &tv );
      pFolder = (LPVOID)tv.lParam;
      }

   /* Delete and refill the control.
    */
   TreeView_DeleteAllItems( hwndTreeCtl );
   pcat = Amdb_GetFirstCategory();
   do {
      if( NULL != pcat )
         pcl = Amdb_GetFirstFolder( pcat );
      do {
         if( NULL != pcl )
            ptl = Amdb_GetFirstTopic( pcl );
         do {
            if( FTYPE_ALL == (wType & FTYPE_TYPEMASK) || ( ptl && Amdb_GetTopicType( ptl ) == (wType & FTYPE_TYPEMASK) ) )
               {
               if( NULL != pcat && pcat != pcatLast && ((FTYPE_CATEGORIES|FTYPE_CONFERENCES|FTYPE_TOPICS) & ( wType & FTYPE_GROUPMASK )) )
                  {
                  BOOL fExpanded;

                  fExpanded = Amdb_GetCategoryFlags( pcat, DF_EXPANDED ) == DF_EXPANDED;
                  htreeCatItem = InsertCategory( hwndTreeCtl, pcat, fExpanded );
                  if( pFolder == pcat )
                     hSelItem = htreeCatItem;
                  pcatLast = pcat;
                  }
               if( NULL != pcl && pcl != pclLast && ((FTYPE_CONFERENCES|FTYPE_TOPICS) & ( wType & FTYPE_GROUPMASK ))  )
                  {
                  BOOL fExpanded;
                  BOOL fChildren;

                  ASSERT( 0L != htreeCatItem );
                  fExpanded = Amdb_GetFolderFlags( pcl, CF_EXPANDED ) == CF_EXPANDED;
                  fChildren = FTYPE_TOPICS & ( wType & FTYPE_GROUPMASK );
                  htreeConfItem = InsertFolder( hwndTreeCtl, htreeCatItem, pcl, fExpanded && fChildren, TVI_LAST, fChildren );
                  if( pFolder == pcl )
                     hSelItem = htreeConfItem;
                  pclLast = pcl;
                  }
               if( NULL != ptl && ( FTYPE_TOPICS & ( wType & FTYPE_GROUPMASK ) ) )
                  {
                  ASSERT( 0L != htreeCatItem );
                  ASSERT( 0L != htreeConfItem );
                  htreeTopicItem = InsertTopic( hwndTreeCtl, htreeConfItem, ptl, TVI_LAST, fShowFolderType );
                  if( pFolder == ptl )
                     hSelItem = htreeTopicItem;
                  }
               }
            if( NULL != ptl )
               ptl = Amdb_GetNextTopic( ptl );
            }
         while( ptl );
         if( NULL != pcl )
            pcl = Amdb_GetNextFolder( pcl );
         }
      while( pcl );
      if( NULL != pcat )
         pcat = Amdb_GetNextCategory( pcat );
      }
   while( pcat );
   SetWindowRedraw( hwndTreeCtl, TRUE );
   InvalidateRect( hwndTreeCtl, NULL, TRUE );

   /* Select the first item.
    */
   if( 0L == hSelItem )
      hSelItem = TreeView_GetRoot( hwndTreeCtl );
   if( 0L != hSelItem )
      TreeView_SelectItem( hwndTreeCtl, hSelItem );
}

/* This function inserts a category into the in-basket.
 */
HTREEITEM FASTCALL InsertCategory( HWND hwndTreeCtl, PCAT pcat, BOOL fExpanded )
{
   TV_INSERTSTRUCT tvins;

   tvins.hParent = 0;
   tvins.hInsertAfter = TVI_LAST;
   tvins.item.pszText = (LPSTR)Amdb_GetCategoryName( pcat );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_STATE;
   tvins.item.state = fExpanded ? TVIS_EXPANDED : 0;
   tvins.item.iImage = I_IMAGECALLBACK;
   tvins.item.iSelectedImage = I_IMAGECALLBACK;
   tvins.item.lParam = (LPARAM)pcat;
   tvins.item.cChildren = 0;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function inserts a folder into the in-basket.
 */
HTREEITEM FASTCALL InsertFolder( HWND hwndTreeCtl, HTREEITEM htreeConfItem, PCL pcl, BOOL fExpanded, HTREEITEM htreeInsertAfter, BOOL fChildren )
{
   TV_INSERTSTRUCT tvins;
   FOLDERINFO foldinfo;

   Amdb_GetFolderInfo( pcl, &foldinfo );
   tvins.hInsertAfter = htreeInsertAfter;
   tvins.hParent = htreeConfItem;
   tvins.item.pszText = (LPSTR)Amdb_GetFolderName( pcl );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_STATE;
   tvins.item.state = fExpanded ? TVIS_EXPANDED : 0;
   tvins.item.iImage = I_IMAGECALLBACK;
   tvins.item.iSelectedImage = I_IMAGECALLBACK;
   tvins.item.lParam = (LPARAM)pcl;
   tvins.item.cChildren = fChildren ? foldinfo.cTopics : 0;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function inserts a topic into the in-basket.
 */
HTREEITEM FASTCALL InsertTopic( HWND hwndTreeCtl, HTREEITEM htreeTopicItem, PTL ptl, HTREEITEM htreeInsertAfter, BOOL fShowTopicType )
{
   TV_INSERTSTRUCT tvins;

   tvins.hInsertAfter = htreeInsertAfter;
   tvins.hParent = htreeTopicItem;
   tvins.item.pszText = (LPSTR)Amdb_GetTopicName( ptl );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE;
   tvins.item.state = 0;
   tvins.item.iImage = fShowTopicType ? GetTopicTypeImage( ptl ) : I_NOIMAGE;
   tvins.item.iSelectedImage = tvins.item.iImage;
   tvins.item.lParam = (LPARAM)ptl;
   tvins.item.cChildren = 0;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function creates the imagelist of treeview icons.
 */
HIMAGELIST WINAPI EXPORT Amdb_CreateFolderImageList( void )
{
   HIMAGELIST hIL;
   HBITMAP hBitmap;
   BOOL fOldLogo;

   /* Create the Image List
    */
   hIL = Amctl_ImageList_Create( 16, 15, ILC_MASK|ILC_COLOR, IBML_MAX, 0 );
   if( !hIL )
      return( NULL );

   /* Add Each bitmap to the ImageList.
    *
    * ImageList_AddMasked will add the bitmap, and treat every
    * pixel that is (in this example) green as a "transparent" pixel,
    * since we specified TRUE for fMask in the above call to
    * ImageList_Create.
    */
   fOldLogo = Amuser_GetPPInt( "Settings", "UseOldBitmaps", FALSE );
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_DATABASE) );
   if( hBitmap )
      {
      Amctl_ImageList_AddMasked( hIL, hBitmap, RGB(255,0,255) );
      DeleteObject( hBitmap );
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_DATABASESELECTED) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_FOLDER) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_FOLDERSELECTED) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_NEWSFOLDER) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_MAILFOLDER) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_UNTYPEDFOLDER) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(fOldLogo ? IDB_CIXFOLDER_OLD : IDB_CIXFOLDER) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   hBitmap = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_LOCALTOPIC) );
   if (hBitmap)
      {
      Amctl_ImageList_AddMasked(hIL, hBitmap, RGB(255,0,255) );
      DeleteObject (hBitmap);
      }
   if( Amctl_ImageList_GetImageCount( hIL ) < IBML_MAX )
      {
      Amctl_ImageList_Destroy( hIL );
      hIL = NULL;
      }
   return( hIL );
}

/* This function returns the HTREEITEM for the specified folder.
 */
HTREEITEM WINAPI Amdb_FindFolderItemInCategory( HWND hwndTreeCtl, PCAT pcat, PCL pcl )
{
   HTREEITEM hCatItem;

   hCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat );
   return( Amdb_FindFolderItemInBranch( hwndTreeCtl, hCatItem, pcl ) );
}

/* This function returns the HTREEITEM for the specified database.
 */
HTREEITEM WINAPI Amdb_FindCategoryItem( HWND hwndTreeCtl, PCAT pcat )
{
   HTREEITEM hCatItem;

   hCatItem = TreeView_GetRoot( hwndTreeCtl );
   while( hCatItem )
      {
      TV_ITEM tv;

      tv.mask = TVIF_PARAM;
      tv.hItem = hCatItem;
      TreeView_GetItem( hwndTreeCtl, &tv );
      if( (PCAT)tv.lParam == pcat )
         return( hCatItem );
      hCatItem = TreeView_GetNextSibling( hwndTreeCtl, hCatItem );
      }
   return( 0 );
}

/* This function returns the HTREEITEM for the specified folder.
 */
HTREEITEM WINAPI Amdb_FindFolderItem( HWND hwndTreeCtl, PCL pcl )
{
   HTREEITEM hCatItem;

   hCatItem = Amdb_FindCategoryItem( hwndTreeCtl, Amdb_CategoryFromFolder( pcl ) );
   if( hCatItem )
      {
      HTREEITEM hConfItem;

      hConfItem = TreeView_GetChild( hwndTreeCtl, hCatItem );
      while( hConfItem )
         {
         TV_ITEM tv;

         tv.mask = TVIF_PARAM;
         tv.hItem = hConfItem;
         TreeView_GetItem( hwndTreeCtl, &tv );
         if( (PCL)tv.lParam == pcl )
            return( hConfItem );
         hConfItem = TreeView_GetNextSibling( hwndTreeCtl, hConfItem );
         }
      }
   return( 0 );
}

/* This function returns the HTREEITEM for the specified topic.
 */
HTREEITEM WINAPI Amdb_FindTopicItemInFolder( HWND hwndTreeCtl, PCL pcl, PTL ptl )
{
   HTREEITEM hConfItem;

   hConfItem = Amdb_FindFolderItem( hwndTreeCtl, pcl );
   return( Amdb_FindTopicItemInBranch( hwndTreeCtl, hConfItem, ptl ) );
}

/* This function returns the HTREEITEM for the specified topic.
 */
HTREEITEM WINAPI Amdb_FindTopicItem( HWND hwndTreeCtl, PTL ptl )
{
   HTREEITEM hConfItem;

   hConfItem = Amdb_FindFolderItem( hwndTreeCtl, Amdb_FolderFromTopic( ptl ) );
   return( Amdb_FindTopicItemInBranch( hwndTreeCtl, hConfItem, ptl ) );
}

/* This function returns the HTREEITEM for the specified topic.
 */
HTREEITEM FASTCALL Amdb_FindTopicItemInBranch( HWND hwndTreeCtl, HTREEITEM hConfItem, PTL ptl )
{
   if( hConfItem )
      {
      HTREEITEM hTopicItem;

      hTopicItem = TreeView_GetChild( hwndTreeCtl, hConfItem );
      while( hTopicItem )
         {
         TV_ITEM tv;

         tv.mask = TVIF_PARAM;
         tv.hItem = hTopicItem;
         TreeView_GetItem( hwndTreeCtl, &tv );
         if( (PTL)tv.lParam == ptl )
            return( hTopicItem );
         hTopicItem = TreeView_GetNextSibling( hwndTreeCtl, hTopicItem );
         }
      }
   return( 0 );
}

/* This function returns the HTREEITEM for the specified folder.
 */
HTREEITEM FASTCALL Amdb_FindFolderItemInBranch( HWND hwndTreeCtl, HTREEITEM hCatItem, PCL pcl )
{
   if( hCatItem )
      {
      HTREEITEM hConfItem;

      hConfItem = TreeView_GetChild( hwndTreeCtl, hCatItem );
      while( hConfItem )
         {
         TV_ITEM tv;

         tv.mask = TVIF_PARAM;
         tv.hItem = hConfItem;
         TreeView_GetItem( hwndTreeCtl, &tv );
         if( (PCL)tv.lParam == pcl )
            return( hConfItem );
         hConfItem = TreeView_GetNextSibling( hwndTreeCtl, hConfItem );
         }
      }
   return( 0 );
}

/* This function returns the index of an image for the specified
 * folder type.
 */
int FASTCALL GetTopicTypeImage( PTL ptl )
{
   switch( Amdb_GetTopicType( ptl ) )
      {
      case FTYPE_NEWS:     return( IBML_NEWS );
      case FTYPE_MAIL:     return( IBML_MAIL );
      case FTYPE_UNTYPED:  return( IBML_UNTYPED );
      case FTYPE_CIX:      return( IBML_CIX );
      case FTYPE_LOCAL:    return( IBML_LOCAL );
      }
   return( -1 );
}
