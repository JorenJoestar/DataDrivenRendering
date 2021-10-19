#pragma once

#include "kernel/primitive_types.hpp"

#include <string.h>

// Utility code ///////////////////////////////////////////////////////////


#define ArraySize(array)        ( sizeof(array)/sizeof((array)[0]) )


#if defined (_MSC_VER)
#define HY_PERFHUD
#define HY_INLINE                               inline
#define HY_FINLINE                              __forceinline
#define HY_DEBUG_BREAK                          __debugbreak();
#else

#endif // MSVC

#define HY_STRINGIZE( L )                       #L 
#define HY_MAKESTRING( L )                      HY_STRINGIZE( L )
#define HY_CONCAT_OPERATOR(x, y)                x##y
#define HY_CONCAT(x, y)                         HY_CONCAT_OPERATOR(x, y)
#define HY_LINE_STRING                          HY_MAKESTRING( __LINE__ ) 
#define HY_FILELINE(MESSAGE)                    __FILE__ "(" HY_LINE_STRING ") : " MESSAGE


void    hprint( cstring format, ... );


#define hy_assert( condition )      if (!(condition)) { hprint(HY_FILELINE("FALSE\n")); HY_DEBUG_BREAK }

#define halloca(size, allocator)    ((allocator)->allocate( size, 1, __FILE__, __LINE__ ))
#define hallocam(size, allocator)   ((u8*)(allocator)->allocate( size, 1, __FILE__, __LINE__ ))

#define hfree(pointer, allocator)   (allocator)->deallocate(pointer)

struct Allocator;

// Utility methods
char* file_read_text( cstring filename, Allocator* allocator, sizet * size );
char* file_read_binary( cstring filename, Allocator * allocator, sizet * size );
void file_write_binary( cstring filename, void* memory, sizet size );

// Data structures used in the demo.

//
// Allocator //////////////////////////////////////////////////////////////
// 
struct Allocator {
    void*               allocate( sizet size, sizet alignment );
    void*               allocate( sizet size, sizet alignment, cstring file, i32 line );

    void                deallocate( void* pointer );
}; // struct Allocator

//
// RelativePointer ////////////////////////////////////////////////////////
// 
template <typename T>
struct RelativePointer {

    T*                  get() const;

    bool                is_equal( const RelativePointer& other ) const;
    bool                is_null() const;
    bool                is_not_null() const;

    // Operator overloading to give a cleaner interface
    T*                  operator->() const;
    T&                  operator*() const;

    void                set( char* raw_pointer );
    void                set_null();

    i32                 offset;

}; // struct RelativePointer


//
// RelativeArray //////////////////////////////////////////////////////////
//
template <typename T>
struct RelativeArray {

    const T&                operator[]( u32 index ) const;
    T&                      operator[]( u32 index );

    const T*                get() const;
    T*                      get();

    void                    set( char* raw_pointer, u32 size );
    void                    set_empty();

    u32                     size;
    RelativePointer<T>      data;
}; // struct RelativeArray


//
// RelativeString /////////////////////////////////////////////////////////
//
struct RelativeString : public RelativeArray<char> {

    cstring                     c_str() const { return data.get(); }

    void                        set( char* pointer, u32 size ) { RelativeArray<char>::set( pointer, size ); }
}; // struct RelativeString


//
// Array //////////////////////////////////////////////////////////////////
//
template <typename T>
struct Array {

    Array();
    ~Array();

    void                        init( Allocator* allocator, u32 initial_capacity, u32 initial_size = 0 );
    void                        shutdown();

    void                        push( const T& element );
    void                        pop();
    void                        delete_swap( u32 index );

    T&                          operator[]( u32 index );
    const T&                    operator[]( u32 index ) const;

    // Relative data interface
    const T*                    get() const;
    T*                          get();

    void                        clear();
    void                        set_size( u32 new_size );
    void                        set_capacity( u32 new_capacity );
    void                        grow( u32 new_capacity );

    u32                         size_in_bytes() const;
    u32                         capacity_in_bytes() const;


    u32                         size;       // Occupied size
    u32                         capacity : 31;   // Allocated capacity
    u32                         relative : 1;
    T*                          data;
    Allocator*                  allocator;

}; // struct Array

//
// CharArray //////////////////////////////////////////////////////////////
// 
struct CharArray : public Array<char> {

    void                        init( Allocator* allocator, cstring string );

    char*                       str() { return relative ? (char*)get() : data; }
    cstring                     c_str() const {
        if ( relative ) {
            char* address = ( ( char* )&size ) + capacity;
            return capacity != 0 ? ( cstring )address : nullptr;
        }
        return (cstring)data;
    }

}; // struct CharArray



// Implementations/////////////////////////////////////////////////////////


