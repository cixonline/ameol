/* COMMAND.C - Command table maintenance
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
#include "command.h"
#include <string.h>
#include "amlib.h"

typedef struct tagCMD {
   struct tagCMD FAR * lpCmdNext;      /* Next item */
   struct tagCMD FAR * lpCmdPrev;      /* Previous item */
   CMDREC cmd;                         /* Command record */
} CMD, FAR * HCMD;

typedef struct tagCATEGORIES {
   struct tagCATEGORIES * lpcNext;  /* Next category */
   WORD wCategory;                  /* Category number */
   char * pCategory;                /* Category description */
} CATEGORIES, FAR * PCATEGORIES;

HCMD lpCmdRoot = NULL;              /* Root of command tree */
PCATEGORIES lpcFirst = NULL;        /* First category in list */

void FASTCALL SaveCustomButtonBitmap( CMDREC FAR * );
BOOL FASTCALL LoadCustomButtonBitmap( CMDREC FAR * );
HBITMAP FASTCALL GetExternalAppBitmap( CMDREC FAR * );
HBITMAP FASTCALL GetAddonToolbarBitmap( CMDREC FAR * );

/* This function creates a category
 */
BOOL FASTCALL CTree_AddCategory( WORD wCategory, WORD nPosition, char * pName )
{
   PCATEGORIES lpcNew;

   INITIALISE_PTR(lpcNew);
   if( fNewMemory( &lpcNew, sizeof(CATEGORIES) ) )
      {
      PCATEGORIES FAR * lpcPre;

      lpcPre = &lpcFirst;
      while( *lpcPre && nPosition-- )
         lpcPre = &(*lpcPre)->lpcNext;
      lpcNew->lpcNext = *lpcPre;
      *lpcPre = lpcNew;
      lpcNew->wCategory = wCategory;
      lpcNew->pCategory = _strdup(pName);
      }
   return( NULL != lpcNew );
}

/* This function returns the name and ID of the specified category.
 */
BOOL FASTCALL CTree_GetCategory( WORD nPosition, LPCATEGORYITEM lpci )
{
   PCATEGORIES lpc;

   lpc = lpcFirst;
   while( lpc && nPosition-- )
      lpc = lpc->lpcNext;
   if( lpc )
      {
      lpci->pCategory = lpc->pCategory;
      lpci->wCategory = lpc->wCategory;
      return( TRUE );
      }
   return( FALSE );
}

/* This command returns whether or not the specified category
 * has been inserted.
 */
BOOL FASTCALL CTree_FindCategory( WORD wCategory )
{
   PCATEGORIES lpc;

   lpc = lpcFirst;
   while( lpc && lpc->wCategory != wCategory )
      lpc = lpc->lpcNext;
   return( NULL != lpc );
}

/* This function deletes all categories.
 */
void FASTCALL CTree_DeleteAllCategories( void )
{
   PCATEGORIES lpcNext;
   PCATEGORIES lpc;

   for( lpc = lpcFirst; lpc; lpc = lpcNext )
      {
      lpcNext = lpc->lpcNext;
      free( lpc->pCategory );
      FreeMemory( &lpc );
      }
}

/* This function deletes all commands from the table
 */
void FASTCALL CTree_DeleteAllCommands( void )
{
   HCMD lpCmdNext;
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmdNext )
      {
      lpCmdNext = lpCmd->lpCmdNext;
      if( lpCmd->cmd.fCustomBitmap )
         {
         if( NULL != lpCmd->cmd.btnBmp.hbmpSmall )
            DeleteBitmap( lpCmd->cmd.btnBmp.hbmpSmall );
         if( NULL != lpCmd->cmd.btnBmp.hbmpLarge )
            DeleteBitmap( lpCmd->cmd.btnBmp.hbmpLarge );
         }
      if( 0 != HIWORD( lpCmd->cmd.lpszString ) )
         free( (char *)lpCmd->cmd.lpszString );
      if( 0 != HIWORD( lpCmd->cmd.lpszCommand ) )
         free( (char *)lpCmd->cmd.lpszCommand );
      if( 0 != HIWORD( lpCmd->cmd.lpszTooltipString ) )
         free( (char *)lpCmd->cmd.lpszTooltipString );
      FreeMemory( &lpCmd );
      }
   lpCmdRoot = NULL;
}

/* This function deletes the specified command from the
 * table.
 */
