#pragma once

#include <stdint.h>

//
// Hydra Lib - v0.12
//
// Simple general functions for log, file, process, time.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/06/20, 19.23
//
// Revision history //////////////////////
//
//      0.12 (2020/03/23) + Added stdout and stderr redirection when executing a process. + Added method to remove filename from a path.
//      0.11 (2020/03/20) + Added Directory struct and utility functions to navigate filesystem.
//      0.10 (2020/03/18) + Added memory allocators to file library + Added memory allocators to StringBuffer and StringArray + Fixed file_write function.
//      0.09 (2020/03/16) + Added Memory Service and first Allocator.
//      0.08 (2020/03/11) + Moved StringArray methods inside the struct. + Renamed lenu methods to size.
//      0.07 (2020/03/10) + Renamed all file methods to start with "file_".
//      0.06 (2020/03/08) + Changed all StringBuffer methods with size to use size_t. + Moved StringRef methods as static inside the struct.
//      0.05 (2020/03/02) + Implemented Time Subsystem. + Improved Execute Process error message.
//      0.04 (2020/02/27) + Removal of STB-dependent parts
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
//      Simple logging function to output to console and visual studio output window.
//
//  #define HY_FILE
//
//      File open, close, read, write utilities.
//      If STB is enabled adds find files and folder utility.
//
//  #define HY_PROCESS
//
//      Execute an external process with output.
//
//  #define HY_TIME
//
//      Time utilities like current application time, time differences. Mostly to get time differences.
//

#define HY_FILE
#define HY_LOG
#define HY_PROCESS
#define HY_TIME
#define HY_STB
//#define HY_STB_LEAKCHECK

#if defined (HY_STB)

#include "stb_ds.h"

using Buffer                        = uint8_t*;

// STB Macro embellishment //////////////////////////////////////////////////////

// Array /////////////////////////////////
#define array(Type)                 Type*

#define array_init(variable)        variable = nullptr;
#define array_free                  arrfree
#define array_length_p              arrlen
#define array_size                  arrlenu
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
#define hash_map_length_p           hmlen
#define hash_map_size               hmlenu
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
#define string_hash_size            shlenu
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

#else
// TODO: add default STD implementation here ?
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
    struct MemoryAllocator;

    #define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )

    // Data structures //////////////////////////////////////////////////////////
#if defined (HY_STB)
    //
    // Array of interned strings. Uses a hash map for each string, but interns string into a big shared buffer.
    // Based on the amazing article by OurMachinery: https://ourmachinery.com/post/data-structures-part-3-arrays-of-arrays/
    // Also, pretty similar to StringBuffer, but adding the individual string hashing.
    struct StringArray {

        struct StringMap {
            size_t                  key;
            uint32_t                value;
        }; // struct StringMap

        void                        init( uint32_t size, MemoryAllocator* allocator );
        void                        terminate();
        void                        clear();

        size_t                      get_string_count() const;
        const char*                 get_string( uint32_t index ) const;

        const char*                 intern( const char* string );

        StringMap*                  string_to_index;

        char*                       data                    = nullptr;
        uint32_t                    buffer_size             = 1024;
        uint32_t                    current_size            = 0;
        
        MemoryAllocator*            allocator               = nullptr;

    }; // struct StringArray


    
