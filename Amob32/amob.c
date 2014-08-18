/* AMOB.C - Handles the Ameol out-basket
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <string.h>
#include "amlib.h"
#include "amuser.h"
#include "amevent.h"
#include "amob.h"
#include "common.bld"

#define  THIS_FILE   __FILE__

#define  MIN_OB_ID         0x000A
#define  MAX_OB_ID         0xFF0A

HINSTANCE hLibInst;                       /* Library instance handle */

char szOutBasketDataName[] = "outbaskt.txt";
char szOutBasketTmpName[] = "outbaskt.tmp";
char szOutBasketIndexName[] = "outbaskt.idx";

char szOutBasketData[ _MAX_DIR ];         /* Full pathname of outbasket data file. */
char szOutBasketTmp[ _MAX_DIR ];          /* Full pathname of outbasket temp file. */
char szOutBasketIndex[ _MAX_DIR ];        /* Full pathname of outbasket index file */

typedef struct tagOBCLASSLIST {
   struct tagOBCLASSLIST FAR * lpcllNext; /* Pointer to next object class */
   HINSTANCE hObLib;                      /* Handle of class handler, if there is one */
   OBCLSID clsid;                         /* Class type */
   DWORD dwType;                          /* Type of object */
   LPFOBPROC lfpObProc;                   /* Pointer to class handler function */
} OBCLASSLIST;

#define  OH_START_SIG      0x5343
#define  OH_END_SIG        0x5243

#define  DOH_START_SIG     0x4A4F
#define  DOH_END_SIG       0x4D52

#pragma pack(1)
typedef struct tagDATAOBJECTHDR {
   WORD wStartSig;                        /* Header start signature */
   WORD wCRC;                             /* Object checksum */
   DWORD dwSize;                          /* Object size */
   WORD wEndSig;                          /* Header end signature */
} DATAOBJECTHDR;
#pragma pack()

typedef OBCLASSLIST FAR * LPOBCLASSLIST;

typedef BOOL (EXPORT WINAPI * INSTANTIATEPROC)( OBCLSID );

static LPOBCLASSLIST lpcllFirst = NULL;      /* Pointer to first registered class */
static LPOBCLASSLIST lpcllLast = NULL;       /* Pointer to last registered class */

int cbOutBasketObjects = 0;                  /* Number of objects in the out-basket */
int nNextObId = MIN_OB_ID;                   /* Object ID code */

/* This header structure precedes all object structures and organises
 * them as a linked list.
 */
typedef struct tagOB {
   struct tagOB FAR * lpobNext;     /* Pointer to next out-basket item */
   struct tagOB FAR * lpobPrev;     /* Pointer to next out-basket item */
   OBHEADER obHdr;                  /* Outbasket header */
   LPOBCLASSLIST lpobcl;            /* Pointer to class for this object */
   LPVOID lpObData;                 /* Class specific data handle */
   HWND hwndEdit;                   /* Handle of edit window */
} OB;

typedef OB FAR * LPOB;

static LPOB lpobFirst = NULL;                /* First out-basket entry */
static LPOB lpobLast = NULL;                 /* Last out-basket entry */
static BOOL fOutBasketLoaded = FALSE;        /* TRUE if out-basket loaded */
static BOOL fOutBasketModified = FALSE;      /* TRUE if out-basket modified */
static BOOL fCompacting = FALSE;             /* TRUE if we're compacting data file */

static HNDFILE fhOb = HNDFILE_ERROR;         /* Outbasket database file handle */
static HNDFILE fhObCmp = HNDFILE_ERROR;      /* Outbasket temporary file handle */

static char szObClsSection[] = "Object Classes";
static char strError1[] = "Handler for object class %lu failed to register class";
static char strError2[] = "Handler for object class %lu is missing InstantiateClass entry point";
static char strError3[] = "Handler for object class %lu not installed";

BOOL FASTCALL NewOb( OBCLSID, LPVOID );
BOOL FASTCALL DeleteOb( OBCLSID, LPVOID );
LPVOID FASTCALL FindOb( OBCLSID, LPVOID );
BOOL FASTCALL CommitOb( OBCLSID, LPVOID );
DWORD FASTCALL GetType( LPOB lpob );
BOOL FASTCALL InvokeNew( OBCLSID, LPVOID );
LPVOID FASTCALL InvokeCopy( OBCLSID, LPVOID );
LPVOID FASTCALL InvokeLoad( LPOB, HNDFILE );
WORD FASTCALL InvokeGetObjectID( OBCLSID, LPVOID );
BOOL FASTCALL InvokeSave( LPOB, HNDFILE, LPVOID );
BOOL FASTCALL InvokeDelete( OBCLSID, LPVOID );
BOOL FASTCALL InvokeRemove( LPOB );
BOOL FASTCALL InvokeEdit( LPOB );
BOOL FASTCALL InvokeFind( OBCLSID, LPVOID, LPVOID );
BOOL FASTCALL InvokeExec( OBCLSID, LPVOID );
BOOL FASTCALL InvokeDescribe( LPOB, LPSTR );
BOOL FASTCALL QueryIsObjectPersistent( OBCLSID );
BOOL FASTCALL LoadOutBasket( void );
BOOL FASTCALL SaveOutBasket( BOOL );
BOOL FASTCALL InstallClassHandler( LPOB );
void FASTCALL UninstallClasses( void );
LPOBCLASSLIST FindObjectClass( OBCLSID );
LRESULT FASTCALL InvokeHandler( LPOB, int, HNDFILE, LPVOID, LPSTR );

