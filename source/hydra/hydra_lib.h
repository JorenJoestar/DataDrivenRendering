#pragma once

#include <stdint.h>

//
// Hydra Lib - v0.03
//
// Simple general functions for log, file, process, time.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/06/20, 19.23
//
// Revision history //////////////////////
//
//      0.03 (2019/12/17) + Interface cleanup. + Added array init macro.
//      0.02 (2019/09/26) added stb.
//      0.01 (2019/06/20) initial implementation.
//
// Documentation /////////////////////////
//
// Different utility functions are defined here.
// Use #define to use the different libraries you need.
//
//  #define HY_STB
//
//      Enables the STB header containing array and hash maps.
//      Includes some macros for easier reading of the code with longer names.
//
//  #define HY_LOG
//
//  #define HY_FILE
//
// Todo //////////////////////////////////
//
//      - Move StringBuffer into this file.
//      - Add String Array.

#define HY_FILE
#define HY_LOG
//#define HY_PROCESS
//#define HY_TIME
#define HY_STB
#define HY_STB_LEAKCHECK

#if defined (HY_STB)

#include "stb_ds.h"

using Buffer                        = uint8_t*;

// STB Macro embellishment //////////////////////////////////////////////////////

// Array /////////////////////////////////
#define array(Type)                 Type*

#define array_init(variable)        variable = nullptr;
#define array_free                  arrfree
#define array_length                arrlen
#define array_length_u              arrlenu
#define array_pop                   arrpop
#define array_push                  arrput
#define array_insert                arrins
#define array_insert_uninitialized  arrinsn
#define array_delete                arrdel
#define array_delete_count          arrdeln
#define array_delete_swap           arrdelswap
#define array_set_length            arrsetlen
#define array_set_max               arrsetcap
#define array_max                   arrcap

// Hash Map //////////////////////////////
#define hash_map(Type)              Type*

#define hash_map_free               hmfree
#define hash_map_length             hmlen
#define hash_map_length_u           hmlenu
#define hash_map_get_index          hmgeti
#define hash_map_get                hmget
#define hash_map_get_structure      hmgets
#define hash_map_set_default        hmdefault
#define hash_map_set_default_structure hmdefaults
#define hash_map_put                hmput
#define hash_map_put_structure      hmputs
#define hash_map_delete             hmdel

// String Hash Map ///////////////////////
#define string_hash(Type)           Type*

#define string_hash_init_arena      sh_new_arena
#define string_hash_init_dynamic    sh_new_strdup
#define string_hash_free            shfree
#define string_hash_length          shlenu
#define string_hash_length_p        shlen
#define string_hash_get_index       shgeti
#define string_hash_get             shget
#define string_hash_get_structure   shgets
#define string_hash_set_default     shdefault
#define string_hash_set_default_structure shdefaults
#define string_hash_put             shput
#define string_hash_put_structure   shputs
#define string_hash_delete          shdel


// Hash //////////////////////////////////
inline void                         set_rand_seed( size_t seed )                { return stbds_rand_seed( seed ); }
inline size_t                       hash_string( char* string, size_t seed )    { return stbds_hash_string( string, seed ); }
inline size_t                       hash_bytes( void* data, size_t length, size_t seed ) { return stbds_hash_bytes( data, length, seed); }


#endif // HY_STB

// TODO: add the non-std versions.
#include <string>
#include <vector>
#include <Windows.h>

template <typename T>
using Array = std::vector<T>;

using String = std::string;

//#endif // HY_FILE

namespace hydra {

    // Common ///////////////////////////////////////////////////////////////////
    typedef const char* cstring;

    #define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )

    // Data structures //////////////////////////////////////////////////////////

    //
    // Array of interned strings. Uses a hash map for each string, but interns string into a big shared buffer.
    // Based on the amazing article by OurMachinery: https://ourmachinery.com/post/data-structures-part-3-arrays-of-arrays/
    // Also, pretty similar to StringBuffer, but adding the individual string hashing.
    struct StringArray {

