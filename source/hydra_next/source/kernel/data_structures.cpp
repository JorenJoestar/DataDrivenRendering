#include "kernel/data_structures.hpp"

namespace hydra {

    static const u32                    k_invalid_index = 0xffffffff;

// Resource Pool ////////////////////////////////////////////////////////////////

void ResourcePool::init( Allocator* allocator_, u32 pool_size_, u32 resource_size_ ) {

    allocator = allocator_;
    pool_size = pool_size_;
    resource_size = resource_size_;

    // Group allocate ( resource size + u32 )
    memory = (u8*)allocator->allocate( pool_size * (resource_size + sizeof(u32)), 1 );

    // Allocate and add free indices
    free_indices = ( u32* )( memory + pool_size * resource_size );
    free_indices_head = 0;

    for ( u32 i = 0; i < pool_size; ++i ) {
        free_indices[i] = i;
    }
}

void ResourcePool::shutdown() {
    hy_assert( free_indices_head == 0 );
    allocator->deallocate( memory );
}

void ResourcePool::free_all_resources() {
    free_indices_head = 0;

    for ( uint32_t i = 0; i < pool_size; ++i ) {
        free_indices[i] = i;
    }
}

u32 ResourcePool::obtain_resource() {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    if ( free_indices_head < pool_size ) {
        const u32 free_index = free_indices[free_indices_head++];
        return free_index;
    }
    // Error: no more resources left!
    hy_assert( false );
    return k_invalid_index;
}

void ResourcePool::release_resource( u32 handle ) {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    free_indices[--free_indices_head] = handle;
}

void* ResourcePool::access_resource( u32 handle ) {
    if ( handle != k_invalid_index ) {
        return &memory[handle * resource_size];
    }
    return nullptr;
}

const void* ResourcePool::access_resource( u32 handle ) const {
    if ( handle != k_invalid_index ) {
        return &memory[handle * resource_size];
    }
    return nullptr;
}


} // namespace hydra