BOOL FASTCALL CTree_DeleteCommand( UINT iCommand )
{
   HCMD FAR * lppceParent;

   lppceParent = &lpCmdRoot;
   while( *lppceParent && (*lppceParent)->cmd.iCommand != iCommand )
      lppceParent = &(*lppceParent)->lpCmdNext;
   if( *lppceParent )
      {
      HCMD lpCmdNext;
      HCMD lpCmd;

      /* Delete the command.
       */
      lpCmd = *lppceParent;
      lpCmdNext = lpCmd->lpCmdNext;
      if( NULL != hwndToolBar )
         {
         int index;

         /* If this command appears on the toolbar, remove it from
          * the toolbar.
          */
         index = (int)SendMessage( hwndToolBar, TB_COMMANDTOINDEX, iCommand, 0L );
         if( -1 != index )
            {
            SendMessage( hwndToolBar, TB_DELETEBUTTON, index, 0L );
            UpdateWindow( hwndToolBar );
            }
         }
      if( lpCmd->cmd.fCustomBitmap )
         {
         if( NULL != lpCmd->cmd.btnBmp.hbmpSmall )
            DeleteBitmap( lpCmd->cmd.btnBmp.hbmpSmall );
         if( NULL != lpCmd->cmd.btnBmp.hbmpLarge )
            DeleteBitmap( lpCmd->cmd.btnBmp.hbmpLarge );
         }
      if( 0 != HIWORD( lpCmd->cmd.lpszString ) )
         free( (char *)lpCmd->cmd.lpszString );
      if( 0 != HIWORD( lpCmd->cmd.lpszCommand ) )
         free( (char *)lpCmd->cmd.lpszCommand );
      if( 0 != HIWORD( lpCmd->cmd.lpszTooltipString ) )
         free( (char *)lpCmd->cmd.lpszTooltipString );

      FreeMemory( &lpCmd );
      *lppceParent = lpCmdNext;
      }
   return( *lppceParent ? TRUE : FALSE );
}

/* This function edits the bitmap button for the
 * specified command.
 */
void FASTCALL EditButtonBitmap( HWND hwnd, UINT iCommand )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      if( lpCmd->cmd.iCommand == iCommand )
         {
         /* Edit the command. If it gets changed, save the changes
          * to the szCustomBitmaps section of the config file.
          */
         if( CmdButtonBitmap( hwnd, &lpCmd->cmd.btnBmp ) )
            {
            SaveCustomButtonBitmap( &lpCmd->cmd );
            if( !lpCmd->cmd.btnBmp.fIsValid )
               {
               if( lpCmd->cmd.iCommand >= IDM_TOOLFIRST && lpCmd->cmd.iCommand <= IDM_TOOLLAST )
                  GetAddonToolbarBitmap( &lpCmd->cmd );
               else if( lpCmd->cmd.iCommand >= IDM_EXTAPPFIRST && lpCmd->cmd.iCommand <= IDM_EXTAPPLAST )
                  GetExternalAppBitmap( &lpCmd->cmd );
               SendMessage( hwndToolBar, TB_CHANGEBITMAP, iCommand, 0L );
               break;
               }
            GetButtonBitmapEntry( &lpCmd->cmd.btnBmp );
            SendMessage( hwndToolBar, TB_CHANGEBITMAP, iCommand, 0L );
            }
         break;
         }
}

/* This function returns the toolbar bitmap associated with the
 * specified command.
 */
HBITMAP FASTCALL CTree_GetCommandBitmap( UINT iCommand )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      if( lpCmd->cmd.iCommand == iCommand )
         {
         /* Check the cache first.
          */
         if( fLargeButtons && lpCmd->cmd.btnBmp.hbmpLarge )
            return( lpCmd->cmd.btnBmp.hbmpLarge );
         if( !fLargeButtons && lpCmd->cmd.btnBmp.hbmpSmall )
            return( lpCmd->cmd.btnBmp.hbmpSmall );

         /* Okay, it isn't cached, so see if there is an entry
          * in the custom bitmaps section of the config for this
          * command.
          */
         if( LoadCustomButtonBitmap( &lpCmd->cmd ) )
            return( GetButtonBitmapEntry( &lpCmd->cmd.btnBmp ) );

         /* Use defaults.
          */
         if( lpCmd->cmd.iCommand >= IDM_TOOLFIRST && lpCmd->cmd.iCommand <= IDM_TOOLLAST )
            return( GetAddonToolbarBitmap( &lpCmd->cmd ) );
         if( lpCmd->cmd.iCommand >= IDM_EXTAPPFIRST && lpCmd->cmd.iCommand <= IDM_EXTAPPLAST )
            return( GetExternalAppBitmap( &lpCmd->cmd ) );
         return( GetButtonBitmapEntry( &lpCmd->cmd.btnBmp ) );
         }
   return( NULL );
}

