
#include "serialization_demo.hpp"
#include "serialization_examples.hpp"

#include "blob.hpp"
#include <iostream>

#include <stdint.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


// Allocator //////////////////////////////////////////////////////////////
void* Allocator::allocate( sizet size, sizet alignment ) {
    return malloc( size );
}

void* Allocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return malloc( size );
}

void Allocator::deallocate( void* pointer ) {
    free( pointer );
}

// Log ////////////////////////////////////////////////////////////////////

static constexpr u32    k_string_buffer_size = 1024 * 1024;
static char             log_buffer[ k_string_buffer_size ];

static void output_console( char* log_buffer_ ) {
    printf( "%s", log_buffer_ );
}

static void output_visual_studio( char* log_buffer_ ) {
    OutputDebugStringA( log_buffer_ );
}

void hprint( cstring format, ... ) {
    va_list args;

    va_start( args, format );
    vsnprintf_s( log_buffer, k_string_buffer_size, format, args );
    log_buffer[ k_string_buffer_size - 1 ] = '\0';
    va_end( args );

    output_console( log_buffer );
#if defined(_MSC_VER)
    output_visual_studio( log_buffer );
#endif // _MSC_VER
}

// CharArray //////////////////////////////////////////////////////////////
void CharArray::init( Allocator* allocator_, cstring string ) {

    size = ( u32 )( strlen( string ) + 1 );
    capacity = size;
    allocator = allocator_;

    data = ( char* )halloca( size, allocator );
    strcpy( data, string );
}

// File ///////////////////////////////////////////////////////////////////

static long file_get_size( FILE* f ) {
    long fileSizeSigned;

    fseek( f, 0, SEEK_END );
    fileSizeSigned = ftell( f );
    fseek( f, 0, SEEK_SET );

    return fileSizeSigned;
}

