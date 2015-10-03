/* UNCMP.C - Does PKUNPAK uncompression
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
#include <string.h>
#include <dos.h>
#include "amlib.h"
#include <io.h>
#include <stdlib.h>
#include <ctype.h>
#include "amctrls.h"
#include "amuser.h"

#define  THIS_FILE   __FILE__

#define DLE             0x90           /* repeat sequence marker */
#define NOHIST          0              /* don't consider previous input */
#define INREP           1              /* sending a repeated value */
#define TABSIZE         4096
#define STACKSIZE       TABSIZE
#define NO_PRED         0xFFFF
#define EMPTY           0xF000
#define NOT_FND         0xFFFF
#define UEOF            0xFFFF
#define UPDATE          TRUE
#define OUTBUFSIZE      16384          /* 16k output buffer */
#define INBUFSIZE       16384          /* 16k input buffer */
#define FIRST           257            /* first free entry */
#define CLEAR           256            /* table clear output code */
#define MAXOUTSIZE      16384
#define BITS            13             /* could be restricted to 12 */
#define INIT_BITS       9              /* initial number of bits/code */
#define HSIZE           9001           /* 91% occupancy */
#define SQEOF           256            /* Squeeze EOF */
#define NUMVALS         257            /* 256 data values plus SQEOF */

/* the following is an inline version of Amcmp_AddCRC for 1 character, implemented
 * as a macro, it really speeds things up.
 */
#define add1crc( x) crc = ( ( crc >> 8) & 0x00FF) ^ unarccrctab[( crc ^ x) & 0x00FF];

#define MAXCODE( n_bits)      ( (  1<<( n_bits)) - 1)

#define tab_prefixof( i)      codetab[i]
#define tab_suffixof( i)      ( ( BYTE FAR *)( htab))[i]
#define de_stack             ( ( BYTE FAR *)&tab_suffixof( 1<<BITS))

static void FASTCALL Amcmp_AddCRC( char *, int);
static int FASTCALL Amcmp_DlzwDecompress( HFILE, HFILE, char);
static void FASTCALL Amcmp_WriteRLE( BYTE, HFILE);
static int FASTCALL Amcmp_GetHeader( HFILE);
static int FASTCALL Amcmp_SQDecompress( HFILE, HFILE);
static BOOL FASTCALL Amcmp_ExtractFile( HFILE, LPSTR);
static int FASTCALL Amcmp_SlzwDecompress( HFILE, HFILE, int );
static int FASTCALL Amcmp_Uncompress( HFILE, HFILE );
static int FASTCALL Amcmp_StoreDecompressed( HFILE, HFILE );
static int FASTCALL Amcmp_RLEDecompress( HFILE, HFILE );
static int FASTCALL Amcmp_GetCode( HFILE );
static int FASTCALL Amcmp_Dispatch( HFILE, HFILE );
static void FASTCALL Amcmp_WriteOut( HFILE, void FAR *, size_t );
static int FASTCALL Amcmp_GetInByte( HFILE );
static UINT FASTCALL Amcmp_GetInBlock( HFILE, HPVOID, UINT );
static void FASTCALL Amcmp_FlushOut( HFILE );
static void FASTCALL Amcmp_CloseOut( HFILE );
static void FASTCALL Amcmp_SetMemory( char FAR *, char, DWORD );
static void FASTCALL Amcmp_InitTable( void);
static UINT FASTCALL Amcmp_Pop( void);
static void FASTCALL Amcmp_Push( UINT);
static int FASTCALL Amcmp_GetCode12( HFILE);
static void FASTCALL Amcmp_UpdateTable( UINT, UINT);
static UINT FASTCALL Amcmp_Hash( UINT, BYTE, int);
static void FASTCALL Amcmp_FreeBuffer( void );
static LPSTR FASTCALL Amcmp_InitBuffer( HFILE );
static void FASTCALL Amcmp_FillBuffer( HFILE );

typedef struct tagEntry {
   char used;
   UINT next;              /* hi bit is 'used' flag      */
   UINT predecessor;       /* 12 bit code                */
   BYTE follower;
} entry;

entry FAR * string_tab;

#pragma pack( 1)
struct archive_header {
   char arcmark;                 /* arc mark = 0x1a */
   char atype;                   /* header version 0 = end, else pack method */
   char name[13];                /* file name */
   DWORD size;                   /* size of compressed file */
   short date;                   /* file date */
   short time;                   /* file time */
   WORD crc;            /* cyclic redundancy check */
   DWORD length;                 /* true file length */
};
#pragma pack( )

