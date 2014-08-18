#ifndef _INI_H

char * FASTCALL IniReadText( char *, char *, int );
char * FASTCALL IniReadInt( char *, int * );
char * FASTCALL IniReadLong( char *, long * );
char * FASTCALL IniWriteText( char *, char * );
char * FASTCALL IniWriteValue( char *, LONG );
char * FASTCALL IniWriteUnsignedValue( char *, DWORD );
char * FASTCALL IniReadDate( char *, AM_DATE * );
char * FASTCALL IniReadTime( char *, AM_TIME * );

#define _INI_H
#endif
