/* GIF.C - Handles reading GIF files
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

#define NOCOMM
#define NOKANJI

#include "warnings.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include "amlib.h"
#include "amuser.h"
#include "dib.h"

#define  FASTCALLS   01
#define  GET_BYTES   01
#define  PAL_SHIFT   0

/*
** Local functions
*/
static short NEAR decoder(short linewidth);
static short      init_exp(short  size);
static short NEAR get_next_code(void);
static void       inittable(void);
static void       close_file(void);

/*
** Local variables
*/
static int                 rowcount;      /* row counter for screen */
static HNDFILE             hin;
static unsigned int        height;

static unsigned char       dacbox[256][3];         /* Video-DAC (filled in by SETVIDEO) */

static unsigned char       decoderline[2049];      /* write-line routines use this */
static unsigned char       win_andmask[8];
static unsigned char       win_notmask[8];
static unsigned char       win_bitshift[8];

static int                 xdots, ydots, colors;
static int                 win_xdots, win_ydots;
static unsigned char HUGEPTR  *pixels;                /* the device-independent bitmap pixels */
static int                 pixels_per_byte;        /* pixels/byte in the pixmap */
static long                pixels_per_bytem1;      /* pixels / byte - 1 (for ANDing) */
static int                 pixelshift_per_byte;    /* 0, 1, 2, or 3 */
static int                 bytes_per_pixelline;    /* pixels/line / pixels/byte */
static long                win_bitmapsize;         /* bitmap size, in bytes */
static int                 bad_code_count = 0;     /* needed by decoder module */
static BYTE                *cbuf = NULL;
static unsigned int        gbuf = 0;
static HANDLE              hcbuf = NULL;
static unsigned int        gbn = 0;

#define  cbufsize (32u * 1024u)     // (16u * 1024u)

static int NEAR
get_byte()
{
   if (gbn == 0)
   {
      gbn = Amfile_Read(hin,cbuf,cbufsize);
      gbuf = 0;
   }

   if (gbn)
   {
      --gbn;
      return (cbuf[gbuf++]) & 0x00ff;
   }
   else
      return (-1);
}

#if   GET_BYTES
static int NEAR get_bytes(unsigned int count, unsigned char *dest)
{
   int   count2 = count;

   if (count <= 0)
   {
      return count;
   }

   while (count)
   {
      if (gbn == 0)
      {
         gbn = Amfile_Read(hin,cbuf,cbufsize);
         gbuf = 0;
         if (!gbn)
            return -1;
      }

      if (gbn >= count)
      {
         if (dest != NULL)
         {
            memcpy(dest, cbuf + gbuf, count);
         }
         gbuf += count;
         gbn -= count;
         count = 0;
      }
      else
      {
         if (dest != NULL)
         {
            memcpy(dest, cbuf + gbuf, gbn);
         }
         count -= gbn;
         dest += gbn;
         gbn = 0;
      }
   }

   return count2;
}
#endif

static void NEAR 
putcolor(int x, int y, int color)
{
   long i;

   i = win_ydots - 1 - y;
   i = (i * win_xdots) + x;

   if (x >= 0 && x < xdots && y >= 0 && y < ydots)
   {
      if (pixelshift_per_byte == 0)
      {
         pixels[i] = color % colors;
      }
      else
      {
         unsigned int j;

         j = (unsigned int)( i & (unsigned int)pixels_per_bytem1 );
         i = i >> pixelshift_per_byte;
         pixels[i] = (pixels[i] & win_notmask[j]) + 
             (((unsigned char) (color % colors)) << win_bitshift[j]);
      }
   }
}

