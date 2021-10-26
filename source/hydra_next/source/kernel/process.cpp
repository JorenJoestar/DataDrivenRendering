#include "process.hpp"
#include "log.hpp"

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace hydra {

#if defined(_WIN64)
// Static buffer to log the error coming from windows.
static const u32    k_process_log_buffer = 256;
char                s_process_log_buffer[k_process_log_buffer];
static char         k_process_output_buffer[ 1025 ];


void win32_get_error( char* buffer, u32 size ) {
    DWORD errorCode = GetLastError();

    char* error_string;
    if ( !FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&error_string, 0, NULL ) )
        return;

    sprintf_s( buffer, size, "%s", error_string );

    LocalFree( error_string );
}

bool process_execute( cstring working_directory, cstring process_fullpath, cstring arguments, cstring search_error_string ) {
    // From the post in https://stackoverflow.com/questions/35969730/how-to-read-output-from-cmd-exe-using-createprocess-and-createpipe/55718264#55718264
    // Create pipes for redirecting output
    HANDLE handle_stdin_pipe_read = NULL;
    HANDLE handle_stdin_pipe_write = NULL;
    HANDLE handle_stdout_pipe_read = NULL;
    HANDLE handle_std_pipe_write = NULL;

    SECURITY_ATTRIBUTES security_attributes = { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE };

    BOOL ok = CreatePipe( &handle_stdin_pipe_read, &handle_stdin_pipe_write, &security_attributes, 0 );
    if ( ok == FALSE )
        return false;
    ok = CreatePipe( &handle_stdout_pipe_read, &handle_std_pipe_write, &security_attributes, 0 );
    if ( ok == FALSE )
        return false;

    // Create startup informations with std redirection
    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof( startup_info );
    startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup_info.hStdInput = handle_stdin_pipe_read;
    startup_info.hStdError = handle_std_pipe_write;
    startup_info.hStdOutput = handle_std_pipe_write;
    startup_info.wShowWindow = SW_SHOW;

    bool execution_success = false;
    // Execute the process
    PROCESS_INFORMATION process_info = {};
    BOOL inherit_handles = TRUE;
    if ( CreateProcessA( process_fullpath, (char*)arguments, 0, 0, inherit_handles, 0, 0, working_directory, &startup_info, &process_info ) ) {

        CloseHandle( process_info.hThread );
        CloseHandle( process_info.hProcess );

        execution_success = true;
    } else {
        win32_get_error( &s_process_log_buffer[0], k_process_log_buffer );

        hprint( "Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"\n", process_fullpath, arguments, working_directory );
        hprint( "Message: %s\n", s_process_log_buffer );
    }
    CloseHandle( handle_stdin_pipe_read );
    CloseHandle( handle_std_pipe_write );

    // Output
    DWORD bytes_read;
    ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr );
    
    // Consume all outputs.
    // Terminate current read and initialize the next.
    while ( ok == TRUE ) {
        k_process_output_buffer[bytes_read] = 0;
        hprint( "%s", k_process_output_buffer );

        ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr );
    }

    if ( strlen(search_error_string) > 0 && strstr( k_process_output_buffer, search_error_string ) ) {
        execution_success = false;
    }

    hprint( "\n" );

    // Close handles.
    CloseHandle( handle_stdout_pipe_read );
    CloseHandle( handle_stdin_pipe_write );

    DWORD process_exit_code = 0;
    GetExitCodeProcess( process_info.hProcess, &process_exit_code );

    return execution_success;
}

cstring process_get_output() {
    return k_process_output_buffer;
}

#else

#endif // WIN64

} // namespace hydra