#endif // HY_STB

    //
    // Simple string that references another one. Used to reference strings in a stream of data.
    struct StringRef {

        size_t                      length;
        char*                       text;

        static bool                 equals( const StringRef& a, const StringRef& b );
        static void                 copy( const StringRef& a, char* buffer, size_t buffer_size );
    }; // struct StringRef


    //
    // Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination when needed.
    struct StringBuffer {

        void                        init( size_t size, MemoryAllocator* allocator );
        void                        terminate();

        void                        append( const char* format, ... );
        void                        append( const StringRef& text );
        void                        append( void* memory, size_t size );
        void                        append( const StringBuffer& other_buffer );

        char*                       append_use( const char* format, ... );
        char*                       append_use( const StringRef& text );    // Append and returns a pointer to the start. Used for strings mostly.
        char*                       append_use_substring( const char* string, uint32_t start_index, uint32_t end_index ); // Append a substring of the passed string.

        char*                       reserve( size_t size );

        void                        clear();

        char*                       data                    = nullptr;
        uint32_t                    buffer_size             = 1024;
        uint32_t                    current_size            = 0;
        MemoryAllocator*            allocator               = nullptr;

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
    using FileTime                  = FILETIME;
#else
    static_assert(false, "Platform not supported!");
#endif

    using FileHandle                = FILE*;

    //
    //
    struct Directory {
        char                        path[MAX_PATH];

#if defined (_WIN64)
        HANDLE                      os_handle;
#endif
    }; // struct Directory


    char*                           file_read( cstring filename, MemoryAllocator& allocator, size_t* size );

    char*                           file_read_into_memory( cstring filename, size_t* size, bool as_text );

    void                            file_open( cstring filename, cstring mode, FileHandle* file );
    void                            file_close( FileHandle file );
    size_t                          file_write( uint8_t* memory, uint32_t element_size, uint32_t count, FileHandle file );

    FileTime                        file_last_write_time( cstring filename );
    uint32_t                        file_full_path( cstring path, char* out_full_path, uint32_t max_size );
    void                            file_remove_filename( char* path );

    void                            file_open_directory( cstring path, Directory* out_directory );
    void                            file_close_directory( Directory* directory );
    void                            file_parent_directory( Directory* directory );
    void                            file_sub_directory( Directory* directory, cstring sub_directory_name );

#if defined (HY_STB)

    void                            file_find_files_in_path( cstring file_pattern, StringArray& files );            // Search files matching file_pattern and puts them in files array.
                                                                                                                    // Examples: "..\\data\\*", "*.bin", "*.*"
    void                            file_find_files_in_path( cstring extension, cstring search_pattern,
                                                             StringArray& files, StringArray& directories );        // Search files and directories using search_patterns.
#endif // HY_STB


    struct ScopedFile {
        ScopedFile( cstring filename, cstring mode );
        ~ScopedFile();

        FileHandle                  file;
    }; // struct ScopedFile

#endif // HY_FILE


    // Process //////////////////////////////////////////////////////////////////
#if defined(HY_PROCESS)

    bool                            process_execute( cstring working_directory, cstring process_fullpath, cstring arguments );

#endif // HY_PROCESS
    

    // Time /////////////////////////////////////////////////////////////////////
#if defined(HY_TIME)

    void                            time_service_init();                // Needs to be called once at startup.
    void                            time_service_terminate();           // Needs to be called at shutdown.

    int64_t                         time_now();                         // Get current time ticks.
    
    double                          time_microseconds( int64_t time );  // Get microseconds from time ticks
    double                          time_milliseconds( int64_t time );  // Get milliseconds from time ticks
    double                          time_seconds( int64_t time );       // Get seconds from time ticks

    int64_t                         time_from( int64_t starting_time ); // Get time difference from start to current time.
    double                          time_from_microseconds( int64_t starting_time ); // Convenience method.
    double                          time_from_milliseconds( int64_t starting_time ); // Convenience method.
    double                          time_from_seconds( int64_t starting_time );      // Convenience method.

#endif // HY_TIME

    // Memory /////////////////////////////////////////////////////////////////////

    //
    //
    struct MemoryAllocator {
        virtual void*               allocate( size_t size, size_t alignment ) = 0;
        virtual void                free( void* pointer ) = 0;
    }; // struct MemoryAllocator


    void                            memory_service_init();
    void                            memory_service_terminate();

    MemoryAllocator*                memory_get_system_allocator();


    void*                           hy_malloc( size_t size );
    void                            hy_free( void* data );

} // namespace hydra