#include "transfer.h"

/* FILETRAN.C */
void FASTCALL InstallAllFileTransferProtocols( void );
BOOL FASTCALL EnumerateFileTransferProtocols( HWND );
HXFER FASTCALL GetFileTransferProtocolHandle( char * );
BOOL FASTCALL SendFileViaProtocol( HXFER, LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL );
BOOL FASTCALL ReceiveFileViaProtocol( HXFER, LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL, WORD );
void FASTCALL CancelTransfer( HXFER, LPCOMMDEVICE );
BOOL FASTCALL IsTransferActive( void );
BOOL FASTCALL GetFileTransferProtocolCommands( HXFER, LPSTR, int );