static int NEAR 
put_line(int rownum, int leftpt, int rightpt, unsigned char *localvalues)
{
   int   i, len;
   long  startloc;

   len = rightpt - leftpt;
   if (rightpt >= xdots)
   {
      len = xdots - leftpt;
   }
   startloc = win_ydots - 1 - rownum;
   startloc = (startloc * win_xdots) + leftpt;

   if (rownum < 0 || rownum >= ydots || leftpt < 0)
   {
      return (0);
   }

   if (pixelshift_per_byte == 0)
   {
      unsigned char HUGEPTR   *ptr = pixels + startloc;

#ifdef WIN32
      memcpy(ptr, localvalues, len);
#else
      hmemcpy(ptr, localvalues, len);
#endif
   }
   else
   {
      unsigned int   j;
      long           k;

      for (i = 0 ;  i <= len ;  i++)
      {
         k = startloc + i;
         j = (unsigned int)( k & (unsigned int)pixels_per_bytem1 );
         k = k >> pixelshift_per_byte;
         pixels[k] = (pixels[k] & win_notmask[j]) + 
             (((unsigned char) (localvalues[i] % colors)) << win_bitshift[j]);
      }
   }
   putcolor(leftpt, rownum, localvalues[0]);

   return (1);
}

/*
** Main entry decoder
*/

