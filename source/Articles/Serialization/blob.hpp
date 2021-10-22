#pragma once

#include "serialization_demo.hpp"

// Memory Blob ////////////////////////////////////////////////////////////
//
//
// Memory blob used to serialize versioned data.
// Uses a serialized offset to track where to read/write memory from/to, and an allocated
// offset to track where to allocate memory from when writing, so that Relative structures
// like pointers and arrays can be serialized.
//
// TODO: when finalized and when reading, if data version matches between the one written in
// the file and the root structure is marked as 'relative only', memory mappable is doable and
// thus serialization is automatic.
//
struct BlobHeader {
    u32                 version;
    u32                 mappable;
}; // struct BlobHeader

struct Blob {
    BlobHeader    header;
}; // struct Blob

struct BlobSerializer {

    
    // Allocate size bytes, set the data version and start writing.
    // Data version will be saved at the beginning of the file.
    template <typename T>
    T*                  write_and_prepare( Allocator* allocator, u32 serializer_version, sizet size );

    template <typename T>
    void                write_and_serialize( Allocator* allocator, u32 serializer_version, sizet size, T* root_data );

    void                write_common( Allocator* allocator, u32 serializer_version, sizet size );

    // Init blob in reading mode from a chunk of preallocated memory.
    // Size is used to check wheter reading is happening outside of the chunk.
    // Allocator is used to allocate memory if needed (for example when reading an array).
    template <typename T>
    T*                  read( Allocator* allocator, u32 serializer_version, char* blob_memory, sizet blob_size, bool force_serialization = false );


    void                shutdown();

    // Methods used both for reading and writing.
    // Leaf of the serialization code.
    void                serialize( char* data );
    void                serialize( i8* data );
    void                serialize( u8* data );
    void                serialize( i16* data );
    void                serialize( u16* data );
    void                serialize( i32* data );
    void                serialize( u32* data );
    void                serialize( i64* data );
    void                serialize( u64* data );
    void                serialize( f32* data );
    void                serialize( f64* data );
    void                serialize( bool* data );
    void                serialize( cstring data );

    template <typename T>
    void                serialize( RelativePointer<T>* data );

    template <typename T>
    void                serialize( RelativeArray<T>* data );

    void                serialize( RelativeString* data );

    template <typename T>
    void                serialize( Array<T>* data );

    void                serialize( CharArray* data );

    template <typename T>
    void                serialize( T* data );


    // Static allocation from the blob allocated memory.
    char*               allocate_static( sizet size );  // Just allocate size bytes and return. Used to fill in structures.

    template <typename T>
    T*                  allocate_static();


    template <typename T>
    void                allocate_and_set( RelativePointer<T>& data, void* source_data = nullptr );

    template <typename T>
    void                allocate_and_set( RelativeArray<T>& data, u32 num_elements, void* source_data = nullptr );

    template <typename T>
    void                allocate_and_set( Array<T>& data, u32 num_elements, void* source_data = nullptr );

    void                allocate_and_set( CharArray& data, cstring string );

    void                allocate_and_set( RelativeString& string, cstring format, ... );

    i32                 get_relative_data_offset( void* data );

    char*               blob_memory         = nullptr;
    char*               data_memory         = nullptr;

    Allocator*          allocator           = nullptr;

    u32                 total_size          = 0;
    u32                 serialized_offset   = 0;
    u32                 allocated_offset    = 0;

    u32                 serializer_version  = 0xffffffff;   // Version coming from the code.
    u32                 data_version        = 0xffffffff;   // Version read from blob or written into blob.

    u16                 is_reading          = 0;
    u16                 is_mappable         = 0;

}; // struct BlobSerializer


// Implementation /////////////////////////////////////////////////////////

template<typename T>
T* BlobSerializer::write_and_prepare( Allocator* allocator_, u32 serializer_version_, sizet size ) {

    write_common( allocator_, serializer_version_, size );

    // Allocate root data
    allocate_static( sizeof( T ) );

    // Manually manage blob serialization.
    data_memory = nullptr;

    return ( T* )blob_memory;
}


template<typename T>
void BlobSerializer::write_and_serialize( Allocator* allocator_, u32 serializer_version_, sizet size, T* data ) {

    hy_assert( data ); // Should always have data passed as parameter!

    write_common( allocator_, serializer_version_, size );

    // Allocate root data
    allocate_static( sizeof( T ) );

    // Save root data memory for offset calculation
    data_memory = ( char* )data;
    // Serialize root data
    serialize( data );
}

template<typename T>
T* BlobSerializer::read( Allocator* allocator_, u32 serializer_version_, char* blob_memory_, sizet size, bool force_serialization ) {

    allocator = allocator_;
    blob_memory = blob_memory_;

    total_size = ( u32 )size;
    serialized_offset = allocated_offset = 0;

    serializer_version = serializer_version_;
    is_reading = 1;

    // Read header from blob.
    BlobHeader* header = ( BlobHeader* )blob_memory;
    data_version = header->version;
    is_mappable = header->mappable;

    // If serializer and data are at the same version, no need to serialize.
    // TODO: is mappable should be taken in consideration.
    if ( serializer_version == data_version && !force_serialization ) {
        return ( T* )( blob_memory );// +sizeof( BlobHeader ) );
    }

    serializer_version = data_version;

    // Allocate data
    data_memory = ( char* )hallocam( size, allocator );
    T* destination_data = ( T* )data_memory;

    serialized_offset += sizeof( BlobHeader );

    allocate_static( sizeof( T ) );
    // Read from blob to data
    serialize( destination_data );

    return destination_data;
}


