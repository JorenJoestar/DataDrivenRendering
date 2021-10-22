#include "blob.hpp"

#include <stdarg.h>
#include <stdio.h>

void BlobSerializer::write_common( Allocator* allocator_, u32 serializer_version_, sizet size ) {
    allocator = allocator_;
    // Allocate memory
    blob_memory = ( char* )halloca( size + sizeof( BlobHeader ), allocator_ );
    hy_assert( blob_memory );

    total_size = ( u32 )size + sizeof( BlobHeader );
    serialized_offset = allocated_offset = 0;

    serializer_version = serializer_version_;
    // This will be written into the blob
    data_version = serializer_version_;
    is_reading = 0;
    is_mappable = 0;

    // Write header
    BlobHeader* header = ( BlobHeader* )allocate_static( sizeof( BlobHeader ) );
    header->version = serializer_version;
    header->mappable = is_mappable;

    serialized_offset = allocated_offset;
}

void BlobSerializer::shutdown() {

    serialized_offset = allocated_offset = 0;
}

void BlobSerializer::serialize( char* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( char ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( char ) );
    }

    serialized_offset += sizeof( char );
}

void BlobSerializer::serialize( i8* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( i8 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( i8 ) );
    }

    serialized_offset += sizeof( i8 );
}

void BlobSerializer::serialize( u8* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( u8 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( u8 ) );
    }

    serialized_offset += sizeof( u8 );
}

void BlobSerializer::serialize( i16* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( i16 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( i16 ) );
    }

    serialized_offset += sizeof( i16 );
}

void BlobSerializer::serialize( u16* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( u16 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( u16 ) );
    }

    serialized_offset += sizeof( u16 );
}

void BlobSerializer::serialize( i32* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( i32 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( i32 ) );
    }

    serialized_offset += sizeof( i32 );
}

void BlobSerializer::serialize( u32* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( u32 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( u32 ) );
    }

    serialized_offset += sizeof( u32 );
}

void BlobSerializer::serialize( i64* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( i64 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( i64 ) );
    }

    serialized_offset += sizeof( i64 );
}

void BlobSerializer::serialize( u64* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( u64 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( u64 ) );
    }

    serialized_offset += sizeof( u64 );
}

void BlobSerializer::serialize( f32* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( f32 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( f32 ) );
    }
    serialized_offset += sizeof( f32 );
}

void BlobSerializer::serialize( f64* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( f64 ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( f64 ) );
    }
}

void BlobSerializer::serialize( bool* data ) {
    if ( is_reading ) {
        memcpy( data, &blob_memory[ serialized_offset ], sizeof( bool ) );
    } else {
        memcpy( &blob_memory[ serialized_offset ], data, sizeof( bool ) );
    }
}

void BlobSerializer::serialize( cstring data ) {
    // sizet len = strlen( data );
}

void BlobSerializer::serialize( RelativeString* data ) {

    if ( is_reading ) {
        // Blob --> Data
        serialize( &data->size );

        i32 source_data_offset;
        serialize( &source_data_offset );

        if ( source_data_offset > 0 ) {
            // Cache serialized
            u32 cached_serialized = serialized_offset;

            serialized_offset = allocated_offset;

            data->data.offset = get_relative_data_offset( data ) - 4;

            // Reserve memory + string ending
            allocate_static( ( sizet )data->size + 1 );

            char* source_data = blob_memory + cached_serialized + source_data_offset - 4;
            memcpy( ( char* )data->c_str(), source_data, ( sizet )data->size + 1 );
            hprint( "Found %s\n", data->c_str() );
            // Restore serialized
            serialized_offset = cached_serialized;
        } else {
            data->set_empty();
        }
    } else {
        // Data --> Blob
        serialize( &data->size );
        // Data will be copied at the end of the current blob
        i32 data_offset = allocated_offset - serialized_offset;
        serialize( &data_offset );

        u32 cached_serialized = serialized_offset;
        // Move serialization to at the end of the blob.
        serialized_offset = allocated_offset;
        // Allocate memory in the blob
        allocate_static( ( sizet )data->size + 1 );

        char* destination_data = blob_memory + serialized_offset;
        memcpy( destination_data, ( char* )data->c_str(), ( sizet )data->size + 1 );
        hprint( "Written %s, Found %s\n", data->c_str(), destination_data );

        // Restore serialized
        serialized_offset = cached_serialized;
    }
}