void WINAPI EXPORT Amob_FreeMemory( void );

/* This is the Ameol DLL entry point. Two separate functions are
 * provided: one for Windows 32 and one for 16-bit Windows.
 */
BOOL WINAPI EXPORT DllMain( HANDLE hInstance, DWORD fdwReason, LPVOID lpvReserved )
{
   switch( fdwReason )
   {
   case DLL_PROCESS_ATTACH:
      hLibInst = hInstance;
      return( 1 );

   case DLL_PROCESS_DETACH:
      UninstallClasses();
        Amob_FreeMemory();
      return( 1 );

   default:
      return( 1 );
   }
}

/* This function returns the version number of Amctrls.
 */
DWORD WINAPI EXPORT GetBuildVersion( void )
{
   return( MAKEVERSION( PRODUCT_MAX_VER, PRODUCT_MIN_VER, PRODUCT_BUILD ) );
}

/* This function dereferences the record pointer pointed by lprp. If the
 * memory handle is NULL, it allocates space for the object in memory, loads
 * the data from the out-basket database and then plugs the memory block.
 */
HPSTR WINAPI EXPORT Amob_DerefObject( RECPTR FAR * lprp )
{
   HPSTR hpStr;

   if( NULL == ( hpStr = lprp->hpStr ) )
      {
      /* This piece of trickery should be frowned upon, but what
       * the hell. We set hpStr to point to the field in the record
       * pointer from where it was derived. If we get here, this
       * field will be NULL and hpStr will point to a NULL byte. So
       * if we fail to dereference the string, we'll return a valid
       * but empty, string. Snork!
       */
      hpStr = (HPSTR)&lprp->hpStr;
      if( lprp->dwSize > 0 )
         {
         if( HNDFILE_ERROR == fhOb )
            fhOb = Amfile_Open( szOutBasketData, AOF_READWRITE );
         if( HNDFILE_ERROR != fhOb )
            if( -1 != Amfile_SetPosition( fhOb, lprp->dwOffset, ASEEK_BEGINNING ) )
               if( fNewMemory32( &lprp->hpStr, lprp->dwSize ) )
                  if( (DWORD)Amfile_Read32( fhOb, lprp->hpStr, lprp->dwSize ) == lprp->dwSize )
                     hpStr = lprp->hpStr;
                  else
                     FreeMemory32( &lprp->hpStr );
         }
      }
   return( hpStr );
}

/* This function saves a referenced object.
 */
BOOL WINAPI EXPORT Amob_SaveRefObject( RECPTR FAR * lprp )
{
   if( NULL == lprp->hpStr )
      {
      if( !fCompacting )
         return( FALSE );
      Amob_DerefObject( lprp );
      }
   if( fCompacting )
      {
      ASSERT( HNDFILE_ERROR != fhObCmp );
      lprp->dwOffset = Amfile_SetPosition( fhObCmp, 0, ASEEK_END );
      if( (DWORD)Amfile_Write32( fhObCmp, lprp->hpStr, lprp->dwSize ) != lprp->dwSize )
         return( FALSE );
      }
   else
      {
      if( HNDFILE_ERROR == fhOb )
         {
         fhOb = Amfile_Open( szOutBasketData, AOF_READWRITE );
         if( HNDFILE_ERROR == fhOb )
            return( FALSE );
         }
      if( -1 == Amfile_SetPosition( fhOb, lprp->dwOffset, ASEEK_BEGINNING ) )
         return( FALSE );
      if( (DWORD)Amfile_Write32( fhOb, lprp->hpStr, lprp->dwSize ) != lprp->dwSize )
         return( FALSE );
      }
   return( TRUE );
}

/* This function writes to the outbasket data file.
 */
BOOL WINAPI EXPORT Amob_SaveDataObject( HNDFILE fh, LPSTR lpData, UINT cbData )
{
   DATAOBJECTHDR doh;

   /* First write a header around the data.
    */
   doh.wStartSig = DOH_START_SIG;
   doh.dwSize = cbData;
   doh.wCRC = Amuser_ComputeCRC( lpData, cbData );
   doh.wEndSig = DOH_END_SIG;
   if( sizeof(DATAOBJECTHDR) != Amfile_Write( fh, (LPCSTR)&doh, sizeof(DATAOBJECTHDR) ) )
      return( FALSE );

   /* Finally write the data itself.
    */
   return( Amfile_Write( fh, lpData, cbData ) == cbData );
}

/* This function reads from the outbasket data file.
 */
BOOL WINAPI EXPORT Amob_LoadDataObject( HNDFILE fh, LPSTR lpData, UINT cbData )
{
   DATAOBJECTHDR doh;

   /* Read the data object header and check that it is valid.
    */
   if( sizeof(DATAOBJECTHDR) != Amfile_Read( fh, &doh, sizeof(DATAOBJECTHDR) ) )
      return( FALSE );
   if( doh.wStartSig != DOH_START_SIG || doh.wEndSig != DOH_END_SIG )
      return( FALSE );
   if( doh.dwSize != cbData )
      return( FALSE );

   /* Read the data itself, using the size as specified in the
    * header.
    */
   if( Amfile_Read( fh, lpData, cbData ) != cbData )
      return( FALSE );

   /* Recompute the CRC and check it against the actual data.
    */
   if( Amuser_ComputeCRC( lpData, cbData ) != doh.wCRC )
      return( FALSE );
   return( TRUE );
}