struct sq_tree {
    int children[2];           /* left, right */
   };                             /* use large buffer */

int unarccrctab[] =            /* CRC lookup table */
{
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

static WORD szBuf;
static WORD wRead;
static WORD wTotal;
static LPSTR lpBuffer;                 /* pointer to input buffer */
static LPSTR lpBufPtr;                 /* current pos in input buffer */
static HFILE out;
static int clear_flg;
static int n_bits;                     /* number of bits/code */
static int max_bits;                   /* user settable max # bits/code */
static int maxcode;                    /* maximum code, given n_bits */
static int maxmaxcode;                 /* should NEVER generate this code */
static int free_ent;                   /* first unused entry */
static BYTE state;                     /* state of ncr packing */
static UINT crc;                       /* crc of current file */
static long sizeleft;                  /* for fileio routines */
static int errors;                     /* number of errors */
static char path[ 63 ];                /* path name to output to */
static char headertype;                /* headertype of archive */
static HWND hwndStatus;
static long total;
static WORD old_count;
static int sp;                         /* current stack pointer */
static LPSTR stack;                    /* stack for pushing and popping */
static BOOL fBuf;
static DWORD dwOutBufSize;
static DWORD dwOutBufCount;
static void FAR * lpOutBuffer;
static BOOL fBuf;
static long FAR * htab;
static WORD FAR * codetab;
static short hsize;                    /* for dynamic table sizing */
static struct archive_header archead;  /* header for current archive */

#ifndef WIN32
static CATCHBUF catchBuf;
#else
static BOOL fEarlyEof;
#endif

/* This function checks whether the specified file is a archived file.
 */
BOOL EXPORT WINAPI Amcmp_IsArcedFile( LPSTR lpFileName )
{
   HFILE fh;
   char ch;

   if( (  fh = _lopen( lpFileName, OF_READ ) ) != HFILE_ERROR )
      {
      int cRead;

      cRead = _lread( fh, &ch, sizeof( ch ) );
      _lclose( fh );
      if( cRead == 1 && ch == 0x1A )
         return( TRUE );
      }
   return( FALSE );
}

/* This uncompresses the specified file.
 */
BOOL EXPORT WINAPI Amcmp_UncompressFile( HWND hwnd, LPSTR lpszInFile, LPSTR lpszOutFile )
{
   HFILE in;
   BOOL fOk = FALSE;

   total = old_count = 0;
   errors = sp = 0;
   clear_flg = 0;
   free_ent = 0;
   max_bits = BITS;
   maxmaxcode = 1 << BITS;
   if( (  in = _lopen( lpszInFile, OF_READ ) ) != HFILE_ERROR )
      {
      LPVOID hBuffer;

      hBuffer = Amcmp_InitBuffer( in );
      fBuf = FALSE;
      fOk = TRUE;
      out = HFILE_ERROR;
#ifndef WIN32
      if( Catch( ( int FAR *)catchBuf ) != 0 )
         {
         if( out != HFILE_ERROR )
            _lclose( out );
         fOk = FALSE;
         }
#else
      fEarlyEof = FALSE;
#endif
      if( fOk )
         {
         if( !Amcmp_GetHeader( in ) )
            fOk = FALSE;
         else {
            hwndStatus = hwnd;
            if( NULL != hwndStatus )
               SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, 0, 0L );
            fOk = Amcmp_ExtractFile( in, lpszOutFile );
            }
         }
#ifdef WIN32
      if( fEarlyEof )
         fOk = FALSE;
#endif
      Amcmp_FreeBuffer( );
      _lclose( in );
      }
   return( fOk );
}

static BOOL FASTCALL Amcmp_ExtractFile( HFILE in, LPSTR filename )
{
   char outfile[80];
   BOOL failure = FALSE;

   /* create filename with specified path */
   lstrcpy( outfile, filename);
   if ( ( out = _lcreat( outfile, 0 ) ) == HFILE_ERROR )
      return( FALSE);
   if( archead.length )
      failure = Amcmp_Uncompress( in, out);
   Amcmp_CloseOut( out );

   /* set date and time, but skip if not MSC since Turbo C has no */
   /* equivalent function */
#ifndef WIN32
   _dos_setftime( out, archead.date, archead.time);
#endif
   _lclose( out);

   /* if errors during uncompression, than delete attempt at uncompression */
   if( failure )
      _unlink( outfile );
   return( !failure );
}

