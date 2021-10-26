#pragma once

#include "kernel/memory.hpp"
#include "kernel/assert.hpp"

namespace hydra {

    // Data structures ////////////////////////////////////////////////////

    // Array //////////////////////////////////////////////////////////////
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

        void                        clear();
        void                        set_size( u32 new_size );
        void                        set_capacity( u32 new_capacity );
        void                        grow( u32 new_capacity );

        u32                         size_in_bytes() const;
        u32                         capacity_in_bytes() const;


        T*                          data;
        u32                         size;       // Occupied size
        u32                         capacity;   // Allocated capacity
        Allocator*                  allocator;

    }; // struct Array


    // Implementation /////////////////////////////////////////////////////

    template<typename T>
    inline Array<T>::Array() {
        //hy_assert( true );
    }

    template<typename T>
    inline Array<T>::~Array() {
        //hy_assert( data == nullptr );
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
            memory_copy( new_data, data, capacity * sizeof( T ) );

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

} // namespace hydra