/* Copy a referenced object.
 */
BOOL WINAPI EXPORT Amob_CopyRefObject( RECPTR FAR * lprpDest, RECPTR FAR * lprpSrc )
{
   if( lprpSrc->dwSize > lprpDest->dwSize || NULL == lprpDest->hpStr )
      {
      lprpDest->dwSize = lprpSrc->dwSize;
      lprpDest->dwOffset = lprpSrc->dwOffset;
      if( NULL != lprpSrc->hpStr )
         {
         if( NULL == lprpDest->hpStr )
            {
            if( !fNewMemory32( &lprpDest->hpStr, lprpDest->dwSize ) )
               return( FALSE );
            }
         else
            {
            if( !fResizeMemory32( &lprpDest->hpStr, lprpDest->dwSize ) )
               return( FALSE );
            }
         hmemcpy( lprpDest->hpStr, lprpSrc->hpStr, lprpDest->dwSize );
         }
      else
         lprpDest->hpStr = NULL;
      }
   else
      hmemcpy( lprpDest->hpStr, lprpSrc->hpStr, lprpDest->dwSize );
   return( TRUE );
}

/* Create a new reference string object.
 */
BOOL WINAPI EXPORT Amob_CreateRefString( RECPTR FAR * lprp, HPSTR hpszText )
{
   if( Amob_CreateRefObject( lprp, hstrlen( hpszText ) + 1 ) )
      hmemcpy( lprp->hpStr, hpszText, lprp->dwSize );
   return( TRUE );
}

/* Create a new reference object.
 */
BOOL WINAPI EXPORT Amob_CreateRefObject( RECPTR FAR * lprp, DWORD dwSize )
{
   if( dwSize > lprp->dwSize || NULL == lprp->hpStr )
      {
      DWORD dwFilePos;

      if( !fOutBasketLoaded )
         LoadOutBasket();
      if( NULL == lprp->hpStr )
         {
         if( !fNewMemory32( &lprp->hpStr, dwSize ) )
            return( FALSE );
         }
      else
         {
         if( !fResizeMemory32( &lprp->hpStr, dwSize ) )
            return( FALSE );
         }
      lprp->dwSize = dwSize;
      lprp->dwOffset = 0L;
      if( HNDFILE_ERROR == fhOb )
         {
         fhOb = Amfile_Open( szOutBasketData, AOF_READWRITE );
         if( HNDFILE_ERROR == fhOb )
            fhOb = Amfile_Create( szOutBasketData, 0 );
         if( HNDFILE_ERROR == fhOb )
            return( FALSE );
         }
      dwFilePos = Amfile_SetPosition( fhOb, 0L, ASEEK_END );
      if( -1 == dwFilePos )
         return( FALSE );
      lprp->dwOffset = dwFilePos;
      if( (DWORD)Amfile_Write32( fhOb, lprp->hpStr, lprp->dwSize ) != lprp->dwSize )
         return( FALSE );
      }
   return( TRUE );
}

/* This function frees up the reference object.
 */
BOOL WINAPI EXPORT Amob_FreeRefObject( RECPTR FAR * lprp )
{
   if( NULL != lprp->hpStr )
      {
      FreeMemory32( &lprp->hpStr );
      lprp->hpStr = NULL;
      }
   return( TRUE );
}

/* Count the number of active out-basket objects.
 */
int WINAPI EXPORT Amob_GetCount( WORD wFlag )
{
   WORD wCount;

   wCount = 0;
   if( LoadOutBasket() )
      {
      LPOB lpob;

      if( 0 == wFlag )
         return( cbOutBasketObjects );
      for( lpob = lpobFirst; lpob; lpob = lpob->lpobNext )
         {
         if( wFlag == OBF_UNFLAGGED )
            {
            if( ( lpob->obHdr.wFlags & (OBF_HOLD|OBF_KEEP) ) == 0 )
               ++wCount;
            }
         else if( ( lpob->obHdr.wFlags & wFlag ) == wFlag )
            ++wCount;
         }
      }
   return( wCount );
}

/* This function returns information about an out-basket
 * object.
 */
void WINAPI EXPORT Amob_GetObInfo( LPOB lpob, OBINFO FAR * lpobinfo )
{
   DWORD       dwType;

   /* Now gets the type by calling the handler. This is because we
    * should allow each object to tell us what it's type is. This
    * is so that mail objects can tell us whether they are CIX mail
    * or IP mail types. Sorts out the bug where IP mail is being
    * sent during a CIX conferencing only blink.
    * YH 08/05/96
    */
   if( !( dwType = GetType( lpob ) ) )
      dwType = lpob->lpobcl->dwType;

   lpobinfo->obHdr = lpob->obHdr;
   lpobinfo->dwClsType = dwType;
   lpobinfo->lpObData = lpob->lpObData;
}

/* This function sets information about an out-basket
 * object.
 */
void WINAPI EXPORT Amob_SetObInfo( LPOB lpob, OBINFO FAR * lpobinfo )
{
   lpob->obHdr = lpobinfo->obHdr;
   lpob->lpObData = lpobinfo->lpObData;
   fOutBasketModified = TRUE;
}

/* This function removes all out-basket items from memory.
 */