/* Amcmp_InitBuffer
 * ( Private Internal)
 *
 * Description:
 *  This function initalises the input buffer to 0xFF00 bytes, and sets
 *  the first byte of the buffer to NULL which triggers the next read of
 *  the buffer to call Amcmp_FillBuffer( ).
 *
 * Input:
 *  fh            is the input file handle
 *
 * Return Value
 *  Returns a pointer to the input buffer
 */

static LPSTR FASTCALL Amcmp_InitBuffer( int fh )
{
   DWORD lszBuf;
   HGLOBAL hBuffer;

   lszBuf = _llseek( fh, 0L, 2 );
   _llseek( fh, 0L, 0 );
   szBuf = ( unsigned)min( lszBuf, 0x0000FF00L );
   if( hBuffer = GlobalAlloc( GHND, ( DWORD)szBuf ) )
      {
      lpBuffer = GlobalLock( hBuffer );
      lpBufPtr = lpBuffer;
      wTotal = wRead = 0;
      Amcmp_FillBuffer( fh );
      }
   return( lpBuffer );
}

static void FASTCALL Amcmp_FreeBuffer( void )
{
   HGLOBAL hg;

#ifdef WIN32
   hg = GlobalHandle( lpBuffer );
#else
   hg = ( HGLOBAL)LOWORD( GlobalHandle( SELECTOROF( lpBuffer ) ) );
#endif
   GlobalUnlock( hg );
   GlobalFree( hg );
}

static UINT FASTCALL Amcmp_GetInBlock( HFILE fh, HPVOID lpBuf, UINT wSize )
{
   WORD wRead = 0;

   while( wSize-- )
      {
      *( (HPSTR)lpBuf)++ = Amcmp_GetInByte( fh );
      ++wRead;
      }
   return( wRead );
}

static int FASTCALL Amcmp_GetInByte( HFILE fh )
{
#ifdef WIN32
   if( fEarlyEof )
      return( 0 );
#endif
   if( wRead == wTotal )
      {
      Amcmp_FillBuffer( fh );
#ifdef WIN32
      if( wTotal == 0 ) {
         fEarlyEof = TRUE;
         return( 0 );
         }
#else
      if( wTotal == 0 )
         Throw( ( int FAR *)catchBuf, 1 );
#endif
      }
   ++wRead;
   return( *lpBufPtr++ );
}

/* Amcmp_FillBuffer
 * ( Private Internal)
 *
 * Description:
 *  This function fills the input buffer from the source file.
 *
 * Input:
 *  fh            is the input file handle
 *
 * Return Value
 *  None
 */

static void FASTCALL Amcmp_FillBuffer( int fh )
{
   lpBufPtr = lpBuffer;
   wTotal = _lread( fh, lpBufPtr, szBuf );
   wRead = 0;
}

static int FASTCALL Amcmp_GetHeader( HFILE in)
{
    /* read in archead minus the length field */

    if ( ( Amcmp_GetInBlock( in, &archead, sizeof( struct archive_header) - sizeof( long))) < 2) {
       return( FALSE );
    }
    /* if archead.arcmark does not have that distinctive arc identifier 0x1a */
    /* then it is not an archive */

    if ( archead.arcmark != 0x1a) {
       return( FALSE);
    }
    /* if atype is 0 then EOF */

    if ( archead.atype == 0)
       return ( FALSE);

    /* if not obsolete header type then the next long is the length field */

    if ( archead.atype != 1) {
       if ( Amcmp_GetInBlock( in, &archead.length, sizeof( long)) != sizeof( long)) {
           return( FALSE);
       }
    }
    /* if obsolete then set length field equal to size field */

    else
       archead.length = archead.size;

    return ( TRUE);
}

static int FASTCALL Amcmp_Uncompress( HFILE in, HFILE out )
{
    switch ( archead.atype) {
       case 1:
       case 2:
       case 3:
       case 4:
       case 5:
       case 6:
       case 7:
       case 8:
       case 9:
           break;

       case 10:
       case 11:
           _llseek( in, archead.size, 1);
           return ( 1);

       default:
           _llseek( in, archead.size, 1);
           errors++;
           return ( 1);
       }
   if( Amcmp_Dispatch( in, out) > 0 )
      {
      errors++;
      return ( 1);
      }
   if ( crc != archead.crc)
      {
      errors++;
      return ( 1);
      }
   return ( 0);
}

