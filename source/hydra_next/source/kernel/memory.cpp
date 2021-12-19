#include "memory.hpp"
#include "memory_utils.hpp"
#include "assert.hpp"

#include "external/tlsf.h"

#include <stdlib.h>
#include <memory.h>

#include "imgui/imgui.h"

// Define this and add StackWalker to heavy memory profile
//#define HYDRA_MEMORY_BEAST

#if defined (HYDRA_MEMORY_BEAST)
#include "external/StackWalker.h"
#endif // HYDRA_MEMORY_BEAST

namespace hydra {

//#define HYDRA_MEMORY_DEBUG
#if defined (HYDRA_MEMORY_DEBUG)
    #define hy_mem_assert(cond) hy_assert(cond)
#else
    #define hy_mem_assert(cond)
#endif // HYDRA_MEMORY_DEBUG

// Memory Service /////////////////////////////////////////////////////////
static MemoryService    s_memory_service;

// Locals
static size_t s_size = 1024 * 1024 * 30 + tlsf_size() + 8;

//
// Walker methods
static void exit_walker( void* ptr, size_t size, int used, void* user );
static void imgui_walker( void* ptr, size_t size, int used, void* user );

MemoryService* MemoryService::instance() {
    return &s_memory_service;
}

//
//
void MemoryService::init( void* configuration ) {

    hprint( "Memory Service Init\n" );
    
    system_allocator.init( s_size );
}

void MemoryService::shutdown() {

    system_allocator.shutdown();

    hprint( "Memory Service Shutdown\n" );
}

void exit_walker( void* ptr, size_t size, int used, void* user ) {
    MemoryStatistics* stats = ( MemoryStatistics* )user;
    stats->add( used ? size : 0 );

    if ( used )
        hprint( "Found active allocation %p, %llu\n", ptr, size );
}
void imgui_walker( void* ptr, size_t size, int used, void* user ) {
    ImGui::Text( "\t%p %s size: %llu\n", ptr, used ? "used" : "free", ( unsigned int )size );

    MemoryStatistics* stats = ( MemoryStatistics* )user;
    stats->add( used ? size : 0 );
}

#if defined HYDRA_IMGUI
void MemoryService::imgui_draw() {

    if ( ImGui::Begin( "Memory Service" ) ) {

        system_allocator.debug_ui();
    }
    ImGui::End();
}
#endif // HYDRA_IMGUI

void MemoryService::test() {

    //static u8 mem[ 1024 ];
    //LinearAllocator la;
    //la.init( mem, 1024 );

    //// Allocate 3 times
    //void* a1 = halloca( 16, &la );
    //void* a2 = halloca( 20, &la );
    //void* a4 = halloca( 10, &la );
    //// Free based on size
    //la.free( 10 );
    //void* a3 = halloca( 10, &la );
    //hy_assert( a3 == a4 );

    //// Free based on pointer
    //hfree( a2, &la );
    //void* a32 = halloca( 10, &la );
    //hy_assert( a32 == a2 );
    //// Test out of bounds 
    //u8* out_bounds = ( u8* )a1 + 10000;
    //hfree( out_bounds, &la );
}

// Memory Structs /////////////////////////////////////////////////////////

// HeapAllocator //////////////////////////////////////////////////////////
HeapAllocator::~HeapAllocator() {
}

void HeapAllocator::init( sizet size ) {
    // Allocate
    memory = malloc( size );
    allocated_size = size;

    tlsf_handle = tlsf_create_with_pool( memory, size );

    hprint( "HeapAllocator of size %llu created\n", size );
}

void HeapAllocator::shutdown() {

    // Check memory at the application exit.
    MemoryStatistics stats{ 0, allocated_size };
    pool_t pool = tlsf_get_pool( tlsf_handle );
    tlsf_walk_pool( pool, exit_walker, ( void* )&stats );

    if ( stats.allocated_bytes ) {
        hprint( "HeapAllocator Shutdown.\n===============\nFAILURE! Allocated memory detected. allocated %llu, total %llu\n===============\n\n", stats.allocated_bytes, stats.total_bytes );
    } else {
        hprint( "HeapAllocator Shutdown - all memory free!\n" );
    }

    hy_assertm( stats.allocated_bytes == 0, "Allocations still present. Check your code!" );

    tlsf_destroy( tlsf_handle );

    free( memory );
}

#if defined HYDRA_IMGUI
void HeapAllocator::debug_ui() {

    ImGui::Separator();
    ImGui::Text( "Heap Allocator" );
    ImGui::Separator();
    MemoryStatistics stats{ 0, s_size };
    pool_t pool = tlsf_get_pool( tlsf_handle );
    tlsf_walk_pool( pool, imgui_walker, ( void* )&stats );

    ImGui::Separator();
    ImGui::Text( "\tAllocation count %d", stats.allocation_count );
    ImGui::Text( "\tAllocated %llu K, free %llu K, total %llu K", stats.allocated_bytes / 1024, ( s_size - stats.allocated_bytes ) / 1024, s_size / 1024 );
}
#endif // HYDRA_IMGUI


#if defined (HYDRA_MEMORY_BEAST)
class HydraStackWalker : public StackWalker {
public:
    HydraStackWalker() : StackWalker() {}
protected:
    virtual void OnOutput( LPCSTR szText ) {
        hprint( "\nStack: \n%s\n", szText );
        StackWalker::OnOutput( szText );
    }
}; // class HydraStackWalker

void* HeapAllocator::allocate( sizet size, sizet alignment ) {
    
    if ( size == 16 ) 
    {
        HydraStackWalker sw;
        sw.ShowCallstack();
    }

    void* mem = tlsf_malloc( tlsf_handle, size );
    hprint( "Mem: %p, size %llu \n", mem, size );
    return mem;
}
#else

void* HeapAllocator::allocate( sizet size, sizet alignment ) {
    return tlsf_malloc( tlsf_handle, size );
}
#endif // HYDRA_MEMORY_BEAST

void* HeapAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void HeapAllocator::deallocate( void* pointer ) {
    tlsf_free( tlsf_handle, pointer );
}

// LinearAllocator /////////////////////////////////////////////////////////

LinearAllocator::~LinearAllocator() {
}

void LinearAllocator::init( sizet size ) {

    memory = ( u8* )malloc( size );
    total_size = size;
    allocated_size = 0;
}

void LinearAllocator::shutdown() {
    clear();
    free( memory );
}

void* LinearAllocator::allocate( sizet size, sizet alignment ) {
    hy_assert( size > 0 );

    const sizet new_start = memory_align( allocated_size, alignment );
    hy_assert( new_start < total_size );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size > total_size ) {
        hy_mem_assert( false && "Overflow" );
        return nullptr;
    }