void WINAPI EXPORT Amob_ClearMemory( void )
{
   LPOB lpob;
   LPOB lpobNext;

   for( lpob = lpobFirst; lpob; lpob = lpobNext )
      {
      lpobNext = lpob->lpobNext;
      Amob_RemoveObject( lpob, FALSE );
      }
   Amob_SaveOutBasket( FALSE );

   /* Re-initialise the linked list.
    */
   lpobFirst = NULL;
   lpobLast = NULL;
   fOutBasketLoaded = FALSE;
}

/* This function removes all out-basket items from memory.
 */
void WINAPI EXPORT Amob_FreeMemory( void )
{
   LPOB lpob;
   LPOB lpobNext;

   for( lpob = lpobFirst; lpob; lpob = lpobNext )
   {
      lpobNext = lpob->lpobNext;
      FreeMemory( &lpob );
   }
   /* Re-initialise the linked list.
    */
   lpobFirst = NULL;
   lpobLast = NULL;
   fOutBasketLoaded = FALSE;
}

/* This function returns the next out-basket object in the
 * list. If lpob is NULL, it returns the first object.
 */
LPOB WINAPI EXPORT Amob_GetOb( LPOB lpob )
{
   if( NULL == lpob )
      {
      LoadOutBasket();
      return( lpobFirst );
      }
   return( lpob->lpobNext );
}

/* This function opens the out-basket index and creates a linked list of
 * objects by calling the Load method for each class type.
 */
BOOL FASTCALL LoadOutBasket( void )
{
   HNDFILE fh;

   /* Exit now if out-basket already loaded.
    */
   if( fOutBasketLoaded )
      return( TRUE );

   /* Get the path to the outbasket index.
    */
   Amuser_GetActiveUserDirectory( szOutBasketIndex, sizeof(szOutBasketIndex) );
   strcat( strcat( szOutBasketIndex, "\\" ), szOutBasketIndexName );

   /* Get the path to the outbasket data file.
    */
   Amuser_GetActiveUserDirectory( szOutBasketData, sizeof(szOutBasketData) );
   strcat( strcat( szOutBasketData, "\\" ), szOutBasketDataName );

   /* Open and read the index file.
    */
   lpobFirst = NULL;
   lpobLast = NULL;
   if( ( fh = Amfile_Open( szOutBasketIndex, AOF_READ ) ) != HNDFILE_ERROR )
      {
      /* Now loop for the entire out-basket index.
       */
      while( TRUE )
         {
         LPOB lpob;
         OBHEADER ob;

         /* Read each object header and validate the data via signatures
          * and checksums.
          */
         INITIALISE_PTR( lpob );
         if( Amfile_Read( fh, &ob, sizeof( OBHEADER ) ) != sizeof( OBHEADER ) )
            break;
         if( ob.wStartSig != OH_START_SIG || ob.wEndSig != OH_END_SIG )
            break;
         if( Amuser_ComputeCRC( (LPBYTE)&ob, sizeof(OBHEADER) - sizeof(WORD) ) != ob.wCRC )
            break;

         /* Okay. All is well, so allocate a memory block for the object
          * and link it to the rest of the out-basket.
          */
         if( fNewMemory( &lpob, sizeof( OB ) ) )
            {
            lpob->lpobcl = FindObjectClass( ob.clsid );
            if( NULL == lpob->lpobcl )
               FreeMemory( &lpob );
            else
               {
               ob.wFlags &= ~(OBF_ERROR|OBF_PENDING|OBF_OPEN|OBF_ACTIVE|OBF_SCRIPT);
               lpob->obHdr = ob;
               if( NULL == lpobFirst )
                  lpobFirst = lpob;
               else
                  lpobLast->lpobNext = lpob;
               lpob->lpobNext = NULL;
               lpob->lpobPrev = lpobLast;
               lpob->hwndEdit = NULL;
               lpobLast = lpob;
               lpob->lpObData = InvokeLoad( lpob, fh );
               ++cbOutBasketObjects;
               }
            }
         }
      Amfile_Close( fh );
      }
   fOutBasketLoaded = TRUE;
   fOutBasketModified = FALSE;
   return( TRUE );
}

/* This function saves the out-basket index file. If the out-basket is empty,
 * then it deletes any existing out-basket index file.
 */