// RelativePointer ////////////////////////////////////////////////////////
template<typename T>
inline T* RelativePointer<T>::get() const {
    char* address = ( ( char* )&this->offset ) + offset;
    return offset != 0 ? ( T* )address : nullptr;
}

template<typename T>
inline bool RelativePointer<T>::is_equal( const RelativePointer& other ) const {
    return get() == other.get();
}

template<typename T>
inline bool RelativePointer<T>::is_null() const {
    return offset == 0;
}

template<typename T>
inline bool RelativePointer<T>::is_not_null() const {
    return offset != 0;
}

template<typename T>
inline T* RelativePointer<T>::operator->() const {
    return get();
}

template<typename T>
inline T& RelativePointer<T>::operator*() const {
    return *( get() );
}


template<typename T>
inline void RelativePointer<T>::set( char* raw_pointer ) {
    offset = raw_pointer ? ( i32 )( raw_pointer - ( char* )this ) : 0;
   /* if ( abs( raw_pointer - ( char* )this ) > u32_max ) {
        hy_assert( false );
    }*/
}

template<typename T>
inline void RelativePointer<T>::set_null() {
    offset = 0;
}

// RelativeArray //////////////////////////////////////////////////////////
template<typename T>
inline const T& RelativeArray<T>::operator[]( u32 index ) const {
    hy_assert( index < size );
    return data.get()[ index ];
}

template<typename T>
inline T& RelativeArray<T>::operator[]( u32 index ) {
    hy_assert( index < size );
    return data.get()[ index ];
}

template<typename T>
inline const T* RelativeArray<T>::get() const {
    return data.get();
}

template<typename T>
inline T* RelativeArray<T>::get() {
    return data.get();
}

template<typename T>
inline void RelativeArray<T>::set( char* raw_pointer, u32 size_ ) {
    data.set( raw_pointer );
    size = size_;
}

template<typename T>
void RelativeArray<T>::set_empty() {
    data.set_null();
    size = 0;
}

// Array //////////////////////////////////////////////////////////////////
template<typename T>
inline Array<T>::Array() {
}

template<typename T>
inline Array<T>::~Array() {
}

template<typename T>
inline void Array<T>::init( Allocator* allocator_, u32 initial_capacity, u32 initial_size ) {
    data = nullptr;
    size = initial_size;
    capacity = 0;
    allocator = allocator_;

    if ( initial_capacity > 0 ) {
        grow( initial_capacity );
    }
}

template<typename T>
inline void Array<T>::shutdown() {
    if ( capacity > 0 ) {
        allocator->deallocate( data );
    }
    data = nullptr;
    size = capacity = 0;
}

template<typename T>
inline void Array<T>::push( const T& element ) {
    if ( size >= capacity ) {
        grow( capacity + 1 );
    }

    data[ size++ ] = element;
}

template<typename T>
inline void Array<T>::pop() {
    hy_assert( size > 0 );
    --size;
}

template<typename T>
inline void Array<T>::delete_swap( u32 index ) {
    hy_assert( size > 0 && index < size );
    data[ index ] = data[ --size ];
}

template<typename T>
inline T& Array<T>::operator []( u32 index ) {
    hy_assert( index < size );
    return data[ index ];
}

template<typename T>
inline const T& Array<T>::operator []( u32 index ) const {
    hy_assert( index < size );
    return data[ index ];
}

template<typename T>
inline void Array<T>::clear() {
    size = 0;
}

template<typename T>
inline void Array<T>::set_size( u32 new_size ) {
    if ( new_size > capacity ) {
        grow( new_size );
    }
    size = new_size;
}

template<typename T>
inline void Array<T>::set_capacity( u32 new_capacity ) {
    if ( new_capacity > capacity ) {
        grow( new_capacity );
    }
}

template<typename T>
inline void Array<T>::grow( u32 new_capacity ) {
    if ( new_capacity < capacity * 2 ) {
        new_capacity = capacity * 2;
    } else if ( new_capacity < 4 ) {
        new_capacity = 4;
    }

    T* new_data = ( T* )allocator->allocate( new_capacity * sizeof( T ), 1 );
    if ( capacity ) {
        memcpy( new_data, data, capacity * sizeof( T ) );

        allocator->deallocate( data );
    }

    data = new_data;
    capacity = new_capacity;
}

template<typename T>
inline u32 Array<T>::size_in_bytes() const {
    return size * sizeof( T );
}

template<typename T>
inline u32 Array<T>::capacity_in_bytes() const {
    return capacity * sizeof( T );
}

// Relative data access.
template<typename T>
inline T* Array<T>::get() {
    if ( relative ) {
        // NOTE: the address is stored relative to size member,
        // as we can't take the address of a bitfield!
        char* address = ( ( char* )&size ) + capacity;
        return capacity != 0 ? ( T* )address : nullptr;
    }
    return nullptr;
}


