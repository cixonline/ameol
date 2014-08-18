#ifndef _INC_LBF

typedef struct tagLBF FAR * LPLBF;

#define  LBF_NO_ERROR         0x0000
#define  LBF_READ_ERROR       0x0001
#define  LBF_WRITE_ERROR         0x0002

/* Parse flags.
 */
#define  PARSEFLG_ARCHIVE     0x0001
#define  PARSEFLG_IGNORECR    0x0002
#define  PARSEFLG_MARKREAD    0x0004
#define  PARSEFLG_IMPORT         0x0008
#define  PARSEFLG_NORULES     0x0010
#define  PARSEFLG_REPLACECR      0x0020

LPLBF FASTCALL Amlbf_Open( HNDFILE, int );
void FASTCALL Amlbf_Close( LPLBF );
BOOL FASTCALL Amlbf_Write( LPLBF, LPSTR, int );
BOOL FASTCALL Amlbf_Read( LPLBF, LPSTR, int, int *, BOOL *, BOOL * );
DWORD FASTCALL Amlbf_GetSize( LPLBF );
DWORD FASTCALL Amlbf_GetCount( LPLBF );
LONG FASTCALL Amlbf_GetPos( LPLBF );
void FASTCALL Amlbf_SetPos( LPLBF, LONG );
void FASTCALL Amlbf_SetFileDateTime( LPLBF, WORD, WORD );
int FASTCALL Amlbf_GetLastError( LPLBF );

#define _INC_LBF
#endif