static int FASTCALL Amcmp_Dispatch( HFILE in, HFILE out )
{
   int err = 0;

   crc = 0;
   sizeleft = archead.size;
   state = 0;
   switch( archead.atype )
      {
      case 1:
      case 2:
           err = Amcmp_StoreDecompressed( in, out);
           break;
      case 3:
           err = Amcmp_RLEDecompress( in, out);
           break;
      case 4:
           err = Amcmp_SQDecompress( in, out);
           break;
      case 5:
      case 6:
      case 7:
           err = Amcmp_SlzwDecompress( in, out, archead.atype);
           break;
      case 8:
      case 9:
           err = Amcmp_DlzwDecompress( in, out, archead.atype);
           break;
      default:
           break;
           /* should never reach this code */
      }
   return( err );
}

/* Amcmp_WriteRLE outputs bytes to a file unchanged, except that runs */
/* more than two characters are compressed to the format:       */
/*       <char> DLE <number>                                    */
/* When DLE is encountered, the next byte is read and Amcmp_WriteRLE  */
/* repeats the previous byte <number> times.  A <number> of 0   */
/* indicates a true DLE character.                              */

static void FASTCALL Amcmp_WriteRLE( BYTE c, HFILE out )
{                              /* put RLE coded bytes */
    static char lastc;

    switch ( state) {           /* action depends on our state */

       case NOHIST:            /* no previous history */
           if ( c == DLE)       /* if starting a series */
               state = INREP;  /* then remember it next time */
           else {
               add1crc( c);
               lastc = c;
               Amcmp_WriteOut( out, &c, sizeof( char) );
           }
           return;

       case INREP:             /* in a repeat */
           if ( c)              /* if count is nonzero */
               while ( --c) {   /* then repeatedly ... */
                   add1crc( lastc);
                  Amcmp_WriteOut( out, &lastc, sizeof( char) );
               }
           else {
               BYTE ch2;
               add1crc( DLE);
               ch2 = DLE;
               Amcmp_WriteOut( out, &ch2, sizeof( char) );
           }
           state = NOHIST;     /* back to no history */
           return;
    }
}

static int FASTCALL Amcmp_SlzwDecompress( HFILE in, HFILE out, int arctype)
{
    UINT c, tempc;
    UINT code, oldcode, incode, finchar, lastchar;
    char unknown = FALSE;
    int code_count = TABSIZE - 256;
    entry FAR *ep;

    INITIALISE_PTR(string_tab);
    INITIALISE_PTR(stack);
    if( !fNewMemory( &string_tab, sizeof( entry) * TABSIZE) )
       return( 1);
    if( !fNewMemory( &stack, sizeof( char) * STACKSIZE) )
       return( 1);

    headertype = arctype;

    lastchar = 0;
    Amcmp_InitTable( );                        /* set up atomic code definitions */
    code = oldcode = Amcmp_GetCode12( in);
    c = string_tab[code].follower;     /* first code always known      */
    if ( headertype == 5) {
       add1crc( c);
       Amcmp_WriteOut( out, &c, sizeof( char ) );
    } else
       Amcmp_WriteRLE( ( BYTE)c, out);
    finchar = c;

    while ( UEOF != ( code = incode = Amcmp_GetCode12( in))) {
       ep = &( string_tab[code]);       /* initialize pointer           */
       if ( !ep->used) {        /* if code isn't known          */
           lastchar = finchar;
           code = oldcode;
           unknown = TRUE;
           ep = &( string_tab[code]);   /* re-initialize pointer        */
       }
       while ( NO_PRED != ep->predecessor) {

           /* decode string backwards into stack */
           Amcmp_Push( ep->follower);

           code = ep->predecessor;
           ep = &( string_tab[code]);
       }

       finchar = ep->follower;

       /* above loop terminates, one way or another, with                  */
       /* string_tab[code].follower = first char in string                 */

       if ( headertype == 5) {
           add1crc( finchar);
           Amcmp_WriteOut( out, &c, sizeof( char ) );
       } else
           Amcmp_WriteRLE( ( BYTE)finchar, out);

       /* Amcmp_Pop anything stacked during code parsing                         */

       while ( EMPTY != ( tempc = Amcmp_Pop( ))) {
           if ( headertype == 5) {
               add1crc( tempc);
               Amcmp_WriteOut( out, &tempc, sizeof( char));
           } else
               Amcmp_WriteRLE( ( BYTE)tempc, out);
       }
       if ( unknown) {          /* if code isn't known the follower char of
                                * last */
           if ( headertype == 5) {
               finchar = lastchar;
               add1crc( finchar);
               Amcmp_WriteOut( out, &finchar, sizeof( char));
           } else
               Amcmp_WriteRLE( ( BYTE)(  finchar = lastchar ), out);
           unknown = FALSE;
       }
       if ( code_count) {
           Amcmp_UpdateTable( oldcode, finchar);
           --code_count;
       }
       oldcode = incode;
    }

    FreeMemory( &stack);
    FreeMemory( &string_tab);

    return ( 0);                        /* close all files and quit */
}

