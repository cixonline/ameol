/* DES.C - DES encryption engine
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
#include "pdefs.h"
#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include "amuser.h"

#ifndef NULL
#define  NULL  0
#endif

#ifdef WIN32X
#define  _fmalloc(x)          malloc((x))
#define  _ffree(x)            free((x))
#define  _fmemset(x,y,z)      memset((x),(y),(z))
#define  _fmemcpy(s,c,d)      memcpy((s),(c),(d))
#endif

static void FASTCALL permute( char FAR *, char FAR [16][16][8], char FAR *);
static void FASTCALL perminit( char FAR [16][16][8], char[64] );
static void FASTCALL spinit( void );
static long FASTCALL f(long, char *);
static int FASTCALL desinit(int);
static void FASTCALL desdone(void);
static int FASTCALL setkey( char (*)[8], char *);
static int FASTCALL endes( char (*)[8], char FAR *);
static int FASTCALL dedes( char (*)[8], char FAR *);

/* Tables defined in the Data Encryption Standard documents */
/* initial permutation IP */
static char ip[] = {
   58, 50, 42, 34, 26, 18, 10,  2,
   60, 52, 44, 36, 28, 20, 12,  4,
   62, 54, 46, 38, 30, 22, 14,  6,
   64, 56, 48, 40, 32, 24, 16,  8,
   57, 49, 41, 33, 25, 17,  9,  1,
   59, 51, 43, 35, 27, 19, 11,  3,
   61, 53, 45, 37, 29, 21, 13,  5,
   63, 55, 47, 39, 31, 23, 15,  7
};

/* final permutation IP^-1 */
static char fp[] = {
   40,  8, 48, 16, 56, 24, 64, 32,
   39,  7, 47, 15, 55, 23, 63, 31,
   38,  6, 46, 14, 54, 22, 62, 30,
   37,  5, 45, 13, 53, 21, 61, 29,
   36,  4, 44, 12, 52, 20, 60, 28,
   35,  3, 43, 11, 51, 19, 59, 27,
   34,  2, 42, 10, 50, 18, 58, 26,
   33,  1, 41,  9, 49, 17, 57, 25
};

/* permuted choice table (key) */
static char pc1[] = {
   57, 49, 41, 33, 25, 17,  9,
    1, 58, 50, 42, 34, 26, 18,
   10,  2, 59, 51, 43, 35, 27,
   19, 11,  3, 60, 52, 44, 36,

   63, 55, 47, 39, 31, 23, 15,
    7, 62, 54, 46, 38, 30, 22,
   14,  6, 61, 53, 45, 37, 29,
   21, 13,  5, 28, 20, 12,  4
};

/* number left rotations of pc1 */
static char totrot[] = {
   1,2,4,6,8,10,12,14,15,17,19,21,23,25,27,28
};

/* permuted choice key (table) */
static char pc2[] = {
   14, 17, 11, 24,  1,  5,
    3, 28, 15,  6, 21, 10,
   23, 19, 12,  4, 26,  8,
   16,  7, 27, 20, 13,  2,
   41, 52, 31, 37, 47, 55,
   30, 40, 51, 45, 33, 48,
   44, 49, 39, 56, 34, 53,
   46, 42, 50, 36, 29, 32
};

/* The (in)famous S-boxes */
static char si[8][64] = {
   /* S1 */
   14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
    0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
    4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
   15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13,

   /* S2 */
   15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
    3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
    0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
   13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9,

   /* S3 */
   10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
   13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
   13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
    1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12,

   /* S4 */
    7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
   13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
   10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
    3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14,

   /* S5 */
    2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
   14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
    4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
   11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3,

   /* S6 */
   12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
   10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
    9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
    4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13,

   /* S7 */
    4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
   13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
    1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
    6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12,

   /* S8 */
   13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
    1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
    7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
    2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
};

/* 32-bit permutation function P used on the output of the S-boxes */
static char p32i[] = {  
   16,  7, 20, 21,
   29, 12, 28, 17,
    1, 15, 23, 26,
    5, 18, 31, 10,
    2,  8, 24, 14,
   32, 27,  3,  9,
   19, 13, 30,  6,
   22, 11,  4, 25
};
/* End of DES-defined tables */

/* Lookup tables initialized once only at startup by desinit() */
static long (FAR *sp)[64];    /* Combined S and P boxes */

static char (FAR *iperm)[16][8]; /* Initial and final permutations */
static char (FAR *fperm)[16][8];


/* bit 0 is left-most in byte */
static int bytebit[] = { 0200, 0100, 040, 020, 010, 04, 02, 01 };

static int nibblebit[] = { 010, 04, 02, 01 };
static int desmode;