HBITMAP EXPORT WINAPI Amuser_LoadGIFFromDisk( LPSTR filename, HPALETTE FAR * phPal )
{
   HANDLE               dib = (HANDLE)NULL, hbiCurrent;
   HBITMAP hBmp;
   unsigned             numcolors;
   unsigned char        buffer[16];
   short             width, finished;
   LPBITMAPINFOHEADER   lpbi;
   RGBQUAD FAR          *pRgb;
   int                  status = 0;
   int                  i, planes;
   DWORD                dwBits, dwLen;

   /* initialize the row count for write-lines */
   rowcount = 0;
   *phPal = NULL;

   /* zero out the full write-line */
   for (width = 0 ;  width < 2049 ;  width++)
      decoderline[width] = 0;

   /* Open the file -- changed to OpenFile by mh */
   hin = Amfile_Open( filename, AOF_READ);
   if (hin == HNDFILE_ERROR)
   {
      return FALSE;
   }

   hcbuf = GlobalAlloc(GHND,cbufsize);
   cbuf = (BYTE FAR *)GlobalLock(hcbuf);
   if (cbuf == NULL)
   {
      close_file();
      return FALSE;
   }
   gbn = 0;
   gbuf = 0;
   hBmp = NULL;

   /* Get the screen description */
   for (i = 0 ; i < 13 ; i++)
   {
      buffer[i] = (unsigned char) status = get_byte();
      if (status < 0)
      {
         close_file();
         return FALSE;
      }
   }

   if (strncmp((char *)buffer, "GIF87a", 3) ||  /* use updated GIF specs */
       buffer[3] < '0' || buffer[3] > '9' || 
       buffer[4] < '0' || buffer[4] > '9' || 
       buffer[5] < 'A' || buffer[5] > 'z')
   {
      close_file();
      return FALSE;
   }

   planes = (buffer[10] & 0xF) + 1;

   if ((buffer[10] & 0x80) == 0) /* color map (better be!) */
   {
      close_file();
      return FALSE;
   }
   numcolors = 1 << planes;

#if   GET_BYTES
   if (numcolors * 3 == (unsigned int)get_bytes(numcolors * 3, (unsigned char *)dacbox))
   {
#if   PAL_SHIFT
      for (i = 0 ; i < numcolors ; ++i)
      {
         for (j = 0 ; j < 3 ; ++j)
         {
            dacbox[i][j] >>= 2;
         }
      }
#endif
   }
   else
   {
      close_file();
      return FALSE;
   }
#else
   for (i = 0 ;  i < numcolors ;  i++)
   {
      for (j = 0 ;  j < 3 ;  j++)
      {
         status = get_byte();
         if (status < 0)
         {
            close_file();
            return FALSE;
         }

         dacbox[i][j] = (status & 0x00ff) >> 2;
      }
   }
#endif

   /* Now display one or more GIF objects */
   finished = 0;
   while (!finished)
   {
      switch (get_byte())
      {
         case ';':            /* End of the GIF dataset */
            finished = 1;
            status = 0;
            break;

         case '!':            /* GIF Extension Block */
            get_byte();    /* read (and ignore) the ID */
            while ((i = get_byte()) > 0)  /* get the data length */
#if   GET_BYTES
               get_bytes(i, NULL);
#else
               for (j = 0;  j < i;  j++)
                  get_byte(); /* flush the data */
#endif
            break;

         case ',':
            /*
             * Start of an image object. Read the image description.
             */

#if   GET_BYTES
            if (9 != get_bytes(9, buffer))
            {
               status = -1;
            }
            else
            {
               status = 0;
            }
#else
            for (i = 0;  i < 9;  i++)
            {
               buffer[i] = (unsigned char) status = get_byte();
               if (status < 0)
               {
                  status = -1;
                  break;
               }
            }
#endif
            if (status < 0)
            {
               finished = 1;
               break;
            }

            width = buffer[4] | (buffer[5] << 8);
            height = buffer[6] | (buffer[7] << 8);
            // fill in DIB stuff
            xdots = width;
            ydots = height;
            colors = numcolors;
            if (colors > 16)
               colors = 256;
            if (colors > 2 && colors < 16)
               colors = 16;
            win_xdots = (xdots + 3) & 0xFFFC;
            win_ydots = ydots;
            pixelshift_per_byte = 0;
            pixels_per_byte = 1;
            pixels_per_bytem1 = 0;
            if (colors == 16)
            {
               win_xdots = (xdots + 7) & 0xFFF8;
               pixelshift_per_byte = 1;
               pixels_per_byte = 2;
               pixels_per_bytem1 = 1;
               win_andmask[0] = 0xF0;
               win_notmask[0] = 0xF;
               win_bitshift[0] = 4;
               win_andmask[1] = 0xF;
               win_notmask[1] = 0xF0;
               win_bitshift[1] = 0;
            }
            if (colors == 2)
            {
               win_xdots = (xdots + 31) & 0xFFE0;
               pixelshift_per_byte = 3;
               pixels_per_byte = 8;
               pixels_per_bytem1 = 7;
               win_andmask[0] = 0x80;
               win_notmask[0] = 0x7F;
               win_bitshift[0] = 7;
               for (i = 1 ;  i < 8 ;  i++)
               {
                  win_andmask[i] = win_andmask[i - 1] >> 1;
                  win_notmask[i] = (win_notmask[i - 1] >> 1) + 0x80;
                  win_bitshift[i] = win_bitshift[i - 1] - 1;
               }
            }
            bytes_per_pixelline = win_xdots >> pixelshift_per_byte;

            hbiCurrent = GlobalAlloc(GHND, (LONG) sizeof(BITMAPINFOHEADER) + 
               colors * sizeof(RGBQUAD));
            if (!hbiCurrent)
               return 0;

            lpbi = (VOID FAR *) GlobalLock(hbiCurrent);
            lpbi->biSize = sizeof(BITMAPINFOHEADER);
            lpbi->biWidth = width;
            lpbi->biHeight = height;
            lpbi->biPlanes = 1;  //nb: NOT equal to planes from GIF
            lpbi->biBitCount = 8 / pixels_per_byte;
            lpbi->biCompression = BI_RGB;
            dwBits = 
               lpbi->biSizeImage = (DWORD) bytes_per_pixelline * win_ydots;
            lpbi->biXPelsPerMeter = 0;
            lpbi->biYPelsPerMeter = 0;
            lpbi->biClrUsed = colors;
            lpbi->biClrImportant = colors;

            win_bitmapsize = (((long) win_xdots * (long) win_ydots) >> pixelshift_per_byte) + 1;

               /* fill in intensities for all palette entry colors */

            pRgb = (RGBQUAD FAR *) ((LPSTR) lpbi + (unsigned int)lpbi->biSize);
            for (i = 0;  i < colors;  i++)
            {
#if   PAL_SHIFT
               pRgb[i].rgbRed = ((BYTE) dacbox[i][0]) << 2;
               pRgb[i].rgbGreen = ((BYTE) dacbox[i][1]) << 2;
               pRgb[i].rgbBlue = ((BYTE) dacbox[i][2]) << 2;
#else
               pRgb[i].rgbRed = ((BYTE) dacbox[i][0]);
               pRgb[i].rgbGreen = ((BYTE) dacbox[i][1]);
               pRgb[i].rgbBlue = ((BYTE) dacbox[i][2]);
#endif
            }
            *phPal = CreateDibPalette(hbiCurrent);
            dwLen = lpbi->biSize + (DWORD) colors * sizeof(RGBQUAD) + dwBits;
            dib = GlobalAlloc(GHND, dwLen > 32767 ? dwLen : 32767 );
            if (!dib)
            {
               finished = 1;
               status = -1;
               break;
            }
            pixels = (unsigned char HUGEPTR *) GlobalLock(dib);
         #ifdef WIN32
            memcpy(pixels, lpbi, (size_t) (dwLen - dwBits));
         #else
            hmemcpy(pixels, lpbi, (size_t) (dwLen - dwBits));
         #endif
            GlobalUnlock(hbiCurrent);
            GlobalFree(hbiCurrent);
            pixels += dwLen - dwBits;

            decoder(width);   //this does the grunt work

            GlobalUnlock(dib);
            if( dib )
               hBmp = DIBToBitmap( dib, *phPal );
            status = 0;
            finished = 1;
            break;

         default:
            status = -1;
            finished = 1;
            break;
      }
   }

   close_file();

   return (status == 0) ? hBmp : 0;
}