static unsigned FASTCALL Amcmp_Hash( UINT pred, BYTE foll, int update )
{
    register UINT local, tempnext;
    static long temp;
    register entry FAR *ep;

    if ( headertype == 7)
       /* I'm not sure if this works, since I've never seen an archive with */
       /* header type 7.  If you encounter one, please try it and tell me   */

       local = ( ( pred + foll) * 15073) & 0xFFF;
    else {

       /* this uses the 'mid-square' algorithm. I.E. for a Amcmp_Hash val of n bits  */
       /* Amcmp_Hash = middle binary digits of ( key * key).  Upon collision, Amcmp_Hash    */
       /* searches down linked list of keys that hashed to that key already.   */
       /* It will NOT notice if the table is full. This must be handled        */
       /* elsewhere.                                                           */

       temp = ( pred + foll) | 0x0800;
       temp *= temp;
       local = ( UINT)( temp >> 6) & 0x0FFF;   /* middle 12 bits of result */
    }

    if ( !string_tab[local].used)
       return local;
    else {

       /* if collision has occured */

       /* a function called eolist used to be here.  tempnext is used */
       /* because a temporary variable was needed and tempnext in not */
       /* used till later on. */

       while ( 0 != ( tempnext = string_tab[local].next))
           local = tempnext;

       /* search for free entry from local + 101 */

       tempnext = ( local + 101) & 0x0FFF;
       ep = &( string_tab[tempnext]);   /* initialize pointer   */
       while ( ep->used) {
           ++tempnext;
           if ( tempnext == TABSIZE) {
               tempnext = 0;   /* handle wrap to beginning of table    */
               ep = string_tab;/* address of first element of table    */
           } else
               ++ep;           /* point to next element in table       */
       }

       /* put new tempnext into last element in collision list             */

       if ( update)             /* if update requested                  */
           string_tab[local].next = tempnext;
       return tempnext;
    }
}

static void FASTCALL Amcmp_InitTable( void )
{
    register UINT i;

    Amcmp_SetMemory( ( char FAR *) string_tab, ( char) 0, ( DWORD) sizeof( entry) * TABSIZE);
    for ( i = 0; i <= 255; i++) {
       Amcmp_UpdateTable( NO_PRED, i);
    }
}

static void FASTCALL Amcmp_UpdateTable( UINT pred, UINT foll)
{
    entry FAR *ep;      /* pointer to current entry */

    /* calculate offset just once */
    ep = &( string_tab[Amcmp_Hash( pred, ( BYTE)foll, UPDATE)]);
    ep->used = TRUE;
    ep->next = 0;
    ep->predecessor = pred;
    ep->follower = foll;
}

/* Amcmp_GetCode fills an input buffer of bits and returns the next 12 bits */
/* from that buffer with each call */

static int FASTCALL Amcmp_GetCode12( HFILE in)
{
    int localbuf, returnval;
    static UINT inbuf = EMPTY;

    if ( EMPTY == inbuf) {      /* On code boundary */
       if ( ( sizeleft - 2) < 0)
           return -1;
       sizeleft -= 2;
       Amcmp_GetInBlock( in, &localbuf, sizeof( char ) );
       localbuf &= 0xFF;
       Amcmp_GetInBlock( in, &inbuf, sizeof( char ) );
       inbuf &= 0xFF;
       returnval = ( ( localbuf << 4) & 0xFF0) + ( ( inbuf >> 4) & 0x00F);
       inbuf &= 0x000F;
    } else {                   /* buffer contains nibble H */
       if ( !sizeleft)
           return -1;
       sizeleft--;
       Amcmp_GetInBlock( in, &localbuf, sizeof( char ) );
       localbuf &= 0xFF;
       returnval = localbuf + ( ( inbuf << 8) & 0xF00);
       inbuf = EMPTY;
    }
    return returnval;
}

static void FASTCALL Amcmp_SetMemory( char FAR * mem, char value, DWORD size )
{
    register DWORD i;

    for ( i = 0; i < size; i++)
       mem[i] = value;
}

static void FASTCALL Amcmp_Push( UINT c )
{
  stack[sp] = ( ( char) c);     /* coerce passed integer into a character */
  ++sp;
  ASSERT( sp < STACKSIZE );
}