template<typename T>
inline void BlobSerializer::serialize( T* data ) {
    // Should not arrive here!
    hy_assert( false );
}

template<typename T>
void BlobSerializer::serialize( RelativePointer<T>* data ) {
    if ( is_reading ) {
        // READING!
        // Blob --> Data
        i32 source_data_offset;
        serialize( &source_data_offset );

        // Early out to not follow null pointers.
        if ( source_data_offset == 0 ) {
            data->offset = 0;
            return;
        }

        data->offset = get_relative_data_offset( data );

        // Allocate memory and set pointer
        allocate_static<T>();

        // Cache source serialized offset.
        u32 cached_serialized = serialized_offset;
        // Move serialization offset.
        // The offset is still "this->offset", and the serialized offset
        // points just right AFTER it, thus move back by sizeof(offset).
        serialized_offset = cached_serialized + source_data_offset - sizeof(u32);
        // Serialize/visit the pointed data structure
        serialize( data->get() );
        // Restore serialization offset
        serialized_offset = cached_serialized;
    } else {
        // WRITING!
        // Data --> Blob
        // Calculate offset used by RelativePointer.
        // Remember this:
        // char* address = ( ( char* )&this->offset ) + offset;
        // Serialized offset points to what will be the "this->offset"
        // Allocated offset points to the still not allocated memory,
        // Where we will allocate from.
        i32 data_offset = allocated_offset - serialized_offset;
        serialize( &data_offset );

        // To jump anywhere and correctly restore the serialization process,
        // cache the current serialization offset
        u32 cached_serialized = serialized_offset;
        // Move serialization to the newly allocated memory at the end of the blob.
        serialized_offset = allocated_offset;
        // Allocate memory in the blob
        allocate_static<T>();
        // Serialize/visit the pointed data structure
        serialize( data->get() );
        // Restore serialized
        serialized_offset = cached_serialized;
    }
}

template<typename T>
inline void BlobSerializer::serialize( RelativeArray<T>* data ) {

    if ( is_reading ) {
        // Blob --> Data
        serialize( &data->size );

        i32 source_data_offset;
        serialize( &source_data_offset );

        // Cache serialized
        u32 cached_serialized = serialized_offset;

        data->data.offset = get_relative_data_offset( data ) - sizeof(u32);

        // Reserve memory
        allocate_static( data->size * sizeof( T ) );

        serialized_offset = cached_serialized + source_data_offset - sizeof(u32);

        for ( u32 i = 0; i < data->size; ++i ) {
            T* destination = &data->get()[ i ];
            serialize( destination );

            destination = destination;
        }
        // Restore serialized
        serialized_offset = cached_serialized;

    } else {
        // Data --> Blob
        serialize( &data->size );
        // Data will be copied at the end of the current blob
        i32 data_offset = allocated_offset - serialized_offset;
        serialize( &data_offset );

        u32 cached_serialized = serialized_offset;
        // Move serialization to the newly allocated memory,
        // at the end of the blob.
        serialized_offset = allocated_offset;
        // Allocate memory in the blob
        allocate_static( data->size * sizeof( T ) );

        for ( u32 i = 0; i < data->size; ++i ) {
            T* source_data = &data->get()[ i ];
            serialize( source_data );

            source_data = source_data;
        }
        // Restore serialized
        serialized_offset = cached_serialized;
    }
}

template<typename T>
inline void BlobSerializer::serialize( Array<T>* data ) {

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
        data->relative = ( packed_data_offset >> 31 );
        // Point straight to the end
        data->data = ( T* )( data_memory + allocated_offset );

        // Reserve memory
        allocate_static( data->size * sizeof( T ) );

        // When reading, remove also the guard size to restore the correct offset.
        serialized_offset = cached_serialized + source_data_offset - sizeof(BlobHeader) - sizeof(u64) * 2;

        for ( u32 i = 0; i < data->size; ++i ) {
            T* destination = &( ( *data )[ i ] );
            serialize( destination );

            destination = destination;
        }
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
        allocate_static( data->size * sizeof( T ) );

        for ( u32 i = 0; i < data->size; ++i ) {
            T* source_data = &( ( *data )[ i ] );
            serialize( source_data );

            source_data = source_data;
        }
        // Restore serialized
        serialized_offset = cached_serialized;
    }
}

template<typename T>
inline T* BlobSerializer::allocate_static() {
    return ( T* )allocate_static( sizeof( T ) );
}

template<typename T>
void BlobSerializer::allocate_and_set( RelativePointer<T>& data, void* source_data ) {
    char* data_memory = allocate_static( sizeof( T ) );
    data.set( data_memory );

    if ( source_data ) {
        memcpy( data_memory, source_data, sizeof( T ) );
    }
}

template<typename T>
inline void BlobSerializer::allocate_and_set( RelativeArray<T>& data, u32 num_elements, void* source_data ) {
    char* data_memory = allocate_static( sizeof( T ) * num_elements );
    data.set( data_memory, num_elements );

    if ( source_data ) {
        memcpy( data_memory, source_data, sizeof( T ) * num_elements );
    }
}

template<typename T>
inline void BlobSerializer::allocate_and_set( Array<T>& data, u32 num_elements, void* source_data ) {
    u32 array_serialized_offset = (u32)(( char* )( &data ) - blob_memory);
    
    u32 data_offset = allocated_offset - array_serialized_offset;

    char* data_memory = allocate_static( sizeof( T ) * num_elements );
    //data.set( data_memory, num_elements );
    data.relative = 1;
    data.size = num_elements;
    data.allocator = 0;
    data.capacity = data_offset;

    if ( source_data ) {
        memcpy( data_memory, source_data, sizeof( T ) * num_elements );
    }
}
