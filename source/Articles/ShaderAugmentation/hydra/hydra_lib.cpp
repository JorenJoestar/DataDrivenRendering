//
// Hydra Lib - v0.12


#include "hydra_lib.h"

#if defined(_WIN64)
#include <stdio.h>
#include <windows.h>
#include <shlwapi.h>
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

#if defined(HY_LOG)
    #define HYDRA_LOG                   hydra::print_format
#else
    #define HYDRA_LOG                   printf
#endif

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

void print_format( cstring format, ... ) {
    va_list args;
    va_start( args, format );
    print_va_list( format, args );
    va_end( args );

    output_console();
    output_visual_studio();
}

void print_format_console( cstring format, ... ) {
    va_list args;
    va_start( args, format );
    print_va_list( format, args );
    va_end( args );

    output_console();
}

#if defined(_MSC_VER)

void print_format_visual_studio( cstring format, ... ) {
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

void file_open( cstring filename, cstring mode, FileHandle* file ) {
    fopen_s( file, filename, mode );
}

void file_close( FileHandle file ) {
    if ( file )
        fclose( file );
}

size_t file_write( uint8_t* memory, uint32_t element_size, uint32_t count, FileHandle file ) {
    return fwrite( memory, element_size, count, file );
}

static long file_get_size( FileHandle f ) {
    long fileSizeSigned;

    fseek( f, 0, SEEK_END );
    fileSizeSigned = ftell( f );
    fseek( f, 0, SEEK_SET );

    return fileSizeSigned;
}

FileTime file_last_write_time( cstring filename ) {
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

uint32_t file_full_path( cstring path, char* out_full_path, uint32_t max_size ) {
#if defined(_WIN64)
    return GetFullPathNameA( path, max_size, out_full_path, nullptr );
#endif // _WIN64
}

void file_remove_filename( char* path ) {
    PathRemoveFileSpecA( path );
}

//
static bool string_ends_with_char( cstring s, char c ) {
    cstring last_entry = strrchr( s, c );
    const size_t index = last_entry - s;
    return index == (strlen( s ) - 1);
}

void file_open_directory( cstring path, Directory* out_directory ) {

    // Open file trying to conver to full path instead of relative.
    // If an error occurs, just copy the name.
    if ( file_full_path( path, out_directory->path, MAX_PATH ) == 0 ) {
        strcpy( out_directory->path, path );
    }

    // Add '\\' if missing
    if ( !string_ends_with_char( path, '\\' ) ) {
        strcat( out_directory->path, "\\" );
    }

    if ( !string_ends_with_char( out_directory->path, '*' ) ) {
        strcat( out_directory->path, "*" );
    }

    out_directory->os_handle = nullptr;

    WIN32_FIND_DATAA find_data;
    HANDLE found_handle;
    if ( (found_handle = FindFirstFileA( out_directory->path, &find_data )) != INVALID_HANDLE_VALUE ) {
        out_directory->os_handle = found_handle;
    }
    else {
        HYDRA_LOG("Could not open directory %s\n", out_directory->path );
    }
}

void file_close_directory( Directory* directory ) {
    if ( directory->os_handle ) {
        FindClose( directory->os_handle );
    }
}

void file_parent_directory( Directory* directory ) {

    Directory new_directory;

    const char* last_directory_separator = strrchr( directory->path, '\\' );
    size_t index = last_directory_separator - directory->path;

    if ( index > 0 ) {

        strncpy( new_directory.path, directory->path, index );
        new_directory.path[index] = 0;

        last_directory_separator = strrchr( new_directory.path, '\\' );
        size_t second_index = last_directory_separator - new_directory.path;

        if ( last_directory_separator > 0 ) {
            new_directory.path[second_index] = 0;
        }
        else {
            new_directory.path[index] = 0;
        }

        file_open_directory( new_directory.path, &new_directory );

        // Update directory
        if ( new_directory.os_handle ) {
            *directory = new_directory;
        }
    }
}

void file_sub_directory( Directory* directory, cstring sub_directory_name ) {

    // Remove the last '*' from the path. It will be re-added by the file_open.
    if ( string_ends_with_char( directory->path, '*' ) ) {
        directory->path[strlen( directory->path ) - 1] = 0;
    }

    strcat( directory->path, sub_directory_name );
    file_open_directory( directory->path, directory );
}

#if defined (HY_STB)

void file_find_files_in_path( cstring file_pattern, StringArray& files ) {

    files.clear();

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( file_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {

            files.intern( find_data.cFileName );

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
    else {
        HYDRA_LOG( "Cannot find file %s\n", file_pattern );
    }
}

void file_find_files_in_path( cstring extension, cstring search_pattern, StringArray& files, StringArray& directories ) {

    files.clear();
    directories.clear();

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( search_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {
            if ( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                directories.intern( find_data.cFileName );
            }
            else {
                // If filename contains the extension, add it
                if ( strstr( find_data.cFileName, extension ) ) {
                    files.intern( find_data.cFileName );
                }
            }

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
    else {
        HYDRA_LOG( "Cannot find directory %s\n", search_pattern );
    }
}

#endif // HY_STB

char* file_read( cstring filename, MemoryAllocator& allocator, size_t* size ) {
    char* text = 0;

    FILE* file = fopen( filename, "rb" );

    if ( file ) {

        // TODO: Use filesize or read result ?
        size_t filesize = file_get_size( file );

        text = (char*)allocator.allocate( filesize + 1, 1 );
        size_t result = fread( text, filesize, 1, file );
        text[filesize] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return text;
}

char* file_read_into_memory( cstring filename, size_t* size, bool as_text ) {
    char* text = 0;

    FILE* file = fopen( filename, as_text ? "r" : "rb" );

    if ( file ) {

        // TODO: Use filesize or read result ?
        size_t filesize = file_get_size( file );

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
    file_open( filename, mode, &file );
}

ScopedFile::~ScopedFile() {
    file_close( file );
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

bool process_execute( cstring working_directory, cstring process_fullpath, cstring arguments ) {
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

        HYDRA_LOG( "Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"\n", process_fullpath, arguments, working_directory );
        HYDRA_LOG( "Message: %s\n", s_process_log_buffer );
    }
    CloseHandle( handle_stdin_pipe_read );
    CloseHandle( handle_std_pipe_write );


    static char k_process_output_buffer[1024];

    DWORD bytes_read;
    ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr );
    
    // Consume all outputs.
    // Terminate current read and initialize the next.
    while ( ok == TRUE ) {
        k_process_output_buffer[bytes_read] = 0;
        HYDRA_LOG( "%s", k_process_output_buffer );

        ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr );
    }

    HYDRA_LOG( "\n" );

    // Close handles.
    CloseHandle( handle_stdout_pipe_read );
    CloseHandle( handle_stdin_pipe_write );

    DWORD process_exit_code = 0;
    GetExitCodeProcess( process_info.hProcess, &process_exit_code );

    return execution_success;
}

#endif // _WIN64 
#endif // HY_PROCESS ////////////////////////////////////////////////////////////


// Time /////////////////////////////////////////////////////////////////////////
#if defined (HY_TIME)
// Cached frequency.
// From Microsoft Docs: (https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency)
// "The frequency of the performance counter is fixed at system boot and is consistent across all processors. 
// Therefore, the frequency need only be queried upon application initialization, and the result can be cached."

static LARGE_INTEGER s_frequency;

//
//
void time_service_init() {
    // Cache this value - by Microsoft Docs it will not change during process lifetime.
    QueryPerformanceFrequency(&s_frequency);
}

//
//
void time_service_terminate() {
    // Nothing to do.
}

// Taken from the Rust code base: https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
// Computes (value*numer)/denom without overflow, as long as both
// (numer*denom) and the overall result fit into i64 (which is the case
// for our time conversions).
static int64_t int64_mul_div( int64_t value, int64_t numer, int64_t denom ) {
    const int64_t q = value / denom;
    const int64_t r = value % denom;
    // Decompose value as (value/denom*denom + value%denom),
    // substitute into (value*numer)/denom and simplify.
    // r < denom, so (denom*numer) is the upper bound of (r*numer)
    return q * numer + r * numer / denom;
}

//
//
int64_t time_now() {
    // Get current time
    LARGE_INTEGER time;
    QueryPerformanceCounter( &time );

    // Convert to microseconds
    // const int64_t microseconds_per_second = 1000000LL;
    const int64_t microseconds = int64_mul_div( time.QuadPart, 1000000LL, s_frequency.QuadPart );
    return microseconds;
}

//
//
int64_t time_from( int64_t starting_time ) {
    return time_now() - starting_time;
}

//
//
double time_from_microseconds( int64_t starting_time ) {
    return time_microseconds( time_from( starting_time ) );
}

//
//
double time_from_milliseconds( int64_t starting_time ) {
    return time_milliseconds( time_from( starting_time ) );
}

//
//
double time_from_seconds( int64_t starting_time ) {
    return time_seconds( time_from( starting_time ) );
}

//
//
double time_microseconds( int64_t time ) {
    return (double)time;
}

//
//
double time_milliseconds( int64_t time ) {
    return (double)time / 1000.0;
}

//
//
double time_seconds( int64_t time ) {
    return (double)time / 1000000.0;
}

#endif // HY_TIME ///////////////////////////////////////////////////////////////

//
// StringRef ////////////////////////////////////////////////////////////////////

bool StringRef::equals( const StringRef& a, const StringRef& b ) {
    if ( a.length != b.length )
        return false;

    for ( uint32_t i = 0; i < a.length; ++i ) {
        if ( a.text[i] != b.text[i] ) {
            return false;
        }
    }

    return true;
}

void StringRef::copy( const StringRef& a, char* buffer, size_t buffer_size ) {
    const size_t max_length = buffer_size - 1 < a.length ? buffer_size - 1 : a.length;
    memcpy( buffer, a.text, max_length );
    buffer[a.length] = 0;
}

//
// StringBuffer /////////////////////////////////////////////////////////////////
void StringBuffer::init( size_t size, MemoryAllocator* allocator_ ) {
    if ( data ) {
        hy_free( data );
    }

    if ( size < 1 ) {
        printf( "ERROR: Buffer cannot be empty!\n" );
        return;
    }
    allocator = allocator_;
    data = (char*)allocator_->allocate( size + 1, 1 );
    data[0] = 0;
    buffer_size = (uint32_t)size;
    current_size = 0;
}

void StringBuffer::terminate() {

    allocator->free( data );

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
    const size_t max_length = current_size + text.length < buffer_size ? text.length : buffer_size - current_size;
    if ( max_length == 0 || max_length >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[current_size], text.text, max_length );
    current_size += (uint32_t)max_length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[current_size] = 0;
}

void StringBuffer::append( void* memory, size_t size ) {

    if ( current_size + size >= buffer_size )
        return;

    memcpy( &data[current_size], memory, size );
    current_size += (uint32_t)size;
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
    ++current_size;

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

char* StringBuffer::reserve( size_t size ) {
    if ( current_size + size >= buffer_size )
        return nullptr;

    uint32_t offset = current_size;
    current_size += (uint32_t)size;

    return data + offset;
}

void StringBuffer::clear() {
    current_size = 0;
    data[0] = 0;
}

//
// StringArray //////////////////////////////////////////////////////////////////

#if defined(HY_STB)

void StringArray::init( uint32_t size, MemoryAllocator* allocator_ ) {

    data = (char*)allocator_->allocate( size, 1 );
    buffer_size = size;
    current_size = 0;

    string_to_index = nullptr;
    hash_map_set_default( string_to_index, 0xffffffff );

    allocator = allocator_;
}

void StringArray::terminate() {
    allocator->free( data );

    buffer_size = current_size = 0;
}

void StringArray::clear() {

    current_size = 0;
    hash_map_free( string_to_index );

    string_to_index = nullptr;
    hash_map_set_default( string_to_index, 0xffffffff );
}

const char* StringArray::intern( const char* string ) {

    static size_t seed = 0xf2ea4ffad;
    const size_t length = strlen( string );
    const size_t hash = hash_bytes( (void*)string, length, seed );

    uint32_t string_index = hash_map_get( string_to_index, hash );
    if ( string_index != 0xffffffff ) {
        return data + string_index;
    }

    string_index = current_size;
    current_size += (uint32_t)length + 1; // null termination

    strcpy( data + string_index, string );

    hash_map_put( string_to_index, hash, string_index );

    return data + string_index;;
}

size_t StringArray::get_string_count() const {
    return hash_map_size( string_to_index );
}

const char* StringArray::get_string( uint32_t index ) const {
    // UNSAFE
    return data + (string_to_index[index].value);
}

#endif // HY_STB

//
// Memory ///////////////////////////////////////////////////////////////////////

// Default system allocator ///////////////////////

struct MallocAllocator : public MemoryAllocator {

    void*                           allocate( size_t size, size_t alignment );
    void                            free( void* pointer );

}; // struct MallocAllocator

void* MallocAllocator::allocate( size_t size, size_t alignment ) {
    return ::malloc( size );
}

void MallocAllocator::free( void* pointer ) {
    return ::free( pointer );
}

// TODO: per thread ?
static MallocAllocator              s_malloc_allocator;

// Memory Service /////////////////////////////////

void memory_service_init() {
    // Stub
}

void memory_service_terminate() {
    // Stub
}

MemoryAllocator* memory_get_system_allocator() {
    return &s_malloc_allocator;
}


void* hy_malloc( size_t size ) {
    return s_malloc_allocator.allocate( size, 1 );
}

void hy_free( void* data ) {
    s_malloc_allocator.free( data );
}



} // namespace hydra