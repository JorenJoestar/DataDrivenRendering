#pragma once

#include "kernel/memory.hpp"
#include "kernel/assert.hpp"

namespace hydra {

    //
    //
    struct ResourcePool {

        void                            init( Allocator* allocator, u32 pool_size, u32 resource_size );
        void                            shutdown();

        u32                             obtain_resource();      // Returns an index to the resource
        void                            release_resource( u32 index );
        void                            free_all_resources();

        void*                           access_resource( u32 index );
        const void*                     access_resource( u32 index ) const;

        u8*                             memory          = nullptr;
        u32*                            free_indices    = nullptr;
        Allocator*                      allocator       = nullptr;

        u32                             free_indices_head   = 0;
        u32                             pool_size           = 16;
        u32                             resource_size       = 4;

    }; // struct ResourcePool

    //
    //
    template <typename T>
    struct ResourcePoolTyped : public ResourcePool {

        void                            init( Allocator* allocator, u32 pool_size );
        void                            shutdown();

        T*                              obtain();
        void                            release( T* resource );

        T*                              get( u32 index);
        const T*                        get( u32 index) const;

    }; // struct ResourcePoolTyped

    template<typename T>
    inline void ResourcePoolTyped<T>::init( Allocator* allocator, u32 pool_size ) {
        ResourcePool::init( allocator, pool_size, sizeof( T ) );
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::shutdown() {
        ResourcePool::shutdown();
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::obtain() {
        u32 resource_index = ResourcePool::obtain_resource();
        T* resource = get( resource_index );
        resource->pool_index = resource_index;
        return resource;
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::release( T* resource ) {
        ResourcePool::release_resource( resource->pool_index );
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::get( u32 index ) {
        return ( T* )ResourcePool::access_resource( index );
    }

    template<typename T>
    inline const T* ResourcePoolTyped<T>::get( u32 index ) const {
        return ( const T* )ResourcePool::access_resource( index );
    }

} // namespace hydra