    allocated_size = new_allocated_size;
    return memory + new_start;
}

void* LinearAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void LinearAllocator::deallocate( void*  ) {
    // This allocator does not allocate on a per-pointer base!
}

void LinearAllocator::clear() {
    allocated_size = 0;
}

// Memory Methods /////////////////////////////////////////////////////////
void memory_copy( void* destination, void* source, sizet size ) {
    memcpy( destination, source, size );
}


void* MallocAllocator::allocate( sizet size, sizet alignment ) {
    return malloc( size );
}

void* MallocAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return malloc( size );
}

void MallocAllocator::deallocate( void* pointer ) {
    free( pointer );
}

// StackAllocator ////////////////////////////////////////////////////////
void StackAllocator::init( sizet size ) {
    memory = (u8*)malloc( size );
    allocated_size = 0;
    total_size = size;
}

void StackAllocator::shutdown() {
    free( memory );
}

void* StackAllocator::allocate( sizet size, sizet alignment ) {
    hy_assert( size > 0 );

    const sizet new_start = memory_align( allocated_size, alignment );
    hy_assert( new_start < total_size );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size > total_size ) {
        hy_mem_assert( false && "Overflow" );
        return nullptr;
    }

    allocated_size = new_allocated_size;
    return memory + new_start;
}

void* StackAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void StackAllocator::deallocate( void* pointer ) {

    hy_assert( pointer >= memory );
    hy_assertm( pointer < memory + total_size, "Out of bound free on linear allocator (outside bounds). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", ( u8* )pointer, ( u8* )pointer - memory, memory, total_size, allocated_size );
    hy_assertm( pointer < memory + allocated_size, "Out of bound free on linear allocator (inside bounds, after allocated). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", ( u8* )pointer, ( u8* )pointer - memory, memory, total_size, allocated_size );

    const sizet size_at_pointer = ( u8* )pointer - memory;

    allocated_size = size_at_pointer;
}

sizet StackAllocator::get_marker() {
    return allocated_size;
}

void StackAllocator::free_marker( sizet marker ) {
    const sizet difference = marker - allocated_size;
    if ( difference > 0 ) {
        allocated_size = marker;
    }
}

void StackAllocator::clear() {
    allocated_size = 0;
}

// DoubleStackAllocator //////////////////////////////////////////////////
void DoubleStackAllocator::init( sizet size ) {
    memory = ( u8* )malloc( size );
    top = size;
    bottom = 0;
    total_size = size;
}

void DoubleStackAllocator::shutdown() {
    free( memory );
}

void* DoubleStackAllocator::allocate( sizet size, sizet alignment ) {
    hy_assert( false );
    return nullptr;
}

void* DoubleStackAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    hy_assert( false );
    return nullptr;
}

void DoubleStackAllocator::deallocate( void* pointer ) {
    hy_assert( false );
}

void* DoubleStackAllocator::allocate_top( sizet size, sizet alignment ) {
    hy_assert( size > 0 );

    const sizet new_start = memory_align( top - size, alignment );
    if ( new_start <= bottom  ) {
        hy_mem_assert( false && "Overflow Crossing" );
        return nullptr;
    }

    top = new_start;
    return memory + new_start;
}

void* DoubleStackAllocator::allocate_bottom( sizet size, sizet alignment ) {
    hy_assert( size > 0 );

    const sizet new_start = memory_align( bottom, alignment );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size >= top ) {
        hy_mem_assert( false && "Overflow Crossing" );
        return nullptr;
    }

    bottom = new_allocated_size;
    return memory + new_start;
}

void DoubleStackAllocator::deallocate_top( sizet size ) {
    if ( size > total_size - top ) {
        top = total_size;
    } else {
        top += size;
    }
}

void DoubleStackAllocator::deallocate_bottom( sizet size ) {
    if ( size > bottom ) {
        bottom = 0;
    } else {
        bottom -= size;
    }
}

sizet DoubleStackAllocator::get_top_marker() {
    return top;
}

sizet DoubleStackAllocator::get_bottom_marker() {
    return bottom;
}

void DoubleStackAllocator::free_top_marker( sizet marker ) {
    if ( marker > top && marker < total_size ) {
        top = marker;
    }
}

void DoubleStackAllocator::free_bottom_marker( sizet marker ) {
    if ( marker < bottom ) {
        bottom = marker;
    }
}

void DoubleStackAllocator::clear_top() {
    top = total_size;
}

void DoubleStackAllocator::clear_bottom() {
    bottom = 0;
}

} // namespace hydra