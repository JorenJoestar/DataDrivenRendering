#pragma once

#include "kernel/primitive_types.hpp"
#include "kernel/memory.hpp"
#include "kernel/assert.hpp"
#include "kernel/array.hpp"

// Defines:
// HYDRA_BLOB_WRITE         - use it in code that can write blueprints,
//                            like data compilers.
#define HYDRA_BLOB_WRITE

namespace hydra {

struct Allocator;

//
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

#if defined HYDRA_BLOB_WRITE
    void                set( char* raw_pointer );
    void                set_null();
#endif // HYDRA_BLOB_WRITE

    i32                 offset;

}; // struct RelativePointer


// RelativeArray //////////////////////////////////////////////////////////

//
//
template <typename T>
struct RelativeArray {

    const T&                operator[](u32 index) const;
    T&                      operator[](u32 index);

    const T*                get() const;
    T*                      get();

#if defined HYDRA_BLOB_WRITE
    void                    set( char* raw_pointer, u32 size );
    void                    set_empty();
#endif // HYDRA_BLOB_WRITE

    u32                     size;
    RelativePointer<T>      data;
}; // struct RelativeArray


// RelativeString /////////////////////////////////////////////////////////

//
//
struct RelativeString : public RelativeArray<char> {

    cstring             c_str() const { return data.get(); }

    void                set( char* pointer_, u32 size_ ) { RelativeArray<char>::set( pointer_, size_ ); }
}; // struct RelativeString



// Implementations/////////////////////////////////////////////////////////

// RelativePointer ////////////////////////////////////////////////////////
template<typename T>
inline T* RelativePointer<T>::get() const {
    char* address = ( ( char* )&offset ) + offset;
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

#if defined HYDRA_BLOB_WRITE
/* // TODO: useful or not ?
template<typename T>
inline void RelativePointer<T>::set( T* pointer ) {
    offset = pointer ? ( i32 )( ( ( char* )pointer ) - ( char* )this ) : 0;
}*/

template<typename T>
inline void RelativePointer<T>::set( char* raw_pointer ) {
    offset = raw_pointer ? ( i32 )( raw_pointer - ( char* )this ) : 0;
}
template<typename T>
inline void RelativePointer<T>::set_null() {
    offset = 0;
}
#endif // HYDRA_BLOB_WRITE

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

#if defined HYDRA_BLOB_WRITE
template<typename T>
inline void RelativeArray<T>::set( char* raw_pointer, u32 size_ ) {
    data.set( raw_pointer );
    size = size_;
}
template<typename T>
inline void RelativeArray<T>::set_empty() {
    size = 0;
    data.set_null();
}
#endif // HYDRA_BLOB_WRITE

} // namespace hydra