BOOL FASTCALL SaveOutBasket( BOOL fCompact )
{
   HNDFILE fh;

   if( fOutBasketModified || fCompact )
      if( !lpobFirst ) 
         {
         if( Amfile_QueryFile( szOutBasketIndex ) ) 
            {
            /* Make sure that the data file is closed.
             */
            if( fhOb != HNDFILE_ERROR )
               {
               Amfile_Close( fhOb );
               fhOb = HNDFILE_ERROR;
               }
            Amfile_Delete( szOutBasketIndex );
            Amfile_Delete( szOutBasketData );
            }
         }
      else
         {
         /* If we're compacting, rename the TXT file so that we create
          * a new, empty, one.
          */
         if( fCompact )
            {
            /* Get the path to the outbasket temp file.
             */
            Amuser_GetActiveUserDirectory( szOutBasketTmp, sizeof(szOutBasketTmp) );
            strcat( strcat( szOutBasketTmp, "\\" ), szOutBasketTmpName );

            /* Open that file for writing.
             */
            fhObCmp = Amfile_Create( szOutBasketTmp, 0 );
            if( HNDFILE_ERROR != fhObCmp )
               fCompacting = TRUE;
            }

         /* Create the index file.
          */
         if( ( fh = Amfile_Create( szOutBasketIndex, 0 ) ) != HNDFILE_ERROR )
            {
            LPOB lpob;

            /* Loop for each out-basket object and write it out to the
             * index file.
             */
            for( lpob = lpobFirst; lpob; lpob = lpob->lpobNext )
               {
               /* Add a checksum and header to the out-basket file.
                */
               lpob->obHdr.wStartSig = OH_START_SIG;
               lpob->obHdr.wEndSig = OH_END_SIG;
               lpob->obHdr.wCRC = Amuser_ComputeCRC( (LPBYTE)&lpob->obHdr, sizeof(OBHEADER) - sizeof(WORD) );
               Amfile_Write( fh, (LPCSTR)&lpob->obHdr, sizeof(OBHEADER) );
               InvokeSave( lpob, fh, lpob->lpObData );
               }
            Amfile_Close( fh );
            fOutBasketModified = FALSE;
            }

         /* Close the data file now.
          */
         if( fhOb != HNDFILE_ERROR )
            {
            Amfile_Close( fhOb );
            fhOb = HNDFILE_ERROR;
            }

         /* Did compact succeed? If so, delete the old data file
          * and rename the temp file.
          */
         if( fCompact )
            {
            Amfile_Close( fhObCmp );
            Amfile_Delete( szOutBasketData );
            Amfile_Rename( szOutBasketTmp, szOutBasketData );
            fCompacting = FALSE;
            }
         }
   return( !fOutBasketModified );
}

/* This function uninstalls all installed object classes.
 */
void FASTCALL UninstallClasses( void )
{
   LPOBCLASSLIST lpcllNext;
   LPOBCLASSLIST lpcll;

   /* Free any installed object class
    * handlers as we go along.
    */
   for( lpcll = lpcllFirst; lpcll; lpcll = lpcllNext )
   {
      lpcllNext = lpcll->lpcllNext;
      if( NULL != lpcll->hObLib )
      {
         FreeLibrary( lpcll->hObLib );
      }
      FreeMemory( &lpcll ); // 20060325 - 2.56.2049.20
   }
}

/* This function registers an out-basket object class.
 */
BOOL WINAPI EXPORT Amob_RegisterObjectClass( OBCLSID obclsid, DWORD dwType, LPFOBPROC lfpObProc )
{
   LPOBCLASSLIST lpcll;
   LPOBCLASSLIST lpcllPrev;
   LPOBCLASSLIST lpcllNew;

   /* Scan the current class list looking for an existing class with
    * the same ID and subclass the existing class if found.
    */
   lpcllPrev = NULL;
   for( lpcll = lpcllFirst; lpcll; lpcllPrev = lpcll, lpcll = lpcll->lpcllNext )
      if( lpcll->clsid >= obclsid )
         break;
   INITIALISE_PTR(lpcllNew);
   if( fNewMemory( &lpcllNew, sizeof( OBCLASSLIST ) ) )
      {
      if( lpcllPrev == NULL )
         lpcllFirst = lpcllNew;
      else
         lpcllPrev->lpcllNext = lpcllNew;
      lpcllNew->lpcllNext = lpcll;
      lpcllNew->clsid = obclsid;
      lpcllNew->dwType = dwType;
      lpcllNew->lfpObProc = lfpObProc;
      lpcllNew->hObLib = NULL;
      return( TRUE );
      }
   return( FALSE );
}

/* This function returns the object class.
 */
DWORD WINAPI EXPORT Amob_GetObjectType( LPOB lpob )
{
   DWORD       dwType;

   /* Now gets the type by calling the handler. This is because we
    * should allow each object to tell us what it's type is. This
    * is so that mail objects can tell us whether they are CIX mail
    * or IP mail types. Sorts out the bug where IP mail is being
    * sent during a CIX conferencing only blink.
    * YH 08/05/96
    */
   if( !( dwType = GetType( lpob ) ) )
      dwType = lpob->lpobcl->dwType;

   return( dwType );
}

/* This function locates the class structure for the
 * specified class.
 */
LPOBCLASSLIST FindObjectClass( OBCLSID clsid )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         break;
   return( lpcll );
}

/* This function creates a new out-basket object and shows the
 * object in the Out Basket window if it is open.
 */
BOOL WINAPI EXPORT Amob_New( OBCLSID clsid, LPVOID dwData )
{
   return( InvokeNew( clsid, dwData ) );
}

/* This function deletes an out-basket object.
 */
BOOL WINAPI EXPORT Amob_Delete( OBCLSID clsid, LPVOID dwData )
{
   return( InvokeDelete( clsid, dwData ) );
}

HWND WINAPI EXPORT Amob_GetEditWindow( LPOB lpob )
{
   return( lpob->hwndEdit );
}

void WINAPI EXPORT Amob_SetEditWindow( LPOB lpob, HWND hwnd )
{
   lpob->hwndEdit = hwnd;
}

/* This function edits an out-basket object.
 */
BOOL WINAPI EXPORT Amob_Edit( LPOB lpob )
{
   lpob->obHdr.wFlags |= OBF_OPEN;
   if( Amuser_CallRegistered( AE_EDITING_OUTBASKET_OBJECT, (LPARAM)lpob, (LPARAM)lpob->obHdr.clsid ) )
      return( InvokeEdit( lpob ) );
   lpob->obHdr.wFlags &= ~OBF_OPEN;
   lpob->hwndEdit = NULL;
   return( FALSE );
}