BOOL WINAPI EXPORT Amuser_Encrypt( LPSTR pStr, LPBYTE lpKey )
{
   char ks[16][8];
   register int c;

   if( desinit( 0 ) == -1 )
      return( FALSE );
   setkey( ks, lpKey );
   for( c = 0; c < 5; ++c )
      endes( ks, pStr + ( sizeof( char ) * ( c * 8 ) ) );
   desdone();
   return( TRUE );
}

BOOL WINAPI EXPORT Amuser_Decrypt( LPSTR pStr, LPBYTE lpKey )
{
   char ks[16][8];
   register int c;

   if( desinit( 0 ) == -1 )
      return( FALSE );
   setkey( ks, lpKey );
   for( c = 0; c < 5; ++c )
      dedes( ks, pStr + ( sizeof( char ) * ( c * 8 ) ) );
   desdone();
   return( TRUE );
}

/* Allocate space and initialize DES lookup arrays
 * mode == 0: standard Data Encryption Algorithm
 * mode == 1: DEA without initial and final permutations for speed
 */
int FASTCALL desinit(int mode)
{
   if(sp != NULL){
      /* Already initialized */
      return 0;
   }
   desmode = mode;
   
   if((sp = (long (FAR *)[64])_fmalloc(sizeof(long) * 8 * 64)) == NULL){
      return 0;
   }
   spinit();
   if(mode == 1)  /* No permutations */
      return 0;

   iperm = (char (FAR *)[16][8])_fmalloc(sizeof(char) * 16 * 16 * 8);
   if(iperm == NULL){
      _ffree((char FAR *)sp);
      return -1;
   }
   perminit(iperm,ip);

   fperm = (char (FAR *)[16][8])_fmalloc(sizeof(char) * 16 * 16 * 8);
   if(fperm == NULL){
      _ffree((char FAR *)sp);
      _ffree((char FAR *)iperm);
      return -1;
   }
   perminit(fperm,fp);
   
   return 0;
}

/* Free up storage used by DES */
void FASTCALL desdone( void )
{
   if(sp == NULL)
      return;  /* Already done */

   _ffree((char FAR *)sp);
   if(iperm != NULL)
      _ffree((char FAR *)iperm);
   if(fperm != NULL)
      _ffree((char FAR *)fperm);

   sp = NULL;
   iperm = NULL;
   fperm = NULL;
}

/* Set key (initialize key schedule array) */
int FASTCALL setkey( char (*kn)[8], char * key )
{
   char pc1m[56];    /* place to modify pc1 into */
   char pcr[56];     /* place to rotate pc1 into */
   register int i,j,l;
   int m;

   if(kn == NULL){
      return -1;
   }

   /* Clear key schedule */
   memset((char *)kn,0,16*8);

   for (j=0; j<56; j++) {     /* convert pc1 to bits of key */
      l=pc1[j]-1;    /* integer bit location  */
      m = l & 07;    /* find bit     */
      pc1m[j]=(key[l>>3] & /* find which key byte l is in */
         bytebit[m]) /* and which bit of that byte */
         ? 1 : 0; /* and store 1-bit result */
   }
   for (i=0; i<16; i++) {     /* key chunk for each iteration */
      for (j=0; j<56; j++) /* rotate pc1 the right amount */
         pcr[j] = pc1m[(l=j+totrot[i])<(j<28? 28 : 56) ? l: l-28];
         /* rotate left and right halves independently */
      for (j=0; j<48; j++){   /* select bits individually */
         /* check bit that goes to kn[j] */
         if (pcr[pc2[j]-1]){
            /* mask it in if it's there */
            l=j % 6;
            kn[i][j/6] |= bytebit[l] >> 2;
         }
      }
   }
   return 0;
}

/* In-place encryption of 64-bit block */
int FASTCALL endes( char (*kn)[8], char FAR *block )
{
   register long left,right;
   register char *knp;
   long work[2];     /* Working data storage */

   if(kn == NULL || block == NULL)
      return -1;
   permute(block,iperm,(char *)work);  /* Initial Permutation */
   left = work[0];
   right = work[1];

   /* Do the 16 rounds.
    * The rounds are numbered from 0 to 15. On even rounds
    * the right half is fed to f() and the result exclusive-ORs
    * the left half; on odd rounds the reverse is done.
    */
   knp = &kn[0][0];
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);
   knp += 8;
   left ^= f(right,knp);
   knp += 8;
   right ^= f(left,knp);

   /* Left/right half swap, plus byte swap if little-endian */
   work[0] = right;
   work[1] = left;
   permute((char FAR *)work,fperm,block); /* Inverse initial permutation */
   return 0;
}

