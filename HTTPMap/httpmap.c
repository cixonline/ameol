#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <process.h>

/* This file creates httpmap.h, which is a C header file which
 * is a map of valid characters in a URL.
 */
void main( void )
{
   register int d;
   register int c;
   FILE * fh;
   char ch;

   /* Open the file. Make sure this utility is run in the
    * same directory as the source files.
    */   
   if( fopen_s( &fh, "httpmap.h", "w" ) != 0 )
      {
      fprintf( stderr, "Error: Cannot create HTTP map file\n" );
      exit( 1 );
      }

   /* Write the header.
    */
   fprintf( fh, "char HTTPChrTable[ 256 ] = {\n" );
   ch = 0;

   /* The array appears as a 16x16 matrix.
    */
   for( c = 0; c < 16; ++c )
      {
      fprintf( fh, "\t" );
      for( d = 0; d < 16; ++d )
         {
         /* Test ch. If it is alphanumeric or one of the chars
          * listed as valid, write a '1' otherwise write a '0'.
          */
         if( !isascii( ch ) )
            fprintf( fh, "0" );
         else if( isalnum( ch ) )
            fprintf( fh, "1" );
         else if( isprint( ch ) && strchr( "~$/\\:_-@%!&.,?=#+{}[]|", ch ) )
            fprintf( fh, "1" );
         else
            fprintf( fh, "0" );
         if( ch < 255 )
            fprintf( fh, "," );
         else if( d < 15 )
            fprintf( fh, ", " );
         ++ch;
         }
      fprintf( fh, "\n" );
      }

//safe = 
//extra = "!" | "*" | "'" | "(" | ")" | ","
//national = "{" | "}" | "|" | "\" | "^" | "~" | "[" | "]" | "`"
//punctuation = "<" | ">" | "#" | "%" | <">


//reserved = ";" | "/" | "?" | ":" | "@" | "&" | "="
//hex = digit | "A" | "B" | "C" | "D" | "E" | "F" |
//"a" | "b" | "c" | "d" | "e" | "f"
//escape = "%" hex hex


   /* Close down.
    */
   fprintf( fh, "};\n" );
   fprintf( fh, "#define\tIsHTTPChr(c) \t(HTTPChrTable[c] != 0)\n" );
   fclose( fh );
}