/* This function executes an out-basket object.
 */
BOOL WINAPI EXPORT Amob_Exec( OBCLSID clsid, LPVOID dwData )
{
   if( Amuser_CallRegistered( AE_EXECUTING_OUTBASKET_OBJECT, (LPARAM)dwData, (LPARAM)clsid ) )
      return( InvokeExec( clsid, dwData ) );
   return( FALSE );
}

/* This function executes an out-basket object.
 */
BOOL WINAPI EXPORT Amob_Describe( LPOB lpob, LPSTR lpszBuf )
{
   return( InvokeDescribe( lpob, lpszBuf ) );
}

/* This function locates a matching out-basket object of the
 * specified type.
 */
LPOB WINAPI EXPORT Amob_Find( OBCLSID clsid, LPVOID dwData )
{
   LPOB lpob;

   if( !LoadOutBasket() )
      return( 0L );
   for( lpob = lpobFirst; lpob; lpob = lpob->lpobNext )
      if( lpob->obHdr.clsid == clsid )
         if( InvokeFind( clsid, dwData, lpob->lpObData ) )
            return( lpob );
   return( 0L );
}

/* This function locates a matching out-basket object of the
 * specified type.
 */
LPOB WINAPI EXPORT Amob_FindNext( LPOB lpob, OBCLSID clsid, LPVOID dwData )
{
   while( lpob = lpob->lpobNext )
      if( lpob->obHdr.clsid == clsid )
         if( InvokeFind( clsid, dwData, lpob->lpObData ) )
            return( lpob );
   return( 0L );
}

/* This function removes an out-basket object given an
 * object ID.
 */
LPOB WINAPI EXPORT Amob_GetByID( LPOB lpob, WORD id )
{
   if( NULL == lpob )
      lpob = lpobFirst;
   while( lpob )
      {
      if( lpob->obHdr.id == id && !(lpob->obHdr.wFlags & (OBF_ERROR|OBF_HOLD|OBF_KEEP|OBF_OPEN)) )
         break;
      lpob = lpob->lpobNext;
      }
   return( lpob );
}

/* This function deletes an out-basket object.
 */
void WINAPI EXPORT Amob_RemoveObject( LPOB lpob, BOOL fSaveAfterwards )
{
   if( Amuser_CallRegistered( AE_DELETING_OUTBASKET_OBJECT, (LPARAM)lpob, (LPARAM)lpob->obHdr.clsid ) )
      if( InvokeDelete( lpob->obHdr.clsid, lpob->lpObData ) )
         if( InvokeRemove( lpob ) )
            {
            OBCLSID clsid;

            clsid = lpob->obHdr.clsid;
            if( lpob->lpobNext )
               lpob->lpobNext->lpobPrev = lpob->lpobPrev;
            else
               lpobLast = lpob->lpobPrev;
            if( lpob->lpobPrev )
               lpob->lpobPrev->lpobNext = lpob->lpobNext;
            else
               lpobFirst = lpob->lpobNext;
            FreeMemory( &lpob );
            --cbOutBasketObjects;
            fOutBasketModified = TRUE;
            Amuser_CallRegistered( AE_DELETED_OUTBASKET_OBJECT, (LPARAM)0L, (LPARAM)clsid );
            if( fSaveAfterwards )
               SaveOutBasket( FALSE );
            }
}

/* This function saves the out-basket to disk.
 */
void WINAPI EXPORT Amob_SaveOutBasket( BOOL fCompact )
{
   SaveOutBasket( fCompact );
}

/* This function commits an out-basket object. If we're online, we
 * call the Exec function directly otherwise we store the object
 * in the out-basket.
 */
BOOL WINAPI EXPORT Amob_Uncommit( LPOB lpobOld )
{
   lpobOld->obHdr.wFlags &= ~OBF_OPEN;
   lpobOld->hwndEdit = NULL;
   fOutBasketModified = TRUE;
   Amuser_CallRegistered( AE_EDITED_OUTBASKET_OBJECT, (LPARAM)lpobOld, (LPARAM)lpobOld->obHdr.clsid );
   return( TRUE );
}

/* This function commits an out-basket object. If we're online, we
 * call the Exec function directly otherwise we store the object
 * in the out-basket.
 */