static UINT FASTCALL Amcmp_Pop( void )
{
  if ( sp > 0) {
    --sp;               /* Amcmp_Push leaves sp pointing to next empty slot   */
    return ( ( int) stack[sp] ); /* make sure Amcmp_Pop returns char           */
    }
  else
    return EMPTY;
}

static void FASTCALL Amcmp_AddCRC( char *cc, int i )
{
    for ( cc--; i--;)
       crc = ( ( crc >> 8) & 0x00ff) ^ unarccrctab[( crc ^ *++cc) & 0x00ff];
}

static int FASTCALL Amcmp_DlzwDecompress( HFILE in, HFILE out, char arctype )
{
    BYTE FAR *stackp;
    register int finchar;
    int oldcode;
    register int code;
    int incode;

    INITIALISE_PTR(htab);
    INITIALISE_PTR(codetab);
    if( !fNewMemory( &htab, sizeof( long) * HSIZE) )
       return( 1 );
    if( !fNewMemory( &codetab, sizeof( WORD) * HSIZE) )
       return( 1 );

    if ( arctype == 8) {                /* UnCrunch */
       hsize = 5003;

       /* every Crunched file must start with a byte equal to 12, */
       /* the maximum bit size of the pointer-length pair */

       if ( !sizeleft) {        /* no bytes in file */
           FreeMemory( &htab);
           FreeMemory( &codetab);
           return( 0 );
           }

         sizeleft--;
         max_bits = Amcmp_GetInByte( in );
         if ( 12 != max_bits ) {
              return( 1);
         }
    } else {                   /* UnSquash */
       max_bits = BITS;
       hsize = 9001;
    }
    maxmaxcode = 1 << max_bits;

    /* start of decompression */

    maxcode = MAXCODE( INIT_BITS);
    n_bits = INIT_BITS;

    for ( code = 255; code >= 0; code--) {
       tab_suffixof( code) = code;
    }
    free_ent = FIRST;
    incode = finchar = oldcode = Amcmp_GetCode( in);
    if ( oldcode == -1) {              /* EOF already? */
       FreeMemory( &codetab);
       FreeMemory( &htab);
       return( 0);                         /* Get out of here */
       }

    if ( arctype == 8)
       Amcmp_WriteRLE( ( char) incode, out);
    else {
       add1crc( ( char) incode);
       Amcmp_WriteOut( out, &incode, sizeof( char));
    }

    stackp = de_stack;

    while ( ( code = Amcmp_GetCode( in)) > -1) {
       if ( code == CLEAR) {
           for ( code = 255; code >= 0; code--)
               tab_prefixof( code) = 0;
           clear_flg = 1;
           free_ent = FIRST - 1;
           if ( ( code = Amcmp_GetCode( in)) == -1) {   /* O, untimely death! */
               break;
           }
       }
       incode = code;

       /* Special case for KwKwK string */

       if ( code >= free_ent) {
           *stackp++ = finchar;
           code = oldcode;
       }
       /* Generate output characters in reverse order Stop if input */
       /* code is in range 0..255 */

       while ( code >= 256) {
           *stackp++ = tab_suffixof( code);
           code = tab_prefixof( code);
       }
       *stackp++ = finchar = tab_suffixof( code);

       /* the following code for arctype 9 used to use memrev( ) to */
       /* reverse the order and then output using fread.  The following */
       /* method was tested to be faster */

       /* characters are read in reverse order from the stack ( like any */
       /* stack) and then output. */

       if ( arctype == 9) {
           do {
               add1crc( *--stackp);
               Amcmp_WriteOut( out, stackp, sizeof( char ) );
           } while ( stackp > de_stack);
       } else {                /* arctype==8  */
           do
               Amcmp_WriteRLE( *--stackp, out);
           while ( stackp > de_stack);
       }

       /* Generate the new entry */

       if ( ( code = free_ent) < maxmaxcode) {
           tab_prefixof( code) = ( WORD) oldcode;
           tab_suffixof( code) = finchar;
           free_ent = code + 1;
       }
       /* Remember previous code */

       oldcode = incode;
    }

    /* it's important to free all memory used, so Amcmp_Uncompress will run on systems */
    /* with limited RAM */

    FreeMemory( &htab);
    FreeMemory( &codetab);
   return( 0 );
}