        struct StringMap {
            size_t                  key;
            uint32_t                value;
        }; // struct StringMap

        char*                       data                    = nullptr;
        uint32_t                    buffer_size             = 1024;
        uint32_t                    current_size            = 0;

        StringMap*                  string_to_index;

    }; // struct StringArray


    void                            init( StringArray& string_array, uint32_t size );
    void                            terminate( StringArray& string_array );
    void                            clear( StringArray& string_array );

    uint32_t                        get_string_count( StringArray& string_array );
    const char*                     get_string( StringArray& string_array, uint32_t index );
    const char*                     intern( StringArray& string_array, const char* string );


    //
    // Simple string that references another one. Used to not allocate strings if not needed.
    struct StringRef {

        size_t                      length;
        char*                       text;

    }; // struct StringRef

    bool                            equals( const StringRef& a, const StringRef& b );
    void                            copy( const StringRef& a, char* buffer, uint32_t buffer_size );


    //
    // Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination when needed.
    struct StringBuffer {

        void                        init( uint32_t size );
        void                        terminate();

        void                        append( const char* format, ... );
        void                        append( const StringRef& text );
        void                        append( void* memory, uint32_t size );
        void                        append( const StringBuffer& other_buffer );

        char*                       append_use( const char* format, ... );
        char*                       append_use( const StringRef& text );    // Append and returns a pointer to the start. Used for strings mostly.
        char*                       append_use_substring( const char* string, uint32_t start_index, uint32_t end_index ); // Append a substring of the passed string.

        char*                       reserve( uint32_t size );

        void                        clear();

        char*                       data = nullptr;
        uint32_t                    buffer_size = 1024;
        uint32_t                    current_size = 0;

    }; // struct StringBuffer



    // Log //////////////////////////////////////////////////////////////////////
#if defined(HY_LOG)

    void                            print_format( cstring format, ... );
    void                            print_format_console( cstring format, ... );
#if defined(_MSC_VER)
    void                            print_format_visual_studio( cstring format, ... );
#endif // _MSC_VER

#endif // HY_LOG

    // File /////////////////////////////////////////////////////////////////////
#if defined(HY_FILE)

#if defined(_WIN64)
    using FileTime = FILETIME;
#endif

    using FileHandle = FILE*;

    void                            read_file( cstring filename, cstring mode, Buffer& memory );
    char*                           read_file_into_memory( const char* filename, size_t* size );

    void                            open_file( cstring filename, cstring mode, FileHandle* file );
    void                            close_file( FileHandle file );
    int32_t                         read_file( uint8_t* memory, uint32_t elementSize, uint32_t count, FileHandle file );

    FileTime                        get_last_write_time( cstring filename );
    uint32_t                        get_full_path_name( cstring path, char* out_full_path, uint32_t max_size );

    void                            find_files_in_path( cstring file_pattern, StringArray& files );         // Search files matching file_pattern and puts them in files array.
                                                                                                            // Examples: "..\\data\\*", "*.bin", "*.*
    void                            find_files_in_path( cstring extension, cstring search_pattern, 
                                                        StringArray& files, StringArray& directories );     // Search files and directories using search_patterns, and 

    struct ScopedFile {
        ScopedFile( cstring filename, cstring mode );
        ~ScopedFile();

        FileHandle          _file;
    };

#endif // HY_FILE


    // Process //////////////////////////////////////////////////////////////////
#if defined(HY_PROCESS)

    bool                            execute_process( cstring working_directory, cstring process_fullpath, cstring arguments );

#endif // HY_PROCESS
    

    // Time /////////////////////////////////////////////////////////////////////
#if defined(HY_TIME)

    void                            time_service_init();


#endif // HY_TIME

    void*                           hy_malloc( size_t size );
    void                            hy_free( void* data );

} // namespace hydra