static void
close_file()
{
   Amfile_Close(hin);  
   if (hcbuf)
   {
      GlobalUnlock(hcbuf);
      GlobalFree(hcbuf);
      cbuf = NULL;
      hcbuf = NULL;
   }
}


/* DECODE.C - An LZW decoder for GIF
 * Copyright (C) 1987, by Steven A. Bennett
 *
 * Permission is given by the author to freely redistribute and include
 * this code in any program as long as this credit is given where due.
 *
 * In accordance with the above, I want to credit Steve Wilhite who wrote
 * the code which this is heavily inspired by...
 *
 * GIF and 'Graphics Interchange Format' are trademarks (tm) of
 * Compuserve, Incorporated, an H&R Block Company.
 *
 * Release Notes: This file contains a decoder routine for GIF images
 * which is similar, structurally, to the original routine by Steve Wilhite.
 * It is, however, somewhat noticably faster in most cases.
 *
 == This routine was modified for use in FRACTINT in two ways.
 ==
 == 1) The original #includes were folded into the routine strictly to hold
 ==    down the number of files we were dealing with.
 ==
 == 2) The 'stack', 'suffix', 'prefix', and 'buf' arrays were changed from
 ==    static and 'malloc()'ed to external only so that the assembler
 ==    program could use the same array space for several independent
 ==    chunks of code.  Also, 'stack' was renamed to 'dstack' for TASM
 ==    compatibility.
 ==
 == 3) The 'out_line()' external function has been changed to reference
 ==    '*outln()' for flexibility (in particular, 3D transformations)
 ==
 == 4) A call to 'keypressed()' has been added after the 'outln()' calls
 ==    to check for the presenc of a key-press as a bail-out signal
 ==
 == (Bert Tyler and Timothy Wegner)
 */
#define LOCAL static
#define IMPORT extern

#define FAST register

typedef unsigned short UWORD;
typedef char TEXT;
typedef unsigned char UTINY;
typedef unsigned long ULONG;
typedef int INT;


/* Various error codes used by decoder
 * and my own routines...   It's okay
 * for you to define whatever you want,
 * as long as it's negative...  It will be
 * returned intact up the various subroutine
 * levels...
 */
#define OUT_OF_MEMORY -10
#define BAD_CODE_SIZE -20
#define READ_ERROR -1
#define WRITE_ERROR -2
#define OPEN_ERROR -3
#define CREATE_ERROR -4



