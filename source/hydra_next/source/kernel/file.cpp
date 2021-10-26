#include "file.hpp"

#include "kernel/memory.hpp"
#include "kernel/assert.hpp"

#include <windows.h>

namespace hydra {


void file_open( cstring filename, cstring mode, FileHandle* file ) {
    fopen_s( file, filename, mode );
}

void file_close( FileHandle file ) {
    if ( file )
        fclose( file );
}

sizet file_write( uint8_t* memory, u32 element_size, u32 count, FileHandle file ) {
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
        lastWriteTime.dwHighDateTime = data.ftLastWriteTime.dwHighDateTime;
        lastWriteTime.dwLowDateTime = data.ftLastWriteTime.dwLowDateTime;
    }

    return lastWriteTime;
#else
    FileTime time = 0;
    return time;
#endif // _WIN64
}

u32 file_resolve_to_full_path( cstring path, char* out_full_path, u32 max_size ) {
#if defined(_WIN64)
    return GetFullPathNameA( path, max_size, out_full_path, nullptr );
#endif // _WIN64
}

void file_directory_from_path( char* path ) {
    // TODO: this is a deprecated function.
    //PathRemoveFileSpecA( path );
    char* last_point = strrchr( path, '.' );
    char* last_separator = strrchr( path, '/' );
    if ( last_point > last_separator ) {
        *(last_separator + 1) = 0;
    }
    else {
        // TODO:
        // Wrong input!
        hy_assertm( false, "Malformed path %s!", path );
    }
}

void file_name_from_path( char* path ) {
    // Copy the name at the beginning of this string
    //sizet path_length = strlen( path );
    
    char* last_separator = strrchr( path, '/' );
    sizet name_length = strlen( last_separator + 1 );

    memcpy( path, last_separator + 1, name_length );
    path[ name_length ] = 0;
}

bool file_exists( cstring path ) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA( path, GetFileExInfoStandard, &unused );
#endif // _WIN64
}

bool file_delete( cstring path ) {
#if defined(_WIN64)
    int result = remove( path );
    return result != 0;
#endif
}


bool directory_exists( cstring path ) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA( path, GetFileExInfoStandard, &unused );
#endif // _WIN64
}

bool directory_create( cstring path ) {
#if defined(_WIN64)
    int result = CreateDirectoryA( path, NULL );
    return result != 0;
#endif // _WIN64
}

bool directory_delete( cstring path ) {
#if defined(_WIN64)
    int result = RemoveDirectoryA( path );
    return result != 0;
#endif // _WIN64
}

void directory_current( Directory* directory ) {
#if defined(_WIN64)
    DWORD written_chars = GetCurrentDirectoryA( k_max_path, directory->path );
    directory->path[ written_chars ] = 0;
#endif // _WIN64
}

void directory_change( cstring path ) {
#if defined(_WIN64)
    if ( !SetCurrentDirectoryA( path ) ) {
        hprint( "Cannot change current directory to %s\n", path );
    }
#endif // _WIN64
}

//
static bool string_ends_with_char( cstring s, char c ) {
    cstring last_entry = strrchr( s, c );
    const sizet index = last_entry - s;
    return index == (strlen( s ) - 1);
}

void file_open_directory( cstring path, Directory* out_directory ) {

    // Open file trying to conver to full path instead of relative.
    // If an error occurs, just copy the name.
    if ( file_resolve_to_full_path( path, out_directory->path, MAX_PATH ) == 0 ) {
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
        hprint("Could not open directory %s\n", out_directory->path );
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
    sizet index = last_directory_separator - directory->path;

    if ( index > 0 ) {

        strncpy( new_directory.path, directory->path, index );
        new_directory.path[index] = 0;

        last_directory_separator = strrchr( new_directory.path, '\\' );
        sizet second_index = last_directory_separator - new_directory.path;

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
        hprint( "Cannot find file %s\n", file_pattern );
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
        hprint( "Cannot find directory %s\n", search_pattern );
    }
}

#endif // HY_STB

void environment_variable_get( cstring name, char* output, u32 output_size ) {
    ExpandEnvironmentStringsA( name, output, output_size );
}

char* file_read_binary( cstring filename, Allocator* allocator, sizet* size ) {
    char* out_data = 0;

    FILE* file = fopen( filename, "rb" );

    if ( file ) {

        // TODO: Use filesize or read result ?
        sizet filesize = file_get_size( file );

        out_data = ( char* )halloca( filesize + 1, allocator );
        fread( out_data, filesize, 1, file );
        out_data[filesize] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return out_data;
}

char* file_read_text( cstring filename, Allocator* allocator, sizet* size ) {
    char* text = 0;

    FILE* file = fopen( filename, "r" );

    if ( file ) {

        sizet filesize = file_get_size( file );
        text = (char*)halloca( filesize + 1, allocator );
        // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
        // AFTER the end of line conversion for Windows (it uses \r\n).
        sizet bytes_read = fread( text, 1, filesize, file );

        text[ bytes_read ] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return text;
}

FileReadResult file_read_binary( cstring filename, Allocator* allocator ) {
    FileReadResult result { nullptr, 0 };

    FILE* file = fopen( filename, "rb" );

    if ( file ) {

        // TODO: Use filesize or read result ?
        sizet filesize = file_get_size( file );

        result.data = ( char* )halloca( filesize, allocator );
        fread( result.data, filesize, 1, file );

        result.size = filesize;

        fclose( file );
    }

    return result;
}

FileReadResult file_read_text( cstring filename, Allocator* allocator ) {
    FileReadResult result{ nullptr, 0 };

    FILE* file = fopen( filename, "r" );

    if ( file ) {

        sizet filesize = file_get_size( file );
        result.data = ( char* )halloca( filesize + 1, allocator );
        // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
        // AFTER the end of line conversion for Windows (it uses \r\n).
        sizet bytes_read = fread( result.data, 1, filesize, file );

        result.data[ bytes_read ] = 0;

        result.size = filesize;

        fclose( file );
    }

    return result;
}

void file_write_binary( cstring filename, void* memory, sizet size ) {
    FILE* file = fopen( filename, "wb" );
    fwrite( memory, size , 1, file );
    fclose( file );
}

// Scoped file //////////////////////////////////////////////////////////////////
ScopedFile::ScopedFile( cstring filename, cstring mode ) {
    file_open( filename, mode, &file );
}

ScopedFile::~ScopedFile() {
    file_close( file );
}
} // namespace hydra
