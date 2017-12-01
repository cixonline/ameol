#ifndef _ADMIN_H

BOOL FASTCALL OpenUsersList( BOOL );
void FASTCALL CloseUsersList( void );
void FASTCALL CmdAdmin( HWND );
BOOL FASTCALL Login( char *, char * );
BOOL FASTCALL AdminSetup( void );
int FASTCALL QueryUserCount( void );
BOOL FASTCALL BlankPassword( char * );
BOOL FASTCALL TestPerm( DWORD );
LPSTR FASTCALL GetActiveUsername( void );
LPSTR FASTCALL GetActivePassword( void );
BOOL FASTCALL RefreshUsersList( void );
void FASTCALL CreateAppCaption( char *, char * );

#define _ADMIN_H
#endif