static int FASTCALL Amcmp_StoreDecompressed( HFILE in, HFILE out)
{
    int c;
    char *buffer;

    /* first time initialization */

    INITIALISE_PTR(buffer);
    if( !fNewMemory( &buffer, sizeof( char) * MAXOUTSIZE) )
      {
       /* do char by char if no room for buffer */

       while ( !sizeleft) {
           sizeleft--;
           Amcmp_GetInBlock( in, &c, sizeof( char ) );
           add1crc( c);
           Amcmp_WriteOut( out, &c, sizeof( char ) );
       }
       return( 0);
    }
    while ( sizeleft >= MAXOUTSIZE) {
       if ( Amcmp_GetInBlock( in, buffer, MAXOUTSIZE) != MAXOUTSIZE)
           return( 1);
       Amcmp_AddCRC( buffer, MAXOUTSIZE);
       Amcmp_WriteOut( out, buffer, MAXOUTSIZE);
       sizeleft -= MAXOUTSIZE;
    }

    if ( Amcmp_GetInBlock( in, buffer, ( UINT)sizeleft) != ( UINT)sizeleft)
       return( 1);
    Amcmp_AddCRC( buffer, ( UINT)sizeleft);
    Amcmp_WriteOut( out, buffer, ( UINT)sizeleft);

    /* free the buffer before exiting */

    free( buffer);
    sizeleft = 0;
   return( 0 );
}

static void FASTCALL Amcmp_WriteOut( HFILE out, void FAR * data, size_t len )
{
   WORD count;

   if( !fBuf )
      {
      HGLOBAL hOutBuffer;

      dwOutBufSize = 0xFF00;
      if( hOutBuffer = GlobalAlloc( GHND, ( DWORD)dwOutBufSize ) )
         {
         lpOutBuffer = GlobalLock( hOutBuffer );
         dwOutBufCount = 0;
         }
      fBuf = TRUE;
      }
   if( !fBuf )
      _lwrite( out, data, len );
   else {
      if( dwOutBufCount + len >= dwOutBufSize )
         Amcmp_FlushOut( out );
#ifdef WIN32
      memcpy( ( LPSTR)lpOutBuffer + dwOutBufCount, data, len );
#else
      _fmemcpy( ( LPSTR)lpOutBuffer + dwOutBufCount, data, len );
#endif
      dwOutBufCount += len;
      }
   total += len;
   count = ( WORD)( (  100.0 / ( double)archead.length ) * ( double)total );
   if( count != old_count )
      {
      if( NULL != hwndStatus )
         SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, count, 0L );
      old_count = count;
      }
}

static void FASTCALL Amcmp_FlushOut( HFILE out )
{
   if( dwOutBufCount )
      {
      _lwrite( out, lpOutBuffer, LOWORD( dwOutBufCount ) );
      dwOutBufCount = 0;
      }
}

static void FASTCALL Amcmp_CloseOut( HFILE out )
{
   Amcmp_FlushOut( out );
   if( fBuf )
      {
      HGLOBAL hg;

#ifdef WIN32      
      hg = GlobalHandle( lpOutBuffer );
#else
      hg = ( HGLOBAL)LOWORD( GlobalHandle( SELECTOROF( lpOutBuffer ) ) );
#endif
      GlobalUnlock( hg );
      GlobalFree( hg );
      }
}

static int FASTCALL Amcmp_RLEDecompress( HFILE in, HFILE out)
{
    int c;
    char *buffer;

    INITIALISE_PTR(buffer);
    if( !fNewMemory( &buffer, sizeof( char) * MAXOUTSIZE) )
      {
       /* uncompress char by char if no room for buffer */

       while ( sizeleft) {
           sizeleft--;
           c = Amcmp_GetInByte( in );
           Amcmp_WriteRLE( ( BYTE)c, out);
       }
    }
    while ( sizeleft >= MAXOUTSIZE) {
      if( Amcmp_GetInBlock( in, buffer, MAXOUTSIZE ) != MAXOUTSIZE )
         return( 1);
       for ( c = 0; c != MAXOUTSIZE; c++)
           Amcmp_WriteRLE( buffer[c], out);
       sizeleft -= MAXOUTSIZE;
    }

    if ( Amcmp_GetInBlock( in, buffer, ( size_t)sizeleft) != ( size_t)sizeleft)
       return( 1);
    for ( c = 0; c != sizeleft; c++)
       Amcmp_WriteRLE( buffer[c], out);
    sizeleft = 0;
    free( buffer);
    return( 0);
}

