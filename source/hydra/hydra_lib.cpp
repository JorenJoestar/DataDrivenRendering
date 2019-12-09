//
// Hydra Lib - v0.02
//
// Simple general functions for log, file, process, time.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/06/20, 19.23
//

#include "hydra_lib.h"

#if defined(_WIN64)
#include <stdio.h>
#include <windows.h>
#endif // _WIN64

#if defined(HY_STB)

#define STBDS_SIPHASH_2_4
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#endif // HY_STB

#if defined (HY_STB_LEAKCHECK)
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"
#endif // HY_STB_LEAKCHECK

namespace hydra {

#if defined(HY_LOG)

// Log //////////////////////////////////////////////////////////////////////////

static const size_t k_string_buffer_size = 1024 * 1024;
static char s_log_buffer[k_string_buffer_size];


static void print_va_list( const char* format, va_list args ) {
    vsnprintf_s( s_log_buffer, ArrayLength( s_log_buffer ), format, args );
    s_log_buffer[ArrayLength( s_log_buffer ) - 1] = '\0';
}

static void output_console() {
    printf( "%s", s_log_buffer );
}

static void output_visual_studio() {
    OutputDebugStringA( s_log_buffer );
}

void print_format( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    print_va_list( format, args );
    va_end( args );

    output_console();
    output_visual_studio();
}

void print_format_console( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    print_va_list( format, args );
    va_end( args );

    output_console();
}

#if defined(_MSC_VER)

void print_format_visual_studio( const char* format, ... ) {
    va_list args;
    va_start( args, format );
    print_va_list( format, args );
    va_end( args );

    output_visual_studio();
}

#endif // _MSC_VER

#endif // HY_LOG ////////////////////////////////////////////////////////////////


// File /////////////////////////////////////////////////////////////////////////
#if defined(HY_FILE)

void open_file( cstring filename, cstring mode, FileHandle* file ) {
    fopen_s( file, filename, mode );
}

void close_file( FileHandle file ) {
    if ( file )
        fclose( file );
}

int32_t read_file( uint8_t* memory, uint32_t elementSize, uint32_t count, FileHandle file ) {
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

void read_file( cstring filename, cstring mode, Buffer& memory ) {
    FileHandle f;
    open_file( filename, mode, &f );
    if ( !f )
        return;

    long fileSizeSigned = GetFileSize( f );

    //memory.resize( fileSizeSigned );
    array_set_length( memory, fileSizeSigned + 1 );

    size_t readsize = fread( &memory[0], 1, fileSizeSigned, f );
    memory[readsize] = 0;
    fclose( f );
}


FileTime get_last_write_time( cstring filename ) {
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
#endif // _WIN64
}

uint32_t get_full_path_name( cstring path, char* out_full_path, uint32_t max_size ) {
#if defined(_WIN64)
    return GetFullPathName( path, max_size, out_full_path, nullptr );
#endif // _WIN64
}

void find_files_in_path( cstring file_pattern, StringArray& files ) {

    clear( files );

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( file_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {

            intern( files, find_data.cFileName );

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
}

void find_files_in_path( cstring extension, cstring search_pattern, StringArray& files, StringArray& directories ) {

    clear( files );

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( search_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {
            if ( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                intern( directories, find_data.cFileName );
            }
            else {
                // If filename contains the extension, add it
                if ( strstr( find_data.cFileName, extension ) ) {
                    intern( files, find_data.cFileName );
                }
            }

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
}


char* read_file_into_memory( const char* filename, size_t* size ) {
    char* text = 0;

    FILE* file = fopen( filename, "rb" );

    if ( file ) {

        fseek( file, 0, SEEK_END );
        size_t filesize = ftell( file );
        fseek( file, 0, SEEK_SET );

        text = (char*)hy_malloc( filesize + 1 );
        size_t result = fread( text, filesize, 1, file );
        text[filesize] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return text;
}

// Scoped file //////////////////////////////////////////////////////////////////
ScopedFile::ScopedFile( cstring filename, cstring mode ) {
    open_file( filename, mode, &_file );
}

ScopedFile::~ScopedFile() {
    close_file( _file );
}

#endif // HY_FILE ///////////////////////////////////////////////////////////////


// Process //////////////////////////////////////////////////////////////////////

#if defined(HY_PROCESS)

#if defined(_WIN64)
// Static buffer to log the error coming from windows.
static const uint32_t       k_process_log_buffer = 256;
char                        s_process_log_buffer[k_process_log_buffer];


void win32_get_error( char* buffer, uint32_t size ) {
    DWORD errorCode = GetLastError();

    char* error_string;
    if ( !FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&error_string, 0, NULL ) )
        return;

    sprintf_s( buffer, size, "%s", error_string );

    LocalFree( error_string );
}

bool execute_process( cstring working_directory, cstring process_fullpath, cstring arguments ) {

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
        win32_get_error( &s_process_log_buffer[0], k_process_log_buffer );

#if defined(HY_LOG)
        // Use the custom print - this outputs also to Visual Studio output window.
        print_format( "Error Compiling: %s\n", s_process_log_buffer );
#else
        printf( "Error Compiling: %s\n", s_process_log_buffer );
#endif // HY_LOG

        return false;
    }
}

#endif // _WIN64 
#endif // HY_PROCESS ////////////////////////////////////////////////////////////


// Time /////////////////////////////////////////////////////////////////////////
#if defined (HY_TIME)
LARGE_INTEGER s_frequency;

void time_service_init() {
    QueryPerformanceFrequency(&s_frequency);
}

// Taken from the Rust code base: https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
// Computes (value*numer)/denom without overflow, as long as both
// (numer*denom) and the overall result fit into i64 (which is the case
// for our time conversions).
int64_t int64_mul_div( int64_t value, int64_t numer, int64_t denom ) {
    int64_t q = value / denom;
    int64_t r = value % denom;
    // Decompose value as (value/denom*denom + value%denom),
    // substitute into (value*numer)/denom and simplify.
    // r < denom, so (denom*numer) is the upper bound of (r*numer)
    return q * numer + r * numer / denom;
}

int64_t time_in_microseconds() {
    // Get time and frequency
    LARGE_INTEGER time;
    QueryPerformanceCounter( &time );
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency( &frequency );

    // Convert to microseconds
    const int64_t micros_per_second = 1000000LL;
    int64_t micros = int64_mul_div( time.QuadPart, micros_per_second, frequency.QuadPart );
    return micros;
}

#endif // HY_TIME ///////////////////////////////////////////////////////////////

//
// StringRef ////////////////////////////////////////////////////////////////////

bool equals( const StringRef& a, const StringRef& b ) {
    if ( a.length != b.length )
        return false;

    for ( uint32_t i = 0; i < a.length; ++i ) {
        if ( a.text[i] != b.text[i] ) {
            return false;
        }
    }

    return true;
}

void copy( const StringRef& a, char* buffer, uint32_t buffer_size ) {
    const uint32_t max_length = buffer_size - 1 < a.length ? buffer_size - 1 : a.length;
    memcpy( buffer, a.text, max_length );
    buffer[a.length] = 0;
}

//
// StringBuffer /////////////////////////////////////////////////////////////////
void StringBuffer::init( uint32_t size ) {
    if ( data ) {
        hy_free( data );
    }

    if ( size < 1 ) {
        printf( "ERROR: Buffer cannot be empty!\n" );
        return;
    }

    data = (char*)hy_malloc( size + 1 );
    data[0] = 0;
    buffer_size = size;
    current_size = 0;
}

void StringBuffer::terminate() {

    hy_free( data );
    buffer_size = current_size = 0;
}

void StringBuffer::append( const char* format, ... ) {
    if ( current_size >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return;
    }

    // TODO: safer version!
    va_list args;
    va_start( args, format );
    int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
    current_size += written_chars > 0 ? written_chars : 0;
    va_end( args );
}

void StringBuffer::append( const StringRef& text ) {
    const uint32_t max_length = current_size + text.length < buffer_size ? text.length : buffer_size - current_size;
    if ( max_length == 0 || max_length >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[current_size], text.text, max_length );
    current_size += max_length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[current_size] = 0;
}

void StringBuffer::append( void* memory, uint32_t size ) {

    if ( current_size + size >= buffer_size )
        return;

    memcpy( &data[current_size], memory, size );
    current_size += size;
}

void StringBuffer::append( const StringBuffer& other_buffer ) {

    if ( other_buffer.current_size == 0 ) {
        return;
    }

    if ( current_size + other_buffer.current_size >= buffer_size ) {
        return;
    }

    memcpy( &data[current_size], other_buffer.data, other_buffer.current_size );
    current_size += other_buffer.current_size;
}

char* StringBuffer::append_use( const char* format, ... ) {
    uint32_t cached_offset = this->current_size;

    // TODO: safer version!
    // TODO: do not copy paste!
    if ( current_size >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return nullptr;
    }

    va_list args;
    va_start( args, format );
    int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
    current_size += written_chars > 0 ? written_chars : 0;
    va_end( args );

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[current_size] = 0;
    ++current_size;

    return this->data + cached_offset;
}

char* StringBuffer::append_use( const StringRef& text ) {
    uint32_t cached_offset = this->current_size;

    append( text );

    return this->data + cached_offset;
}

char * StringBuffer::append_use_substring( const char* string, uint32_t start_index, uint32_t end_index ) {
    uint32_t size = end_index - start_index;
    if ( current_size + size >= buffer_size )
        return nullptr;

    uint32_t cached_offset = this->current_size;

    memcpy( &data[current_size], string, size );
    current_size += size;

    data[current_size] = 0;
    ++current_size;

    return this->data + cached_offset;
}

char* StringBuffer::reserve( uint32_t size ) {
    if ( current_size + size >= buffer_size )
        return nullptr;

    uint32_t offset = current_size;
    current_size += size;

    return data + offset;
}

void StringBuffer::clear() {
    current_size = 0;
    data[0] = 0;
}

//
// StringArray //////////////////////////////////////////////////////////////////

void init( StringArray& string_array, uint32_t size ) {

    string_array.data = (char*)hy_malloc( size );
    string_array.buffer_size = size;
    string_array.current_size = 0;

    string_array.string_to_index = nullptr;
    hash_map_set_default( string_array.string_to_index, 0xffffffff );
}

void terminate( StringArray& string_array ) {
    hy_free( string_array.data );

    string_array.buffer_size = string_array.current_size = 0;
}

void clear( StringArray& string_array ) {

    string_array.current_size = 0;
    hash_map_free( string_array.string_to_index );

    string_array.string_to_index = nullptr;
    hash_map_set_default( string_array.string_to_index, 0xffffffff );
}

const char* intern( StringArray& string_array, const char* string ) {

    static size_t seed = 0xf2ea4ffad;
    const size_t length = strlen( string );
    const size_t hash = hash_bytes( (void*)string, length, seed );

    uint32_t string_index = hash_map_get( string_array.string_to_index, hash );
    if ( string_index != 0xffffffff ) {
        return string_array.data + string_index;
    }

    string_index = string_array.current_size;
    string_array.current_size += length + 1; // null termination

    strcpy( string_array.data + string_index, string );

    hash_map_put( string_array.string_to_index, hash, string_index );

    return string_array.data + string_index;;
}

uint32_t get_string_count( StringArray& string_array ) {
    return hash_map_length_u( string_array.string_to_index );
}

const char* get_string( StringArray& string_array, uint32_t index ) {
    // UNSAFE
    return string_array.data + (string_array.string_to_index[index].value);
}


//
// Memory ///////////////////////////////////////////////////////////////////////
void* hy_malloc( size_t size ) {
    return malloc( size );
}

void hy_free( void* data ) {
    free( data );
}



} // namespace hydra