LPOB WINAPI EXPORT Amob_Commit( LPOB lpobOld, OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpobcl;
   LPOB lpob;
   WORD wFlags;
   BOOL fExec;

   /* Make sure the out-basket is loaded.
    */
   if( !LoadOutBasket() )
      return( NULL );

   /* If we're replacing an old object, we do an in-place
    * update. Also broadcast an event to indicate that the
    * update is complete.
    */
   if( NULL != lpobOld )
      {
      lpobOld->obHdr.wFlags &= ~OBF_OPEN;
      lpobOld->hwndEdit = NULL;
      lpobOld->lpObData = InvokeCopy( clsid, dwData );
      fOutBasketModified = TRUE;
      Amuser_CallRegistered( AE_EDITED_OUTBASKET_OBJECT, (LPARAM)lpobOld, (LPARAM)clsid );
      return( lpobOld );
      }

   /* Make sure the object class exists.
    */
   lpobcl = FindObjectClass( clsid );
   if( NULL == lpobcl )
      return( NULL );

   /* Okay. Now go ahead and process.
    */
   wFlags = 0;
   fExec = Amuser_CallRegistered( AE_INSERTING_OUTBASKET_OBJECT, (LPARAM)dwData, (LPARAM)clsid );
   if( fExec || !QueryIsObjectPersistent( clsid ) )
      {
      int nExecResult;

      nExecResult = InvokeExec( clsid, dwData );
      if( nExecResult == POF_PROCESSCOMPLETED || nExecResult == POF_PROCESSSTARTED )
         {
         InvokeDelete( clsid, dwData );
         return( LPOB_PROCESSED );
         }
      if( nExecResult != POF_HELDBYSCRIPT )
         wFlags = OBF_PENDING;
      }
   INITIALISE_PTR( lpob ); 
   if( fNewMemory( &lpob, sizeof( OB ) ) )
      {
      if( NULL == lpobFirst )
         lpobFirst = lpob;
      else
         lpobLast->lpobNext = lpob;
      lpob->lpobNext = NULL;
      lpob->lpobPrev = lpobLast;
      lpobLast = lpob;
      if( nNextObId == MAX_OB_ID )
         nNextObId = MIN_OB_ID;
      lpob->obHdr.id = InvokeGetObjectID( clsid, dwData );
      lpob->obHdr.clsid = clsid;
      lpob->obHdr.wFlags = wFlags;
      lpob->obHdr.wDate = Amdate_GetPackedCurrentDate();
      lpob->obHdr.wTime = Amdate_GetPackedCurrentTime();
      lpob->obHdr.wActionDate = 0;
      lpob->obHdr.wActionTime = 0;
      lpob->obHdr.wLastActionDate = 0;
      lpob->obHdr.wLastActionTime = 0;
      lpob->obHdr.wActionFlags = OBSCHED_RUNONCE;
      lpob->obHdr.wActionFreq = 0;
      lpob->lpobcl = lpobcl;
      lpob->hwndEdit = NULL;
      lpob->lpObData = InvokeCopy( clsid, dwData );
      ++cbOutBasketObjects;
      fOutBasketModified = TRUE;
      Amuser_CallRegistered( AE_INSERTED_OUTBASKET_OBJECT, (LPARAM)lpob, (LPARAM)clsid );
      SaveOutBasket( FALSE );
      return( lpob );
      }
   return( NULL );
}

/* This function resyncs the object IDs.
 */
void WINAPI EXPORT Amob_ResyncObjectIDs( void )
{
   LPOB lpob;

   nNextObId = MIN_OB_ID;
   for( lpob = lpobFirst; lpob; lpob = lpob->lpobNext )
      lpob->obHdr.id = InvokeGetObjectID( lpob->obHdr.clsid, lpob->lpObData );
}

/* This function calls the method to retrieve an object ID for
 * the specified out-basket object.
 */
WORD FASTCALL InvokeGetObjectID( OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         {
         if( lpcll->lfpObProc )
            return( (WORD)lpcll->lfpObProc( OBEVENT_GETCLSID, 0, dwData, 0 ) );
         break;
         }
   return( Amob_GetNextObjectID() );
}

/* This function returns the next available object ID.
 */
WORD WINAPI EXPORT Amob_GetNextObjectID( void )
{
   if( MAX_OB_ID == nNextObId )
      nNextObId = MIN_OB_ID;
   return( nNextObId++ );
}

/* This function invokes the method for getting the objects type.
 * It returns the type or 0 if there was no get type method for this
 * this object.
 * YH 08/05/96
 */
DWORD FASTCALL GetType( LPOB lpob )
{
   return( InvokeHandler( lpob, OBEVENT_GETTYPE, 0, lpob->lpObData, 0L ) );
}

/* This function calls the New method for the specified class.
 * BUGBUG: Replace this with hash tables.
 */
BOOL FASTCALL InvokeNew( OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         {
         if( lpcll->lfpObProc )
            return( (BOOL)lpcll->lfpObProc( OBEVENT_NEW, 0, dwData, 0 ) );
         break;
         }
   return( TRUE );
}

/* This function calls the Copy method for the specified class.
 * BUGBUG: Replace this with hash tables.
 */
LPVOID FASTCALL InvokeCopy( OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         {              
         if( lpcll->lfpObProc )
            return( (LPVOID)lpcll->lfpObProc( OBEVENT_COPY, 0, dwData, 0 ) );
         break;
         }
   return( NULL );
}

/* This function calls the Save method for the specified class.
 */
BOOL FASTCALL InvokeSave( LPOB lpob, HNDFILE fh, LPVOID dwData )
{
   return( (BOOL)InvokeHandler( lpob, OBEVENT_SAVE, fh, dwData, 0L ) );
}

/* This function calls the Load method for the specified class.
 */
LPVOID FASTCALL InvokeLoad( LPOB lpob, HNDFILE fh )
{
   return( (LPVOID)InvokeHandler( lpob, OBEVENT_LOAD, fh, 0L, 0L ) );
}

/* This function calls the Remove method for the specified class.
 */
BOOL FASTCALL InvokeRemove( LPOB lpob )
{
   InvokeHandler( lpob, OBEVENT_REMOVE, 0, lpob->lpObData, 0L );
   return( TRUE );
}

/* This function calls the Describe method for the specified class.
 */
BOOL FASTCALL InvokeDescribe( LPOB lpob, LPSTR lpszBuf )
{
   return( (BOOL)InvokeHandler( lpob, OBEVENT_DESCRIBE, 0, lpob->lpObData, lpszBuf ) );
}