char* file_read_binary( cstring filename, Allocator* allocator, sizet* size ) {
    char* out_data = 0;

    FILE* file = fopen( filename, "rb" );

    if ( file ) {

        // TODO: Use filesize or read result ?
        sizet filesize = file_get_size( file );

        out_data = ( char* )halloca( filesize + 1, allocator );
        fread( out_data, filesize, 1, file );
        out_data[ filesize ] = 0;

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
        text = ( char* )halloca( filesize + 1, allocator );
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

void file_write_binary( cstring filename, void* memory, sizet size ) {
    FILE* file = fopen( filename, "wb" );
    fwrite( memory, size, 1, file );
    fclose( file );
}

// Vec2s //////////////////////////////////////////////////////////////////

// Forward declaration for template specialization.
template<>
void MemoryBlob::serialize<vec2s>( vec2s* data );

// Serialization binary
template<>
void MemoryBlob::serialize<vec2s>( vec2s* data ) {
    serialize( &data->x );
    serialize( &data->y );
}

// OtherData //////////////////////////////////////////////////////////////
// Data structure added just to have a pointer to test.
// 
struct OtherData {
    f32     a;
    u32     b;
};

template<>
void MemoryBlob::serialize<OtherData>( OtherData* data ) {
    serialize( &data->a );
    serialize( &data->b );
}

// GameDataV0 /////////////////////////////////////////////////////////////
//
// First version of the game data.
// Used to write older binaries and test versioning.
struct GameDataV0 {
    vec2s                   position;
    RelativeArray<u32>      all_effs;
    OtherData               other;
    RelativeString          name;
    RelativePointer<OtherData> other_pointer;

    static constexpr u32    k_version = 0;
}; // struct GameDataV0


template<>
void MemoryBlob::serialize<GameDataV0>( GameDataV0* data ) {
    serialize( &data->position );
    serialize( &data->all_effs );
    serialize( &data->other );
    serialize( &data->name );
    serialize( &data->other_pointer );
}

//
// Second version of game data.
// 
struct GameDataV1 {
    vec2s                   position;
    RelativeArray<u32>      all_effs;
    OtherData               other;
    RelativeArray<u32>      all_cs;         // Added in V1
    RelativeString          name;
    RelativePointer<OtherData> other_pointer;

    static constexpr u32    k_version = 1;
}; // struct GameDataV1


template<>
void MemoryBlob::serialize<GameDataV1>( GameDataV1* data ) {
    serialize( &data->position );
    serialize( &data->all_effs );
    serialize( &data->other );
    if ( serializer_version > 0 ) {
        serialize( &data->all_cs );
    }
    serialize( &data->name );
    serialize( &data->other_pointer );
}

//
//
struct GameData {
    vec2s                   position;
    RelativeArray<u32>      all_effs;
    OtherData               other;
    RelativeArray<u32>      all_cs;         // Added in V1
    Array<u32>              all_as;         // Added in V2
    CharArray               new_name;       // Added in V2
    RelativeString          name;
    RelativePointer<OtherData> other_pointer;

    static constexpr u32    k_version = 2;
}; // struct GameData

template<>
void MemoryBlob::serialize<GameData>( GameData* data ) {
    serialize( &data->position );
    serialize( &data->all_effs );
    serialize( &data->other );
    if ( serializer_version > 0 ) {
        serialize( &data->all_cs );
    }
    else {
        data->all_cs.set_empty();
    }
    if ( serializer_version > 1 ) {
        serialize( &data->all_as );
        serialize( &data->new_name );
    }
    else {
        
    }
    serialize( &data->name );
    serialize( &data->other_pointer );
}

static Allocator s_heap_allocator;

int main() {
    
    hprint( "Serialization demo\n" );

    Allocator* allocator = &s_heap_allocator;

    // 1. Resource Compilation and Inspection /////////////////////////////

    compile_cutscene( allocator, "..//data//articles//serializationdemo//cutscene.json", "..//data//bin//cutscene.bin" );
    inspect_cutscene( allocator, "..//data//bin//cutscene.bin" );

    compile_scene( allocator, "..//data//articles//serializationdemo//new_game.json", "..//data//bin//new_game.bin" );
    inspect_scene( allocator, "..//data//bin//new_game.bin" );

    // 2. Write GameDataV0 binary
    MemoryBlob write_blob_v0, read_blob_v0;
    {
        // NOTE: this is a non-optimal way of writing the blob, but still doable.
        // Write V0 and read with GameData (V2)
        char* memory = (char*)malloc( 1000 );
        memset( memory, 0, 1000 );

        GameDataV0* writing_data = ( GameDataV0* )memory;
        writing_data->position = { 100,200 };
        writing_data->other.a = 7.0f;
        writing_data->other.b = 0xffff;
        u32* effs = ( u32* )( memory + sizeof( GameDataV0 ) );
        *effs = 0xffffffff;
        char* name_memory = ( char* )( effs + 1 );
        strcpy( name_memory, "IncredibleName" );

        OtherData* other_pointer = ( OtherData* )( name_memory + strlen( name_memory ) + 1 );
        other_pointer->a = 16.f;
        other_pointer->b = 0xaaaaaaaa;

        // Close to the allocate_and_set method used in blobs.
        // Calculate the offset for the arrays
        writing_data->all_effs.set( ( char* )effs, 1 );
        writing_data->name.set( name_memory, strlen( name_memory ) );
        // Calculate the offset for the pointer
        writing_data->other_pointer.set( ( char* )other_pointer );
        // Write to blob using serialization methods, by passing the writing data.
        write_blob_v0.write( allocator, 0, 1000, writing_data );

        // 
        GameData* game_data = read_blob_v0.read<GameData>( allocator, GameData::k_version, write_blob_v0.blob_memory, write_blob_v0.allocated_offset * 2 );
        if ( game_data ) {
            OtherData* other_data = game_data->other_pointer.get();
            hy_assert( game_data->position.x == 100.f );
            hy_assert( other_data->a == 16.f );
            hy_assert( other_data->b == 0xaaaaaaaa );
            hy_assert( strcmp( game_data->name.c_str(), "IncredibleName" ) == 0 );
            hy_assert( game_data->all_effs[ 0 ] == 0xffffffff );

            hprint( "V0 Read Done %s!\n", game_data->name.c_str() );
        }
    }
    // Write GameDataV1 binary
    MemoryBlob write_blob_v1, read_blob_v1;
    {
        // Use write blob to already fill the data.
        // Allocate just a blob with 200 bytes
        write_blob_v1.write<GameDataV1>( allocator, 1, 200, nullptr );

        GameDataV1* game_data_v1 = ( GameDataV1* )write_blob_v1.allocate_static( sizeof( GameDataV1 ) );
        game_data_v1->position.x = 700.f;
        game_data_v1->position.y = 42.f;

        u32 all_effs[] = { 0xffffffff, 0xffffffff };
        write_blob_v1.allocate_and_set( game_data_v1->all_effs, 2, all_effs );

        // other
        game_data_v1->other.a = 8.f;
        game_data_v1->other.b = 0xbbbbbbbb;
        // all c
        u32 all_cs[] = { 0xcccccccc, 0xcccccccc, 0xcccccccc };
        write_blob_v1.allocate_and_set( game_data_v1->all_cs, 3, all_cs );
        // name
        cstring name_string = "GameDataV1Awesomeness";
        write_blob_v1.allocate_and_set( game_data_v1->name, "%s", name_string );
        // other pointer
        write_blob_v1.allocate_and_set( game_data_v1->other_pointer, nullptr );
        OtherData* other_data_pointed = game_data_v1->other_pointer.get();
        other_data_pointed->a = 32.f;
        other_data_pointed->b = 0xdddddddd;

        // Read V1 blob, again with the v2 serializer.
        GameData* game_data = read_blob_v1.read<GameData>( allocator, GameData::k_version, write_blob_v1.blob_memory, write_blob_v1.allocated_offset * 2 );
        if ( game_data ) {
            OtherData* other_data = game_data->other_pointer.get();
            hy_assert( game_data->position.x == 700.f );
            hy_assert( game_data->other.a == 8.f );
            hy_assert( game_data->other.b == 0xbbbbbbbb );
            hy_assert( other_data->a == 32.f );
            hy_assert( other_data->b == 0xdddddddd );
            hy_assert( strcmp( game_data->name.c_str(), "GameDataV1Awesomeness" ) == 0 );
            hy_assert( game_data->all_effs[ 1 ] == 0xffffffff );
            hy_assert( game_data->other_pointer->a == 32.f );

            hprint( "V1 Read Done %s!\n", game_data->name.c_str() );
        }
    }

    // Write GameDataV2 binary
    MemoryBlob write_blob_v2, read_blob_v2, read_blob_v2_serialized;
    {
        write_blob_v2.write<GameData>( allocator, GameData::k_version, 300, nullptr );

        GameData* game_data = ( GameData* )write_blob_v2.allocate_static( sizeof( GameData ) );
        game_data->position.x = 700.f;
        game_data->position.y = 42.f;

        u32 all_effs[] = { 0xffffffff, 0xffffffff };
        write_blob_v2.allocate_and_set( game_data->all_effs, 2, all_effs );

        // other
        game_data->other.a = 8.f;
        game_data->other.b = 0xbbbbbbbb;
        // all c
        u32 all_cs[] = { 0xcccccccc, 0xcccccccc, 0xcccccccc };
        write_blob_v2.allocate_and_set( game_data->all_cs, 3, all_cs );
        // all as
        u32 all_as[] = { 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa };
        write_blob_v2.allocate_and_set( game_data->all_as, 4, all_as );
        // new name
        cstring new_name_string = "GameDataV2Plus";
        write_blob_v2.allocate_and_set( game_data->new_name, new_name_string );
        // name
        cstring name_string = "GameDataV2Awesomeness";
        write_blob_v2.allocate_and_set( game_data->name, "%s", name_string );
        // other pointer
        write_blob_v2.allocate_and_set( game_data->other_pointer, nullptr );
        OtherData* other_data_pointed = game_data->other_pointer.get();
        other_data_pointed->a = 32.f;
        other_data_pointed->b = 0xdddddddd;

        // Read V2 blob. This is MEMORY MAPPED.
        GameData* mmap_game_data = read_blob_v2.read<GameData>( allocator, GameData::k_version, write_blob_v2.blob_memory, write_blob_v2.allocated_offset * 2 );
        // Force serialization of game data and test fields.
        GameData* serialized_game_data = read_blob_v2_serialized.read<GameData>( allocator, GameData::k_version, write_blob_v2.blob_memory, write_blob_v2.allocated_offset * 2, true );
        if ( mmap_game_data && serialized_game_data ) {
            OtherData* mmap_other_data = mmap_game_data->other_pointer.get();
            OtherData* serialized_other_data = serialized_game_data->other_pointer.get();
            hy_assert( mmap_game_data->position.x == serialized_game_data->position.x );
            hy_assert( mmap_game_data->position.x == 700.f );
            hy_assert( mmap_other_data->a == serialized_other_data->a );
            hy_assert( mmap_other_data->a == 32.f );
            hy_assert( mmap_other_data->b == serialized_other_data->b );
            hy_assert( mmap_other_data->b == 0xdddddddd );
            hy_assert( strcmp( mmap_game_data->name.c_str(), serialized_game_data->name.c_str() ) == 0 );
            hy_assert( mmap_game_data->all_effs[ 1 ] == serialized_game_data->all_effs[ 1 ] );
            hy_assert( mmap_game_data->all_effs[ 1 ] == 0xffffffff );
            hy_assert( mmap_game_data->other_pointer->a == serialized_game_data->other_pointer->a );
            hy_assert( mmap_game_data->all_as.get()[3] == serialized_game_data->all_as[ 3 ] );
            hy_assert( serialized_game_data->all_as[ 3 ] == 0xaaaaaaaa );
            hy_assert( serialized_game_data->all_as[ 0 ] == 0xaaaaaaaa );
            hy_assert( mmap_game_data->all_as.get()[ 3 ] == 0xaaaaaaaa );
            hy_assert( mmap_game_data->all_as.get()[ 0 ] == 0xaaaaaaaa );
            cstring aa0 = mmap_game_data->new_name.get();
            hy_assert( strcmp( mmap_game_data->new_name.get(), "GameDataV2Plus" ) == 0 );
            cstring aa1 = serialized_game_data->new_name.c_str();
            hy_assert( strcmp( serialized_game_data->new_name.c_str(), "GameDataV2Plus" ) == 0 );

            hprint( "V2 Read Done %s!\n", mmap_game_data->name.c_str() );
        }
    }

    hprint( "Test finished SUCCESSFULLY!\n" );
}
