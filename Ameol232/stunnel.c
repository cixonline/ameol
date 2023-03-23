#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "amuser.h"
#include "amlib.h"
#include "stunnel.h"




struct StunnelDefinition {
	char* smtp_host;
	int smtp_port;

	char* pop3_host;
	int pop3_port;
};

void writestring(HNDFILE fh, char* s) {
	Amfile_Write32(fh, s, hstrlen(s));
}

void MakeConfig() {
	// size_t i;
	// char chTempBuf[1024];
	HNDFILE fh;

    fh = Amfile_Create( "C:\\Users\\cixonline\\Desktop\\stunnel\\config\\test-file.conf", 0 );

	writestring(fh, "taskbar = no\n");

	/*
	for (i=0; i<def_count; i++) {
		struct StunnelDefinition def = definitions[i];

		wsprintf(chTempBuf, "[%s]", def.name);
		writestring(fh, chTempBuf);

		writestring(fh, "client = yes\n");

		wsprintf(chTempBuf, "accept = localhost:%d\n", def.local_port);
		writestring(fh, chTempBuf);

		wsprintf(chTempBuf, "connect = %s:%d\n", def.connect_to_host, def.remote_port);
		writestring(fh, chTempBuf);

		writestring(fh, "cafile = C:\\Users\\cixonline\\Desktop\\stunnel\\config\\ca-certs.pem\n");
		writestring(fh, "verify = 2\n");

		wsprintf(chTempBuf, "checkhost = %s\n", def.connect_to_host);
		writestring(fh, chTempBuf);
	}
	*/

	Amfile_Close(fh);
	
/*
[telnet-ssl]
client = yes
accept = localhost:23
connect = cix.co.uk:992
cafile = C:\Users\cixonline\Desktop\stunnel\config\ca-certs.pem
verify = 2
checkhost = cix.co.uk

[cix-pop3]
protocol = pop3
client = yes
accept = 110
connect = mail.interdns.co.uk:110
cafile = C:\Users\cixonline\Desktop\stunnel\config\ca-certs.pem
verify = 2
checkhost = mail.interdns.co.uk

[cix-smtp]
protocol = smtp
client = yes
accept = 25
connect = smtp.interdns.co.uk:587
cafile = C:\Users\cixonline\Desktop\stunnel\config\ca-certs.pem
verify = 2
checkhost = smtp.interdns.co.uk
*/
}

void stop_stunnel( )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( !CreateProcess( NULL,   // No module name (use command line)
        "C:\\Users\\cixonline\\Desktop\\stunnel\\bin\\stunnel.exe -exit -quiet", // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
    {
        printf( "CreateProcess failed (%d).\n", GetLastError() );
        return;
    }

	// Wait for a bit for it to start. This is hacky.
	SleepEx(7000, FALSE);

    // Wait until child process exits.
    // WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles. 
    // CloseHandle( pi.hProcess );
    // CloseHandle( pi.hThread );
	// MakeConfig({
	// 	StunnelDefinition { 23, "cix.co.uk", 992, "telnet-ssl" }
	// }, 1);
}

void start_stunnel(BLINKENTRY beCur)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

	MakeConfig();

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( !CreateProcess( NULL,   // No module name (use command line)
        "C:\\Users\\cixonline\\Desktop\\stunnel\\bin\\stunnel.exe -quiet C:\\Users\\cixonline\\Desktop\\stunnel\\config\\stunnel.conf", // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
    {
        printf( "CreateProcess failed (%d).\n", GetLastError() );
        return;
    }

	Sleep(1000);

    // We need to start it twice???
    if( !CreateProcess( NULL,   // No module name (use command line)
        "C:\\Users\\cixonline\\Desktop\\stunnel\\bin\\stunnel.exe -quiet C:\\Users\\cixonline\\Desktop\\stunnel\\config\\stunnel.conf", // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
    {
        printf( "CreateProcess failed (%d).\n", GetLastError() );
        return;
    }


	// Wait for a bit for it to start. This is hacky.
	Sleep(1000);

    // Wait until child process exits.
    // WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles. 
    // CloseHandle( pi.hProcess );
    // CloseHandle( pi.hThread );
}