/* whups, here are more globals, added by PB: */
static INT skipxdots; /* 0 to get every dot, 1 for every 2nd, 2 every 3rd, ... */
static INT skipydots; /* ditto for rows */

#define MAX_CODES   4095

/* Static variables */
LOCAL short curr_size;          /* The current code size */
LOCAL short clear;           /* Value for a clear code */
LOCAL short ending;          /* Value for a ending code */
LOCAL short newcodes;           /* First available code */
LOCAL short top_slot;           /* Highest code for current size */
LOCAL short slot;         /* Last read code */

/* The following static variables are used
 * for seperating out codes
 */
LOCAL short navail_bytes = 0;      /* # bytes left in block */
LOCAL short nbits_left = 0;        /* # bits left in current byte */
LOCAL UTINY b1;           /* Current byte */
LOCAL UTINY byte_buff[257];        /* Current block, reuse shared mem */
LOCAL UTINY *pbytes;         /* Pointer to next byte in block */

LOCAL LONG code_mask[13] = {
     0,
     0x0001, 0x0003,
     0x0007, 0x000F,
     0x001F, 0x003F,
     0x007F, 0x00FF,
     0x01FF, 0x03FF,
     0x07FF, 0x0FFF
     };


/* This function initializes the decoder for reading a new image.
 */

LOCAL short init_exp(short size)
{
   curr_size = size + 1;
   top_slot = 1 << curr_size;
   clear = 1 << size;
   ending = clear + 1;
   slot = newcodes = ending + 1;
   navail_bytes = nbits_left = 0;

   return (0);
}

#define  GET_BLOCK   01

#if   GET_BLOCK
static int NEAR
get_block(void)
{
   register int   count;
   register char  *dest = byte_buff;

   count = get_byte();
   if (count < 0)
   {
      return count;
   }
   navail_bytes = count;

   while (count)
   {
      if (gbn == 0)
      {
         gbn = Amfile_Read(hin,cbuf,cbufsize);
         gbuf = 0;

         if (!gbn)
            return -1;
      }

      if (gbn >= (unsigned int)count)
      {
         memcpy(dest, cbuf + gbuf, count);
         gbuf += count;
         gbn -= count;
         count = 0;
      }
      else
      {
         memcpy(dest, cbuf + gbuf, gbn);
         count -= gbn;
         dest += gbn;
         gbn = 0;
      }
   }

   return navail_bytes;
}
#endif

/* get_next_code()
 * - gets the next code from the GIF file.  Returns the code, or else
 * a negative number in case of file errors...
 */

LOCAL short NEAR
get_next_code(void)
{
   ULONG ret;

   if (nbits_left == 0)
   {
      if (navail_bytes <= 0)
      {
         /*
         ** Out of bytes in current block, so read next block
         */
         pbytes = byte_buff;
#if GET_BLOCK
         if ((navail_bytes = get_block()) < 0)
            return (navail_bytes);
#else
         if ((navail_bytes = get_byte()) < 0)
            return (navail_bytes);
         else if (navail_bytes)
         {
            for (i = 0 ; i < navail_bytes ; ++i)
            {
               if ((x = get_byte()) < 0)
                  return (x);
               byte_buff[i] = x;
            }
         }
#endif
      }
      b1 = *pbytes++;
      nbits_left = 8;
      --navail_bytes;
   }

   ret = b1 >> (8 - nbits_left);
   while (curr_size > nbits_left)
   {
      if (navail_bytes <= 0)
      {
         /*
         ** Out of bytes in current block, so read next block
         */
         pbytes = byte_buff;
#if   GET_BLOCK
         if ((navail_bytes = get_block()) < 0)
            return (navail_bytes);
#else
         if ((navail_bytes = get_byte()) < 0)
            return (navail_bytes);
         else if (navail_bytes)
         {
            for (i = 0 ; i < navail_bytes ; ++i)
            {
               if ((x = get_byte()) < 0)
                  return (x);
               byte_buff[i] = x;
            }
         }
#endif
      }
      b1 = *pbytes++;
      ret |= b1 << nbits_left;
      nbits_left += 8;
      --navail_bytes;
   }
   nbits_left -= curr_size;
   ret &= code_mask[curr_size];
   return ((short) (ret));
}


