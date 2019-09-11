/////////////////////////////////////////////////////////////////////////////////
//  
//  Hydra Lib: simple general functions for log, file, process, time.
//
//  Source code     : https://www.github.com/jorenjoestar/Phoenix
//
//  Version         : 0.01
//
/////////////////////////////////////////////////////////////////////////////////

#include "hydra_lib.h"

#if defined(_WIN64)
#include <stdio.h>
#include <windows.h>
#endif

namespace hydra {


#if defined(HY_LOG)
/////////////////////////////////////////////////////////////////////////////////
// Log
/////////////////////////////////////////////////////////////////////////////////

static const size_t k_string_buffer_size = 1024 * 1024;
static char s_log_buffer[k_string_buffer_size];

//////////////////////////////////////////////////////////////////////////
static void PrintVaList( const char* format, va_list args ) {
    vsnprintf_s( s_log_buffer, ArrayLength( s_log_buffer ), format, args );
    s_log_buffer[ArrayLength( s_log_buffer ) - 1] = '\0';
}

static void OutputConsole() {
    printf( "%s", s_log_buffer );
}

static void OutputVisualStudio() {
    OutputDebugStringA( s_log_buffer );
}

void PrintFormat( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    PrintVaList( format, args );
    va_end( args );

    OutputConsole();
    OutputVisualStudio();
}

void PrintFormatConsole( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    PrintVaList( format, args );
    va_end( args );

    OutputConsole();
}

#if defined(_MSC_VER)

void PrintFormatVisualStudio( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    PrintVaList( format, args );
    va_end( args );

    OutputVisualStudio();
}

#endif // _MSC_VER

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
#endif // HY_LOG



/////////////////////////////////////////////////////////////////////////////////
// File
/////////////////////////////////////////////////////////////////////////////////
#if defined(HY_FILE)

void OpenFile( cstring filename, cstring mode, FileHandle* file ) {
    fopen_s( file, filename, mode );
}

void CloseFile( FileHandle file ) {
    if ( file )
        fclose( file );
}

int32_t ReadFile( uint8_t* memory, uint32_t elementSize, uint32_t count, FileHandle file ) {
    int32_t bytesRead = (int32_t)fread( memory, elementSize, count, file );
    return bytesRead;
}

static long GetFileSize( FileHandle f ) {
    long fileSizeSigned;

    fseek( f, 0, SEEK_END );
    fileSizeSigned = ftell( f );
    fseek( f, 0, SEEK_SET );

    return fileSizeSigned;
}

void ReadFileIntoMemory( cstring filename, cstring mode, Buffer& memory ) {
    FileHandle f;
    OpenFile( filename, mode, &f );
    if ( !f )
        return;

    long fileSizeSigned = GetFileSize( f );

    memory.resize( fileSizeSigned );

    size_t readsize = fread( &memory[0], 1, fileSizeSigned, f );
    fclose( f );
}


FileTime GetLastWriteTime( cstring filename ) {
#if defined(_WIN64)
    FILETIME lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if ( GetFileAttributesExA( filename, GetFileExInfoStandard, &data ) ) {
        lastWriteTime = data.ftLastWriteTime;
    }

    return lastWriteTime;
#else
    FileTime time = 0;
    return time;
#endif
}

void FindFilesInPath( cstring name, Array<String>& files ) {

    std::string pattern( name );
    pattern.append( "\\*" );

    WIN32_FIND_DATAA data;
    HANDLE hFind;
    if ( ( hFind = FindFirstFileA( pattern.c_str(), &data ) ) != INVALID_HANDLE_VALUE ) {
        do {
            files.push_back( data.cFileName );
        } while ( FindNextFileA( hFind, &data ) != 0 );
        FindClose( hFind );
    }
}

void FindFilesInPath( cstring extension, cstring searchPath, Array<String>& files, Array<String>& directories ) {

    std::string pattern( searchPath );
    pattern.append( "\\*" );

    WIN32_FIND_DATAA data;
    HANDLE hFind;
    if ( ( hFind = FindFirstFileA( pattern.c_str(), &data ) ) != INVALID_HANDLE_VALUE ) {
        do {
            if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                directories.push_back( data.cFileName );
            }
            else {
                if ( strstr( data.cFileName, extension ) ) {
                    files.push_back( data.cFileName );
                }
            }

        } while ( FindNextFileA( hFind, &data ) != 0 );
        FindClose( hFind );
    }
}


//////////////////////////////////////////
// Scoped file
ScopedFile::ScopedFile( cstring filename, cstring mode ) {
    OpenFile( filename, mode, &_file );
}

ScopedFile::~ScopedFile() {
    CloseFile( _file );
}

#endif // HY_FILE

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
// Process
/////////////////////////////////////////////////////////////////////////////////
#if defined(HY_PROCESS)

#if defined(_WIN64)
// Static buffer to log the error coming from windows.
static const uint32_t       k_process_log_buffer = 256;
char                        s_process_log_buffer[k_process_log_buffer];


void Win32GetError( char* buffer, uint32_t size ) {
    DWORD errorCode = GetLastError();

    char* error_string;
    if ( !FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&error_string, 0, NULL ) )
        return;

    sprintf_s( buffer, size, "%s", error_string );

    LocalFree( error_string );
}

bool ExecuteProcess( cstring working_directory, cstring process_fullpath, cstring arguments ) {

    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof( startup_info );
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_SHOW;

    PROCESS_INFORMATION process_info = {};
    if ( CreateProcessA( process_fullpath, (char*)arguments, 0, 0, FALSE, 0, 0, working_directory, &startup_info, &process_info ) ) {
        // Blocking version.
        WaitForSingleObject( process_info.hProcess, INFINITE );
        CloseHandle( process_info.hThread );
        CloseHandle( process_info.hProcess );

        return true;
    } else {
        Win32GetError( &s_process_log_buffer[0], k_process_log_buffer );

#if defined(HY_LOG)
        // Use the custom print - this outputs also to Visual Studio output window.
        PrintFormat( "Error Compiling: %s\n", s_process_log_buffer );
#else
        printf( "Error Compiling: %s\n", s_process_log_buffer );
#endif // HY_LOG

        return false;
    }
}

#endif // _WIN64
#endif // HY_PROCESS

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// Time
/////////////////////////////////////////////////////////////////////////////////
#if defined (HY_TIME)
LARGE_INTEGER s_frequency;

void TimeServiceInit() {
    QueryPerformanceFrequency(&s_frequency);
}

// Taken from the Rust code base: https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
// Computes (value*numer)/denom without overflow, as long as both
// (numer*denom) and the overall result fit into i64 (which is the case
// for our time conversions).
int64_t int64MulDiv( int64_t value, int64_t numer, int64_t denom ) {
    int64_t q = value / denom;
    int64_t r = value % denom;
    // Decompose value as (value/denom*denom + value%denom),
    // substitute into (value*numer)/denom and simplify.
    // r < denom, so (denom*numer) is the upper bound of (r*numer)
    return q * numer + r * numer / denom;
}

int64_t timeInMicros() {
    // Get time and frequency
    LARGE_INTEGER time;
    QueryPerformanceCounter( &time );
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency( &frequency );

    // Convert to microseconds
    const int64_t microsPerSecond = 1000000LL;
    int64_t micros = int64MulDiv( time.QuadPart, microsPerSecond, frequency.QuadPart );
    return micros;
}

#endif // HY_TIME

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
} // namespace hydra