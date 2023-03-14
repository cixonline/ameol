#include <windows.h>
#include <stdio.h>
#include <tchar.h>

void start_stunnel( )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

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

void stop_stunnel( )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( !CreateProcess( NULL,   // No module name (use command line)
        "C:\\Users\\cixonline\\Desktop\\stunnel\\bin\\stunnel.exe -exit -quiet C:\\Users\\cixonline\\Desktop\\stunnel\\config\\stunnel.conf", // Command line
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
}