/* The reason we have these seperated like this instead of using
 * a structure like the original Wilhite code did, is because this
 * stuff generally produces significantly faster code when compiled...
 * This code is full of similar speedups...  (For a good book on writing
 * C for speed or for space optomisation, see Efficient C by Tom Plum,
 * published by Plum-Hall Associates...)
 */

#define  NEAR_STACK  0
#define  ALLOC_STACK 0

#if   !ALLOC_STACK   /* Moved into decoder() - lw */
/*
I removed the LOCAL identifiers in the arrays below and replaced them
with 'extern's so as to declare (and re-use) the space elsewhere.
The arrays are actually declared in the assembler source.
                  Bert Tyler

I put them back -- m heller
*/

LOCAL UTINY dstack[MAX_CODES + 1];        /* Stack for storing pixels */
LOCAL UTINY suffix[MAX_CODES + 1];        /* Suffix table */
LOCAL UWORD prefix[MAX_CODES + 1];        /* Prefix linked list */
#endif

/* short decoder(linewidth)
 *    short linewidth;         * Pixels per line of image *
 *
 * - This function decodes an LZW image, according to the method used
 * in the GIF spec.  Every *linewidth* "characters" (ie. pixels) decoded
 * will generate a call to out_line(), which is a user specific function
 * to display a line of pixels.  The function gets its codes from
 * get_next_code() which is responsible for reading blocks of data and
 * seperating them into the proper size codes.  Finally, get_byte() is
 * the global routine to read the next byte from the GIF file.
 *
 * It is generally a good idea to have linewidth correspond to the actual
 * width of a line (as specified in the Image header) to make your own
 * code a bit simpler, but it isn't absolutely necessary.
 *
 * Returns: 0 if successful, else negative.  (See ERRS.H)
 *
 */