void BlobSerializer::serialize( CharArray* data ) {
    if ( is_reading ) {
        // Blob --> Data
        serialize( &data->size );

        u32 packed_data_offset;
        serialize( &packed_data_offset );
        i32 source_data_offset = ( u32 )( packed_data_offset & 0x7fffffff );

        u64 data_guard;
        serialize( &data_guard );
        serialize( &data_guard );

        // Cache serialized
        u32 cached_serialized = serialized_offset;

        data->allocator = nullptr;
        data->capacity = data->size;
        data->relative = 0;// ( packed_data_offset >> 31 );
        // Point straight to the end
        data->data = ( char* )( data_memory + allocated_offset );

        // Reserve memory
        allocate_static( data->size * sizeof( char ) );
        // Blob memory contains also the BlobHeader, so to get the correct source data
        // we need to remove its size.
        // Remove also the guard size to restore the correct offset.
        char* source_data = blob_memory + cached_serialized + source_data_offset - sizeof(BlobHeader) - sizeof( u64 ) * 2;
        memcpy( ( char* )data->data, source_data, ( sizet )data->size + 1 );
        hprint( "Found %s\n", data->data );

        // Restore serialized
        serialized_offset = cached_serialized;

    } else {
        // Data --> Blob
        serialize( &data->size );
        // Data will be copied at the end of the current blob
        // We add 4 bytes because the offset is used starting from the address of
        // the size member, we can't take the address of a bitfield to calculate things!
        // Read Array::get() code for more details.
        i32 data_offset = allocated_offset - serialized_offset + sizeof(u32);
        // Set higher bit of flag
        u32 packed_data_offset = ( ( u32 )data_offset | ( 1 << 31 ) );
        serialize( &packed_data_offset );

        u64 data_guard = 0;
        serialize( &data_guard );
        serialize( &data_guard );

        u32 cached_serialized = serialized_offset;
        // Move serialization to the newly allocated memory,
        // at the end of the blob.
        serialized_offset = allocated_offset;
        // Allocate memory in the blob
        allocate_static( data->size * sizeof( char ) );

        char* destination_data = blob_memory + serialized_offset;
        memcpy( destination_data, ( char* )data->c_str(), ( sizet )data->size + 1 );
        hprint( "Written %s, Found %s\n", data->c_str(), destination_data );

        // Restore serialized
        serialized_offset = cached_serialized;
    }
}

char* BlobSerializer::allocate_static( sizet size ) {
    // TODO: handle memory reallocation correctly.
    if ( allocated_offset + size >= total_size ) {
        hy_assert( false );
        return nullptr;
    }

    u32 offset = allocated_offset;
    allocated_offset += ( u32 )size;

    return is_reading ? data_memory + offset : blob_memory + offset;
}


void BlobSerializer::allocate_and_set( CharArray& data, cstring string ) {

    if ( !string ) {
        data.relative = 0;
        data.size = 0;
        data.allocator = 0;
        data.capacity = 0;
        return;
    }
    
    const u32 array_serialized_offset = ( u32 )( ( char* )( &data ) - blob_memory );
    const u32 data_offset = allocated_offset - array_serialized_offset;
    const u32 num_elements = strlen( string ) + 1;

    char* data_memory = allocate_static( num_elements );
    data.relative = 1;
    data.size = num_elements;
    data.allocator = 0;
    data.capacity = data_offset;

    if ( string ) {
        memcpy( data_memory, string, num_elements );
    }
}

void BlobSerializer::allocate_and_set( RelativeString& string, cstring format, ... ) {

    u32 cached_offset = allocated_offset;

    char* destination_memory = is_reading ? data_memory : blob_memory;

    va_list args;
    va_start( args, format );
    int written_chars = vsnprintf_s( &destination_memory[ allocated_offset ], total_size - allocated_offset, _TRUNCATE, format, args );
    allocated_offset += written_chars > 0 ? written_chars : 0;
    va_end( args );

    if ( written_chars < 0 ) {
        hprint( "New string too big for current buffer! Please allocate more size.\n" );
    }

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    destination_memory[ allocated_offset ] = 0;
    ++allocated_offset;

    string.set( destination_memory + cached_offset, written_chars );
}

i32 BlobSerializer::get_relative_data_offset( void* data ) {
    // data_memory points to the newly allocated data structure to be used at runtime.
    const i32 data_offset_from_start = ( i32 )( ( char* )data - data_memory );
    const i32 data_offset = allocated_offset - data_offset_from_start;
    return data_offset;
}