/* This function calls the Edit method for the specified class.
 */
BOOL FASTCALL InvokeEdit( LPOB lpob )
{
   return( (BOOL)InvokeHandler( lpob, OBEVENT_EDIT, 0, lpob, 0L ) );
}

/* Returns whether or not the specified object can be
 * placed in the out-basket.
 */
BOOL FASTCALL QueryIsObjectPersistent( OBCLSID clsid )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         return( (BOOL)lpcll->lfpObProc( OBEVENT_PERSISTENCE, 0, 0, 0L ) );
   return( FALSE );
}

/* Returns whether or not the specified object is editable.
 */
BOOL WINAPI EXPORT Amob_IsEditable( OBCLSID clsid )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         return( (BOOL)lpcll->lfpObProc( OBEVENT_EDITABLE, 0, 0, 0L ) );
   return( FALSE );
}

/* This function invokes the object handler procedure with the specified
 * event and parameters.
 */
LRESULT FASTCALL InvokeHandler( LPOB lpob, int obEvent, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   if( NULL != lpob->lpobcl )
      {
      if( lpob->lpobcl->lfpObProc )
         return( (LRESULT)lpob->lpobcl->lfpObProc( obEvent, fh, dwData, lpBuf ) );
      return( TRUE );
      }

   /* No class handler found for this class, so
    * try to load it.
    */
   if( InstallClassHandler( lpob ) )
      {
      if( lpob->lpobcl->lfpObProc )
         return( (LRESULT)lpob->lpobcl->lfpObProc( obEvent, fh, dwData, lpBuf ) );
      return( TRUE );
      }
   return( FALSE );
}

/* This function calls the Exec method for the specified class.
 * BUGBUG: Replace this with hash tables.
 */
int FASTCALL InvokeExec( OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         return( (int)lpcll->lfpObProc( OBEVENT_EXEC, 0, dwData, 0L ) );
   return( POF_CANNOTACTION );
}

/* This function calls the Find method for the specified class.
 * BUGBUG: Replace this with hash tables.
 */
BOOL FASTCALL InvokeFind( OBCLSID clsid, LPVOID dwData1, LPVOID dwData2 )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         return( (BOOL)lpcll->lfpObProc( OBEVENT_FIND, 0, dwData1, dwData2 ) );
   return( FALSE );
}

/* This function calls the Delete method for the specified class.
 * BUGBUG: Replace this with hash tables.
 */
BOOL FASTCALL InvokeDelete( OBCLSID clsid, LPVOID dwData )
{
   LPOBCLASSLIST lpcll;

   for( lpcll = lpcllFirst; lpcll; lpcll = lpcll->lpcllNext )
      if( lpcll->clsid == clsid )
         return( (BOOL)lpcll->lfpObProc( OBEVENT_DELETE, 0, dwData, 0L ) );
   return( TRUE );
}

/* This function attempts to install the DLL for the
 * specified class. We look up the [Object Classes] section
 * of the system file and use that to extract the filename of
 * the respective handler. If found, we load the DLL and call
 * the InstantiateClass function which ought to register the
 * class anew.
 *
 * If this all fails, write an entry to the event log and fail
 * the load procedure.
 */
BOOL FASTCALL InstallClassHandler( LPOB lpob )
{
   char szFilename[ _MAX_PATH ];
   char szClsId[ 20 ];

   wsprintf( szClsId, "%lu", lpob->obHdr.clsid );
   if( Amuser_GetLMString( szObClsSection, szClsId, "", szFilename, sizeof(szFilename) ) )
      {
      HINSTANCE hObLib;

      if( NULL == ( hObLib = LoadLibrary( szFilename ) ) )
         {
         INSTANTIATEPROC lpInstantiateProc;
         LPOBCLASSLIST lpobcl;

         /* Find the InstantiateClass entry point and, if found, call it
          * and check that it successfully registered the object class
          * we needed registered.
          */
         if( ( lpInstantiateProc = (INSTANTIATEPROC)GetProcAddress( hObLib, "InstantiateClass" ) ) == NULL )
            {
            if( lpInstantiateProc( lpob->obHdr.clsid ) )
               if( NULL != ( lpobcl = FindObjectClass( lpob->obHdr.clsid ) ) )
                  {
                  /* Now save the handle of this object
                   * library, and ALSO save it in lpob.
                   */
                  lpobcl->hObLib = hObLib;
                  lpob->lpobcl = lpobcl;
                  return( TRUE );
                  }

            /* Error, the InstantiateClass entry point returned FALSE or
             * the class was not registered.
             */
            wsprintf( szFilename, strError1, szClsId );
            Amevent_AddEventLogItem( ETYP_DISKFILE|ETYP_SEVERE, szFilename );
            }

         /* Error. Handler is missing InstantiateClass function.
          */
         else
            {
            wsprintf( szFilename, strError2, szClsId );
            Amevent_AddEventLogItem( ETYP_DISKFILE|ETYP_SEVERE, szFilename );
            }
         FreeLibrary( hObLib );
         }
      }

   /* Failed. Write to event log and return error.
    */
   else
      {
      wsprintf( szFilename, strError3, szClsId );
      Amevent_AddEventLogItem( ETYP_DISKFILE|ETYP_SEVERE, szFilename );
      }
   return( FALSE );
}