static int FASTCALL Amcmp_SQDecompress( HFILE in, HFILE out)
{
    register int i;            /* generic loop index */
    register int bitpos;       /* last bit position read */
    int curbyte;               /* last byte value read */
    int numnodes;
    struct sq_tree FAR *dnode;

    /* Allocate memory for decoding tree
     */
    INITIALISE_PTR(dnode);
    if( !fNewMemory( &dnode, sizeof( struct sq_tree) * NUMVALS) )
       return( 1);

    /* get number of nodes in tree, this uses two character input calls */
    /* instead of one integer input call for speed */

    if ( !( sizeleft - 1) <= 0)
       numnodes = -1;
    sizeleft -= 2;
    numnodes = Amcmp_GetInByte( in );
    numnodes += Amcmp_GetInByte( in ) * 256;

    if ( ( numnodes < 0) || ( numnodes >= NUMVALS)) {
       return( 1);
    }
    /* initialize for possible empty tree ( SQEOF only) */

    dnode[0].children[0] = -( SQEOF + 1);
    dnode[0].children[1] = -( SQEOF + 1);

    for ( i = 0; i < numnodes; ++i) {   /* get decoding tree from file */
      ASSERT( sizeleft - 3 > 0 );
      sizeleft -= 4;
      dnode[i].children[0] = Amcmp_GetInByte( in );
      dnode[i].children[0] += Amcmp_GetInByte( in ) * 256;
      dnode[i].children[1] = Amcmp_GetInByte( in );
      dnode[i].children[1] += Amcmp_GetInByte( in ) * 256;
    }

    bitpos = 8;                        /* set to above read condition */

   curbyte = 0;
   while ( i != -1)
      {
      for ( i = 0; i >= 0;)
         {  /* traverse tree               */
         if ( ++bitpos > 7)
            {
            if ( !sizeleft)
               {
               FreeMemory( &dnode);
               return( 0);
               }
            sizeleft--;
            curbyte = Amcmp_GetInByte( in );
            bitpos = 0;
            i = dnode[i].children[1 & curbyte];
            }
         else
            i = dnode[i].children[1 & ( curbyte >>= 1)];
         }

      /* decode fake node index to original data value */
      i = -( i + 1);

      /* decode special endfile token to normal EOF */
      i = ( i == SQEOF) ? -1 : i;
      if ( i != -1)
      Amcmp_WriteRLE( ( BYTE)i, out);
      }

   /* free up decoding table for later use */
   FreeMemory( &dnode);
   return( 0);
}

static int FASTCALL Amcmp_GetCode( HFILE in )
{
    static char iobuf[BITS];
    int code;
    static int offset = 0;
    static int size = 0;
    register int r_off;
    int bits;
    BYTE *bp = iobuf;

    static BYTE rmask[9] = {  /* for use with Amcmp_GetCode( ) */
       0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
    };

    if ( clear_flg > 0 || offset >= size || free_ent > maxcode) {

       /* set size to a register variable.  Since size is used often, and */
       /* only two registers may be defined in MSC, I'm using r_off instead */
       /* of size, because r_off is a register variable. */

       r_off = size;

       /* If the next entry will be too big for the current code */
       /* size, then we must increase the size.  This implies */
       /* reading a new buffer full, too. */

       if ( free_ent > maxcode) {
           n_bits++;
           if ( n_bits == max_bits)
               maxcode = maxmaxcode;   /* won't get any bigger now */
           else
               maxcode = MAXCODE( n_bits);
       }
       if ( clear_flg > 0) {
           maxcode = MAXCODE( INIT_BITS);
           n_bits = INIT_BITS;
           clear_flg = 0;
       }
       for ( r_off = 0; r_off < n_bits; r_off++) {
           if ( !sizeleft)
               break;          /* if EOF */
           sizeleft--;
           code = Amcmp_GetInByte( in );
           iobuf[r_off] = code;
       }

       if ( r_off <= 0)
           return -1;          /* end of file */

       offset = 0;

       /* Round size down to integral number of codes */

       r_off = ( r_off << 3) - ( n_bits - 1);

       size = r_off;           /* set size back to r_off */
    }
    r_off = offset;
    bits = n_bits;

    /* Get to the first byte. */

    bp += ( r_off >> 3);
    r_off &= 7;

    /* Get first part ( low order bits) */

    code = ( *bp++ >> r_off);
    bits -= ( 8 - r_off);
    r_off = 8 - r_off;         /* now, offset into code word */

    /* Get any 8 bit parts in the middle ( <=1 for up to 16 bits). */

    if ( bits >= 8) {
       code |= *bp++ << r_off;
       r_off += 8;
       bits -= 8;
    }
    /* high order bits. */

    code |= ( *bp & rmask[bits]) << r_off;
    offset += n_bits;

    return code;
}