/* In-place decryption of 64-bit block. This function is the mirror
 * image of encryption; exactly the same steps are taken, but in
 * reverse order
 */
int FASTCALL dedes( char (*kn)[8], char FAR *block )
{
   register long left,right;
   register char *knp;
   long work[2];  /* Working data storage */

   if(kn == NULL || block == NULL)
      return -1;
   permute(block,iperm,(char *)work);  /* Initial permutation */

   /* Left/right half swap, plus byte swap if little-endian */
   right = work[0];
   left = work[1];
   /* Do the 16 rounds in reverse order.
    * The rounds are numbered from 15 to 0. On even rounds
    * the right half is fed to f() and the result exclusive-ORs
    * the left half; on odd rounds the reverse is done.
    */
   knp = &kn[15][0];
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);
   knp -= 8;
   right ^= f(left,knp);
   knp -= 8;
   left ^= f(right,knp);

   work[0] = left;
   work[1] = right;
   permute((char FAR *)work,fperm,block); /* Inverse initial permutation */
   return 0;
}

/* Permute inblock with perm */
static void FASTCALL permute( char FAR * inblock, char FAR perm[16][16][8], char FAR *outblock)
{
   register char FAR *ib;
   register char FAR *ob;     /* ptr to input or output block */
   register char FAR *p;
   register char FAR *q;
   register int j;

   if(perm == NULL){
      /* No permutation, just copy */
      _fmemcpy(outblock,inblock,8);
      return;
   }
   /* Clear output block */
   _fmemset(outblock,'\0',8);

   ib = inblock;
   for (j = 0; j < 16; j += 2, ib++) { /* for each input nibble */
      ob = outblock;
      p = perm[j][(*ib >> 4) & 0xf];
      q = perm[j + 1][*ib & 0xf];
      /* and each output byte, OR the masks together */
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
      *ob++ |= *p++ | *q++;
   }
}

/* The nonlinear function f(r,k), the heart of DES */
static long FASTCALL f( long r, char * subkey)
{
   register long FAR *spp;
   register long rval,rt;
   register int er;

   /* Run E(R) ^ K through the combined S & P boxes.
    * This code takes advantage of a convenient regularity in
    * E, namely that each group of 6 bits in E(R) feeding
    * a single S-box is a contiguous segment of R.
    */
   subkey += 7;

   /* Compute E(R) for each block of 6 bits, and run thru boxes */
   er = ((int)r << 1) | ((r & 0x80000000) ? 1 : 0);
   spp = &sp[7][0];
   rval = spp[(er ^ *subkey--) & 0x3f];
   spp -= 64;
   rt = (unsigned long)r >> 3;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rval |= spp[((int)rt ^ *subkey--) & 0x3f];
   spp -= 64;
   rt >>= 4;
   rt |= (r & 1) << 5;
   rval |= spp[((int)rt ^ *subkey) & 0x3f];
   return rval;
}

/* initialize a perm array */
static void FASTCALL perminit( char FAR perm[16][16][8], char p[64] )
{
   register int l, j, k;
   int i,m;

   /* Clear the permutation array */
   _fmemset((char FAR *)perm,0,16*16*8);

   for (i=0; i<16; i++)    /* each input nibble position */
      for (j = 0; j < 16; j++)/* each possible input nibble */
      for (k = 0; k < 64; k++)/* each output bit position */
      {   l = p[k] - 1; /* where does this bit come from*/
         if ((l >> 2) != i)  /* does it come from input posn?*/
         continue;   /* if not, bit k is 0    */
         if (!(j & nibblebit[l & 3]))
         continue;   /* any such bit in input? */
         m = k & 07; /* which bit is this in the byte*/
         perm[i][j][k>>3] |= bytebit[m];
      }
}

/* Initialize the lookup table for the combined S and P boxes */
static void FASTCALL spinit(void)
{
   char pbox[32];
   int p,i,s,j,rowcol;
   long val;

   /* Compute pbox, the inverse of p32i.
    * This is easier to work with
    */
   for(p=0;p<32;p++){
      for(i=0;i<32;i++){
         if(p32i[i]-1 == p){
            pbox[p] = i;
            break;
         }
      }
   }
   for(s = 0; s < 8; s++){       /* For each S-box */
      for(i=0; i<64; i++){    /* For each possible input */
         val = 0;
         /* The row number is formed from the first and last
          * bits; the column number is from the middle 4
          */
         rowcol = (i & 32) | ((i & 1) ? 16 : 0) | ((i >> 1) & 0xf);
         for(j=0;j<4;j++){ /* For each output bit */
            if(si[s][rowcol] & (8 >> j)){
             val |= 1L << (31 - pbox[4*s + j]);
            }
         }
         sp[s][i] = val;

      }
   }
}
