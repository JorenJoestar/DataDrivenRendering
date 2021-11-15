#include "graphics/gpu_device.hpp"

#include "kernel/memory.hpp"

namespace hydra {
namespace gfx {


void Device::init( const DeviceCreation& creation ) {

    hprint( "Gpu Device init\n" );
    // 1. Perform common code
    allocator = creation.allocator;
    string_buffer.init( 1024 * 1024, creation.allocator );
    
    // 2. Perform backend specific code
    backend_init( creation );
}

void Device::shutdown() {
    
    backend_shutdown();

    string_buffer.shutdown();

    hprint( "Gpu Device shutdown\n" );
}


// Device ///////////////////////////////////////////////////////////////////////

BufferHandle Device::get_fullscreen_vertex_buffer() const {
    return fullscreen_vertex_buffer;
}

RenderPassHandle Device::get_swapchain_pass() const {
    return swapchain_pass;
}

TextureHandle Device::get_dummy_texture() const {
    return dummy_texture;
}

BufferHandle Device::get_dummy_constant_buffer() const {
    return dummy_constant_buffer;
}

void Device::resize( uint16_t width, uint16_t height ) {
    swapchain_width = width;
    swapchain_height = height;

    resized = true;
}


// Resource Access //////////////////////////////////////////////////////////////
ShaderStateAPIGnostic* Device::access_shader_state( ShaderStateHandle shader ) {
    return (ShaderStateAPIGnostic*)shaders.access_resource( shader.index );
}

const ShaderStateAPIGnostic* Device::access_shader_state( ShaderStateHandle shader ) const {
    return (const ShaderStateAPIGnostic*)shaders.access_resource( shader.index );
}

TextureAPIGnostic* Device::access_texture( TextureHandle texture ) {
    return (TextureAPIGnostic*)textures.access_resource( texture.index );
}

const TextureAPIGnostic * Device::access_texture( TextureHandle texture ) const {
    return (const TextureAPIGnostic*)textures.access_resource( texture.index );
}

BufferAPIGnostic* Device::access_buffer( BufferHandle buffer ) {
    return (BufferAPIGnostic*)buffers.access_resource( buffer.index );
}

const BufferAPIGnostic* Device::access_buffer( BufferHandle buffer ) const {
    return (const BufferAPIGnostic*)buffers.access_resource( buffer.index );
}

PipelineAPIGnostic* Device::access_pipeline( PipelineHandle pipeline ) {
    return (PipelineAPIGnostic*)pipelines.access_resource( pipeline.index );
}

const PipelineAPIGnostic* Device::access_pipeline( PipelineHandle pipeline ) const {
    return (const PipelineAPIGnostic*)pipelines.access_resource( pipeline.index );
}

SamplerAPIGnostic* Device::access_sampler( SamplerHandle sampler ) {
    return (SamplerAPIGnostic*)samplers.access_resource( sampler.index );
}

const SamplerAPIGnostic* Device::access_sampler( SamplerHandle sampler ) const {
    return (const SamplerAPIGnostic*)samplers.access_resource( sampler.index );
}

ResourceLayoutAPIGnostic* Device::access_resource_layout( ResourceLayoutHandle resource_layout ) {
    return (ResourceLayoutAPIGnostic*)resource_layouts.access_resource( resource_layout.index );
}

const ResourceLayoutAPIGnostic* Device::access_resource_layout( ResourceLayoutHandle resource_layout ) const {
    return (const ResourceLayoutAPIGnostic*)resource_layouts.access_resource( resource_layout.index );
}

ResourceListAPIGnostic* Device::access_resource_list( ResourceListHandle resource_list ) {
    return (ResourceListAPIGnostic*)resource_lists.access_resource( resource_list.index );
}

const ResourceListAPIGnostic* Device::access_resource_list( ResourceListHandle resource_list ) const {
    return (const ResourceListAPIGnostic*)resource_lists.access_resource( resource_list.index );
}

RenderPassAPIGnostic* Device::access_render_pass( RenderPassHandle render_pass ) {
    return (RenderPassAPIGnostic*)render_passes.access_resource( render_pass.index );
}

const RenderPassAPIGnostic* Device::access_render_pass( RenderPassHandle render_pass ) const {
    return (const RenderPassAPIGnostic*)render_passes.access_resource( render_pass.index );
}
/*
// Building Helpers /////////////////////////////////////////////////////////////

// SortKey //////////////////////////////////////////////////////////////////////
static const u64                    k_stage_shift               = (56);

u64 SortKey::get_key( u64 stage_index ) {
    return ( ( stage_index << k_stage_shift ) );
}*/

// GPU Timestamp Manager ////////////////////////////////////////////////////////

void GPUTimestampManager::init( Allocator* allocator_, u16 queries_per_frame_, u16 max_frames ) {

    allocator = allocator_;
    queries_per_frame = queries_per_frame_;

    // Data is start, end in 2 u64 numbers.
    const u32 k_data_per_query = 2;
    const sizet allocated_size = sizeof( GPUTimestamp ) * queries_per_frame * max_frames + sizeof( u64 ) * queries_per_frame * max_frames * k_data_per_query;
    u8* memory = hallocam( allocated_size, allocator );

    timestamps = ( GPUTimestamp* )memory;
    // Data is start, end in 2 u64 numbers.
    timestamps_data = ( u64* )( memory + sizeof( GPUTimestamp ) * queries_per_frame * max_frames );

    reset();
}

void GPUTimestampManager::shutdown() {

    hfree( timestamps, allocator );
}

void GPUTimestampManager::reset() {
    current_query = 0;
    parent_index = 0;
    current_frame_resolved = false;
    depth = 0;
}

bool GPUTimestampManager::has_valid_queries() const {
    // Even number of queries means asymettrical queries, thus we don't sample.
    return current_query > 0 && (depth == 0);
}

u32 GPUTimestampManager::resolve( u32 current_frame, GPUTimestamp* timestamps_to_fill ) {
    hydra::memory_copy( timestamps_to_fill, &timestamps[ current_frame * queries_per_frame ], sizeof( GPUTimestamp ) * current_query );
    return current_query;
}

u32 GPUTimestampManager::push( u32 current_frame, const char* name ) {
    u32 query_index = ( current_frame * queries_per_frame ) + current_query;

    GPUTimestamp& timestamp = timestamps[ query_index ];
    timestamp.parent_index = (u16)parent_index;
    timestamp.start = query_index * 2;
    timestamp.end = timestamp.start + 1;
    timestamp.name = name;
    timestamp.depth = (u16)depth++;

    parent_index = current_query;
    ++current_query;

    return (query_index * 2);
}

u32 GPUTimestampManager::pop( u32 current_frame ) {

    u32 query_index = ( current_frame * queries_per_frame ) + parent_index;
    GPUTimestamp& timestamp = timestamps[ query_index ];
    // Go up a level
    parent_index = timestamp.parent_index;
    --depth;

    return ( query_index * 2 ) + 1;
}

DeviceCreation& DeviceCreation::set_window( u32 width_, u32 height_, void* handle ) {
    width = ( u16 )width_;
    height = ( u16 )height_;
    window = handle;
    return *this;
}

DeviceCreation& DeviceCreation::set_allocator( Allocator* allocator_ ) {
    allocator = allocator_;
    return *this;
}

} // namespace gfx
} // namespace hydra