static short NEAR decoder(short linewidth)
{
#if   NEAR_STACK
   FAST UTINY  _ds *sp;
   FAST UTINY  _ds *bufptr;
   UTINY       _ds *buf;
#else
   FAST UTINY  *sp;
   FAST UTINY  *bufptr;
   UTINY       *buf;
#endif
   FAST short  code, fc, oc, bufcnt;
   short       c, size, ret;
   short       xskip, yskip;
#if   ALLOC_STACK
   UTINY       *dstack;       /* Stack for storing pixels */
   UTINY       *suffix;       /* Suffix table */
   UWORD       *prefix;       /* Prefix linked list */
#endif

   /* Initialize for decoding a new image...
    */
   if ((size = get_byte()) < 0)
      return (size);
   if (size < 2 || 9 < size)
      return (BAD_CODE_SIZE);
   init_exp(size);
   xskip = yskip = 0;

#if   ALLOC_STACK
   /*
   ** Allocate space for tables and stack
   */
   dstack = (UTINY *)malloc((2 * sizeof(UTINY) + sizeof(UWORD)) * (MAX_CODES + 1));
   suffix = (UTINY *)(((char *)dstack) + sizeof(UTINY) * (MAX_CODES + 1));
   prefix = (UWORD *)(((char *)suffix) + sizeof(UTINY) * (MAX_CODES + 1));
#endif

   /* Initialize in case they forgot to put in a clear code.
    * (This shouldn't happen, but we'll try and decode it anyway...)
    */
   oc = fc = 0;

   buf = decoderline;

   /* Set up the stack pointer and decode buffer pointer
    */
   sp = dstack;
   bufptr = buf;
   bufcnt = linewidth;

   /* This is the main loop.  For each code we get we pass through the
    * linked list of prefix codes, pushing the corresponding "character" for
    * each code onto the stack.  When the list reaches a single "character"
    * we push that on the stack too, and then start unstacking each
    * character for output in the correct order.  Special handling is
    * included for the clear code, and the whole thing ends when we get
    * an ending code.
    */

   while ((c = get_next_code()) != ending)
   {

      /* If we had a file error, return without completing the decode
       */
      if (c < 0)
      {
#if   ALLOC_STACK
         free(dstack);
#endif
         return (0);
      }

      /* If the code is a clear code, reinitialize all necessary items.
       */
      if (c == clear)
      {
         curr_size = size + 1;
         slot = newcodes;
         top_slot = 1 << curr_size;

    /* Continue reading codes until we get a non-clear code
     * (Another unlikely, but possible case...)
     */
         while ((c = get_next_code()) == clear)
            ;

    /* If we get an ending code immediately after a clear code
     * (Yet another unlikely case), then break out of the loop.
     */
         if (c == ending)
            break;

    /* Finally, if the code is beyond the range of already set codes,
     * (This one had better NOT happen... I have no idea what will
     * result from this, but I doubt it will look good...) then set it
     * to color zero.
     */
         if (c >= slot)
            c = 0;

         oc = fc = c;

    /* And let us not forget to put the char into the buffer... */
         *sp++ = (unsigned char)c;     /* let the common code outside the if else stuff it */
      }
      else 
      {

    /* In this case, it's not a clear code or an ending code, so
     * it must be a code code...  So we can now decode the code into
     * a stack of character codes. (Clear as mud, right?)
     */
         code = c;

    /* Here we go again with one of those off chances...  If, on the
     * off chance, the code we got is beyond the range of those already
     * set up (Another thing which had better NOT happen...) we trick
     * the decoder into thinking it actually got the last code read.
     * (Hmmn... I'm not sure why this works...  But it does...)
     */
         if (code >= slot)
         {
            if (code > slot)
               ++bad_code_count;
            code = oc;
            *sp++ = (unsigned char)fc;
         }

    /* Here we scan back along the linked list of prefixes, pushing
     * helpless characters (ie. suffixes) onto the stack as we do so.
     */
         while (code >= newcodes)
         {
            *sp++ = suffix[code];
            code = prefix[code];
         }

    /* Push the last character on the stack, and set up the new
     * prefix and suffix, and if the required slot number is greater
     * than that allowed by the current bit size, increase the bit
     * size.  (NOTE - If we are all full, we *don't* save the new
     * suffix and prefix...  I'm not certain if this is correct...
     * it might be more proper to overwrite the last code...
     */
         *sp++ = (unsigned char)code;
         if (slot < top_slot)
         {
            fc = code;
            suffix[slot] = (unsigned char)fc;;
            prefix[slot++] = oc;
            oc = c;
         }
         if (slot >= top_slot)
            if (curr_size < 12)
            {
               top_slot <<= 1;
               ++curr_size;
            }
      }

      /* Now that we've pushed the decoded string (in reverse order)
       * onto the stack, lets pop it off and put it into our decode
       * buffer...  And when the decode buffer is full, write another
       * line...
       */
      while (sp > dstack)
      {
         --sp;
         if (--xskip < 0)
         {
            xskip = skipxdots;
            *bufptr++ = *sp;
         }
         if (--bufcnt == 0)   /* finished an input row? */
         {
            if (--yskip < 0)
            {
//             if ((ret = (*outln)(buf, bufptr - buf)) < 0)
//             if ((ret = out_line(buf, bufptr - buf)) < 0)
               if ((ret = put_line(rowcount++, 0, bufptr - buf, buf)) < 0)
                  return (ret);
               yskip = skipydots;
            }
            bufptr = buf;
            bufcnt = linewidth;
            xskip = 0;
         }
      }
   }

#if   ALLOC_STACK
   free(dstack);
#endif

   /* PB note that if last line is incomplete, we're not going to try
      to emit it;  original code did, but did so via out_line and therefore
      couldn't have worked well in all cases... */
   return (0);
}