/* This function traverses the command tree, locating the
 * specified command.
 */
BOOL FASTCALL CTree_GetCommand( CMDREC FAR * lpmsi )
{
   HCMD FAR * lppceParent;

   lppceParent = &lpCmdRoot;
   while( *lppceParent && (*lppceParent)->cmd.iCommand != lpmsi->iCommand )
      lppceParent = &(*lppceParent)->lpCmdNext;
   if( *lppceParent )
      *lpmsi = (*lppceParent)->cmd;
   return( *lppceParent ? TRUE : FALSE );
}

/* This function updates the command specified.
 */
BOOL FASTCALL CTree_PutCommand( CMDREC FAR * lpmsi )
{
   HCMD FAR * lppceParent;

   lppceParent = &lpCmdRoot;
   while( *lppceParent && (*lppceParent)->cmd.iCommand != lpmsi->iCommand )
      lppceParent = &(*lppceParent)->lpCmdNext;
   if( *lppceParent )
      {
      HCMD lpCmd;

      lpCmd = *lppceParent;
      if( 0 != HIWORD( lpCmd->cmd.lpszString ) )
         free( (char *)lpCmd->cmd.lpszString );
      if( 0 != HIWORD( lpCmd->cmd.lpszCommand ) )
         free( (char *)lpCmd->cmd.lpszCommand );
      if( 0 != HIWORD( lpCmd->cmd.lpszTooltipString ) )
         free( (char *)lpCmd->cmd.lpszTooltipString );
      lpCmd->cmd = *lpmsi;
      if( 0 != HIWORD( lpmsi->lpszString ) )
         lpCmd->cmd.lpszString = _strdup( lpmsi->lpszString );
      if( 0 != HIWORD( lpmsi->lpszCommand ) )
         lpCmd->cmd.lpszCommand = _strdup( lpmsi->lpszCommand );
      if( 0 != HIWORD( lpmsi->lpszTooltipString ) )
         lpCmd->cmd.lpszTooltipString = _strdup( lpmsi->lpszTooltipString );
      return( TRUE );
      }
   return( CTree_InsertCommand( lpmsi ) );
}

/* This function traverses the command tree, locating the
 * specified command name.
 */
BOOL FASTCALL CTree_FindCommandName( char * lpszCommand, CMDREC FAR * lpmsi )
{
   HCMD FAR * lppceParent;

   lppceParent = &lpCmdRoot;
   while( *lppceParent )
      {
      if( _strcmpi( (*lppceParent)->cmd.lpszCommand, lpszCommand ) == 0 )
         break;
      lppceParent = &(*lppceParent)->lpCmdNext;
      }
   if( *lppceParent )
      *lpmsi = (*lppceParent)->cmd;
   return( *lppceParent ? TRUE : FALSE );
}

/* This function retrieves the HCMD for the specified command
 * name.
 */
HCMD FASTCALL CTree_CommandToHandle( char * lpszCommand )
{
   HCMD FAR * lppceParent;

   lppceParent = &lpCmdRoot;
   while( *lppceParent )
      {
      if( _strcmpi( (*lppceParent)->cmd.lpszCommand, lpszCommand ) == 0 )
         return( *lppceParent );
      lppceParent = &(*lppceParent)->lpCmdNext;
      }
   return( NULL );
}

/* This function returns the command name given the command
 * handle (HCMD).
 */
LPCSTR FASTCALL CTree_GetCommandName( HCMD lpCmd )
{
   return( (LPCSTR)lpCmd->cmd.lpszCommand );
}

/* This function inserts the specified command into the
 * tree.
 */
BOOL FASTCALL CTree_InsertCommand( CMDREC FAR * lpmsi )
{
   HCMD lpCmdNew;
   HCMD FAR * lppceParent;

   INITIALISE_PTR(lpCmdNew);
   lppceParent = &lpCmdRoot;
   while( *lppceParent && (*lppceParent)->cmd.iCommand != lpmsi->iCommand )
      lppceParent = &(*lppceParent)->lpCmdNext;
   if( fNewMemory( &lpCmdNew, sizeof(CMD) ) )
      {
      lpCmdNew->lpCmdNext = NULL;
      lpCmdNew->lpCmdPrev = *lppceParent;
      lpCmdNew->cmd = *lpmsi;
      if( 0 != HIWORD( lpmsi->lpszString ) )
         lpCmdNew->cmd.lpszString = _strdup( lpmsi->lpszString );
      if( 0 != HIWORD( lpmsi->lpszCommand ) )
         lpCmdNew->cmd.lpszCommand = _strdup( lpmsi->lpszCommand );
      if( 0 != HIWORD( lpmsi->lpszTooltipString ) )
         lpCmdNew->cmd.lpszTooltipString = _strdup( lpmsi->lpszTooltipString );
      *lppceParent = lpCmdNew;
      }
   return( lpCmdNew ? TRUE : FALSE );
}

/* This command loops through the tree, filling the specified
 * command structure.
 */
HCMD FASTCALL CTree_EnumTree( HCMD lpCmdn, CMDREC FAR * lpmsi )
{
   if( NULL == lpCmdn )
      lpCmdn = lpCmdRoot;
   else
      lpCmdn = lpCmdn->lpCmdNext;
   if( lpCmdn )
      *lpmsi = lpCmdn->cmd;
   return( lpCmdn );
}

/* This function traverses the command tree, locating the
 * specified keystroke.
 */
BOOL FASTCALL CTree_FindKey( CMDREC FAR * lpmsi )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      if( lpCmd->cmd.wKey == lpmsi->wKey )
         {
         *lpmsi = lpCmd->cmd;
         return( TRUE );
         }
   return( FALSE );
}

/* This function traverses the command tree, locating the
 * specified keystroke.
 */
BOOL FASTCALL CTree_FindNewKey( CMDREC FAR * lpmsi )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      if( lpCmd->cmd.wNewKey == lpmsi->wNewKey )
         {
         *lpmsi = lpCmd->cmd;
         return( TRUE );
         }
   return( FALSE );
}

/* This function scans the tree, setting wDefKey to wKey
 * for each command.
 */
void FASTCALL CTree_SetDefaultKey( void )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      lpCmd->cmd.wKey = lpCmd->cmd.wDefKey;
}

/* Given a command, this function changes the key assigned
 * to that command.
 */
BOOL FASTCALL CTree_ChangeKey( UINT iCommand, WORD wKeyCode )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      if( lpCmd->cmd.iCommand == iCommand )
         {
         if( ( wKeyCode != lpCmd->cmd.wDefKey ) && wKeyCode < 0x80 )
            lpCmd->cmd.iActiveMode = ( WIN_OUTBASK|WIN_THREAD|WIN_MESSAGE|WIN_INFOBAR|WIN_RESUMES|WIN_SCHEDULE );
         lpCmd->cmd.wKey = wKeyCode;
         return( TRUE );
         }
   return( FALSE );
}

/* This function scans the tree, saving wKey in wNewKey.
 * for each command.
 */
void FASTCALL CTree_SaveKey( void )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      lpCmd->cmd.wNewKey = lpCmd->cmd.wKey;
}

/* This function scans the tree, setting wNewKey to wDefKey
 * for each command.
 */
void FASTCALL CTree_ResetKey( void )
{
   HCMD lpCmd;

   for( lpCmd = lpCmdRoot; lpCmd; lpCmd = lpCmd->lpCmdNext )
      lpCmd->cmd.wNewKey = lpCmd->cmd.wDefKey;
}

/* This function returns the record for the specified item.
 */
void FASTCALL CTree_GetItem( HCMD hCmd, CMDREC FAR * lpmsi )
{
   *lpmsi = hCmd->cmd;
}

/* This function sets the record for the specified item.
 */
void FASTCALL CTree_SetItem( HCMD hCmd, CMDREC FAR * lpmsi )
{
   hCmd->cmd = *lpmsi;
}

/* This function returns the key code associated with the
 * specified command.
 */
BOOL FASTCALL CTree_GetCommandKey( char * pszCommand, WORD * pwKey )
{
   HCMD hCmd;

   if( NULL != ( hCmd = CTree_CommandToHandle( pszCommand ) ) )
      if( hCmd->cmd.wKey != 0 )
         {
         *pwKey = hCmd->cmd.wKey;
         return( TRUE );
         }
   return( FALSE );
}
