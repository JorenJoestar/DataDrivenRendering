//
//  Hydra Graphics - v0.28

#include "hydra_graphics.h"

#include <string.h>
#include <malloc.h>

#if defined (HYDRA_GFX_SDL)
#include <SDL.h>
#include <SDL_vulkan.h>
#endif // HYDRA_GFX_SDL

#if defined (HYDRA_VULKAN)
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif // HYDRA_VULKAN

// Malloc/free leak checking library
//#include "stb_leakcheck.h"

// Defines //////////////////////////////////////////////////////////////////////

// Use shared libraries to enhance different functions.
#define HYDRA_LIB
//#define HYDRA_PROFILER
//#define HYDRA_MULTITHREADING

#if defined(HYDRA_LIB)
    #include "hydra_lib.h"

    #define HYDRA_LOG                                           hydra::print_format

    #define HYDRA_MALLOC( size )                                hydra::hy_malloc( size )
    #define HYDRA_FREE( pointer )                               hydra::hy_free( pointer )

#else

    #define HYDRA_LOG                                           printf

    #define HYDRA_MALLOC( size )                                malloc( size )
    #define HYDRA_FREE( pointer )                               free( pointer )

#endif // HYDRA_LIB

#if !defined(HYDRA_LOG)
#include <stdio.h>
    #define HYDRA_LOG( message, ... )                           printf(message, ##__VA_ARGS__)
#endif // HYDRA_LOG

#if !defined(HYDRA_ASSERT)
#include <assert.h>
    #define HYDRA_ASSERT( condition, message, ... )             assert( condition )
#endif // HYDRA_ASSET

#if !defined(ArrayLength)
    #define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )
#endif // ArrayLength

#if defined (HYDRA_PROFILER)
    #include "optick/optick.h"
#else
    #define OPTICK_EVENT(a)
    #define OPTICK_TAG(a, b)
#endif // HYDRA_PROFILER

#if defined (HYDRA_MULTITHREADING)
    #include "enkits/TaskScheduler.h"
#endif // HYDRA_MULTITHREADING

// Sorting: for now use std::sort
#include <algorithm>

#if !defined(HYDRA_STRINGBUFFER)
#include <stdio.h>
#include <string>
#include <windows.h>

struct StringBufferGfx {

    void init( uint32_t size ) {
        if ( data ) {
            free( data );
        }

        data = (char*)malloc( size );
        buffer_size = size;
        current_size = 0;
    }

    void terminate() {
        free( data );
    }

    void append( const char* format, ... ) {
        if ( current_size >= buffer_size ) {
            HYDRA_LOG( "String buffer overflow! Buffer size %u\n", buffer_size );
            return;
        }

        va_list args;
        va_start( args, format );
        int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
        current_size += written_chars;
        va_end( args );
    }


    char* append_use( const char* format, ... ) {
        uint32_t cached_offset = this->current_size;

        // TODO: safer version!
        // TODO: do not copy paste!
        if ( current_size >= buffer_size ) {
            printf( "Buffer full! Please allocate more size.\n" );
            return nullptr;
        }

        va_list args;
        va_start( args, format );
        int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
        current_size += written_chars > 0 ? written_chars : 0;
        va_end( args );

        // Add null termination for string.
        // By allocating one extra character for the null termination this is always safe to do.
        data[current_size] = 0;
        ++current_size;

        return this->data + cached_offset;
    }

    void clear() {
        current_size = 0;
    }

    char* reserve( size_t size ) {
        if ( current_size + size >= buffer_size )
            return nullptr;

        uint32_t offset = current_size;
        current_size += (uint32_t)size;

        return data + offset;
    }

    char*                           data                = nullptr;
    uint32_t                        buffer_size         = 1024;
    uint32_t                        current_size        = 0;
}; // struct StringBuffer

#define HYDRA_STRINGBUFFER StringBufferGfx
#endif // HYDRA_STRINGBUFFER


/////////////////////////////////////////////////////////////////////////////////

namespace hydra {
namespace graphics {

// Resource Pool ////////////////////////////////////////////////////////////////

void ResourcePool::init( uint32_t pool_size, uint32_t resource_size_ ) {

    this->size = pool_size;
    this->resource_size = resource_size_;

    memory = (uint8_t*)HYDRA_MALLOC( pool_size * resource_size );

    // Allocate and add free indices
    free_indices = (uint32_t*)HYDRA_MALLOC( pool_size * sizeof( uint32_t ) );
    free_indices_head = 0;

    for ( uint32_t i = 0; i < pool_size; ++i ) {
        free_indices[i] = i;
    }
}

void ResourcePool::terminate() {

    HYDRA_FREE( memory );
    HYDRA_FREE( free_indices );
}

void ResourcePool::free_all_resources() {
    free_indices_head = 0;

    for ( uint32_t i = 0; i < size; ++i ) {
        free_indices[i] = i;
    }
}

uint32_t ResourcePool::obtain_resource() {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    if ( free_indices_head < size ) {
        const uint32_t free_index = free_indices[free_indices_head++];
        return free_index;
    }

    return k_invalid_handle;
}

void ResourcePool::release_resource( uint32_t handle ) {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    free_indices[--free_indices_head] = handle;
}

void* ResourcePool::access_resource( uint32_t handle ) {
    if ( handle != k_invalid_handle ) {
        return &memory[handle * resource_size];
    }
    return nullptr;
}

const void* ResourcePool::access_resource( uint32_t handle ) const {
    if ( handle != k_invalid_handle ) {
        return &memory[handle * resource_size];
    }
    return nullptr;
}


// Device ///////////////////////////////////////////////////////////////////////

HYDRA_STRINGBUFFER s_string_buffer;

void Device::init( const DeviceCreation& creation ) {
    // 1. Perform common code
    s_string_buffer.init( 1024 * 10 );

    // 2. Perform backend specific code
    backend_init( creation );
}

void Device::terminate() {
    
    backend_terminate();
    
    s_string_buffer.terminate();
}

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
    return (ShaderStateAPIGnostic*)shaders.access_resource( shader.handle );
}

const ShaderStateAPIGnostic* Device::access_shader_state( ShaderStateHandle shader ) const {
    return (const ShaderStateAPIGnostic*)shaders.access_resource( shader.handle );
}

TextureAPIGnostic* Device::access_texture( TextureHandle texture ) {
    return (TextureAPIGnostic*)textures.access_resource( texture.handle );
}

const TextureAPIGnostic * Device::access_texture( TextureHandle texture ) const {
    return (const TextureAPIGnostic*)textures.access_resource( texture.handle );
}

BufferAPIGnostic* Device::access_buffer( BufferHandle buffer ) {
    return (BufferAPIGnostic*)buffers.access_resource( buffer.handle );
}

const BufferAPIGnostic* Device::access_buffer( BufferHandle buffer ) const {
    return (const BufferAPIGnostic*)buffers.access_resource( buffer.handle );
}

PipelineAPIGnostic* Device::access_pipeline( PipelineHandle pipeline ) {
    return (PipelineAPIGnostic*)pipelines.access_resource( pipeline.handle );
}

const PipelineAPIGnostic* Device::access_pipeline( PipelineHandle pipeline ) const {
    return (const PipelineAPIGnostic*)pipelines.access_resource( pipeline.handle );
}

SamplerAPIGnostic* Device::access_sampler( SamplerHandle sampler ) {
    return (SamplerAPIGnostic*)samplers.access_resource( sampler.handle );
}

const SamplerAPIGnostic* Device::access_sampler( SamplerHandle sampler ) const {
    return (const SamplerAPIGnostic*)samplers.access_resource( sampler.handle );
}

ResourceListLayoutAPIGnostic* Device::access_resource_list_layout( ResourceListLayoutHandle resource_layout ) {
    return (ResourceListLayoutAPIGnostic*)resource_list_layouts.access_resource( resource_layout.handle );
}

const ResourceListLayoutAPIGnostic* Device::access_resource_list_layout( ResourceListLayoutHandle resource_layout ) const {
    return (const ResourceListLayoutAPIGnostic*)resource_list_layouts.access_resource( resource_layout.handle );
}

ResourceListAPIGnostic* Device::access_resource_list( ResourceListHandle resource_list ) {
    return (ResourceListAPIGnostic*)resource_lists.access_resource( resource_list.handle );
}

const ResourceListAPIGnostic* Device::access_resource_list( ResourceListHandle resource_list ) const {
    return (const ResourceListAPIGnostic*)resource_lists.access_resource( resource_list.handle );
}

RenderPassAPIGnostic* Device::access_render_pass( RenderPassHandle render_pass ) {
    return (RenderPassAPIGnostic*)render_passes.access_resource( render_pass.handle );
}

const RenderPassAPIGnostic* Device::access_render_pass( RenderPassHandle render_pass ) const {
    return (const RenderPassAPIGnostic*)render_passes.access_resource( render_pass.handle );
}

// Building Helpers /////////////////////////////////////////////////////////////

// DepthStencilCreation ////////////////////////////////////

DepthStencilCreation& DepthStencilCreation::set_depth( bool write, ComparisonFunction::Enum comparison_test ) {
    depth_write_enable = write;
    depth_comparison = comparison_test;
    // Setting depth like this means we want to use the depth test.
    depth_enable = 1;

    return *this;
}

// BlendState  /////////////////////////////////////////////
BlendState& BlendState::set_color( Blend::Enum source, Blend::Enum destination, BlendOperation::Enum operation ) {
    source_color = source;
    destination_color = destination;
    color_operation = operation;
    blend_enabled = 1;

    return *this;
}

BlendState& BlendState::set_alpha( Blend::Enum source, Blend::Enum destination, BlendOperation::Enum operation ) {
    source_alpha = source;
    destination_alpha = destination;
    alpha_operation = operation;
    separate_blend = 1;

    return *this;
}

BlendState& BlendState::set_color_write_mask( ColorWriteEnabled::Mask value ) {
    color_write_mask = value;

    return *this;
}

// BlendStateCreation //////////////////////////////////////
BlendStateCreation& BlendStateCreation::reset() {
    active_states = 0;

    return *this;
}

BlendState& BlendStateCreation::add_blend_state() {
    return blend_states[active_states++];
}

// BufferCreation //////////////////////////////////////////

BufferCreation& BufferCreation::set( BufferType::Enum type_, ResourceUsageType::Enum usage_, uint32_t size_ ) {
    type = type_;
    usage = usage_;
    size = size_;

    return *this;
}

BufferCreation& BufferCreation::set_data( void* data_ ) {
    initial_data = data_;

    return *this;
}

BufferCreation& BufferCreation::set_name( const char* name_ ) {
    name = name_;

    return *this;
}

// TextureCreation /////////////////////////////////////////
TextureCreation& TextureCreation::set_size( uint16_t width_, uint16_t height_, uint16_t depth_ ) {
    width = width_;
    height = height_;
    depth = depth_;

    return *this;
}

TextureCreation& TextureCreation::set_flags( uint8_t mipmaps_, uint8_t flags_ ) {
    mipmaps = mipmaps_;
    flags = flags_;

    return *this;
}

TextureCreation& TextureCreation::set_format_type( TextureFormat::Enum format_, TextureType::Enum type_ ) {
    format = format_;
    type = type_;

    return *this;
}

TextureCreation& TextureCreation::set_name( const char* name_ ) {
    name = name_;

    return *this;
}

TextureCreation& TextureCreation::set_data( void* data_ ) {
    initial_data = data_;

    return *this;
}

// SamplerCreation /////////////////////////////////////////
SamplerCreation& SamplerCreation::set_min_mag_mip( TextureFilter::Enum min, TextureFilter::Enum mag, TextureMipFilter::Enum mip ) {
    min_filter = min;
    mag_filter = mag;
    mip_filter = mip;

    return *this;
}

SamplerCreation& SamplerCreation::set_address_mode_u( TextureAddressMode::Enum u ) {
    address_mode_u = u;

    return *this;
}

SamplerCreation& SamplerCreation::set_address_mode_uv( TextureAddressMode::Enum u, TextureAddressMode::Enum v ) {
    address_mode_u = u;
    address_mode_v = v;

    return *this;
}

SamplerCreation& SamplerCreation::set_address_mode_uvw( TextureAddressMode::Enum u, TextureAddressMode::Enum v, TextureAddressMode::Enum w ) {
    address_mode_u = u;
    address_mode_v = v;
    address_mode_w = w;

    return *this;
}

SamplerCreation& SamplerCreation::set_name( const char* name_ ) {
    name = name_;

    return *this;
}


// ShaderStateCreation /////////////////////////////////////
ShaderStateCreation& ShaderStateCreation::reset() {
    stages_count = 0;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::set_name( const char* name_ ) {
    name = name_;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::add_stage( const char* code, uint32_t code_size, ShaderStage::Enum type ) {
    stages[stages_count].code = code;
    stages[stages_count].code_size = code_size;
    stages[stages_count].type = type;
    ++stages_count;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::set_spv_input( bool value ) {
    spv_input = value;
    return *this;
}

// ResourceListLayoutCreation //////////////////////////////
ResourceListLayoutCreation& ResourceListLayoutCreation::reset() {
    num_bindings = 0;

    return *this;
}

ResourceListLayoutCreation& ResourceListLayoutCreation::add_binding( const Binding& binding ) {
    bindings[num_bindings++] = binding;
    return *this;
}

ResourceListLayoutCreation& ResourceListLayoutCreation::set_name( const char* name ) {
    this->name = name;
    return *this;
}


// ResourceListCreation ////////////////////////////////////
ResourceListCreation& ResourceListCreation::reset() {
    num_resources = 0;
    return *this;
}

ResourceListCreation& ResourceListCreation::set_layout( ResourceListLayoutHandle layout_ ) {
    layout = layout_;
    return *this;
}

ResourceListCreation& ResourceListCreation::add_resource( ResourceHandle resource ) {
    resources[num_resources++] = resource;
    return *this;
}

ResourceListCreation& ResourceListCreation::set_resources( ResourceHandle* resources_, uint32_t num_resources_ ) {
    memcpy( &resources[0], resources_, num_resources_ * sizeof( ResourceHandle ) );
    num_resources = num_resources_;
    return *this;
}

ResourceListCreation& ResourceListCreation::set_name( const char* name ) {
    this->name = name;
    return *this;
}

// VertexInputCreation /////////////////////////////////////
VertexInputCreation& VertexInputCreation::reset() {
    num_vertex_streams = num_vertex_attributes = 0;
    return *this;
}

VertexInputCreation& VertexInputCreation::add_vertex_stream( const VertexStream& stream ) {
    vertex_streams[num_vertex_streams++] = stream;
    return *this;
}

VertexInputCreation& VertexInputCreation::add_vertex_attribute( const VertexAttribute& attribute ) {
    vertex_attributes[num_vertex_attributes++] = attribute;
    return *this;
}

// PipelineCreation ////////////////////////////////////////
PipelineCreation& PipelineCreation::add_resource_list_layout( ResourceListLayoutHandle handle ) {
    resource_list_layout[num_active_layouts++] = handle;
    return *this;
}

// RenderPassCreation //////////////////////////////////////
RenderPassCreation& RenderPassCreation::reset() {
    num_render_targets = 0;
    depth_stencil_texture.handle = k_invalid_handle;
    resize = 0;
    scale_x = 1.f;
    scale_y = 1.f;

    return *this;
}

RenderPassCreation& RenderPassCreation::add_render_texture( TextureHandle texture ) {
    output_textures[num_render_targets++] = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_scaling( f32 scale_x_, f32 scale_y_, u8 resize_ ) {
    scale_x = scale_x_;
    scale_y = scale_y_;
    resize = resize_;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_depth_stencil_texture( TextureHandle texture ) {
    depth_stencil_texture = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_name( const char* name_ ) {
    name = name_;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_type( RenderPassType::Enum type_ ) {
    type = type_;

    return *this;
}

// ExecutionBarrier ////////////////////////////////////////
ExecutionBarrier& ExecutionBarrier::set( PipelineStage::Enum source, PipelineStage::Enum destination ) {
    source_pipeline_stage = source;
    destination_pipeline_stage = destination;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::add_image_barrier( const ImageBarrier& image_barrier ) {
    image_barriers[num_image_barriers++] = image_barrier;

    return *this;
}

// SortKey //////////////////////////////////////////////////////////////////////
static const u64                    k_stage_shift               = (56);

u64 SortKey::get_key( u64 stage_index ) {
    return ( ( stage_index << k_stage_shift ) );
}

// GPU Timestamp Manager ////////////////////////////////////////////////////////

struct GPUTimestampManager {

    void                            init( u16 queries_per_frame );
    void                            shutdown();

    void                            reset();
    u32                             resolve( u32 current_frame, GPUTimestamp* timestamps_to_fill );    // Returns the total queries for this frame.

    u32                             push( u32 current_frame, const char* name );    // Returns the timestamp query index.
    u32                             pop( u32 current_frame );

    GPUTimestamp*                   timestamps                  = nullptr;
    u64*                            timestamps_data             = nullptr;

    u32                             queries_per_frame           = 0;
    u32                             current_query               = 0;
    u32                             parent_index                = 0;
    u32                             depth                       = 0;

    bool                            current_frame_resolved      = false;    // Used to query the GPU only once per frame if get_gpu_timestamps is called more than once per frame.

}; // struct GPUTimestampManager

void GPUTimestampManager::init( u16 queries_per_frame_ ) {

    queries_per_frame = queries_per_frame_;

    timestamps = ( GPUTimestamp* )malloc( sizeof(GPUTimestamp) * queries_per_frame * Device::k_max_frames );
    // Data is start, end in 2 u64 numbers.
    timestamps_data = ( u64* )malloc( sizeof( u64 ) * queries_per_frame * Device::k_max_frames * 2 );

    reset();
}

void GPUTimestampManager::shutdown() {

    free( timestamps );
    free( timestamps_data );
}

void GPUTimestampManager::reset() {
    current_query = 0;
    parent_index = 0;
    current_frame_resolved = false;
    depth = 0;
}

u32 GPUTimestampManager::resolve( u32 current_frame, GPUTimestamp* timestamps_to_fill ) {
    memcpy( timestamps_to_fill, &timestamps[ current_frame * queries_per_frame ], sizeof( GPUTimestamp ) * current_query );
    return current_query;
}

u32 GPUTimestampManager::push( u32 current_frame, const char* name ) {
    u32 query_index = ( current_frame * queries_per_frame ) + current_query;

    GPUTimestamp& timestamp = timestamps[ query_index ];
    timestamp.parent_index = parent_index;
    timestamp.start = query_index * 2;
    timestamp.end = timestamp.start + 1;
    timestamp.name = name;
    timestamp.depth = depth++;

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

// TODO:

// API Specific /////////////////////////////////////////////////////////////////


// OpenGL ///////////////////////////////////////////////////////////////////////

#if defined (HYDRA_OPENGL)

// Defines

// Enum translations. Use tables or switches depending on the case.
static GLuint to_gl_target( TextureType::Enum type ) {
    static GLuint s_gl_target[TextureType::Count] = { GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_CUBE_MAP_ARRAY };
    return s_gl_target[type];
}

// Following the tables here:
// https://www.khronos.org/opengl/wiki/GLAPI/glTexImage2D
// https://gist.github.com/Kos/4739337
//
static GLuint to_gl_internal_format( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::R32G32B32A32_FLOAT:
            return GL_RGBA32F;
        case TextureFormat::R32G32B32A32_UINT:
            return GL_RGBA32UI;
        case TextureFormat::R32G32B32A32_SINT:
            return GL_RGBA32I;
        case TextureFormat::R32G32B32_FLOAT:
            return GL_RGB32F;
        case TextureFormat::R32G32B32_UINT:
            return GL_RGB32UI;
        case TextureFormat::R32G32B32_SINT:
            return GL_RGB32I;
        case TextureFormat::R16G16B16A16_FLOAT:
            return GL_RGBA16F;
        case TextureFormat::R16G16B16A16_UNORM:
            return GL_RGBA16;
        case TextureFormat::R16G16B16A16_UINT:
            return GL_RGBA16UI;
        case TextureFormat::R16G16B16A16_SNORM:
            return GL_RGBA16_SNORM;
        case TextureFormat::R16G16B16A16_SINT:
            return GL_RGBA16I;
        case TextureFormat::R32G32_FLOAT:
            return GL_RG32F;
        case TextureFormat::R32G32_UINT:
            return GL_RG32UI;
        case TextureFormat::R32G32_SINT:
            return GL_RG32I;
        case TextureFormat::R10G10B10A2_TYPELESS:
            return GL_RGB10_A2;
        case TextureFormat::R10G10B10A2_UNORM:
            return GL_RGB10_A2;
        case TextureFormat::R10G10B10A2_UINT:
            return GL_RGB10_A2UI;
        case TextureFormat::R11G11B10_FLOAT:
            return GL_R11F_G11F_B10F;
        case TextureFormat::R8G8B8A8_TYPELESS:
            return GL_RGBA8;
        case TextureFormat::R8G8B8A8_UNORM:
            return GL_RGBA8;
        case TextureFormat::R8G8B8A8_UNORM_SRGB:
            return GL_SRGB8_ALPHA8;
        case TextureFormat::R8G8B8A8_UINT:
            return GL_RGBA8UI;
        case TextureFormat::R8G8B8A8_SNORM:
            return GL_RGBA8_SNORM;
        case TextureFormat::R8G8B8A8_SINT:
            return GL_RGBA8I;
        case TextureFormat::R16G16_TYPELESS:
            return GL_RG16UI;
        case TextureFormat::R16G16_FLOAT:
            return GL_RG16F;
        case TextureFormat::R16G16_UNORM:
            return GL_RG16;
        case TextureFormat::R16G16_UINT:
            return GL_RG16UI;
        case TextureFormat::R16G16_SNORM:
            return GL_RG16_SNORM;
        case TextureFormat::R16G16_SINT:
            return GL_RG16I;
        case TextureFormat::R32_TYPELESS:
            return GL_R32UI;
        case TextureFormat::R32_FLOAT:
            return GL_R32F;
        case TextureFormat::R32_UINT:
            return GL_R32UI;
        case TextureFormat::R32_SINT:
            return GL_R32I;
        case TextureFormat::R8G8_TYPELESS:
            return GL_RG8UI;
        case TextureFormat::R8G8_UNORM:
            return GL_RG8;
        case TextureFormat::R8G8_UINT:
            return GL_RG8UI;
        case TextureFormat::R8G8_SNORM:
            return GL_RG8_SNORM;
        case TextureFormat::R8G8_SINT:
            return GL_RG8I;
        case TextureFormat::R16_TYPELESS:
            return GL_R16UI;
        case TextureFormat::R16_FLOAT:
            return GL_R16F;
        case TextureFormat::R16_UNORM:
            return GL_R16;
        case TextureFormat::R16_UINT:
            return GL_R16UI;
        case TextureFormat::R16_SNORM:
            return GL_R16_SNORM;
        case TextureFormat::R16_SINT:
            return GL_R16I;
        case TextureFormat::R8_TYPELESS:
            return GL_R8UI;
        case TextureFormat::R8_UNORM:
            return GL_R8;
        case TextureFormat::R8_UINT:
            return GL_R8UI;
        case TextureFormat::R8_SNORM:
            return GL_R8_SNORM;
        case TextureFormat::R8_SINT:
            return GL_R8I;
        case TextureFormat::R9G9B9E5_SHAREDEXP:
            return GL_RGB9_E5;
        case TextureFormat::R32G32B32A32_TYPELESS:
            return GL_RGBA32UI;
        case TextureFormat::R32G32B32_TYPELESS:
            return GL_RGB32UI;
        case TextureFormat::R16G16B16A16_TYPELESS:
            return GL_RGBA16UI;
        case TextureFormat::R32G32_TYPELESS:
            return GL_RG32UI;
        // Depth formats  
        case TextureFormat::D32_FLOAT:
            return GL_DEPTH_COMPONENT32F;
        case TextureFormat::D32_FLOAT_S8X24_UINT:
            return GL_DEPTH32F_STENCIL8;
        case TextureFormat::D24_UNORM_X8_UINT:
            return GL_DEPTH_COMPONENT24;
        case TextureFormat::D24_UNORM_S8_UINT:
            return GL_DEPTH24_STENCIL8;
        case TextureFormat::D16_UNORM:
            return GL_DEPTH_COMPONENT16;
        case TextureFormat::S8_UINT:
            return GL_STENCIL;

        // Compressed
        case TextureFormat::BC1_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC1_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC1_UNORM_SRGB:
            return GL_RGBA32F;
        case TextureFormat::BC2_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC2_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC2_UNORM_SRGB:
            return GL_RGBA32F;
        case TextureFormat::BC3_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC3_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC3_UNORM_SRGB:
            return GL_RGBA32F;
        case TextureFormat::BC4_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC4_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC4_SNORM:
            return GL_RGBA32F;
        case TextureFormat::BC5_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC5_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC5_SNORM:
            return GL_RGBA32F;
        case TextureFormat::B5G6R5_UNORM:
            return GL_RGBA32F;
        case TextureFormat::B5G5R5A1_UNORM:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8A8_UNORM:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8X8_UNORM:
            return GL_RGBA32F;
        case TextureFormat::R10G10B10_XR_BIAS_A2_UNORM:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8A8_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8A8_UNORM_SRGB:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8X8_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::B8G8R8X8_UNORM_SRGB:
            return GL_RGBA32F;
        case TextureFormat::BC6H_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC6H_UF16:
            return GL_RGBA32F;
        case TextureFormat::BC6H_SF16:
            return GL_RGBA32F;
        case TextureFormat::BC7_TYPELESS:
            return GL_RGBA32F;
        case TextureFormat::BC7_UNORM:
            return GL_RGBA32F;
        case TextureFormat::BC7_UNORM_SRGB:
            return GL_RGBA32F;

        case TextureFormat::UNKNOWN:
        default:
            //HYDRA_ASSERT( false, "Format %s not supported on GL.", EnumNamesTextureFormat()[format] );
            break;
    }

    return 0;
}

static GLuint to_gl_format( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::UNKNOWN:
        
        case TextureFormat::R16G16B16A16_FLOAT:
        case TextureFormat::R32G32B32A32_FLOAT:        
        case TextureFormat::R16G16B16A16_UNORM:
        case TextureFormat::R16G16B16A16_SNORM:
        case TextureFormat::R10G10B10A2_TYPELESS:
        case TextureFormat::R10G10B10A2_UNORM:
        case TextureFormat::R8G8B8A8_TYPELESS:
        case TextureFormat::R8G8B8A8_UNORM:
        case TextureFormat::R8G8B8A8_UNORM_SRGB:
        case TextureFormat::R8G8B8A8_SNORM:
            return GL_RGBA;

        case TextureFormat::R32G32B32A32_TYPELESS:
        case TextureFormat::R16G16B16A16_TYPELESS:
        case TextureFormat::R32G32B32A32_UINT:
        case TextureFormat::R32G32B32A32_SINT:
        case TextureFormat::R16G16B16A16_UINT:
        case TextureFormat::R16G16B16A16_SINT:
        case TextureFormat::R10G10B10A2_UINT:
        case TextureFormat::R8G8B8A8_UINT:
        case TextureFormat::R8G8B8A8_SINT:
            return GL_RGBA_INTEGER;
        
        case TextureFormat::R32G32B32_FLOAT:
        case TextureFormat::R11G11B10_FLOAT:
        case TextureFormat::R9G9B9E5_SHAREDEXP:
            return GL_RGB;

        case TextureFormat::R32G32B32_TYPELESS:
        case TextureFormat::R32G32B32_UINT:
        case TextureFormat::R32G32B32_SINT:
            return GL_RGB_INTEGER;

        
        case TextureFormat::R32G32_FLOAT:
        case TextureFormat::R16G16_FLOAT:
        case TextureFormat::R16G16_UNORM:
        case TextureFormat::R16G16_SNORM:
        case TextureFormat::R8G8_UNORM:
        case TextureFormat::R8G8_SNORM:
            return GL_RG;

        case TextureFormat::R32G32_TYPELESS:
        case TextureFormat::R32G32_UINT:
        case TextureFormat::R32G32_SINT:
        case TextureFormat::R16G16_TYPELESS:
        case TextureFormat::R16G16_UINT:
        case TextureFormat::R16G16_SINT:
        case TextureFormat::R8G8_TYPELESS:
        case TextureFormat::R8G8_UINT:
        case TextureFormat::R8G8_SINT:
            return GL_RG_INTEGER;
        
        case TextureFormat::R32_FLOAT:
        case TextureFormat::R16_FLOAT:
        case TextureFormat::R16_UNORM:
        case TextureFormat::R16_SNORM:
        case TextureFormat::R8_UNORM:
        case TextureFormat::R8_SNORM:

            return GL_RED;
        
        case TextureFormat::R32_UINT:
        case TextureFormat::R32_SINT:
        case TextureFormat::R32_TYPELESS:
        case TextureFormat::R16_TYPELESS:
        case TextureFormat::R8_TYPELESS:
        case TextureFormat::R16_UINT:
        case TextureFormat::R16_SINT:
        case TextureFormat::R8_UINT:
        case TextureFormat::R8_SINT:
        case TextureFormat::S8_UINT:
            return GL_RED_INTEGER;
        

        case TextureFormat::D32_FLOAT_S8X24_UINT:
        case TextureFormat::D24_UNORM_S8_UINT:
            return GL_DEPTH_STENCIL;

        case TextureFormat::D24_UNORM_X8_UINT:
        case TextureFormat::D32_FLOAT:
        case TextureFormat::D16_UNORM:
            return GL_DEPTH_COMPONENT;

        case TextureFormat::BC1_TYPELESS:
        case TextureFormat::BC1_UNORM:
        case TextureFormat::BC1_UNORM_SRGB:
        case TextureFormat::BC2_TYPELESS:
        case TextureFormat::BC2_UNORM:
        case TextureFormat::BC2_UNORM_SRGB:
        case TextureFormat::BC3_TYPELESS:
        case TextureFormat::BC3_UNORM:
        case TextureFormat::BC3_UNORM_SRGB:
        case TextureFormat::BC4_TYPELESS:
        case TextureFormat::BC4_UNORM:
        case TextureFormat::BC4_SNORM:
        case TextureFormat::BC5_TYPELESS:
        case TextureFormat::BC5_UNORM:
        case TextureFormat::BC5_SNORM:
        case TextureFormat::B5G6R5_UNORM:
        case TextureFormat::B5G5R5A1_UNORM:
        case TextureFormat::B8G8R8A8_UNORM:
        case TextureFormat::B8G8R8X8_UNORM:
        case TextureFormat::R10G10B10_XR_BIAS_A2_UNORM:
        case TextureFormat::B8G8R8A8_TYPELESS:
        case TextureFormat::B8G8R8A8_UNORM_SRGB:
        case TextureFormat::B8G8R8X8_TYPELESS:
        case TextureFormat::B8G8R8X8_UNORM_SRGB:
        case TextureFormat::BC6H_TYPELESS:
        case TextureFormat::BC6H_UF16:
        case TextureFormat::BC6H_SF16:
        case TextureFormat::BC7_TYPELESS:
        case TextureFormat::BC7_UNORM:
        case TextureFormat::BC7_UNORM_SRGB:

        default:
            //HYDRA_ASSERT( false, "Format %s not supported on GL.", EnumNamesTextureFormat()[format] );
            break;
    }

    return 0;
}

static GLuint to_gl_format_type( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::R32G32B32A32_FLOAT:
        case TextureFormat::R32G32B32_FLOAT:
        case TextureFormat::R16G16B16A16_FLOAT:
        case TextureFormat::R32G32_FLOAT:
        case TextureFormat::R11G11B10_FLOAT:
        case TextureFormat::R16G16_FLOAT:
        case TextureFormat::R16_FLOAT:
        case TextureFormat::D32_FLOAT:
        case TextureFormat::R32_FLOAT:
            return GL_FLOAT;

        case TextureFormat::R10G10B10A2_TYPELESS:
        case TextureFormat::R10G10B10A2_UNORM:
        case TextureFormat::R10G10B10A2_UINT:
            return GL_UNSIGNED_INT_10_10_10_2;

        case TextureFormat::UNKNOWN:
        case TextureFormat::R32G32B32A32_TYPELESS:
        case TextureFormat::R32G32B32A32_UINT:
        case TextureFormat::R32G32B32_TYPELESS:
        case TextureFormat::R32G32B32_UINT:
        case TextureFormat::R32G32_TYPELESS:
        case TextureFormat::R32G32_UINT:
        case TextureFormat::R32_TYPELESS:
        case TextureFormat::R32_UINT:
        case TextureFormat::D24_UNORM_X8_UINT:
            return GL_UNSIGNED_INT;

        case TextureFormat::R32G32B32A32_SINT:
        case TextureFormat::R32G32B32_SINT:
        case TextureFormat::R32G32_SINT:
        case TextureFormat::R32_SINT:
            return GL_INT;

        case TextureFormat::R16G16B16A16_TYPELESS:
        case TextureFormat::R16G16B16A16_UNORM:
        case TextureFormat::R16G16B16A16_UINT:
        case TextureFormat::R16G16_TYPELESS:
        case TextureFormat::R16G16_UNORM:
        case TextureFormat::R16G16_UINT:
        case TextureFormat::R16_TYPELESS:
        case TextureFormat::D16_UNORM:
        case TextureFormat::R16_UNORM:
        case TextureFormat::R16_UINT:
            return GL_UNSIGNED_SHORT;

        case TextureFormat::R16G16B16A16_SNORM:
        case TextureFormat::R16G16B16A16_SINT:
        case TextureFormat::R16G16_SNORM:
        case TextureFormat::R16G16_SINT:
        case TextureFormat::R16_SNORM:
        case TextureFormat::R16_SINT:
            return GL_SHORT;
        
        case TextureFormat::R8G8B8A8_TYPELESS:
        case TextureFormat::R8G8B8A8_UNORM:
        case TextureFormat::R8G8B8A8_UNORM_SRGB:
        case TextureFormat::R8G8B8A8_UINT:
        case TextureFormat::R8G8_TYPELESS:
        case TextureFormat::R8G8_UNORM:
        case TextureFormat::R8G8_UINT:
        case TextureFormat::R8_TYPELESS:
        case TextureFormat::R8_UNORM:
        case TextureFormat::R8_UINT:
        case TextureFormat::S8_UINT:
            return GL_UNSIGNED_BYTE;

        case TextureFormat::R8G8B8A8_SNORM:
        case TextureFormat::R8G8B8A8_SINT:
        case TextureFormat::R8G8_SNORM:
        case TextureFormat::R8G8_SINT:
        case TextureFormat::R8_SNORM:
        case TextureFormat::R8_SINT:
            return GL_BYTE;

        case TextureFormat::D24_UNORM_S8_UINT:
            return GL_UNSIGNED_INT_24_8;
       
        case TextureFormat::D32_FLOAT_S8X24_UINT:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

        case TextureFormat::R9G9B9E5_SHAREDEXP:
            return GL_UNSIGNED_INT_5_9_9_9_REV;

        case TextureFormat::BC1_TYPELESS:
        case TextureFormat::BC1_UNORM:
        case TextureFormat::BC1_UNORM_SRGB:
        case TextureFormat::BC2_TYPELESS:
        case TextureFormat::BC2_UNORM:
        case TextureFormat::BC2_UNORM_SRGB:
        case TextureFormat::BC3_TYPELESS:
        case TextureFormat::BC3_UNORM:
        case TextureFormat::BC3_UNORM_SRGB:
        case TextureFormat::BC4_TYPELESS:
        case TextureFormat::BC4_UNORM:
        case TextureFormat::BC4_SNORM:
        case TextureFormat::BC5_TYPELESS:
        case TextureFormat::BC5_UNORM:
        case TextureFormat::BC5_SNORM:
        case TextureFormat::B5G6R5_UNORM:
        case TextureFormat::B5G5R5A1_UNORM:
        case TextureFormat::B8G8R8A8_UNORM:
        case TextureFormat::B8G8R8X8_UNORM:
        case TextureFormat::R10G10B10_XR_BIAS_A2_UNORM:
        case TextureFormat::B8G8R8A8_TYPELESS:
        case TextureFormat::B8G8R8A8_UNORM_SRGB:
        case TextureFormat::B8G8R8X8_TYPELESS:
        case TextureFormat::B8G8R8X8_UNORM_SRGB:
        case TextureFormat::BC6H_TYPELESS:
        case TextureFormat::BC6H_UF16:
        case TextureFormat::BC6H_SF16:
        case TextureFormat::BC7_TYPELESS:
        case TextureFormat::BC7_UNORM:
        case TextureFormat::BC7_UNORM_SRGB:

        default:
            //HYDRA_ASSERT( false, "Format %s not supported on GL.", EnumNamesTextureFormat()[format] );
            break;
    }

    return 0;
}


//
// Magnification filter conversion to GL values.
//
static GLuint to_gl_mag_filter_type( TextureFilter::Enum filter ) {
    static GLuint s_gl_mag_filter_type[TextureFilter::Count] = { GL_NEAREST, GL_LINEAR };

    return s_gl_mag_filter_type[filter];
}

//
// Minification filter conversion to GL values.
//
static GLuint to_gl_min_filter_type( TextureFilter::Enum filter, TextureMipFilter::Enum mipmap ) {
    static GLuint s_gl_min_filter_type[4] = { GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };
    
    return s_gl_min_filter_type[(filter * 2) + mipmap];
}

//
// Texture address mode conversion to GL values.
//
static GLuint to_gl_texture_address_mode( TextureAddressMode::Enum mode ) {
    static GLuint s_gl_texture_address_mode[TextureAddressMode::Count] = { GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER };
    
    return s_gl_texture_address_mode[mode];
}

//
// Shader stage conversion
//
static GLuint to_gl_shader_stage( ShaderStage::Enum stage ) {
    // TODO: hull/domain shader not supported for now.
    static GLuint s_gl_shader_stage[ShaderStage::Count] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_COMPUTE_SHADER, 0, 0 };
    return s_gl_shader_stage[stage];
}

//
//
//
static GLuint to_gl_buffer_type( BufferType::Enum type ) {
    static GLuint s_gl_buffer_types[BufferType::Count] = { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER, GL_DRAW_INDIRECT_BUFFER };
    return s_gl_buffer_types[type];
}

//
//
//
static GLuint to_gl_buffer_usage( ResourceUsageType::Enum type ) {
    static GLuint s_gl_buffer_types[ResourceUsageType::Count] = { GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_DYNAMIC_DRAW };
    return s_gl_buffer_types[type];
}

//
//
//
static GLuint to_gl_comparison( ComparisonFunction::Enum comparison ) {
    static GLuint s_gl_comparison[ComparisonFunction::Enum::Count] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };
    return s_gl_comparison[comparison];
}

//
//
//
static GLenum to_gl_blend_function( Blend::Enum blend ) {
    static GLenum s_gl_blend_function[] = { GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                                            GL_SRC_ALPHA_SATURATE, GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR, GL_SRC1_ALPHA, GL_ONE_MINUS_SRC1_ALPHA };
    return s_gl_blend_function[blend];
}

//
//
//
static GLenum to_gl_blend_equation( BlendOperation::Enum blend ) {
    static GLenum s_gl_blend_equation[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX };
    return s_gl_blend_equation[blend];
}

//
//
// Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Count
static GLuint to_gl_components( VertexComponentFormat::Enum format ) {
    static GLuint s_gl_components[] = { 1, 2, 3, 4, 16, 1, 4, 1, 4, 2, 2, 4, 4 };
    return s_gl_components[format];
}

static GLenum to_gl_vertex_type( VertexComponentFormat::Enum format ) {
    static GLenum s_gl_vertex_type[] = { GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_BYTE, GL_BYTE, GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_SHORT, GL_SHORT, GL_SHORT };
    return s_gl_vertex_type[format];
}

static GLboolean to_gl_vertex_norm( VertexComponentFormat::Enum format ) {
    static GLboolean s_gl_vertex_norm[] = { GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE };
    return s_gl_vertex_norm[format];
}



// Structs //////////////////////////////////////////////////////////////////////

//
//
struct ShaderStateGL {

    const char*                     name                = nullptr;
    GLuint                          gl_program          = 0;

}; // struct ShaderState

//
//
struct BufferGL {
    
    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    uint32_t                        size                = 0;
    const char*                     name                = nullptr;

    BufferHandle                    handle;

    GLuint                          gl_handle           = 0;
    GLuint                          gl_type             = 0;
    GLuint                          gl_usage            = 0;

}; // struct BufferGL

//
//
struct TextureGL {

    uint16_t                        width               = 1;
    uint16_t                        height              = 1;
    uint16_t                        depth               = 1;
    uint8_t                         mipmaps             = 1;
    uint8_t                         render_target       = 0;

    TextureHandle                   handle;

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

    GLuint                          gl_handle           = 0;
    GLuint                          gl_target           = 0;

    const char*                     name                = nullptr;

}; // struct TextureGL

struct VertexInputGL {

    uint32_t                        num_streams         = 0;
    uint32_t                        num_attributes      = 0;
    VertexStream                    vertex_streams[k_max_vertex_streams];
    VertexAttribute                 vertex_attributes[k_max_vertex_attributes];

}; // struct VertexInputGL

//
//
struct PipelineGL {

    ShaderStateHandle               shader_state;
    GLuint                          gl_program_cached   = 0;
    GLuint                          gl_vao              = 0;

    const ResourceListLayoutGL*     resource_list_layout[k_max_resource_layouts];
    ResourceListLayoutHandle        resource_list_layout_handle[k_max_resource_layouts];
    uint32_t                        num_active_layouts  = 0;
    
    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    VertexInputGL                   vertex_input;
    RasterizationCreation           rasterization;

    PipelineHandle                  handle;
    bool                            graphics_pipeline   = true;

}; // struct PipelineGL

//
//
struct SamplerGL {

    SamplerCreation                 creation;

}; // struct SamplerGL

//
//
struct RenderPassGL {

    uint32_t                        is_swapchain        = true;

    TextureGL*                      render_targets[k_max_image_outputs];
    TextureGL*                      depth_stencil       = nullptr;
    
    GLuint                          fbo_handle          = 0;

    uint16_t                        dispatch_x          = 0;
    uint16_t                        dispatch_y          = 0;
    uint16_t                        dispatch_z          = 0;

    uint8_t                         clear_color         = false;
    uint8_t                         fullscreen          = false;
    uint8_t                         num_render_targets;

}; // struct RenderPassGL

//
//
struct ResourceBindingGL {

    uint16_t                        type                = 0;    // ResourceType
    uint16_t                        start               = 0;
    uint16_t                        count               = 0;
    uint16_t                        set                 = 0;

    const char*                     name                = nullptr;

    GLuint                          gl_block_index      = 0;
    GLint                           gl_block_binding    = 0;
};

//
//
struct ResourceListLayoutGL {

    ResourceBindingGL*              bindings            = nullptr;
    uint32_t                        num_bindings        = 0;

    ResourceListLayoutHandle        handle;

}; // struct ResourceListLayoutGL

//
//
struct ResourceListGL {

    const ResourceListLayoutGL*     layout              = nullptr;
    ResourceData*                   resources           = nullptr;
    uint32_t                        num_resources       = 0;


    void                            set( const uint32_t* offsets, uint32_t num_offsets ) const;

}; // struct ResourceListGL

//
// Holds all the states necessary to render.
struct DeviceStateGL {

    GLuint                          fbo_handle          = 0;
    GLuint                          ib_handle           = 0;

    struct VertexBufferBinding {
        GLuint                      vb_handle;
        uint32_t                    binding;
        uint32_t                    offset;
    };

    VertexBufferBinding             vb_bindings[8];
    uint32_t                        num_vertex_streams = 0;

    const Viewport*                 viewport            = nullptr;
    const Rect2DInt*                scissor             = nullptr;
    const PipelineGL*               pipeline            = nullptr;
    const ResourceListGL*           resource_lists[k_max_resource_layouts];
    uint32_t                        resource_offsets[k_max_resource_layouts];
    uint32_t                        num_lists           = 0;
    uint32_t                        num_offsets         = 0;

    float                           clear_color[4];
    float                           clear_depth_value;
    uint8_t                         clear_stencil_value;
    bool                            clear_color_flag    = false;
    bool                            clear_depth_flag    = false;
    bool                            clear_stencil_flag  = false;

    bool                            swapchain_flag      = false;
    bool                            end_pass_flag       = false;    // End pass after last draw/dispatch.

    void                            apply();

}; // struct DeviceStateGL

// Methods //////////////////////////////////////////////////////////////////////

// Forward declarations
static GLuint                       compile_shader( GLuint stage, const char* source, const char* shader_name );
static bool                         get_compile_info( GLuint shader, GLuint status, const char* shader_name );
static bool                         get_link_info( GLuint shader, GLuint status, const char* shader_name );

static void                         create_fbo( const RenderPassCreation& creation, RenderPassGL& fbo, Device& device );

static void                         cache_resource_bindings( GLuint shader, const ResourceListLayoutGL* resource_list_layout );

// Tests
static void                         test_texture_creation( Device& device );
static void                         test_pool( Device& device );
static void                         test_command_buffer( Device& device );

void GLAPIENTRY                     gl_message_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam );

//#define HYDRA_GRAPHICS_TEST

void Device::backend_init( const DeviceCreation& creation ) {
    uint32_t result = glewInit();
    HYDRA_LOG( "Glew Init\n" );

    // Init pools
    shaders.init( 128, sizeof( ShaderStateGL ) );
    textures.init( 128, sizeof( TextureGL ) );
    buffers.init( 128, sizeof( BufferGL ) );
    pipelines.init( 128, sizeof( PipelineGL ) );
    samplers.init( 32, sizeof( SamplerGL ) );
    resource_list_layouts.init( 128, sizeof( ResourceListLayoutGL ) );
    resource_lists.init( 128, sizeof( ResourceListGL ) );
    render_passes.init( 256, sizeof( RenderPassGL ) );
    command_buffers.init( 32, sizeof( CommandBuffer ) );

    for ( uint32_t i = 0; i < 32; i++ ) {
        CommandBuffer* command_buffer = (CommandBuffer*)command_buffers.access_resource(i);
        command_buffer->init( QueueType::Graphics, 10000, 1000, false );
    }

    // During init, enable debug output
    glEnable( GL_DEBUG_OUTPUT );
    glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glDebugMessageCallback( gl_message_callback, 0 );

    // Disable notification messages.
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE );

    device_state = (DeviceStateGL*)malloc( sizeof( DeviceStateGL ) );
    memset( device_state, 0, sizeof( DeviceStateGL ) );

#if defined (HYDRA_GRAPHICS_TEST)
    test_texture_creation( *this );
    test_pool( *this );
    test_command_buffer( *this );
#endif // HYDRA_GRAPHICS_TEST

    //
    // Init primitive resources
    BufferCreation fullscreen_vb_creation = { BufferType::Vertex, ResourceUsageType::Immutable, 0, nullptr, "Fullscreen_vb" };
    fullscreen_vertex_buffer = create_buffer( fullscreen_vb_creation );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type( RenderPassType::Swapchain );
    swapchain_pass = create_render_pass( swapchain_pass_creation );

    // Init Dummy resources
    TextureCreation dummy_texture_creation = { nullptr, 1, 1, 1, 1, 0, TextureFormat::R8_UINT, TextureType::Texture2D };
    dummy_texture = create_texture( dummy_texture_creation );

    BufferCreation dummy_constant_buffer_creation = { BufferType::Constant, ResourceUsageType::Immutable, 16, nullptr, "Dummy_cb" };
    dummy_constant_buffer = create_buffer( dummy_constant_buffer_creation );

    queued_command_buffers = (CommandBuffer**)malloc( sizeof( CommandBuffer* ) * 128 );
}

void Device::backend_terminate() {

    glDisable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glDisable( GL_DEBUG_OUTPUT );

    free( queued_command_buffers );
    destroy_buffer( fullscreen_vertex_buffer );
    destroy_render_pass( swapchain_pass );
    destroy_texture( dummy_texture );
    destroy_buffer( dummy_constant_buffer );

    free( device_state );

    for ( size_t i = 0; i < 32; i++ ) {
        CommandBuffer* command_buffer = (CommandBuffer*)command_buffers.access_resource( i );
        command_buffer->terminate();
    }

    pipelines.terminate();
    buffers.terminate();
    shaders.terminate();
    textures.terminate();
    samplers.terminate();
    resource_list_layouts.terminate();
    resource_lists.terminate();
    render_passes.terminate();
    command_buffers.terminate();
}

void Device::link_texture_sampler( TextureHandle texture, SamplerHandle sampler ) {

    TextureGL* texture_gl = access_texture( texture );
    SamplerGL* sampler_gl = access_sampler( sampler );

    glBindTexture( texture_gl->gl_target, texture_gl->gl_handle );

    glTexParameteri( texture_gl->gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );//GL_LINEAR_MIPMAP_LINEAR to_gl_min_filter_type( sampler_gl->creation.min_filter, sampler_gl->creation.mip_filter ) );
    glTexParameteri( texture_gl->gl_target, GL_TEXTURE_MAG_FILTER, to_gl_mag_filter_type( sampler_gl->creation.mag_filter ) );

    glBindTexture( texture_gl->gl_target, 0 );

    //glTextureParameteri( texture_gl->gl_handle, GL_TEXTURE_MIN_FILTER, to_gl_min_filter_type(sampler_gl->creation.min_filter, sampler_gl->creation.mip_filter) );
    //glTextureParameteri( texture_gl->gl_handle, GL_TEXTURE_MAG_FILTER, to_gl_mag_filter_type( sampler_gl->creation.mag_filter ) );
}

// Resource Creation ////////////////////////////////////////////////////////////
TextureHandle Device::create_texture( const TextureCreation& creation ) {

    uint32_t resource_index = textures.obtain_resource();
    TextureHandle handle = { resource_index };
    if ( resource_index == k_invalid_handle ) {
        return handle;
    }

    GLuint gl_handle;
    glGenTextures( 1, &gl_handle );
    const GLuint gl_target = to_gl_target(creation.type);

    glBindTexture( gl_target, gl_handle );

    // For some unknown reasons, not setting any parameter results in an unusable texture.
    glTexParameteri( gl_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( gl_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    const GLuint gl_internal_format = to_gl_internal_format(creation.format);
    const GLuint gl_format = to_gl_format(creation.format);
    const GLuint gl_type = to_gl_format_type(creation.format);

    // TODO: when is this needed ?
    //#ifdef GL_UNPACK_ROW_LENGTH
    //    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    //#endif

    switch ( creation.type ) {
        case TextureType::Texture2D:
        {
            GLint level = 0;
            GLint border = 0;
            glTexImage2D( gl_target, level, gl_internal_format, creation.width, creation.height, border, gl_format, gl_type, creation.initial_data );
            break;
        }

        default:
        {
            break;
        }
    }

    GLenum gl_error = glGetError();
    if ( gl_error != 0 && false) {
        HYDRA_LOG( "Error creating texture: format %s\n", TextureFormat::ToString(creation.format) );
        // Release and invalidate resource.
        textures.release_resource( resource_index );
        handle.handle = k_invalid_handle;
    }
    else {

        TextureGL* texture = access_texture( handle );
        texture->width = creation.width;
        texture->height = creation.height;
        texture->depth = creation.depth;
        texture->mipmaps = creation.mipmaps;
        texture->format = creation.format;
        texture->type = creation.type;
        texture->render_target = (creation.flags & TextureCreationFlags::RenderTarget_mask != 0) ? 1 : 0;

        texture->gl_handle = gl_handle;
        texture->gl_target = gl_target;

        texture->name = creation.name;

        texture->handle = handle;
    }
    
    return handle;
}

ShaderStateHandle Device::create_shader_state( const ShaderStateCreation& creation ) {

    ShaderStateHandle handle = { k_invalid_handle };

    if ( creation.stages_count == 0 || creation.stages == nullptr ) {
        HYDRA_LOG( "Shader %s does not contain shader stages.\n", creation.name );
        return handle;
    }
    
    handle.handle = shaders.obtain_resource();
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    // For each shader stage, compile them individually.
    uint32_t compiled_shaders = 0;
    // Create the program first - and then attach all the shader stages.
    GLuint gl_program = glCreateProgram();

    for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
        const ShaderStateCreation::Stage& stage = creation.stages[compiled_shaders];
        GLuint gl_shader = compile_shader( to_gl_shader_stage(stage.type), stage.code, creation.name );

        if ( !gl_shader ) {
            break;
        }

        glAttachShader( gl_program, gl_shader );
        // Compiled shaders are not needed anymore
        glDeleteShader( gl_shader );

    }

    // If all the stages are compiled, link them.
    bool creation_failed = compiled_shaders != creation.stages_count;
    if (!creation_failed) {

        glLinkProgram( gl_program );

        if ( !get_link_info( gl_program, GL_LINK_STATUS, creation.name ) ) {
            glDeleteProgram( gl_program );
            gl_program = 0;

            creation_failed = true;
        }

        ShaderStateGL* shader_state = access_shader_state( handle );
        shader_state->gl_program = gl_program;
        shader_state->name = creation.name;
    }

    if ( creation_failed ) {
        shaders.release_resource( handle.handle );
        handle.handle = k_invalid_handle;

        // Dump shader code
        HYDRA_LOG( "Error in creation of shader %s. Dumping all shader informations.\n", creation.name );
        for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
            const ShaderStateCreation::Stage& stage = creation.stages[compiled_shaders];
            HYDRA_LOG("%s:\n%s\n", ShaderStage::ToString(stage.type), stage.code);
        }
    }

    return handle;
}

PipelineHandle Device::create_pipeline( const PipelineCreation& creation ) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }
    
    // Create all necessary resources
    ShaderStateHandle shader_state = create_shader_state( creation.shaders );
    if ( shader_state.handle == k_invalid_handle ) {
        // Shader did not compile.
        handle.handle = k_invalid_handle;

        return handle;
    }

    // Now that shaders have compiled we can create the pipeline.
    PipelineGL* pipeline = access_pipeline( handle );
    ShaderStateGL* shader_state_data = access_shader_state( shader_state );

    pipeline->shader_state = shader_state;
    pipeline->gl_program_cached = shader_state_data->gl_program;
    pipeline->handle = handle;
    pipeline->graphics_pipeline = true;


    for ( size_t i = 0; i < creation.shaders.stages_count; ++i ) {
        if ( creation.shaders.stages[i].type == ShaderStage::Compute ) {
            pipeline->graphics_pipeline = false;
            break;
        }
    }

    if ( pipeline->graphics_pipeline ) {
        // Copy render states from creation
        pipeline->depth_stencil = creation.depth_stencil;
        pipeline->blend_state = creation.blend_state;
        pipeline->rasterization = creation.rasterization;

        VertexInputGL& vertex_input = pipeline->vertex_input;
        // Copy vertex input (streams + attributes)
        const VertexInputCreation& vertex_input_creation = creation.vertex_input;

        vertex_input.num_streams = vertex_input_creation.num_vertex_streams;
        vertex_input.num_attributes = vertex_input_creation.num_vertex_attributes;

        memcpy( vertex_input.vertex_streams, vertex_input_creation.vertex_streams, vertex_input_creation.num_vertex_streams * sizeof( VertexStream ) );
        memcpy( vertex_input.vertex_attributes, vertex_input_creation.vertex_attributes, vertex_input_creation.num_vertex_attributes * sizeof( VertexAttribute ) );

        glCreateVertexArrays( 1, &pipeline->gl_vao );
        glBindVertexArray( pipeline->gl_vao );

        for ( uint32_t i = 0; i < vertex_input.num_streams; i++ ) {
            const VertexStream& stream = vertex_input.vertex_streams[i];
            glVertexBindingDivisor( stream.binding, stream.input_rate == VertexInputRate::PerVertex ? 0 : 1 );
        }

        for ( uint32_t i = 0; i < vertex_input.num_attributes; i++ ) {
            const VertexAttribute& attribute = vertex_input.vertex_attributes[i];
            glEnableVertexAttribArray( attribute.location );
            glVertexAttribFormat( attribute.location, to_gl_components( attribute.format ), to_gl_vertex_type( attribute.format ),
                                  to_gl_vertex_norm( attribute.format ), attribute.offset );
            glVertexAttribBinding( attribute.location, attribute.binding );
        }

        glBindVertexArray( 0 );
    }

    // Resource List Layout
    for ( uint32_t l = 0; l < creation.num_active_layouts; ++l ) {
        pipeline->resource_list_layout[l] = access_resource_list_layout( creation.resource_list_layout[l] );
        pipeline->resource_list_layout_handle[l] = creation.resource_list_layout[l];

        cache_resource_bindings( pipeline->gl_program_cached, pipeline->resource_list_layout[l] );
    }

    if ( creation.num_active_layouts == 0 ) {
        print_format( "Error in pipeline: no resources layouts are specificed!\n" );
    }

    return handle;
}

BufferHandle Device::create_buffer( const BufferCreation& creation ) {
    BufferHandle handle = { buffers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    BufferGL* buffer = access_buffer( handle );

    buffer->name = creation.name;
    buffer->size = creation.size;
    buffer->type = creation.type;
    buffer->usage = creation.usage;

    buffer->gl_type = to_gl_buffer_type( creation.type );
    buffer->gl_usage = to_gl_buffer_usage( creation.usage );
    
    buffer->handle = handle;

    switch ( creation.type ) {
        case BufferType::Constant:
        {
            // Use glCreateBuffers to use the named versions of calls.
            glCreateBuffers( 1, &buffer->gl_handle );
            glNamedBufferData( buffer->gl_handle, buffer->size, creation.initial_data, buffer->gl_usage );

            break;
        }

        case BufferType::Vertex:
        {
            glCreateBuffers( 1, &buffer->gl_handle );
            glNamedBufferData( buffer->gl_handle, buffer->size, creation.initial_data, buffer->gl_usage );

            break;
        }

        case BufferType::Index:
        {
            glCreateBuffers( 1, &buffer->gl_handle );
            glNamedBufferData( buffer->gl_handle, buffer->size, creation.initial_data, buffer->gl_usage );

            break;
        }

        default:
        {
            HYDRA_ASSERT( false, "Not implemented!" );
            break;
        }
    }

    return handle;
}

SamplerHandle Device::create_sampler( const SamplerCreation& creation ) {
    SamplerHandle handle = { samplers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    SamplerGL* sampler = access_sampler( handle );
    sampler->creation = creation;

    return handle;
}

ResourceListLayoutHandle Device::create_resource_list_layout( const ResourceListLayoutCreation& creation ) {
    ResourceListLayoutHandle handle = { resource_list_layouts.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceListLayoutGL* resource_layout = access_resource_list_layout( handle );

    // TODO: add support for multiple sets.
    // Create flattened binding list
    resource_layout->num_bindings = creation.num_bindings;
    resource_layout->bindings = (ResourceBindingGL*)HYDRA_MALLOC( sizeof( ResourceBindingGL ) * creation.num_bindings );
    resource_layout->handle = handle;

    for ( uint32_t r = 0; r < creation.num_bindings; ++r ) {
        ResourceBindingGL& binding = resource_layout->bindings[r];
        binding.start = r;
        binding.count = 1;
        binding.type = (uint16_t)creation.bindings[r].type;
        binding.name = creation.bindings[r].name;
    }

    return handle;
}

ResourceListHandle Device::create_resource_list( const ResourceListCreation& creation ) {
    ResourceListHandle handle = { resource_lists.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceListGL* resources = access_resource_list( handle );
    resources->layout = access_resource_list_layout( creation.layout );

    resources->resources = (ResourceData*)HYDRA_MALLOC( sizeof( ResourceData ) * creation.num_resources );
    resources->num_resources = creation.num_resources;
    
    // Set all resources
    for ( uint32_t r = 0; r < creation.num_resources; ++r ) {
        ResourceData& resource = resources->resources[r];

        const ResourceBindingGL binding = resources->layout->bindings[r];
        
        switch ( binding.type ) {
            case ResourceType::Texture:
            case ResourceType::ImageRW:
            {
                TextureHandle texture_handle = { creation.resources[r] };
                TextureGL* texture_data = access_texture( texture_handle );
                resource.data = (void*)texture_data;

                break;
            }

            case ResourceType::Buffer:
            case ResourceType::Constants:
            {
                BufferHandle buffer_handle = { creation.resources[r] };
                BufferGL* buffer = access_buffer( buffer_handle );
                resource.data = (void*)buffer;
                break;
            }

            default:
                HYDRA_LOG( "Binding not supported %s\n", ResourceType::ToString( (ResourceType::Enum)binding.type ) );
                break;
        }
    }

    return handle;
}

RenderPassHandle Device::create_render_pass( const RenderPassCreation& creation ) {
    RenderPassHandle handle = { render_passes.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    RenderPassGL* render_pass = access_render_pass( handle );

    // This is a special case for OpenGL.
    // If we are creating a render pass for that renders straight to the framebuffer, and thus into the swapchain,
    // there is nothing to create.
    render_pass->is_swapchain = creation.is_swapchain;
    // Init the rest of the struct.
    render_pass->num_render_targets = 0;
    render_pass->fbo_handle = 0;
    render_pass->dispatch_x = 0;
    render_pass->dispatch_y = 0;
    render_pass->dispatch_z = 0;
    render_pass->clear_color = false;
    render_pass->fullscreen = false;
    render_pass->num_render_targets = 0;
    render_pass->depth_stencil = nullptr;

    // Create the FBO only if it actually outputs to textures.
    // Compute post-processes and framebuffer passes do not output to FBOs in OpenGL.
    if ( !creation.is_swapchain && !creation.is_compute ) {
        create_fbo( creation, *render_pass, *this );
    }

    return handle;
}

// Resource Destruction /////////////////////////////////////////////////////////

void Device::destroy_buffer( BufferHandle buffer ) {
    if ( buffer.handle != k_invalid_handle ) {
        BufferGL* gl_buffer = access_buffer( buffer );
        if ( gl_buffer ) {
            glDeleteBuffers( 1, &gl_buffer->gl_handle );
        }

        buffers.release_resource( buffer.handle );
    }
}

void Device::destroy_texture( TextureHandle texture ) {

    if ( texture.handle != k_invalid_handle ) {
        TextureGL* texture_data = access_texture( texture );
        if ( texture_data ) {
            glDeleteTextures( 1, &texture_data->gl_handle );
        }
        textures.release_resource( texture.handle );
    }
}

void Device::destroy_shader_state( ShaderStateHandle shader ) {

    if ( shader.handle != k_invalid_handle ) {
        ShaderStateGL* shader_state = access_shader_state( shader );
        if ( shader_state ) {
            glDeleteProgram( shader_state->gl_program );
        }

        shaders.release_resource( shader.handle );
    }
}

void Device::destroy_pipeline( PipelineHandle pipeline ) {
    if ( pipeline.handle != k_invalid_handle ) {
        
        pipelines.release_resource( pipeline.handle );
    }
}

void Device::destroy_sampler( SamplerHandle sampler ) {
    if ( sampler.handle != k_invalid_handle ) {

        samplers.release_resource( sampler.handle );
    }
}

void Device::destroy_resource_list_layout( ResourceListLayoutHandle resource_layout ) {
    if ( resource_layout.handle != k_invalid_handle ) {
        ResourceListLayoutGL* state = access_resource_list_layout( resource_layout );
        HYDRA_FREE( state->bindings );
        resource_list_layouts.release_resource( resource_layout.handle );
    }
}

void Device::destroy_resource_list( ResourceListHandle resource_list ) {
    if ( resource_list.handle != k_invalid_handle ) {
        ResourceListGL* state = access_resource_list( resource_list );
        HYDRA_FREE( state->resources );
        resource_lists.release_resource( resource_list.handle );
    }
}

void Device::destroy_render_pass( RenderPassHandle render_pass ) {
    if ( render_pass.handle != k_invalid_handle ) {

        render_passes.release_resource( render_pass.handle );
    }
}

// Resource Description Query ///////////////////////////////////////////////////

void Device::query_buffer( BufferHandle buffer, BufferDescription& out_description ) {
    if ( buffer.handle != k_invalid_handle ) {
        const BufferGL* buffer_data = access_buffer( buffer );

        out_description.name = buffer_data->name;
        out_description.size = buffer_data->size;
        out_description.type = buffer_data->type;
        out_description.usage = buffer_data->usage;
        out_description.native_handle = (void*)&buffer_data->gl_handle;
    }
}

void Device::query_texture( TextureHandle texture, TextureDescription& out_description ) {
    if ( texture.handle != k_invalid_handle ) {
        const TextureGL* texture_data = access_texture( texture );

        out_description.width = texture_data->width;
        out_description.height = texture_data->height;
        out_description.depth = texture_data->depth;
        out_description.format = texture_data->format;
        out_description.mipmaps = texture_data->mipmaps;
        out_description.type = texture_data->type;
        out_description.render_target = texture_data->render_target;
        out_description.native_handle = (void*)&texture_data->gl_handle;
    }
}

void Device::query_shader_state( ShaderStateHandle shader, ShaderStateDescription& out_description ) {
    if ( shader.handle != k_invalid_handle ) {
        const ShaderStateGL* shader_state = access_shader_state( shader );

        out_description.name = shader_state->name;
        out_description.native_handle = (void*)&shader_state->gl_program;
    }
}

void Device::query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description ) {
    if ( pipeline.handle != k_invalid_handle ) {
        const PipelineGL* pipeline_data = access_pipeline( pipeline );

        out_description.shader = pipeline_data->shader_state;
    }
}

void Device::query_sampler( SamplerHandle sampler, SamplerDescription& out_description ) {
    if ( sampler.handle != k_invalid_handle ) {
        const SamplerGL* sampler_data = access_sampler( sampler );
    }
}

void Device::query_resource_list_layout( ResourceListLayoutHandle resource_list_layout, ResourceListLayoutDescription& out_description ) {
    if ( resource_list_layout.handle != k_invalid_handle ) {
        const ResourceListLayoutGL* resource_list_layout_data = access_resource_list_layout( resource_list_layout );

        const uint32_t num_bindings = resource_list_layout_data->num_bindings;
        for ( size_t i = 0; i < num_bindings; i++ ) {
            out_description.bindings[i].name = resource_list_layout_data->bindings[i].name;
            out_description.bindings[i].type = resource_list_layout_data->bindings[i].type;
        }
        
        out_description.num_active_bindings = resource_list_layout_data->num_bindings;
    }
}

void Device::query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description ) {
    if ( resource_list.handle != k_invalid_handle ) {
        const ResourceListGL* resource_list_data = access_resource_list( resource_list );
    }
}

// Resource Map/Unmap ///////////////////////////////////////////////////////////

void* Device::map_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.handle == k_invalid_handle )
        return nullptr;

    BufferGL* buffer = access_buffer( parameters.buffer );
    uint32_t mapping_size = parameters.size == 0 ? buffer->size : parameters.size;  
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    return glMapNamedBufferRange( buffer->gl_handle, parameters.offset, mapping_size, flags );
}

void Device::unmap_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.handle == k_invalid_handle )
        return;

    BufferGL* buffer = access_buffer( parameters.buffer );
    glUnmapNamedBuffer( buffer->gl_handle );
}

// Other methods ////////////////////////////////////////////////////////////////

static void resize_texture( TextureGL* texture , uint16_t width, uint16_t height ) {

    const GLuint gl_internal_format = to_gl_internal_format( texture->format );
    const GLuint gl_format = to_gl_format( texture->format );
    const GLuint gl_type = to_gl_format_type( texture->format );

    glBindTexture( texture->gl_target, texture->gl_handle );

    switch ( texture->type ) {
        case TextureType::Texture2D:
        {
            GLint level = 0;
            GLint border = 0;
            glTexImage2D( texture->gl_target, level, gl_internal_format, width, height, border, gl_format, gl_type, 0 );
            break;
        }

        default:
        {
            break;
        }
    }

    // Update texture informations
    texture->width = width;
    texture->height = height;

    glBindTexture( texture->gl_target, 0 );
}

void Device::resize_output_textures( RenderPassHandle render_pass, uint16_t width, uint16_t height ) {

    RenderPassGL* render_pass_gl = access_render_pass( render_pass );
    if ( render_pass_gl ) {

        for ( size_t i = 0; i < render_pass_gl->num_render_targets; ++i ) {
            TextureGL* texture = render_pass_gl->render_targets[i];
            resize_texture( texture, width, height );
        }

        if ( render_pass_gl->depth_stencil ) {
            TextureGL* texture = render_pass_gl->depth_stencil;
            resize_texture( texture, width, height );
        }
    }
}

void Device::queue_command_buffer( CommandBuffer* command_buffer ) {

    queued_command_buffers[num_queued_command_buffers++] = command_buffer;
}

CommandBuffer* Device::get_command_buffer( QueueType::Enum type, bool baked ) {
    uint32_t handle = command_buffers.obtain_resource();
    if ( handle != k_invalid_handle ) {
        CommandBuffer* command_buffer = (CommandBuffer*)command_buffers.access_resource( handle );
        command_buffer->resource_handle = handle;
        command_buffer->swapchain_frame_issued = 0;
        command_buffer->baked = baked;

        return command_buffer;
    }
    return nullptr;
}

void Device::free_command_buffer( CommandBuffer* command_buffer ) {
    command_buffers.release_resource( command_buffer->resource_handle );
}

void Device::present() {

    OPTICK_EVENT();

    // TODO:
    // 1. Merge and sort all command buffers.
    // 2. Execute command buffers.

    uint32_t num_submits = 0;

    static const uint32_t k_max_submits = 1000;
    static uint64_t merged_keys[k_max_submits];
    static uint8_t merged_types[k_max_submits];
    static void* merged_data[k_max_submits];
    static std::vector<uint32_t> merged_indices;// [k_max_submits];
    merged_indices.resize( 1000 );
    // Copy all commands
    {
        OPTICK_EVENT( "Merge_Command_Lists" );

        for ( uint32_t c = 0; c < num_queued_command_buffers; c++ ) {

            CommandBuffer* command_buffer = queued_command_buffers[c];
            for ( uint32_t s = 0; s < command_buffer->current_command; ++s ) {
                merged_keys[num_submits] = command_buffer->keys[s];
                merged_types[num_submits] = command_buffer->types[s];
                merged_data[num_submits] = command_buffer->datas[s];
                merged_indices[num_submits] = num_submits;
                
                ++num_submits;
            }

            command_buffer->reset();

            if ( !command_buffer->baked )
                command_buffers.release_resource( command_buffer->resource_handle );
        }
    }
    

    // TODO: missing implementation.
    // Sort them
    {
        OPTICK_EVENT( "Sort_Commands" );
        std::stable_sort( merged_indices.begin(), merged_indices.begin() + num_submits,
                          [&data = merged_keys]( const uint32_t a, const uint32_t b ) { return data[a] < data[b];  } );
    }

    // Execute
    {
        OPTICK_EVENT("Execute_Commands");
        OPTICK_TAG( "CommandCount", num_submits );

        for ( uint32_t s = 0; s < num_submits; ++s ) {
            CommandType::Enum command_type = (CommandType::Enum)merged_types[s];

            switch ( command_type ) {

                case CommandType::BeginPass:
                {
                    const commands::BindPassData& begin_pass = *(const commands::BindPassData*)merged_data[s];
                    RenderPassGL* render_pass_gl = access_render_pass( begin_pass.handle );

                    device_state->fbo_handle = render_pass_gl->fbo_handle;
                    device_state->swapchain_flag = render_pass_gl->is_swapchain;
                    device_state->scissor = nullptr;
                    device_state->viewport = nullptr;

                    break;
                }

                case CommandType::BindVertexBuffer:
                {
                    const commands::BindVertexBufferData& binding = *(const commands::BindVertexBufferData*)merged_data[s];
                    BufferGL* buffer = access_buffer( binding.buffer );

                    DeviceStateGL::VertexBufferBinding& vb_binding = device_state->vb_bindings[device_state->num_vertex_streams++];
                    vb_binding.vb_handle = buffer->gl_handle;
                    vb_binding.offset = binding.byte_offset;
                    vb_binding.binding = binding.binding;

                    break;
                }

                case CommandType::BindIndexBuffer:
                {
                    const commands::BindIndexBufferData& binding = *(const commands::BindIndexBufferData*)merged_data[s];
                    BufferGL* buffer = access_buffer( binding.buffer );

                    device_state->ib_handle = buffer->gl_handle;

                    break;
                }

                case CommandType::SetViewport:
                {
                    const commands::SetViewportData& set = *(const commands::SetViewportData*)merged_data[s];
                    device_state->viewport = &set.viewport;

                    break;
                }

                case CommandType::SetScissor:
                {
                    const commands::SetScissorData& set = *(const commands::SetScissorData*)merged_data[s];
                    device_state->scissor = &set.rect;

                    break;
                }

                case CommandType::Clear:
                {
                    const commands::ClearData& clear = *(const commands::ClearData*)merged_data[s];
                    memcpy( device_state->clear_color, clear.clear_color, sizeof( float ) * 4 );
                    device_state->clear_color_flag = true;

                    break;
                }

                case CommandType::ClearDepth:
                {
                    const commands::ClearDepthData& clear = *(const commands::ClearDepthData*)merged_data[s];
                    device_state->clear_depth_value = clear.value;
                    device_state->clear_depth_flag = true;

                    break;
                }

                case CommandType::ClearStencil:
                {
                    const commands::ClearStencilData& clear = *(const commands::ClearStencilData*)merged_data[s];
                    device_state->clear_stencil_value = clear.value;
                    device_state->clear_stencil_flag = true;

                    break;
                }

                case CommandType::BindPipeline:
                {
                    const commands::BindPipelineData& binding = *(const commands::BindPipelineData*)merged_data[s];
                    const PipelineGL* pipeline = access_pipeline( binding.handle );

                    device_state->pipeline = pipeline;

                    break;
                }

                case CommandType::BindResourceSet:
                {
                    const commands::BindResourceListData& binding = *(const commands::BindResourceListData*)merged_data[s];

                    for ( uint32_t l = 0; l < binding.num_lists; ++l ) {
                        const ResourceListGL* resource_list = access_resource_list( binding.handles[l] );
                        device_state->resource_lists[l] = resource_list;
                    }

                    device_state->num_lists = binding.num_lists;

                    for ( uint32_t l = 0; l < binding.num_offsets; ++l ) {
                        device_state->resource_offsets[l] = binding.offsets[l];
                    }

                    device_state->num_offsets = binding.num_offsets;

                    break;
                }

                case CommandType::Dispatch:
                {
                    device_state->apply();

                    const commands::DispatchData& dispatch = *(const commands::DispatchData*)merged_data[s];
                    glDispatchCompute( dispatch.group_x, dispatch.group_y, dispatch.group_z );

                    // TODO: barrier handling.
                    glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

                    break;
                }

                case CommandType::Draw:
                {
                    device_state->apply();

                    const commands::DrawData& draw = *(const commands::DrawData*)merged_data[s];
                    if ( draw.instance_count ) {
                        glDrawArraysInstanced( GL_TRIANGLES, draw.first_vertex, draw.vertex_count, draw.instance_count );
                    } else {
                        glDrawArrays( GL_TRIANGLES, draw.first_vertex, draw.vertex_count );
                    }

                    break;
                }

                case CommandType::DrawIndexed:
                {
                    device_state->apply();

                    const commands::DrawIndexedData& draw = *(const commands::DrawIndexedData*)merged_data[s];
                    const uint32_t index_buffer_size = 2;
                    const GLuint start = 0;
                    const GLuint start_index_offset = draw.first_index;
                    const GLuint end_index_offset = start_index_offset + draw.index_count;
                    size_t indices = (start_index_offset * index_buffer_size);
                    if ( draw.instance_count ) {
                        glDrawElementsInstancedBaseVertexBaseInstance( GL_TRIANGLES, (GLsizei)draw.index_count, GL_UNSIGNED_SHORT, (void*)indices, draw.instance_count, draw.vertex_offset, draw.first_instance );
                    } else {
                        glDrawRangeElementsBaseVertex( GL_TRIANGLES, start_index_offset, end_index_offset, (GLsizei)draw.index_count, GL_UNSIGNED_SHORT, (void*)indices, draw.vertex_offset );
                    }

                    break;
                }

                default:
                {
                    HYDRA_ASSERT( false, "Not implemented" );
                    break;
                }
            }
        }
    }

    // Reset state
    num_queued_command_buffers = 0;
}

// ResourceListGL ///////////////////////////////////////////////////////////////

void ResourceListGL::set( const uint32_t* offsets, uint32_t num_offsets ) const {

    if ( layout == nullptr ) {
        return;
    }

    // Track current constant buffer index. Used to retrieve offsets.
    uint32_t c = 0;

    for ( uint32_t r = 0; r < layout->num_bindings; ++r ) {
        const ResourceBindingGL& binding = layout->bindings[r];

        if ( binding.gl_block_binding == -1 )
            continue;

        const uint32_t resource_type = binding.type;

        switch ( resource_type ) {
            case ResourceType::Texture:
            {
                const TextureGL* texture_data = (const TextureGL*)resources[r].data;
                glBindTextureUnit( binding.gl_block_binding, texture_data->gl_handle );

                break;
            }

            case ResourceType::TextureRW:
            {
                const TextureGL* texture_data = (const TextureGL*)resources[r].data;
                glBindImageTexture( binding.gl_block_binding, texture_data->gl_handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, to_gl_internal_format( texture_data->format ) );

                break;
            }

            case ResourceType::Constants:
            {
                const BufferGL* buffer = (const BufferGL*)resources[r].data;
                const GLuint buffer_offset = 0;// num_offsets ? offsets[c] : 0;
                const GLsizei buffer_size = buffer->size;// num_offsets ? 64 : buffer->size;
                //int align;
                //glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align );
                glBindBufferRange( buffer->gl_type, binding.gl_block_binding, buffer->gl_handle, buffer_offset, buffer_size );

                ++c;

                break;
            }

            default:
            {
                //HYDRA_ASSERT( false, "Resource type not handled, %u" );
                break;
            }
        }
    }
}

// DeviceStateGL ////////////////////////////////////////////////////////////////
void DeviceStateGL::apply()  {

    if ( pipeline->graphics_pipeline ) {

        // Bind FrameBuffer: always do it. Framebuffer 0 is the swapchain.
        {
            glBindFramebuffer( GL_FRAMEBUFFER, fbo_handle );
        }

        if ( viewport ) {
            glViewport( viewport->rect.x, viewport->rect.y, viewport->rect.width, viewport->rect.height );
        }

        if ( scissor ) {
            glEnable( GL_SCISSOR_TEST );
            glScissor( scissor->x, scissor->y, scissor->width, scissor->height );
        }
        else {
            glDisable( GL_SCISSOR_TEST );
        }

        // Bind shaders
        glUseProgram( pipeline->gl_program_cached );

        if ( num_lists ) {
            for ( uint32_t l = 0; l < num_lists; ++l ) {
                resource_lists[l]->set( resource_offsets, num_offsets );
            }
        }

        // Set depth
        if ( pipeline->depth_stencil.depth_enable ) {
            glEnable( GL_DEPTH_TEST );
            glDepthFunc( to_gl_comparison( pipeline->depth_stencil.depth_comparison ) );
            glDepthMask( pipeline->depth_stencil.depth_write_enable );
        }
        else {
            glDisable( GL_DEPTH_TEST );
            glDepthMask( false );
        }

        // Set stencil
        if ( pipeline->depth_stencil.stencil_enable ) {
            HYDRA_ASSERT( false, "Not implemented." );
        }
        else {
            glDisable( GL_STENCIL_TEST );
        }

        if ( clear_color_flag || clear_depth_flag || clear_stencil_flag ) {
            glClearColor( clear_color[0], clear_color[1], clear_color[2], clear_color[3] );

            // TODO[gabriel]: single color mask enabling/disabling.
            GLuint clear_mask = GL_COLOR_BUFFER_BIT;
            clear_mask = clear_depth_flag ? clear_mask | GL_DEPTH_BUFFER_BIT : clear_mask;
            clear_mask = clear_stencil_flag ? clear_mask | GL_STENCIL_BUFFER_BIT : clear_mask;

            if ( clear_depth_flag ) {
                glClearDepth( clear_depth_value );
            }

            if ( clear_stencil_flag ) {
                glClearStencil( clear_stencil_value );
            }

            glClear( clear_mask );

            // TODO: use this version instead ?
            //glClearBufferfv( GL_COLOR, col_buff_index, &rgba );
        }

        // Set blend
        if ( pipeline->blend_state.active_states ) {
            // If there is different states, set them accordingly.
            glEnablei( GL_BLEND, 0 );

            const BlendState& blend_state = pipeline->blend_state.blend_states[0];
            glBlendFunc( to_gl_blend_function( blend_state.source_color ),
                         to_gl_blend_function( blend_state.destination_color ) );

            glBlendEquation( to_gl_blend_equation( blend_state.color_operation ) );
        }
        else if ( pipeline->blend_state.active_states > 1 ) {
            HYDRA_ASSERT( false, "Not implemented." );

            //glBlendFuncSeparate( glSrcFunction, glDstFunction, glSrcAlphaFunction, glDstAlphaFunction );
        }
        else {
            glDisable( GL_BLEND );
        }

        const RasterizationCreation& rasterization = pipeline->rasterization;
        if ( rasterization.cull_mode == CullMode::None ) {
            glDisable( GL_CULL_FACE );
        }
        else {
            glEnable( GL_CULL_FACE );
            glCullFace( rasterization.cull_mode == CullMode::Front ? GL_FRONT : GL_BACK );
        }

        glFrontFace( rasterization.front == FrontClockwise::True ? GL_CW : GL_CCW );

        // Bind vertex array, containing vertex attributes.
        glBindVertexArray( pipeline->gl_vao );

        // Bind Index Buffer
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ib_handle );

        // Bind Vertex Buffers with offsets.
        const VertexInputGL& vertex_input = pipeline->vertex_input;
        for ( uint32_t i = 0; i < vertex_input.num_streams; i++ ) {
            const VertexStream& stream = vertex_input.vertex_streams[i];

            glBindVertexBuffer( stream.binding, vb_bindings[i].vb_handle, vb_bindings[i].offset, stream.stride );
        }

        // Reset cached states
        clear_color_flag = false;
        clear_depth_flag = false;
        clear_stencil_flag = false;
        num_vertex_streams = 0;
    }
    else {

        glUseProgram( pipeline->gl_program_cached );

        if ( num_lists ) {
            for ( uint32_t l = 0; l < num_lists; ++l ) {
                resource_lists[l]->set( resource_offsets, num_offsets );
            }
        }
    }

}

// CommandBuffer ////////////////////////////////////////////////////////////////

void CommandBuffer::init( QueueType::Enum type_, uint32_t buffer_size_, uint32_t submit_size, bool baked_ ) {
    this->type = type_;
    this->buffer_size = buffer_size_;
    this->baked = baked_;

    buffer_data = (uint8_t*)malloc( buffer_size_ );
    read_offset = write_offset = 0;

    keys = (uint64_t*)malloc( sizeof( uint64_t ) * submit_size );
    types = (uint8_t*)malloc( sizeof( uint8_t ) * submit_size );
    datas = (void**)malloc( sizeof( void* ) * submit_size );
    current_command = 0;
}

void CommandBuffer::terminate() {

    free( keys );
    free( types );
    free( datas );
    free( buffer_data );

    read_offset = write_offset = buffer_size = 0;
}

void CommandBuffer::bind_pass( uint64_t sort_key, RenderPassHandle handle ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::BeginPass;
    datas[current_command] = buffer_data + write_offset;
    commands::BindPassData* command_data = (commands::BindPassData*)(buffer_data + write_offset);
    command_data->handle = handle;

    write_offset += sizeof(commands::BindPassData);
    ++current_command;
}

void CommandBuffer::bind_pipeline( uint64_t sort_key, PipelineHandle handle ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::BindPipeline;
    datas[current_command] = buffer_data + write_offset;
    commands::BindPipelineData* command_data = (commands::BindPipelineData*)(buffer_data + write_offset);
    command_data->handle = handle;

    write_offset += sizeof( commands::BindPipelineData );
    ++current_command;
}

void CommandBuffer::bind_vertex_buffer( uint64_t sort_key, BufferHandle handle, uint32_t binding, uint32_t offset ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::BindVertexBuffer;
    datas[current_command] = buffer_data + write_offset;
    commands::BindVertexBufferData* command_data = (commands::BindVertexBufferData*)(buffer_data + write_offset);
    command_data->buffer = handle;
    command_data->binding = binding;
    command_data->byte_offset = offset;

    write_offset += sizeof( commands::BindVertexBufferData );
    ++current_command;
}

void CommandBuffer::bind_index_buffer( uint64_t sort_key, BufferHandle handle ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::BindIndexBuffer;
    datas[current_command] = buffer_data + write_offset;
    commands::BindIndexBufferData* command_data = (commands::BindIndexBufferData*)(buffer_data + write_offset);
    command_data->buffer = handle;

    write_offset += sizeof( commands::BindIndexBufferData );
    ++current_command;
}

void CommandBuffer::bind_resource_list( uint64_t sort_key, ResourceListHandle* handles, uint32_t num_lists, uint32_t* offsets, uint32_t num_offsets ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::BindResourceSet;
    datas[current_command] = buffer_data + write_offset;
    commands::BindResourceListData* command_data = (commands::BindResourceListData*)(buffer_data + write_offset);
    for ( uint32_t l = 0; l < num_lists; ++l ) {
        command_data->handles[l] = handles[l];
    }

    for ( uint32_t l = 0; l < num_offsets; ++l ) {
        command_data->offsets[l] = offsets[l];
    }

    command_data->num_lists = num_lists;
    command_data->num_offsets = num_offsets;

    write_offset += sizeof( commands::BindResourceListData );
    ++current_command;
}

void CommandBuffer::set_viewport( uint64_t sort_key, const Viewport& viewport ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::SetViewport;
    datas[current_command] = buffer_data + write_offset;
    commands::SetViewportData* command_data = (commands::SetViewportData*)(buffer_data + write_offset);
    command_data->viewport = viewport;

    write_offset += sizeof( commands::SetViewportData );
    ++current_command;
}

void CommandBuffer::set_scissor( uint64_t sort_key, const Rect2DInt& rect ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::SetScissor;
    datas[current_command] = buffer_data + write_offset;
    commands::SetScissorData* command_data = (commands::SetScissorData*)(buffer_data + write_offset);
    command_data->rect = rect;

    write_offset += sizeof( commands::SetScissorData );
    ++current_command;
}

void CommandBuffer::clear( uint64_t sort_key, float red, float green, float blue, float alpha ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::Clear;
    datas[current_command] = buffer_data + write_offset;
    commands::ClearData* command_data = (commands::ClearData*)(buffer_data + write_offset);
    command_data->clear_color[0] = red;
    command_data->clear_color[1] = green;
    command_data->clear_color[2] = blue;
    command_data->clear_color[3] = alpha;

    write_offset += sizeof( commands::ClearData );
    ++current_command;
}

void CommandBuffer::clear_depth( uint64_t sort_key, float value ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::ClearDepth;
    datas[current_command] = buffer_data + write_offset;
    commands::ClearDepthData* command_data = (commands::ClearDepthData*)(buffer_data + write_offset);
    command_data->value = value;

    write_offset += sizeof( commands::ClearDepthData );
    ++current_command;
}

void CommandBuffer::clear_stencil( uint64_t sort_key, uint8_t value ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::ClearStencil;
    datas[current_command] = buffer_data + write_offset;
    commands::ClearStencilData* command_data = (commands::ClearStencilData*)(buffer_data + write_offset);
    command_data->value = value;

    write_offset += sizeof( commands::ClearStencilData );
    ++current_command;
}

void CommandBuffer::draw( uint64_t sort_key, TopologyType::Enum topology, uint32_t first_vertex, uint32_t vertex_count, uint32_t instance_count ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::Draw;
    datas[current_command] = buffer_data + write_offset;
    commands::DrawData* command_data = (commands::DrawData*)(buffer_data + write_offset);
    command_data->topology = topology;
    command_data->first_vertex = first_vertex;
    command_data->vertex_count = vertex_count;
    command_data->instance_count = instance_count;

    write_offset += sizeof( commands::DrawData );
    ++current_command;
}

void CommandBuffer::draw_indexed( uint64_t sort_key, TopologyType::Enum topology, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::DrawIndexed;
    datas[current_command] = buffer_data + write_offset;
    commands::DrawIndexedData* command_data = (commands::DrawIndexedData*)(buffer_data + write_offset);
    command_data->topology = topology;
    command_data->index_count = index_count;
    command_data->instance_count = instance_count;
    command_data->first_index = first_index;
    command_data->vertex_offset = vertex_offset;
    command_data->first_instance = first_instance;

    write_offset += sizeof( commands::DrawIndexedData );
    ++current_command;
}

void CommandBuffer::dispatch( uint64_t sort_key, uint32_t group_x, uint32_t group_y, uint32_t group_z ) {
    keys[current_command] = sort_key;
    types[current_command] = (uint8_t)CommandType::Dispatch;
    datas[current_command] = buffer_data + write_offset;
    commands::DispatchData* command_data = (commands::DispatchData*)(buffer_data + write_offset);
    command_data->group_x = (uint16_t)group_x;
    command_data->group_y = (uint16_t)group_y;
    command_data->group_z = (uint16_t)group_z;

    write_offset += sizeof( commands::BindPassData );
    ++current_command;
}

void CommandBuffer::reset() {
    read_offset = 0;

    // Reset all writing properties.
    if ( !baked ) {
        write_offset = 0;
    }

    current_command = 0;
}

// Utility methods //////////////////////////////////////////////////////////////

static bool checkFrameBuffer() {
    GLenum result = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    if ( result != GL_FRAMEBUFFER_COMPLETE ) {
        switch ( result ) {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            case GL_FRAMEBUFFER_UNSUPPORTED:
            {
                return false;
                break;
            }
        }
    }

    return true;
}

void create_fbo( const RenderPassCreation& creation, RenderPassGL& render_pass, Device& device ) {

    // build fbo
    GLuint framebuffer_handle;
    glGenFramebuffers(1, &framebuffer_handle );

    int result = glCheckFramebufferStatus( GL_FRAMEBUFFER );

    if ( result != GL_FRAMEBUFFER_COMPLETE ) {
    //    deleteDepthStencil();
    //    deleteFbo();

        if ( result == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ) {
            HYDRA_LOG( "frame buffer couldn't be constructed: incomplete attachment" );
        }
        if ( result == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT ) {
            HYDRA_LOG( "frame buffer couldn't be constructed: missing attachment" );
        }
        if ( result == GL_FRAMEBUFFER_UNSUPPORTED ) {
            HYDRA_LOG( "frame buffer couldn't be constructed: unsupported combination of formats" );
        }
        HYDRA_LOG( "frame buffer couldn't be constructed: unknown error " + result );
    }
    else {

        glBindFramebuffer( GL_FRAMEBUFFER, framebuffer_handle );
    }

    // Attach textures
    render_pass.num_render_targets = (uint8_t)creation.num_render_targets;

    for ( uint16_t i = 0; i < creation.num_render_targets; ++i ) {
    
        TextureGL* texture = device.access_texture( creation.output_textures[i] );

        // Cache texture access
        render_pass.render_targets[i] = texture;

        if ( !texture ) {
            continue;
        }

        glBindTexture( texture->gl_target, texture->gl_handle );

        if ( (texture->gl_target == GL_TEXTURE_CUBE_MAP) || (texture->gl_target == GL_TEXTURE_CUBE_MAP_ARRAY) ) {
            glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->gl_handle, 0 );
        }
        else {
            glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->gl_target, texture->gl_handle, 0 );
        }

        if ( !checkFrameBuffer() ) {
            HYDRA_LOG( "Error" );
        }
    }

    // TODO: if textures are not any input in the pipeline, they could become Render Buffer Objects - faster to render to.
    // Attach depth/stencil
    render_pass.depth_stencil = nullptr;

    if ( creation.depth_stencil_texture.handle != k_invalid_handle ) {
        TextureGL* texture = device.access_texture( creation.depth_stencil_texture );

        render_pass.depth_stencil = texture;

        if ( texture ) {

            glBindTexture( texture->gl_target, texture->gl_handle );

            const bool depth_stencil = is_depth_stencil( texture->format );
            const bool only_depth = is_depth_only( texture->format );
            const bool only_stencil = is_stencil_only( texture->format );

            if ( (texture->gl_target == GL_TEXTURE_CUBE_MAP) || (texture->gl_target == GL_TEXTURE_CUBE_MAP_ARRAY) ) {

                if ( depth_stencil ) {
                    glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture->gl_handle, 0 );
                }
                else {
                    if ( only_depth ) {
                        glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture->gl_handle, 0 );
                    }

                    if ( only_stencil ) {
                        glFramebufferTexture( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, texture->gl_handle, 0 );
                    }
                }

            }
            else {
                if ( depth_stencil ) {
                    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture->gl_target, texture->gl_handle, 0 );
                }
                else {
                    if ( only_depth ) {
                        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture->gl_target, texture->gl_handle, 0 );
                    }

                    if ( only_stencil ) {
                        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, texture->gl_target, texture->gl_handle, 0 );
                    }
                }
            }

            if ( !checkFrameBuffer() ) {
                HYDRA_LOG( "Error" );
            }
        }
    }

    const GLuint draw_buffers[8] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
    glDrawBuffers( creation.num_render_targets, draw_buffers );

    render_pass.fbo_handle = framebuffer_handle;

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

}

GLuint compile_shader( GLuint stage, const char* source, const char* shader_name ) {
    GLuint shader = glCreateShader( stage );
    if ( !shader ) {
        HYDRA_LOG( "Error creating GL shader.\n" );
        return shader;
    }

    // Attach source code and compile.
    glShaderSource( shader, 1, &source, 0 );
    glCompileShader( shader );

    if ( !get_compile_info( shader, GL_COMPILE_STATUS, shader_name ) ) {
        glDeleteShader( shader );
        shader = 0;

        HYDRA_LOG( "Error compiling GL shader.\n" );
    }

    return shader;
}

bool get_compile_info( GLuint shader, GLuint status, const char* shader_name ) {
    GLint result;
    // Status is either compile (for shaders) or link (for programs).
    glGetShaderiv( shader, status, &result );
    // Compilation failed. Get info log.
    if ( !result ) {
        GLint info_log_length = 0;
        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &info_log_length );
        if ( info_log_length > 0 ) {
            glGetShaderInfoLog( shader, s_string_buffer.buffer_size, &info_log_length, s_string_buffer.data );
            HYDRA_LOG( "Error compiling shader %s\n%s\n", shader_name, s_string_buffer.data );
        }
        return false;
    }

    return true;
}

bool get_link_info( GLuint program, GLuint status, const char* shader_name ) {
    GLint result;
    // Status is either compile (for shaders) or link (for programs).
    glGetProgramiv( program, status, &result );
    // Compilation failed. Get info log.
    if ( !result ) {
        GLint info_log_length = 0;
        glGetProgramiv( program, GL_INFO_LOG_LENGTH, &info_log_length );
        if ( info_log_length > 0 ) {
            glGetProgramInfoLog( program, s_string_buffer.buffer_size, &info_log_length, s_string_buffer.data );
            HYDRA_LOG( "Error linking shader %s\n%s\n", shader_name, s_string_buffer.data );
        }
        return false;
    }

    return true;
}

static cstring to_string_message_type( GLenum type ) {
    switch ( type )
    {
        case GL_DEBUG_TYPE_ERROR_ARB:
            return "GL ERROR       ";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
            return "GL Deprecated  ";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
            return "GL Undefined   ";
        case GL_DEBUG_TYPE_PORTABILITY_ARB:
            return "GL Portability ";
        case GL_DEBUG_TYPE_PERFORMANCE_ARB:
            return "GL Performance ";
        case GL_DEBUG_TYPE_MARKER:
            return "GL Marker      ";
        case GL_DEBUG_TYPE_PUSH_GROUP:
            return "GL Push Group  ";
        case GL_DEBUG_TYPE_POP_GROUP:
            return "GL Pop Group   ";
        default:
            return "GL Generic     ";
    }
}

static cstring to_string_message_severity( GLenum severity ) {
    switch ( severity )
    {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "-Log -:";
        case GL_DEBUG_SEVERITY_HIGH_ARB:
            return "-High-:";
        case GL_DEBUG_SEVERITY_MEDIUM_ARB:
            return "-Mid -:";
        case GL_DEBUG_SEVERITY_LOW_ARB:
            return "-Low -:";
        default:
            return "-    -:";
    }
}

void GLAPIENTRY gl_message_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam ) {
    HYDRA_LOG( "%s - %s :%s\n", to_string_message_type(type), to_string_message_severity(severity), message );
}

void cache_resource_bindings( GLuint shader, const ResourceListLayoutGL* resource_list_layout ) {

    for ( uint32_t i = 0; i < resource_list_layout->num_bindings; i++ ) {
        ResourceBindingGL& binding = resource_list_layout->bindings[i];

        binding.gl_block_binding = 0xffffffff;
        
        switch ( binding.type ) {
            case ResourceType::Constants:
            {
                binding.gl_block_index = glGetUniformBlockIndex( shader, binding.name );
                if ( binding.gl_block_index != 0xffffffff )
                    glGetActiveUniformBlockiv( shader, binding.gl_block_index, GL_UNIFORM_BLOCK_BINDING, &binding.gl_block_binding );

                break;
            }

            case ResourceType::Texture:
            case ResourceType::TextureRW:
            {
                binding.gl_block_index = glGetUniformLocation( shader, binding.name );
                if ( binding.gl_block_index != 0xffffffff )
                    glGetUniformiv( shader, binding.gl_block_index, &binding.gl_block_binding );

                break;
            }
        }
    }
}

// Testing //////////////////////////////////////////////////////////////////////
void test_texture_creation( Device& device ) {
    TextureCreation first_rt = {};
    first_rt.width = 1;
    first_rt.height = 1;
    first_rt.render_target = 1;

    HYDRA_LOG("==================================================================\n" );
    HYDRA_LOG( "Test texture creation start.\n" );

    for ( uint32_t i = 0; i < TextureFormat::BC1_TYPELESS; ++i ) {
        first_rt.format = (TextureFormat::Enum)i;
        HYDRA_LOG( "Testing creation of a texture with format %s\n", TextureFormat::ToString( first_rt.format ) );
        TextureHandle t = device.create_texture( first_rt );
        device.destroy_texture( t );
    }

    HYDRA_LOG( "Test finished\n" );
    HYDRA_LOG( "==================================================================\n" );
}

void test_pool( Device & device ) {
    TextureCreation texture_creation = {};
    texture_creation.width = 1;
    texture_creation.height = 1;
    texture_creation.render_target = 1;
    texture_creation.format = TextureFormat::R8_UINT;

    TextureHandle t0 = device.create_texture( texture_creation );
    TextureHandle t1 = device.create_texture( texture_creation );
    TextureHandle t2 = device.create_texture( texture_creation );

    TextureDescription t1_info;
    device.query_texture( t1, t1_info );

    device.destroy_texture( t1 );
    device.destroy_texture( t0 );
    device.destroy_texture( t2 );
}

void test_command_buffer( Device& device ) {
    graphics::CommandBuffer* commands = device.get_command_buffer( graphics::QueueType::Graphics, false );

    commands->draw( 0, graphics::TopologyType::Triangle, 0, 3 );

    const graphics::commands::DrawData& draw = *(const graphics::commands::DrawData*)commands->datas[0];
    HYDRA_ASSERT( draw.first_vertex == 0, "First vertex should be 0 instead of %u", draw.first_vertex );
    HYDRA_ASSERT( draw.vertex_count == 3, "Vertex count should be 3 instead of %u", draw.vertex_count );
    HYDRA_ASSERT( draw.topology == TopologyType::Triangle, "Topology should be triangle instead of %s", TopologyType::ToString(draw.topology) );
}


// Vulkan ///////////////////////////////////////////////////////////////////////

#elif defined (HYDRA_VULKAN)

// Enum translations. Use tables or switches depending on the case. ////////////


static VkFormat to_vk_format( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::R32G32B32A32_FLOAT:
              return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::R32G32B32A32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case TextureFormat::R32G32B32A32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case TextureFormat::R32G32B32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::R32G32B32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case TextureFormat::R32G32B32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case TextureFormat::R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case TextureFormat::R16G16B16A16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case TextureFormat::R16G16B16A16_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case TextureFormat::R16G16B16A16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case TextureFormat::R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::R32G32_UINT:
            return VK_FORMAT_R32G32_UINT;
        case TextureFormat::R32G32_SINT:
            return VK_FORMAT_R32G32_SINT;
        //case TextureFormat::R10G10B10A2_TYPELESS:
        //    return GL_RGB10_A2;
        case TextureFormat::R10G10B10A2_UNORM:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R10G10B10A2_UINT:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R11G11B10_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        //case TextureFormat::R8G8B8A8_TYPELESS:
        //    return GL_RGBA8;
        case TextureFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        //case TextureFormat::R8G8B8A8_UNORM_SRGB:
        //    return GL_SRGB8_ALPHA8;
        case TextureFormat::R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::R8G8B8A8_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        //case TextureFormat::R16G16_TYPELESS:
        //    return GL_RG16UI;
        case TextureFormat::R16G16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::R16G16_UNORM:
            return VK_FORMAT_R16G16_UNORM;
        case TextureFormat::R16G16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case TextureFormat::R16G16_SNORM:
            return VK_FORMAT_R16G16_SNORM;
        case TextureFormat::R16G16_SINT:
            return VK_FORMAT_R16G16_SINT;
        //case TextureFormat::R32_TYPELESS:
        //    return GL_R32UI;
        case TextureFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case TextureFormat::R32_SINT:
            return VK_FORMAT_R32_SINT;
        //case TextureFormat::R8G8_TYPELESS:
        //    return GL_RG8UI;
        case TextureFormat::R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::R8G8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case TextureFormat::R8G8_SNORM:
            return VK_FORMAT_R8G8_SNORM;
        case TextureFormat::R8G8_SINT:
            return VK_FORMAT_R8G8_SINT;
        //case TextureFormat::R16_TYPELESS:
        //    return GL_R16UI;
        case TextureFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::R16_UNORM:
            return VK_FORMAT_R16_UNORM;
        case TextureFormat::R16_UINT:
            return VK_FORMAT_R16_UINT;
        case TextureFormat::R16_SNORM:
            return VK_FORMAT_R16_SNORM;
        case TextureFormat::R16_SINT:
            return VK_FORMAT_R16_SINT;
        //case TextureFormat::R8_TYPELESS:
        //    return GL_R8UI;
        case TextureFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::R8_UINT:
            return VK_FORMAT_R8_UINT;
        case TextureFormat::R8_SNORM:
            return VK_FORMAT_R8_SNORM;
        case TextureFormat::R8_SINT:
            return VK_FORMAT_R8_SINT;
        //case TextureFormat::R9G9B9E5_SHAREDEXP:
        //    return GL_RGB9_E5;
        //case TextureFormat::R32G32B32A32_TYPELESS:
        //    return GL_RGBA32UI;
        //case TextureFormat::R32G32B32_TYPELESS:
        //    return GL_RGB32UI;
        //case TextureFormat::R16G16B16A16_TYPELESS:
        //    return GL_RGBA16UI;
        //case TextureFormat::R32G32_TYPELESS:
        //    return GL_RG32UI;
        //// Depth formats  
        case TextureFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::D32_FLOAT_S8X24_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TextureFormat::D24_UNORM_X8_UINT:
            return VK_FORMAT_X8_D24_UNORM_PACK32;
        case TextureFormat::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case TextureFormat::S8_UINT:
            return VK_FORMAT_S8_UINT;

        //// Compressed
        //case TextureFormat::BC1_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC1_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC1_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_SNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_SNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B5G6R5_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B5G5R5A1_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::R10G10B10_XR_BIAS_A2_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_UF16:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_SF16:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_UNORM_SRGB:
        //    return GL_RGBA32F;

        case TextureFormat::UNKNOWN:
        default:
            //HYDRA_ASSERT( false, "Format %s not supported on Vulkan.", EnumNamesTextureFormat()[format] );
            break;
    }

    return VK_FORMAT_UNDEFINED;
}

//
//
static VkImageType to_vk_image_type( TextureType::Enum type ) {
    static VkImageType s_vk_target[TextureType::Count] = { VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D };
    return s_vk_target[type];
}

//
//
static VkImageViewType to_vk_image_view_type( TextureType::Enum type ) {
    static VkImageViewType s_vk_data[] = { VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY };
    return s_vk_data[type];
}

//
//
static VkDescriptorType to_vk_descriptor_type( ResourceType::Enum type ) {
    // Sampler, Texture, Image, ImageRW, Constants, Buffer, BufferRW, Count
    static VkDescriptorType s_vk_type[ResourceType::Count] = { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER };
    return s_vk_type[type];
}

//
//
static VkShaderStageFlagBits to_vk_shader_stage( ShaderStage::Enum value ) {
    // Vertex, Fragment, Geometry, Compute, Hull, Domain, Count
    static VkShaderStageFlagBits s_vk_stage[ShaderStage::Count] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT };

    return s_vk_stage[value];
}

//
//
static VkFormat to_vk_vertex_format( VertexComponentFormat::Enum value ) {
    // Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Count
    static VkFormat s_vk_vertex_formats[VertexComponentFormat::Count] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, /*MAT4 TODO*/VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                          VK_FORMAT_R8_SINT, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SNORM,
                                                                          VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SNORM };

    return s_vk_vertex_formats[value];
}

//
//
static VkCullModeFlags to_vk_cull_mode( CullMode::Enum value ) {
    // TODO: there is also front_and_back!
    static VkCullModeFlags s_vk_cull_mode[CullMode::Count] = { VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT };
    return s_vk_cull_mode[value];
}

//
//
static VkFrontFace to_vk_front_face( FrontClockwise::Enum value ) {

    return value == FrontClockwise::True ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

//
//
static VkBlendFactor to_vk_blend_factor( Blend::Enum value ) {
    // Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor, SrcAlphasat, Src1Color, InvSrc1Color, Src1Alpha, InvSrc1Alpha, Count
    static VkBlendFactor s_vk_blend_factor[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                                                 VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
                                                 VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_SRC1_COLOR,
                                                 VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA };

    return s_vk_blend_factor[value];
}

//
//
static VkBlendOp to_vk_blend_operation( BlendOperation::Enum value ) {
    //Add, Subtract, RevSubtract, Min, Max, Count
    static VkBlendOp s_vk_blend_op[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
    return s_vk_blend_op[value];
}

//
//
static VkCompareOp to_vk_compare_operation( ComparisonFunction::Enum value ) {
    static VkCompareOp s_vk_values[] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS };
    return s_vk_values[value];
}

//
//
static VkPipelineStageFlags to_vk_pipeline_stage( PipelineStage::Enum value ) {
    static VkPipelineStageFlags s_vk_values[] = { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
    return s_vk_values[value];
}

//
//
// Repeat, Mirrored_Repeat, Clamp_Edge, Clamp_Border, Count
static VkSamplerAddressMode to_vk_address_mode( TextureAddressMode::Enum value ) {
    static VkSamplerAddressMode s_vk_values[] = { VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER };
    return s_vk_values[ value ];
}

//
//
static VkFilter to_vk_filter( TextureFilter::Enum value ) {
    static VkFilter s_vk_values[] = { VK_FILTER_NEAREST, VK_FILTER_LINEAR };
    return s_vk_values[ value ];
}

//
//
static VkSamplerMipmapMode to_vk_mipmap( TextureMipFilter::Enum value ) {
    static VkSamplerMipmapMode s_vk_values[] = { VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR };
    return s_vk_values[ value ];
}

// Structs //////////////////////////////////////////////////////////////////////

//
//
struct BufferVulkan {

    VkBuffer                        vk_buffer;
    VmaAllocation                   vma_allocation;
    VkDeviceMemory                  vk_device_memory;
    VkDeviceSize                    vk_device_size;

    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    u32                             size                = 0;
    const char*                     name                = nullptr;

    BufferHandle                    handle;

}; // struct BufferVulkan


//
//
struct SamplerVulkan {

    VkSampler                       vk_sampler;

    TextureFilter::Enum             min_filter          = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter          = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter          = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w      = TextureAddressMode::Repeat;

    const char*                     name                = nullptr;

}; // struct SamplerVulkan

//
//
struct TextureVulkan {

    VkFormat                        vk_format;
    VkImage                         vk_image;
    VkImageView                     vk_image_view;
    VkImageLayout                   vk_image_layout;
    VmaAllocation                   vma_allocation;

    u16                             width               = 1;
    u16                             height              = 1;
    u16                             depth               = 1;
    u8                              mipmaps             = 1;
    u8                              render_target       = 0;
    u8                              flags               = 0;

    TextureHandle                   handle;

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

    SamplerVulkan*                  sampler             = nullptr;

    const char*                     name                = nullptr;
}; // struct TextureVulkan

//
//
struct ShaderStateVulkan {

    VkPipelineShaderStageCreateInfo shader_stage_info[k_max_shader_stages];

    const char*                     name                = nullptr;
    u32                             active_shaders      = 0;
    bool                            graphics_pipeline   = false;
};

//
//
struct PipelineVulkan {

    VkPipeline                      vk_pipeline;
    VkPipelineBindPoint             vk_bind_point;
    VkPipelineLayout                vk_pipeline_layout;

    ShaderStateHandle               shader_state;

    const ResourceListLayoutVulkan* resource_list_layout[k_max_resource_layouts];
    ResourceListLayoutHandle        resource_list_layout_handle[k_max_resource_layouts];
    u32                             num_active_layouts = 0;

    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    //VertexInputGL                   vertex_input;
    RasterizationCreation           rasterization;

    PipelineHandle                  handle;
    bool                            graphics_pipeline = true;

}; // struct PipelineVulkan


//
//
struct RenderPassVulkan {

    VkRenderPass                    vk_render_pass;
    VkFramebuffer                   vk_frame_buffer;

    TextureHandle                   output_textures[ k_max_image_outputs ];
    TextureHandle                   output_depth;

    RenderPassType::Enum            type;

    f32                             scale_x         = 1.f;
    f32                             scale_y         = 1.f;
    u16                             width           = 0;
    u16                             height          = 0;
    u16                             dispatch_x      = 0;
    u16                             dispatch_y      = 0;
    u16                             dispatch_z      = 0;

    u8                              clear_color     = 0;
    u8                              resize          = 0;
    u8                              num_render_targets = 0;

    const char*                     name            = nullptr;
}; // struct RenderPassVulkan

//
//
struct ResourceBindingVulkan {

    u16                             type    = 0;    // ResourceType
    u16                             start   = 0;
    u16                             count   = 0;
    u16                             set     = 0;

    const char*                     name    = nullptr;
}; // struct ResourceBindingVulkan

//
//
struct ResourceListLayoutVulkan {

    VkDescriptorSetLayout           vk_descriptor_set_layout;

    VkDescriptorSetLayoutBinding*   vk_binding  = nullptr;
    ResourceBindingVulkan*          bindings    = nullptr;
    u32                             num_bindings = 0;

    ResourceListLayoutHandle        handle;

}; // struct ResourceListLayoutVulkan

//
//
struct ResourceListVulkan {
    
    VkDescriptorSet                 vk_descriptor_set;

    ResourceData*                   resources           = nullptr;

    const ResourceListLayoutVulkan* layout              = nullptr;
    u32                             num_resources       = 0;
}; // struct ResourceListVulkan

// Methods //////////////////////////////////////////////////////////////////////

// Enable this to add debugging capabilities.
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html
#define VULKAN_DEBUG_REPORT

static const char* s_requested_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    // Platform specific extension
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_MACOS_MVK
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XLIB_KHR
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_WAYLAND_KHR
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_MIR_KHR || VK_USE_PLATFORM_DISPLAY_KHR
        VK_KHR_DISPLAY_EXTENSION_NAME,
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_IOS_MVK
        VK_MVK_IOS_SURFACE_EXTENSION_NAME,
#endif // VK_USE_PLATFORM_WIN32_KHR

#if defined (VULKAN_DEBUG_REPORT)
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // VULKAN_DEBUG_REPORT
};

static const char* s_requested_layers[] = {
#if defined (VULKAN_DEBUG_REPORT)
    "VK_LAYER_KHRONOS_validation",
    //"VK_LAYER_LUNARG_core_validation",
    //"VK_LAYER_LUNARG_image",
    //"VK_LAYER_LUNARG_parameter_validation",
    //"VK_LAYER_LUNARG_object_tracker"
#else
    "",
#endif // VULKAN_DEBUG_REPORT
};

#ifdef VULKAN_DEBUG_REPORT

// Old debug callback.
//static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData ) {
//    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
//    HYDRA_LOG( "[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
//    return VK_FALSE;
//}

static VkBool32 debug_utils_callback( VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT types,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                            void* user_data ) {
    HYDRA_LOG( " MessageID: %s %i\nMessage: %s\n\n", callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage );
    return VK_FALSE;
}

#endif // VULKAN_DEBUG_REPORT

static void                         check( VkResult result );

static SDL_Window*                  sdl_window;

PFN_vkSetDebugUtilsObjectNameEXT    pfnSetDebugUtilsObjectNameEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT    pfnCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT      pfnCmdEndDebugUtilsLabelEXT;


#if defined(VULKAN_DEBUG_REPORT)


// TODO:
// GPU Timestamps ///////////////////////////////////////////////////

VkDebugUtilsMessengerCreateInfoEXT create_debug_utiles_messenger_info() {
    VkDebugUtilsMessengerCreateInfoEXT creation_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    creation_info.pfnUserCallback = debug_utils_callback;
    creation_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    creation_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

    return creation_info;
}
#endif // VULKAN_DEBUG_REPORT

void Device::backend_init( const DeviceCreation& creation ) {
    

    //////// Init Vulkan instance.
    VkResult result;
    vulkan_allocation_callbacks = nullptr;

    VkApplicationInfo application_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Hydra Graphics Device", 1, "Hydra", 1, VK_MAKE_VERSION( 1, 2, 0 ) };

    VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &application_info,
#if defined(VULKAN_DEBUG_REPORT)
                                         ArrayLength( s_requested_layers ), s_requested_layers,
#else
                                         0, nullptr,
#endif
                                         ArrayLength( s_requested_extensions ), s_requested_extensions };
#ifdef VULKAN_DEBUG_REPORT
    const VkDebugUtilsMessengerCreateInfoEXT debug_create_info = create_debug_utiles_messenger_info();
    create_info.pNext = &debug_create_info;
#endif
    //// Create Vulkan Instance
    result = vkCreateInstance( &create_info, vulkan_allocation_callbacks, &vulkan_instance );
    check( result );

    swapchain_width = creation.width;
    swapchain_height = creation.height;

    //// Choose extensions
#ifdef VULKAN_DEBUG_REPORT
    {
        uint32_t num_instance_extensions;
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, nullptr );
        VkExtensionProperties* extensions = ( VkExtensionProperties* )malloc( sizeof( VkExtensionProperties ) * num_instance_extensions );
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, extensions );
        for ( size_t i = 0; i < num_instance_extensions; i++ ) {

            if ( !strcmp( extensions[ i ].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) ) {
                debug_utils_extension_present = true;
                break;
            }
        }

        if ( !debug_utils_extension_present ) {
            hydra::print_format( "Extension %s for debugging non present.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        }
        else {
            //// Create debug callback
            //auto vkCreateDebugReportCallbackEXT = ( PFN_vkCreateDebugReportCallbackEXT )vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugReportCallbackEXT" );
            //HYDRA_ASSERT( vkCreateDebugReportCallbackEXT != NULL, "" );

            //// Setup the debug report callback
            /*VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_callback;
            debug_report_ci.pUserData = NULL;
            result = vkCreateDebugReportCallbackEXT( vulkan_instance, &debug_report_ci, vulkan_allocation_callbacks, &vulkan_debug_callback );
            check( result );*/

            // Create new debug utils callback
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = ( PFN_vkCreateDebugUtilsMessengerEXT )vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugUtilsMessengerEXT" );
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = create_debug_utiles_messenger_info();

            vkCreateDebugUtilsMessengerEXT( vulkan_instance, &debug_messenger_create_info, vulkan_allocation_callbacks, &vulkan_debug_utils_messenger );
        }
    }
#endif

    //////// Choose physical device
    uint32_t num_physical_device;
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, NULL );
    check( result );

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc( sizeof( VkPhysicalDevice ) * num_physical_device );
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, gpus );
    check( result );

    // TODO: improve - choose the first gpu.
    vulkan_physical_device = gpus[0];
    free( gpus );

    vkGetPhysicalDeviceProperties( vulkan_physical_device, &vulkan_physical_properties );
    gpu_timestamp_frequency = vulkan_physical_properties.limits.timestampPeriod / ( 1000 * 1000 );

    //////// Create logical device
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( vulkan_physical_device, &queue_family_count, nullptr );

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)malloc( sizeof( VkQueueFamilyProperties ) * queue_family_count );
    vkGetPhysicalDeviceQueueFamilyProperties( vulkan_physical_device, &queue_family_count, queue_families );

    uint32_t family_index = 0;
    for ( ; family_index < queue_family_count; ++family_index ) {
        VkQueueFamilyProperties queue_family = queue_families[family_index];
        if ( queue_family.queueCount > 0 && queue_family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) ) {
            //indices.graphicsFamily = i;
            break;
        }

        //VkBool32 presentSupport = false;
        //vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, i, _surface, &presentSupport );
        //if ( queue_family.queueCount && presentSupport ) {
        //    indices.presentFamily = i;
        //}

        //if ( indices.isComplete() ) {
        //    break;
        //}
    }

    free( queue_families );

    int device_extension_count = 1;
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = family_index;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = sizeof( queue_info ) / sizeof( queue_info[0] );
    device_create_info.pQueueCreateInfos = queue_info;
    device_create_info.enabledExtensionCount = device_extension_count;
    device_create_info.ppEnabledExtensionNames = device_extensions;
    result = vkCreateDevice( vulkan_physical_device, &device_create_info, vulkan_allocation_callbacks, &vulkan_device );
    check( result );

    //  Get the function pointers to Debug Utils functions.
    if ( debug_utils_extension_present ) {
        pfnSetDebugUtilsObjectNameEXT = ( PFN_vkSetDebugUtilsObjectNameEXT )vkGetDeviceProcAddr( vulkan_device, "vkSetDebugUtilsObjectNameEXT" );
        pfnCmdBeginDebugUtilsLabelEXT = ( PFN_vkCmdBeginDebugUtilsLabelEXT )vkGetDeviceProcAddr( vulkan_device, "vkCmdBeginDebugUtilsLabelEXT" );
        pfnCmdEndDebugUtilsLabelEXT     = ( PFN_vkCmdEndDebugUtilsLabelEXT )vkGetDeviceProcAddr( vulkan_device, "vkCmdEndDebugUtilsLabelEXT" );
    }

    vkGetDeviceQueue( vulkan_device, family_index, 0, &vulkan_queue );

    vulkan_queue_family = family_index;

    //////// Create drawable surface
#if defined (HYDRA_GFX_SDL)

    // Create surface
    SDL_Window* window = (SDL_Window*)creation.window;
    if ( SDL_Vulkan_CreateSurface( window, vulkan_instance, &vulkan_window_surface ) == SDL_FALSE ) {
        HYDRA_LOG( "Failed to create Vulkan surface.\n" );
    }

    sdl_window = window;

    // Create Framebuffers
    int window_width, window_height;
    SDL_GetWindowSize( window, &window_width, &window_height );
#else
    static_assert(false, "Create surface manually!");
#endif // HYDRA_GFX_SDL

    //// Select Surface Format
    const VkFormat surface_image_formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    uint32_t supported_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, NULL );
    VkSurfaceFormatKHR* supported_formats = (VkSurfaceFormatKHR*)malloc( sizeof( VkSurfaceFormatKHR ) * supported_count );
    vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, supported_formats );

    //// Check for supported formats
    bool format_found = false;
    const uint32_t surface_format_count = ArrayLength( surface_image_formats );

    for ( int i = 0; i < surface_format_count; i++ ) {
        for ( uint32_t j = 0; j < supported_count; j++ ) {
            if ( supported_formats[j].format == surface_image_formats[i] && supported_formats[j].colorSpace == surface_color_space ) {
                vulkan_surface_format = supported_formats[j];
                format_found = true;
                break;
            }
        }

        if ( format_found )
            break;
    }

    // Default to the first format supported.
    if ( !format_found )
        vulkan_surface_format = supported_formats[0];

    //// Select present mode
    //VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
    supported_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, NULL );
    VkPresentModeKHR* supported_modes = (VkPresentModeKHR*)malloc( sizeof( VkPresentModeKHR ) * supported_count );
    vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, supported_modes );

    uint32_t i = 0;
    bool mode_found = false;
    const uint32_t modes_count = ArrayLength( present_modes );

    for ( ; i < modes_count; i++ ) {
        for ( uint32_t j = 0; j < supported_count; j++ ) {
            if ( present_modes[i] == supported_modes[j] ) {
                mode_found = true;
                break;
            }
        }

        if ( mode_found )
            break;
    }

    vulkan_present_mode = i >= modes_count ? VK_PRESENT_MODE_FIFO_KHR : present_modes[i];
    
    //////// Create swapchain
    vulkan_swapchain_image_count = vulkan_present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? 3 : 2;

    create_swapchain();

    //////// Create VMA Allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = vulkan_physical_device;
    allocatorInfo.device = vulkan_device;

    result = vmaCreateAllocator( &allocatorInfo, &vma_allocator );
    check( result );

    ////////  Create pools
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * ArrayLength( pool_sizes );
    pool_info.poolSizeCount = (uint32_t)ArrayLength( pool_sizes );
    pool_info.pPoolSizes = pool_sizes;
    result = vkCreateDescriptorPool( vulkan_device, &pool_info, vulkan_allocation_callbacks, &vulkan_descriptor_pool );
    check( result );

    // Create timestamp query pool used for GPU timings.
    VkQueryPoolCreateInfo vqpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP, creation.gpu_time_queries_per_frame * 2 * k_max_frames, 0 };
    vkCreateQueryPool( vulkan_device, &vqpci, vulkan_allocation_callbacks, &vulkan_timestamp_query_pool );

#if defined (HYDRA_GRAPHICS_TEST)
    test_texture_creation( *this );
    test_pool( *this );
    test_command_buffer( *this );
#endif // HYDRA_GRAPHICS_TEST


    //// Init pools
    buffers.init( 128, sizeof( BufferVulkan ) );
    textures.init( 128, sizeof( TextureVulkan ) );
    render_passes.init( 256, sizeof( RenderPassVulkan ) );
    resource_list_layouts.init( 128, sizeof( ResourceListLayoutVulkan ) );
    pipelines.init( 128, sizeof( PipelineVulkan ) );
    shaders.init( 128, sizeof( ShaderStateVulkan ) );
    resource_lists.init( 128, sizeof( ResourceListVulkan ) );
    samplers.init( 32, sizeof( SamplerVulkan ) );
    command_buffers.init( 128, sizeof( CommandBuffer ) );

    // Init render frame informations. This includes fences, semaphores, command buffers, ...
    render_frames = (DeviceRenderFrame*)malloc( sizeof( DeviceRenderFrame ) * k_max_frames );
    for ( size_t i = 0; i < k_max_frames; i++ ) {
        render_frames[i].init( this, 1 );
    }

    //
    // Init primitive resources
    BufferCreation fullscreen_vb_creation = { BufferType::Vertex, ResourceUsageType::Immutable, 0, nullptr, "Fullscreen_vb" };
    fullscreen_vertex_buffer = create_buffer( fullscreen_vb_creation );

    // Create depth image
    TextureCreation depth_texture_creation = { nullptr, swapchain_width, swapchain_height, 1, 1, 0, TextureFormat::D24_UNORM_X8_UINT, TextureType::Texture2D, "DepthImage_Texture" };
    depth_texture = create_texture( depth_texture_creation );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type( RenderPassType::Swapchain ).set_name( "Swapchain" );
    swapchain_pass = create_render_pass( swapchain_pass_creation );

    // Init Dummy resources
    TextureCreation dummy_texture_creation = { nullptr, 1, 1, 1, 1, 0, TextureFormat::R8_UINT, TextureType::Texture2D };
    dummy_texture = create_texture( dummy_texture_creation );

    SamplerCreation sc{};
    sc.set_address_mode_uvw( TextureAddressMode::Repeat, TextureAddressMode::Repeat, TextureAddressMode::Repeat )
        .set_min_mag_mip( TextureFilter::Linear, TextureFilter::Linear, TextureMipFilter::Linear ).set_name( "Sampler Default" );
    default_sampler = create_sampler( sc );

    BufferCreation dummy_constant_buffer_creation = { BufferType::Constant, ResourceUsageType::Immutable, 16, nullptr, "Dummy_cb" };
    dummy_constant_buffer = create_buffer( dummy_constant_buffer_creation );

    queued_command_buffers = (CommandBuffer**)malloc( sizeof( CommandBuffer* ) * 128 );

    for ( size_t i = 0; i < command_buffers.size; i++ ) {
        CommandBuffer* command_buffer = (CommandBuffer*)command_buffers.access_resource( i );
        command_buffer->reset();
    }

    vulkan_image_index = 0;
    current_frame = 0;
    absolute_frame = 0;
    timestamps_enabled = false;

    // Get binaries path
    char* vulkan_env = s_string_buffer.reserve( 512 );
    ExpandEnvironmentStringsA( "%VULKAN_SDK%", vulkan_env, 512 );
    char* compiler_path = s_string_buffer.append_use( "%s\\Bin\\", vulkan_env );

    strcpy( vulkan_binaries_path, compiler_path );
    s_string_buffer.clear();

    gpu_timestamp_manager = ( GPUTimestampManager* )malloc( sizeof( GPUTimestampManager ) );
    gpu_timestamp_manager->init( creation.gpu_time_queries_per_frame );
}

void Device::backend_terminate() {

    // Run another present to finish using command buffers.
    for ( size_t i = 0; i < k_max_frames; i++ ) {
        present();
    }
    
    for ( size_t i = 0; i < k_max_frames; i++ ) {
        render_frames[ i ].terminate();
    }

    gpu_timestamp_manager->shutdown();

    free( queued_command_buffers );
    free( render_frames );

    destroy_texture( depth_texture );
    destroy_buffer( fullscreen_vertex_buffer );
    destroy_render_pass( swapchain_pass );
    destroy_texture( dummy_texture );
    destroy_buffer( dummy_constant_buffer );
    destroy_sampler( default_sampler );

    // Destroy all pending resources.
    for ( size_t i = 0; i < num_deletion_queue; i++ ) {
        ResourceDeletion& resource_deletion = resource_deletion_queue[ i ];

        // Skip just freed resources.
        if ( resource_deletion.current_frame == -1 )
            continue;

        switch ( resource_deletion.type ) {

            case ResourceDeletionType::Buffer:
            {
                destroy_buffer_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Pipeline:
            {
                destroy_pipeline_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::RenderPass:
            {
                destroy_render_pass_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ResourceList:
            {
                destroy_resource_list_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ResourceListLayout:
            {
                destroy_resource_list_layout_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Sampler:
            {
                destroy_sampler_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ShaderState:
            {
                destroy_shader_state_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Texture:
            {
                destroy_texture_instant( resource_deletion.handle );
                break;
            }
        }
    }
    num_deletion_queue = 0;

    destroy_swapchain();
    vkDestroySurfaceKHR( vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks );

    vmaDestroyAllocator( vma_allocator );

    command_buffers.terminate();
    pipelines.terminate();
    buffers.terminate();
    shaders.terminate();
    textures.terminate();
    samplers.terminate();
    resource_list_layouts.terminate();
    resource_lists.terminate();
    render_passes.terminate();
#ifdef VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    //auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkDestroyDebugReportCallbackEXT" );
    //vkDestroyDebugReportCallbackEXT( vulkan_instance, vulkan_debug_callback, vulkan_allocation_callbacks );

    auto vkDestroyDebugUtilsMessengerEXT = ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( vulkan_instance, "vkDestroyDebugUtilsMessengerEXT" );
    vkDestroyDebugUtilsMessengerEXT( vulkan_instance, vulkan_debug_utils_messenger, vulkan_allocation_callbacks );
#endif // IMGUI_VULKAN_DEBUG_REPORT

    vkDestroyDescriptorPool( vulkan_device, vulkan_descriptor_pool, vulkan_allocation_callbacks );
    vkDestroyQueryPool( vulkan_device, vulkan_timestamp_query_pool, vulkan_allocation_callbacks );

    vkDestroyDevice( vulkan_device, vulkan_allocation_callbacks );
    
    vkDestroyInstance( vulkan_instance, vulkan_allocation_callbacks );
}

void transition_image_layout( VkCommandBuffer command_buffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout ) {
    
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        //throw std::invalid_argument( "unsupported layout transition!" );
    }

    vkCmdPipelineBarrier( command_buffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}

static u32 clamp( u32 value, u32 min, u32 max ) {
    u32 return_value = value < min ? min : value;
    return return_value > max ? max : return_value;
}


void Device::create_swapchain() {

    //// Check if surface is supported
    // TODO: Windows only!
    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, vulkan_queue_family, vulkan_window_surface, &surface_supported );
    if ( surface_supported != VK_TRUE ) {
        HYDRA_LOG( "Error no WSI support on physical device 0\n" );
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vulkan_physical_device, vulkan_window_surface, &surface_capabilities );

    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    if ( swapchain_extent.width == UINT32_MAX || swapchain_extent.height == UINT32_MAX ) {
        swapchain_extent.width = clamp( swapchain_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width );
        swapchain_extent.height = clamp( swapchain_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height );
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = vulkan_window_surface;
    swapchain_create_info.minImageCount = vulkan_swapchain_image_count;
    swapchain_create_info.imageFormat = vulkan_surface_format.format;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = vulkan_present_mode;

    VkResult result = vkCreateSwapchainKHR( vulkan_device, &swapchain_create_info, 0, &vulkan_swapchain );
    check( result );

    //// Cache swapchain images
    vkGetSwapchainImagesKHR( vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, NULL );
    vkGetSwapchainImagesKHR( vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, vulkan_swapchain_images );

    for ( size_t iv = 0; iv < vulkan_swapchain_image_count; iv++ ) {
        // Create an image view which we can render into.
        VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = vulkan_surface_format.format;
        view_info.image = vulkan_swapchain_images[iv];
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;

        check( vkCreateImageView( vulkan_device, &view_info, vulkan_allocation_callbacks, &vulkan_swapchain_image_views[iv] ) );
    }
}

void Device::destroy_swapchain() {

    for ( size_t iv = 0; iv < vulkan_swapchain_image_count; iv++ ) {
        vkDestroyImageView( vulkan_device, vulkan_swapchain_image_views[ iv ], vulkan_allocation_callbacks );
        vkDestroyFramebuffer( vulkan_device, vulkan_swapchain_framebuffers[ iv], vulkan_allocation_callbacks );
    }

    vkDestroySwapchainKHR( vulkan_device, vulkan_swapchain, vulkan_allocation_callbacks );
}

// Resource Creation ////////////////////////////////////////////////////////////
static void vulkan_create_texture( Device& gpu, const TextureCreation& creation, TextureHandle handle, TextureVulkan* texture ) {

    texture->width = creation.width;
    texture->height = creation.height;
    texture->depth = creation.depth;
    texture->mipmaps = creation.mipmaps;
    texture->format = creation.format;
    texture->type = creation.type;
    texture->render_target = creation.flags & TextureCreationFlags::RenderTarget_mask;
    texture->name = creation.name;
    texture->vk_format = to_vk_format( creation.format );
    texture->sampler = nullptr;
    texture->flags = creation.flags;

    texture->handle = handle;

    //// Create the image
    VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.format = texture->vk_format;
    image_info.flags = 0;
    image_info.imageType = to_vk_image_type( creation.type );
    image_info.extent.width = creation.width;
    image_info.extent.height = creation.height;
    image_info.extent.depth = creation.depth;
    image_info.mipLevels = creation.mipmaps;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    if ( TextureFormat::has_depth_or_stencil( creation.format ) ) {
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.usage |= ( texture->render_target ) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
        image_info.usage |= ( creation.flags & TextureCreationFlags::ComputeOutput_mask ) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    } else {
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TODO
        image_info.usage |= ( texture->render_target ) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        image_info.usage |= ( creation.flags & TextureCreationFlags::ComputeOutput_mask ) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    }

    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check( vmaCreateImage( gpu.vma_allocator, &image_info, &memory_info,
                           &texture->vk_image, &texture->vma_allocation, nullptr ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE, ( uint64_t )texture->vk_image, creation.name );

    //// Create the image view
    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    info.image = texture->vk_image;
    info.viewType = to_vk_image_view_type( creation.type );
    info.format = image_info.format;

    if ( TextureFormat::has_depth_or_stencil( creation.format ) ) {

        info.subresourceRange.aspectMask = TextureFormat::has_depth( creation.format ) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        // TODO:gs
        //info.subresourceRange.aspectMask |= TextureFormat::has_stencil( creation.format ) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    } else {
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;
    check( vkCreateImageView( gpu.vulkan_device, &info, gpu.vulkan_allocation_callbacks, &texture->vk_image_view ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE_VIEW, ( uint64_t )texture->vk_image_view, creation.name );

    texture->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

TextureHandle Device::create_texture( const TextureCreation& creation ) {

    uint32_t resource_index = textures.obtain_resource();
    TextureHandle handle = { resource_index };
    if ( resource_index == k_invalid_handle ) {
        return handle;
    }

    TextureVulkan* texture = access_texture( handle );

    vulkan_create_texture( *this, creation, handle, texture );

    //// Copy buffer_data if present
    if ( creation.initial_data ) {
        // Create stating buffer
        VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        uint32_t image_size = creation.width * creation.height * 4;
        buffer_info.size = image_size;

        VmaAllocationCreateInfo memory_info{};
        memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VmaAllocationInfo allocation_info{};
        VkBuffer staging_buffer;
        VmaAllocation staging_allocation;
        check( vmaCreateBuffer( vma_allocator, &buffer_info, &memory_info,
               &staging_buffer, &staging_allocation, &allocation_info ) );

        // Copy buffer_data
        void* data;
        vmaMapMemory( vma_allocator, staging_allocation, &data );
        memcpy( data, creation.initial_data, static_cast<size_t>( image_size ) );
        vmaUnmapMemory( vma_allocator, staging_allocation );

        // Execute command buffer
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        CommandBuffer* command_buffer = get_command_buffer( QueueType::Graphics, false, false );
        vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { creation.width, creation.height, creation.depth };

        // Transition
        transition_image_layout( command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
        // Copy
        vkCmdCopyBufferToImage( command_buffer->vk_command_buffer, staging_buffer, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
        // Transition
        transition_image_layout( command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

        vkEndCommandBuffer( command_buffer->vk_command_buffer );

        // Submit command buffer
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command_buffer->vk_command_buffer;

        vkQueueSubmit( vulkan_queue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( vulkan_queue );

        vmaDestroyBuffer( vma_allocator, staging_buffer, staging_allocation );

        // TODO: free command buffer
        vkResetCommandBuffer( command_buffer->vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT );

        texture->vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    return handle;
}

static const char* s_shader_compiler_stage[ShaderStage::Enum::Count] = { "vert", "frag", "geom", "comp", "tesc", "tese" };

ShaderStateHandle Device::create_shader_state( const ShaderStateCreation& creation ) {

    ShaderStateHandle handle = { k_invalid_handle };

    if ( creation.stages_count == 0 || creation.stages == nullptr ) {
        HYDRA_LOG( "Shader %s does not contain shader stages.\n", creation.name );
        return handle;
    }

    handle.handle = shaders.obtain_resource();
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }
    
    // For each shader stage, compile them individually.
    uint32_t compiled_shaders = 0;

    ShaderStateVulkan* shader_state = access_shader_state( handle );
    shader_state->graphics_pipeline = true;
    shader_state->active_shaders = 0;
    
    for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
        const ShaderStateCreation::Stage& stage = creation.stages[compiled_shaders];

        // Gives priority to compute: if any is present (and it should not be) then it is not a graphics pipeline.
        if ( stage.type == ShaderStage::Compute ) {
            shader_state->graphics_pipeline = false;
        }

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        bool compiled = false;

        if ( creation.spv_input ) {
            createInfo.codeSize = stage.code_size;
            createInfo.pCode = reinterpret_cast<const uint32_t*>( stage.code );
        }
        else {
            // Compile from glsl to SpirV.
            // TODO: detect if input is HLSL.
            const char* temp_filename = "temp.shader";
            // Write current shader to file.
            FILE* temp_shader_file = fopen( temp_filename, "w" );
            fwrite( stage.code, stage.code_size, 1, temp_shader_file );
            fclose( temp_shader_file );

            // Compile to SPV
            char* glsl_compiler_path = s_string_buffer.append_use( "%sglslangValidator.exe", vulkan_binaries_path );
            char* final_shader_filename = s_string_buffer.append_use( "shader_final.spv" );
            char* arguments = s_string_buffer.append_use( "glslangValidator.exe %s -V -o %s -S %s", temp_filename, final_shader_filename, s_shader_compiler_stage[stage.type] );
            hydra::process_execute( ".", glsl_compiler_path, arguments );

            // Read back SPV file.
            createInfo.pCode = reinterpret_cast<const uint32_t*>(hydra::file_read_into_memory( final_shader_filename, &createInfo.codeSize, false, *memory_get_system_allocator() ) );

            compiled = true;
        }

        // Compile shader module
        VkPipelineShaderStageCreateInfo& shader_stage_info = shader_state->shader_stage_info[compiled_shaders];
        memset( &shader_stage_info, 0, sizeof( VkPipelineShaderStageCreateInfo ) );
        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info.pName = "main";
        shader_stage_info.stage = to_vk_shader_stage( stage.type );

        if ( vkCreateShaderModule( vulkan_device, &createInfo, nullptr, &shader_state->shader_stage_info[compiled_shaders].module ) != VK_SUCCESS ) {

            break;
        }

        if ( compiled ) {
            hydra::memory_get_system_allocator()->free_( (void*)createInfo.pCode );
        }

        set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( uint64_t )shader_state->shader_stage_info[ compiled_shaders ].module, creation.name );
    }

    bool creation_failed = compiled_shaders != creation.stages_count;
    if ( !creation_failed ) {
        shader_state->active_shaders = compiled_shaders;
        shader_state->name = creation.name;
    }

    if ( creation_failed ) {
        destroy_shader_state( handle );
        handle.handle = k_invalid_handle;

        // Dump shader code
        HYDRA_LOG( "Error in creation of shader %s. Dumping all shader informations.\n", creation.name );
        for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
            const ShaderStateCreation::Stage& stage = creation.stages[compiled_shaders];
            HYDRA_LOG( "%s:\n%s\n", ShaderStage::ToString( stage.type ), stage.code );
        }
    }

    return handle;
}

PipelineHandle Device::create_pipeline( const PipelineCreation& creation ) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ShaderStateHandle shader_state = create_shader_state( creation.shaders );
    if ( shader_state.handle == k_invalid_handle ) {
        // Shader did not compile.
        pipelines.release_resource( handle.handle );
        handle.handle = k_invalid_handle;

        return handle;
    }

    // Now that shaders have compiled we can create the pipeline.
    PipelineVulkan* pipeline = access_pipeline( handle );
    ShaderStateVulkan* shader_state_data = access_shader_state( shader_state );

    pipeline->shader_state = shader_state;

    VkDescriptorSetLayout vk_layouts[k_max_resource_layouts];

    // Create VkPipelineLayout
    for ( uint32_t l = 0; l < creation.num_active_layouts; ++l ) {
        pipeline->resource_list_layout[l] = access_resource_list_layout( creation.resource_list_layout[l] );
        pipeline->resource_list_layout_handle[l] = creation.resource_list_layout[l];

        vk_layouts[l] = pipeline->resource_list_layout[l]->vk_descriptor_set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_info.pSetLayouts = vk_layouts;
    pipeline_layout_info.setLayoutCount = creation.num_active_layouts;

    VkPipelineLayout pipeline_layout;
    check( vkCreatePipelineLayout( vulkan_device, &pipeline_layout_info, vulkan_allocation_callbacks, &pipeline_layout ) );
    // Cache pipeline layout
    pipeline->vk_pipeline_layout = pipeline_layout;
    pipeline->num_active_layouts = creation.num_active_layouts;

    // Create full pipeline
    if ( shader_state_data->graphics_pipeline ) {
        VkGraphicsPipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

        //// Shader stage
        pipeline_info.pStages = shader_state_data->shader_stage_info;
        pipeline_info.stageCount = shader_state_data->active_shaders;
        //// PipelineLayout
        pipeline_info.layout = pipeline_layout;

        //// Vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        // Vertex attributes.
        VkVertexInputAttributeDescription vertex_attributes[8];
        if ( creation.vertex_input.num_vertex_attributes ) {

            for ( uint32_t i = 0; i < creation.vertex_input.num_vertex_attributes; ++i ) {
                const VertexAttribute& vertex_attribute = creation.vertex_input.vertex_attributes[i];
                vertex_attributes[i] = { vertex_attribute.location, vertex_attribute.binding, to_vk_vertex_format(vertex_attribute.format), vertex_attribute.offset };
            }

            vertex_input_info.vertexAttributeDescriptionCount = creation.vertex_input.num_vertex_attributes;
            vertex_input_info.pVertexAttributeDescriptions = vertex_attributes;
        }
        else {
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }
        // Vertex bindings
        VkVertexInputBindingDescription vertex_bindings[8];
        if ( creation.vertex_input.num_vertex_streams ) {
            vertex_input_info.vertexBindingDescriptionCount = creation.vertex_input.num_vertex_streams;

            for ( uint32_t i = 0; i < creation.vertex_input.num_vertex_streams; ++i ) {
                const VertexStream& vertex_stream = creation.vertex_input.vertex_streams[i];
                VkVertexInputRate vertex_rate = vertex_stream.input_rate == VertexInputRate::PerVertex ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX : VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE;
                vertex_bindings[i] = { vertex_stream.binding, vertex_stream.stride, vertex_rate };
            }
            vertex_input_info.pVertexBindingDescriptions = vertex_bindings;
        } else {
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
        }

        pipeline_info.pVertexInputState = &vertex_input_info;

        //// Input Assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        pipeline_info.pInputAssemblyState = &input_assembly;

        //// Color Blending
        VkPipelineColorBlendAttachmentState color_blend_attachment[8];

        if ( creation.blend_state.active_states ) {
            for ( size_t i = 0; i < creation.blend_state.active_states; i++ ) {
                const BlendState& blend_state = creation.blend_state.blend_states[i];

                color_blend_attachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                color_blend_attachment[i].blendEnable = blend_state.blend_enabled ? VK_TRUE : VK_FALSE;
                color_blend_attachment[i].srcColorBlendFactor = to_vk_blend_factor( blend_state.source_color );
                color_blend_attachment[i].dstColorBlendFactor = to_vk_blend_factor( blend_state.destination_color );
                color_blend_attachment[i].colorBlendOp = to_vk_blend_operation( blend_state.color_operation );

                if ( blend_state.separate_blend ) {
                    color_blend_attachment[i].srcAlphaBlendFactor = to_vk_blend_factor( blend_state.source_alpha );
                    color_blend_attachment[i].dstAlphaBlendFactor = to_vk_blend_factor( blend_state.destination_alpha );
                    color_blend_attachment[i].alphaBlendOp = to_vk_blend_operation( blend_state.alpha_operation );
                }
                else {
                    color_blend_attachment[i].srcAlphaBlendFactor = to_vk_blend_factor( blend_state.source_color );
                    color_blend_attachment[i].dstAlphaBlendFactor = to_vk_blend_factor( blend_state.destination_color );
                    color_blend_attachment[i].alphaBlendOp = to_vk_blend_operation( blend_state.color_operation );
                }
            }
        }
        else {
            // Default non blended state
            color_blend_attachment[0] = {};
            color_blend_attachment[0].blendEnable = VK_FALSE;
            color_blend_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo color_blending { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blending.attachmentCount = creation.blend_state.active_states ? creation.blend_state.active_states : 1; // Always have 1 blend defined.
        color_blending.pAttachments = color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f; // Optional
        color_blending.blendConstants[1] = 0.0f; // Optional
        color_blending.blendConstants[2] = 0.0f; // Optional
        color_blending.blendConstants[3] = 0.0f; // Optional

        pipeline_info.pColorBlendState = &color_blending;

        //// Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depth_stencil { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

        depth_stencil.depthWriteEnable = creation.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.stencilTestEnable = creation.depth_stencil.stencil_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthTestEnable = creation.depth_stencil.depth_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = to_vk_compare_operation(creation.depth_stencil.depth_comparison);
        if ( creation.depth_stencil.stencil_enable ) {
            // TODO: add stencil
            assert( false );
        }

        pipeline_info.pDepthStencilState = &depth_stencil;
        
        //// Multisample
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        pipeline_info.pMultisampleState = &multisampling;

        //// Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = to_vk_cull_mode( creation.rasterization.cull_mode );
        rasterizer.frontFace = to_vk_front_face( creation.rasterization.front );
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        pipeline_info.pRasterizationState = &rasterizer;

        //// Tessellation
        pipeline_info.pTessellationState;
        

        //// Viewport state
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_width;
        viewport.height = (float)swapchain_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { swapchain_width, swapchain_height };

        VkPipelineViewportStateCreateInfo viewport_state { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        pipeline_info.pViewportState = &viewport_state;

        //// Render Pass
        RenderPassVulkan* render_pass_vulkan = access_render_pass( creation.render_pass );
        pipeline_info.renderPass = render_pass_vulkan->vk_render_pass;

        //// Dynamic states
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic_state.dynamicStateCount = ArrayLength( dynamic_states );
        dynamic_state.pDynamicStates = dynamic_states;

        pipeline_info.pDynamicState = &dynamic_state;

        vkCreateGraphicsPipelines( vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline );

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
    else {
        VkComputePipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

        pipeline_info.stage = shader_state_data->shader_stage_info[0];
        pipeline_info.layout = pipeline_layout;

        vkCreateComputePipelines( vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline );

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    return handle;
}

BufferHandle Device::create_buffer( const BufferCreation& creation ) {
    BufferHandle handle = { buffers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    BufferVulkan* buffer = access_buffer( handle );

    buffer->name = creation.name;
    buffer->size = creation.size;
    buffer->type = creation.type;
    buffer->usage = creation.usage;
    buffer->handle = handle;

    VkBufferUsageFlags buffer_usage;

    // TODO: improve flag creation.

    switch ( creation.type ) {
        case BufferType::Constant:
        {
            buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        }

        case BufferType::Vertex:
        {
            buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        }

        case BufferType::Index:
        {
            buffer_usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        }

        default:
        {
            HYDRA_ASSERT( false, "Not implemented!" );
            break;
        }
    }

    VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.usage = buffer_usage;
    buffer_info.size = creation.size > 0 ? creation.size : 1;       // 0 sized creations are not permitted.

    VmaAllocationCreateInfo memory_info{};
    memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    

    VmaAllocationInfo allocation_info{};
    check( vmaCreateBuffer( vma_allocator, &buffer_info, &memory_info, 
                            &buffer->vk_buffer, &buffer->vma_allocation, &allocation_info ) );

    set_resource_name( VK_OBJECT_TYPE_BUFFER, ( uint64_t )buffer->vk_buffer, creation.name );

    buffer->vk_device_memory = allocation_info.deviceMemory;

    if ( creation.initial_data ) {
        void* data;
        vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );
        memcpy( data, creation.initial_data, (size_t)creation.size );
        vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
    }

    // TODO
    //if ( persistent )
    //{
    //    mapped_data = static_cast<uint8_t *>(allocation_info.pMappedData);
    //}

    return handle;
}

SamplerHandle Device::create_sampler( const SamplerCreation& creation ) {
    SamplerHandle handle = { samplers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    SamplerVulkan* sampler = access_sampler( handle );

    sampler->address_mode_u = creation.address_mode_u;
    sampler->address_mode_v = creation.address_mode_v;
    sampler->address_mode_w = creation.address_mode_w;
    sampler->min_filter = creation.min_filter;
    sampler->mag_filter = creation.mag_filter;
    sampler->mip_filter = creation.mip_filter;
    sampler->name = creation.name;

    VkSamplerCreateInfo create_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    create_info.addressModeU = to_vk_address_mode( creation.address_mode_u );
    create_info.addressModeV = to_vk_address_mode( creation.address_mode_v );
    create_info.addressModeW = to_vk_address_mode( creation.address_mode_w );
    create_info.minFilter = to_vk_filter( creation.min_filter );
    create_info.magFilter = to_vk_filter( creation.mag_filter );
    create_info.mipmapMode = to_vk_mipmap( creation.mip_filter );
    create_info.anisotropyEnable = 0;
    create_info.compareEnable = 0;
    create_info.unnormalizedCoordinates = 0;
    create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    // TODO:
    /*float                   mipLodBias;
    float                   maxAnisotropy;
    VkCompareOp             compareOp;
    float                   minLod;
    float                   maxLod;
    VkBorderColor           borderColor;
    VkBool32                unnormalizedCoordinates;*/

    vkCreateSampler( vulkan_device, &create_info, vulkan_allocation_callbacks, &sampler->vk_sampler );

    set_resource_name( VK_OBJECT_TYPE_SAMPLER, ( uint64_t )sampler->vk_sampler, creation.name );

    return handle;
}

ResourceListLayoutHandle Device::create_resource_list_layout( const ResourceListLayoutCreation& creation ) {
    ResourceListLayoutHandle handle = { resource_list_layouts.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceListLayoutVulkan* resource_layout = access_resource_list_layout( handle );

    // TODO: add support for multiple sets.
    // Create flattened binding list
    resource_layout->num_bindings = creation.num_bindings;
    resource_layout->bindings = (ResourceBindingVulkan*)HYDRA_MALLOC( sizeof( ResourceBindingVulkan ) * creation.num_bindings );
    resource_layout->vk_binding = (VkDescriptorSetLayoutBinding*)HYDRA_MALLOC( sizeof( VkDescriptorSetLayoutBinding ) * creation.num_bindings );
    resource_layout->handle = handle;

    for ( uint32_t r = 0; r < creation.num_bindings; ++r ) {
        ResourceBindingVulkan& binding = resource_layout->bindings[r];
        binding.start = r;
        binding.count = 1;
        binding.type = creation.bindings[r].type;
        binding.name = creation.bindings[r].name;

        VkDescriptorSetLayoutBinding& vk_binding = resource_layout->vk_binding[r];
        vk_binding.binding = r;
        vk_binding.descriptorCount = 1;
        vk_binding.descriptorType = to_vk_descriptor_type( creation.bindings[r].type );
        // TODO
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = creation.num_bindings;
    layoutInfo.pBindings = resource_layout->vk_binding;

    vkCreateDescriptorSetLayout( vulkan_device, &layoutInfo, vulkan_allocation_callbacks, &resource_layout->vk_descriptor_set_layout );

    return handle;
}

ResourceListHandle Device::create_resource_list( const ResourceListCreation& creation ) {
    ResourceListHandle handle = { resource_lists.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceListVulkan* resource_list = access_resource_list( handle );
    ResourceListLayoutVulkan* resource_list_layout = access_resource_list_layout( creation.layout );

    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = vulkan_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &resource_list_layout->vk_descriptor_set_layout;

    vkAllocateDescriptorSets( vulkan_device, &allocInfo, &resource_list->vk_descriptor_set );

    VkWriteDescriptorSet descriptor_write[8];
    VkDescriptorBufferInfo buffer_info[8];
    VkDescriptorImageInfo image_info[8];

    SamplerVulkan* vk_default_sampler = access_sampler( default_sampler );

    resource_list->num_resources = creation.num_resources;

    for ( size_t i = 0; i < creation.num_resources; i++ ) {

        const auto& binding = resource_list_layout->bindings[i];

        descriptor_write[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptor_write[i].dstSet = resource_list->vk_descriptor_set;
        descriptor_write[i].dstBinding = i;
        descriptor_write[i].dstArrayElement = 0;

        switch ( binding.type ) {
            case ResourceType::Texture:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                TextureHandle texture_handle = { creation.resources[i] };
                TextureVulkan* texture_data = access_texture( texture_handle );
                
                image_info[i].sampler = texture_data->sampler ? texture_data->sampler->vk_sampler : vk_default_sampler->vk_sampler;
                image_info[i].imageLayout = TextureFormat::has_depth_or_stencil( texture_data->format ) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case ResourceType::Image:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { creation.resources[i] };
                TextureVulkan* texture_data = access_texture( texture_handle );

                image_info[i].sampler = nullptr;
                image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case ResourceType::ImageRW:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { creation.resources[i] };
                TextureVulkan* texture_data = access_texture( texture_handle );

                image_info[i].sampler = nullptr;
                image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case ResourceType::Constants:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                BufferHandle buffer_handle = { creation.resources[i] };
                BufferVulkan* buffer = access_buffer( buffer_handle );

                buffer_info[i].buffer = buffer->vk_buffer;
                buffer_info[i].offset = 0;
                buffer_info[i].range = buffer->size;

                descriptor_write[i].pBufferInfo = &buffer_info[i];
                
                break;
            }
        }
        
        descriptor_write[i].descriptorCount = 1;
    }
    
    vkUpdateDescriptorSets( vulkan_device, creation.num_resources, descriptor_write, 0, nullptr );

    return handle;
}

static void vulkan_create_framebuffer( Device& gpu, RenderPassVulkan* render_pass, const TextureHandle* output_textures, u32 num_render_targets, TextureHandle depth_stencil_texture ) {
    // Create framebuffer
    VkFramebufferCreateInfo framebuffer_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_info.renderPass = render_pass->vk_render_pass;
    framebuffer_info.width = render_pass->width;
    framebuffer_info.height = render_pass->height;
    framebuffer_info.layers = 1;

    VkImageView framebuffer_attachments[ k_max_image_outputs + 1 ]{};
    u32 active_attachments = 0;
    for ( ; active_attachments < num_render_targets; ++active_attachments ) {
        TextureVulkan* texture_vk = gpu.access_texture( output_textures[ active_attachments ] );
        framebuffer_attachments[ active_attachments ] = texture_vk->vk_image_view;
    }

    if ( depth_stencil_texture.handle != k_invalid_handle ) {
        TextureVulkan* depth_texture_vk = gpu.access_texture( depth_stencil_texture );
        framebuffer_attachments[ active_attachments++ ] = depth_texture_vk->vk_image_view;
    }
    framebuffer_info.pAttachments = framebuffer_attachments;
    framebuffer_info.attachmentCount = active_attachments;

    vkCreateFramebuffer( gpu.vulkan_device, &framebuffer_info, nullptr, &render_pass->vk_frame_buffer );
    gpu.set_resource_name( VK_OBJECT_TYPE_FRAMEBUFFER, ( uint64_t )render_pass->vk_frame_buffer, render_pass->name );
    
}

RenderPassHandle Device::create_render_pass( const RenderPassCreation& creation ) {
    RenderPassHandle handle = { render_passes.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    RenderPassVulkan* render_pass = access_render_pass( handle );
    render_pass->type = creation.type;
    // Init the rest of the struct.
    render_pass->num_render_targets = creation.num_render_targets;
    render_pass->dispatch_x = 0;
    render_pass->dispatch_y = 0;
    render_pass->dispatch_z = 0;
    render_pass->clear_color = false;
    render_pass->name = creation.name;
    render_pass->vk_frame_buffer = nullptr;
    render_pass->vk_render_pass = nullptr;
    render_pass->scale_x = creation.scale_x;
    render_pass->scale_y = creation.scale_y;
    render_pass->resize = creation.resize;

    switch ( creation.type ) {
        case RenderPassType::Swapchain:
        {
            // Color attachment
            VkAttachmentDescription color_attachment = {};
            color_attachment.format = vulkan_surface_format.format;
            color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference color_attachment_ref = {};
            color_attachment_ref.attachment = 0;
            color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            // Depth attachment
            VkAttachmentDescription depth_attachment{};
            TextureVulkan* depth_texture_vk = access_texture( depth_texture );
            depth_attachment.format = to_vk_format( depth_texture_vk->format );
            depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depth_attachment_ref{};
            depth_attachment_ref.attachment = 1;
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment_ref;
            subpass.pDepthStencilAttachment = &depth_attachment_ref;

            VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
            VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
            render_pass_info.attachmentCount = 2;
            render_pass_info.pAttachments = attachments;
            render_pass_info.subpassCount = 1;
            render_pass_info.pSubpasses = &subpass;

            check( vkCreateRenderPass( vulkan_device, &render_pass_info, nullptr, &render_pass->vk_render_pass ) );

            set_resource_name( VK_OBJECT_TYPE_RENDER_PASS, ( uint64_t )render_pass->vk_render_pass, creation.name );

            // Create framebuffer into the device.
            VkFramebufferCreateInfo framebuffer_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            framebuffer_info.renderPass = render_pass->vk_render_pass;
            framebuffer_info.attachmentCount = 2;
            framebuffer_info.width = swapchain_width;
            framebuffer_info.height = swapchain_height;
            framebuffer_info.layers = 1;

            VkImageView framebuffer_attachments[2];
            framebuffer_attachments[1] = depth_texture_vk->vk_image_view;

            for ( size_t i = 0; i < vulkan_swapchain_image_count; i++ ) {
                framebuffer_attachments[0] = vulkan_swapchain_image_views[i];
                framebuffer_info.pAttachments = framebuffer_attachments;

                vkCreateFramebuffer( vulkan_device, &framebuffer_info, nullptr, &vulkan_swapchain_framebuffers[i] );
                set_resource_name( VK_OBJECT_TYPE_FRAMEBUFFER, ( uint64_t )vulkan_swapchain_framebuffers[ i ], creation.name );
            }

            render_pass->width = swapchain_width;
            render_pass->height = swapchain_height;

            break;
        }

        case RenderPassType::Compute:
        {
            break;
        }

        case RenderPassType::Standard:
        {
            VkAttachmentDescription color_attachments[8] = {};
            VkAttachmentReference color_attachments_ref[8] = {};

            // Color attachments
            uint32_t c = 0;
            for ( ; c < creation.num_render_targets; ++c ) {
                TextureVulkan* texture_vk = access_texture( creation.output_textures[c] );

                render_pass->width = texture_vk->width;
                render_pass->height = texture_vk->height;

                VkAttachmentDescription& color_attachment = color_attachments[c];
                color_attachment.format = to_vk_format( texture_vk->format );
                color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkAttachmentReference& color_attachment_ref = color_attachments_ref[c];
                color_attachment_ref.attachment = c;
                color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                // Cache texture handles
                render_pass->output_textures[ c ] = creation.output_textures[ c ];
            }

            // Depth attachment
            VkAttachmentDescription depth_attachment{};
            VkAttachmentReference depth_attachment_ref{};

            if ( creation.depth_stencil_texture.handle != k_invalid_handle ) {

                TextureVulkan* depth_texture_vk = access_texture( creation.depth_stencil_texture );
                render_pass->width = depth_texture_vk->width;
                render_pass->height = depth_texture_vk->height;

                depth_attachment.format = to_vk_format( depth_texture_vk->format );
                depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                depth_attachment_ref.attachment = c;
                depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            render_pass->output_depth = creation.depth_stencil_texture;

            // Create subpass.
            // TODO: for now is just a simple subpass, evolve API.
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            // Calculate active attachments for the subpass
            VkAttachmentDescription attachments[k_max_image_outputs + 1]{};
            uint32_t active_attachments = 0;
            for ( ; active_attachments < creation.num_render_targets; ++active_attachments ) {
                attachments[active_attachments] = color_attachments[active_attachments];
                ++active_attachments;
            }
            subpass.colorAttachmentCount = active_attachments ? active_attachments - 1 : 0;
            subpass.pColorAttachments = color_attachments_ref;

            subpass.pDepthStencilAttachment = nullptr;

            uint32_t depth_stencil_count = 0;
            if ( creation.depth_stencil_texture.handle != k_invalid_handle ) {
                attachments[subpass.colorAttachmentCount] = depth_attachment;

                subpass.pDepthStencilAttachment = &depth_attachment_ref;

                depth_stencil_count = 1;
            }

            VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
            
            render_pass_info.attachmentCount = (active_attachments ? active_attachments - 1 : 0) + depth_stencil_count;
            render_pass_info.pAttachments = attachments;
            render_pass_info.subpassCount = 1;
            render_pass_info.pSubpasses = &subpass;

            check( vkCreateRenderPass( vulkan_device, &render_pass_info, nullptr, &render_pass->vk_render_pass ) );
            set_resource_name( VK_OBJECT_TYPE_RENDER_PASS, ( uint64_t )render_pass->vk_render_pass, creation.name );

            vulkan_create_framebuffer( *this, render_pass, creation.output_textures, creation.num_render_targets, creation.depth_stencil_texture );

            break;
        }
    }

    return handle;
}

// Resource Destruction /////////////////////////////////////////////////////////

void Device::destroy_buffer( BufferHandle buffer ) {
    if ( buffer.handle < buffers.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Buffer, buffer.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid Buffer %u\n", buffer.handle );
    }
}

void Device::destroy_texture( TextureHandle texture ) {
    if ( texture.handle < textures.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Texture, texture.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid Texture %u\n", texture.handle );
    }
}

void Device::destroy_pipeline( PipelineHandle pipeline ) {
    if ( pipeline.handle < pipelines.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Pipeline, pipeline.handle, current_frame };
        // Shader state creation is handled internally when creating a pipeline, thus add this to track correctly.
        PipelineVulkan* v_pipeline = access_pipeline( pipeline );
        destroy_shader_state( v_pipeline->shader_state );
    }
    else {
        print_format( "Graphics error: trying to free invalid Pipeline %u\n", pipeline.handle );
    }
}

void Device::destroy_sampler( SamplerHandle sampler ) {
    if ( sampler.handle < samplers.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Sampler, sampler.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid Sampler %u\n", sampler.handle );
    }
}

void Device::destroy_resource_list_layout( ResourceListLayoutHandle resource_layout ) {
    if ( resource_layout.handle < resource_list_layouts.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ResourceListLayout, resource_layout.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid ResourceListLayout %u\n", resource_layout.handle );
    }
}

void Device::destroy_resource_list( ResourceListHandle resource_list ) {
    if ( resource_list.handle < resource_lists.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ResourceList, resource_list.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid ResourceList %u\n", resource_list.handle );
    }
}

void Device::destroy_render_pass( RenderPassHandle render_pass ) {
    if ( render_pass.handle < render_passes.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::RenderPass, render_pass.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid RenderPass %u\n", render_pass.handle );
    }
}

void Device::destroy_shader_state( ShaderStateHandle shader ) {
    if ( shader.handle < shaders.size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ShaderState, shader.handle, current_frame };
    }
    else {
        print_format( "Graphics error: trying to free invalid Shader %u\n", shader.handle );
    }
}

// Real destruction methods - the other enqueue only the resources.
void Device::destroy_buffer_instant( ResourceHandle buffer ) {

    BufferVulkan* v_buffer = ( BufferVulkan* )buffers.access_resource( buffer );

    if ( v_buffer ) {
        vmaDestroyBuffer( vma_allocator, v_buffer->vk_buffer, v_buffer->vma_allocation );
    }
    buffers.release_resource( buffer );
}

void Device::destroy_texture_instant( ResourceHandle texture ) {
    TextureVulkan* v_texture = ( TextureVulkan* )textures.access_resource( texture );

    if ( v_texture ) {
        vkDestroyImageView( vulkan_device, v_texture->vk_image_view, vulkan_allocation_callbacks );
        vmaDestroyImage( vma_allocator, v_texture->vk_image, v_texture->vma_allocation );
    }
    textures.release_resource( texture );
}

void Device::destroy_pipeline_instant( ResourceHandle pipeline ) {
    PipelineVulkan* v_pipeline = ( PipelineVulkan* )pipelines.access_resource( pipeline );

    if ( v_pipeline ) {
        vkDestroyPipeline( vulkan_device, v_pipeline->vk_pipeline, vulkan_allocation_callbacks );

        vkDestroyPipelineLayout( vulkan_device, v_pipeline->vk_pipeline_layout, vulkan_allocation_callbacks );
    }
    pipelines.release_resource( pipeline );
}

void Device::destroy_sampler_instant( ResourceHandle sampler ) {
    SamplerVulkan* v_sampler = ( SamplerVulkan* )samplers.access_resource( sampler );

    if ( v_sampler ) {
        vkDestroySampler( vulkan_device, v_sampler->vk_sampler, vulkan_allocation_callbacks );
    }
    samplers.release_resource( sampler );
}

void Device::destroy_resource_list_layout_instant( ResourceHandle resource_layout ) {
    ResourceListLayoutVulkan* v_resource_list_layout = ( ResourceListLayoutVulkan* )resource_list_layouts.access_resource( resource_layout );

    if ( v_resource_list_layout ) {
        vkDestroyDescriptorSetLayout( vulkan_device, v_resource_list_layout->vk_descriptor_set_layout, vulkan_allocation_callbacks );

        HYDRA_FREE(v_resource_list_layout->bindings);
        HYDRA_FREE(v_resource_list_layout->vk_binding);
    }
    resource_list_layouts.release_resource( resource_layout );
}

void Device::destroy_resource_list_instant( ResourceHandle resource_list ) {
    ResourceListVulkan* v_resource_list = ( ResourceListVulkan* )resource_lists.access_resource( resource_list );

    if ( v_resource_list ) {
        // This is freed with the DescriptorSet pool.
        //vkFreeDescriptorSets
    }
    resource_lists.release_resource( resource_list );
}

void Device::destroy_render_pass_instant( ResourceHandle render_pass ) {
    RenderPassVulkan* v_render_pass = ( RenderPassVulkan* )render_passes.access_resource( render_pass );

    if ( v_render_pass ) {

        if ( v_render_pass->num_render_targets )
            vkDestroyFramebuffer( vulkan_device, v_render_pass->vk_frame_buffer, vulkan_allocation_callbacks );

        vkDestroyRenderPass( vulkan_device, v_render_pass->vk_render_pass, vulkan_allocation_callbacks );
    }
    render_passes.release_resource( render_pass );
}

void Device::destroy_shader_state_instant( ResourceHandle shader ) {
    ShaderStateVulkan* v_shader_state = ( ShaderStateVulkan* )shaders.access_resource( shader );
    if ( v_shader_state ) {

        for ( size_t i = 0; i < v_shader_state->active_shaders; i++ ) {
            vkDestroyShaderModule( vulkan_device, v_shader_state->shader_stage_info[ i ].module, vulkan_allocation_callbacks );
        }
    }
    shaders.release_resource( shader );
}

void Device::set_resource_name( VkObjectType type, uint64_t handle, const char* name ) {

    if ( !debug_utils_extension_present ) {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name;
    pfnSetDebugUtilsObjectNameEXT( vulkan_device, &name_info );
}

// Resource Description Query ///////////////////////////////////////////////////

void Device::query_buffer( BufferHandle buffer, BufferDescription& out_description ) {
    if ( buffer.handle != k_invalid_handle ) {
        const BufferVulkan* buffer_data = access_buffer( buffer );

        out_description.name = buffer_data->name;
        out_description.size = buffer_data->size;
        out_description.type = buffer_data->type;
        out_description.usage = buffer_data->usage;
        out_description.native_handle = (void*)&buffer_data->vk_buffer;
    }
}

void Device::query_texture( TextureHandle texture, TextureDescription& out_description ) {
    if ( texture.handle != k_invalid_handle ) {
        const TextureVulkan* texture_data = access_texture( texture );

        out_description.width = texture_data->width;
        out_description.height = texture_data->height;
        out_description.depth = texture_data->depth;
        out_description.format = texture_data->format;
        out_description.mipmaps = texture_data->mipmaps;
        out_description.type = texture_data->type;
        out_description.render_target = texture_data->render_target;
        out_description.native_handle = (void*)&texture_data->vk_image;
    }
}

void Device::query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description ) {
    if ( pipeline.handle != k_invalid_handle ) {
        const PipelineVulkan* pipeline_data = access_pipeline( pipeline );

        out_description.shader = pipeline_data->shader_state;
    }
}

void Device::query_sampler( SamplerHandle sampler, SamplerDescription& out_description ) {
    if ( sampler.handle != k_invalid_handle ) {
        const SamplerVulkan* sampler_data = access_sampler( sampler );
    }
}

void Device::query_resource_list_layout( ResourceListLayoutHandle resource_list_layout, ResourceListLayoutDescription& out_description ) {
    if ( resource_list_layout.handle != k_invalid_handle ) {
        const ResourceListLayoutVulkan* resource_list_layout_data = access_resource_list_layout( resource_list_layout );

        const uint32_t num_bindings = resource_list_layout_data->num_bindings;
        for ( size_t i = 0; i < num_bindings; i++ ) {
            out_description.bindings[i].name = resource_list_layout_data->bindings[i].name;
            out_description.bindings[i].type = resource_list_layout_data->bindings[i].type;
        }

        out_description.num_active_bindings = resource_list_layout_data->num_bindings;
    }
}

void Device::query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description ) {
    if ( resource_list.handle != k_invalid_handle ) {
        const ResourceListVulkan* resource_list_data = access_resource_list( resource_list );

        out_description.num_active_resources = resource_list_data->num_resources;
        for ( u32 i = 0; i < out_description.num_active_resources; ++i ) {
            out_description.resources[ i ].data = resource_list_data->resources[ i ].data;
        }
    }
}

// Resource Map/Unmap ///////////////////////////////////////////////////////////

void* Device::map_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.handle == k_invalid_handle )
        return nullptr;

    BufferVulkan* buffer = access_buffer( parameters.buffer );
    
    void* data;
    vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );
    
    return data;
}

void Device::unmap_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.handle == k_invalid_handle )
        return;

    BufferVulkan* buffer = access_buffer( parameters.buffer );
    vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
}

// Other methods ////////////////////////////////////////////////////////////////
void Device::queue_command_buffer( CommandBuffer* command_buffer ) {

    queued_command_buffers[num_queued_command_buffers++] = command_buffer;
}

CommandBuffer* Device::get_command_buffer( QueueType::Enum type, bool baked, bool begin ) {
    CommandBuffer* cb = render_frames[current_frame].get_command_buffer( 0, begin );
    
    // The first commandbuffer issued in the frame is used to reset the timestamp queries used.
    if ( gpu_timestamp_reset && begin ) {
        // These are currently indices!
        vkCmdResetQueryPool( cb->vk_command_buffer, vulkan_timestamp_query_pool, current_frame * gpu_timestamp_manager->queries_per_frame * 2, gpu_timestamp_manager->queries_per_frame );

        gpu_timestamp_reset = false;
    }

    return cb;
}

void Device::free_command_buffer( CommandBuffer* command_buffer ) {
    //command_buffer->terminate();
    //free( command_buffer );
}

void Device::resize_swapchain() {

    vkDeviceWaitIdle( vulkan_device );

    destroy_render_pass( swapchain_pass );
    destroy_texture( depth_texture );

    destroy_swapchain();
    vkDestroySurfaceKHR( vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks );

    // Recreate window surface
    if ( SDL_Vulkan_CreateSurface( sdl_window, vulkan_instance, &vulkan_window_surface ) == SDL_FALSE ) {
        HYDRA_LOG( "Failed to create Vulkan surface.\n" );
    }

    create_swapchain();

    TextureCreation depth_texture_creation = { nullptr, swapchain_width, swapchain_height, 1, 1, 0, TextureFormat::D24_UNORM_X8_UINT, TextureType::Texture2D };
    depth_texture = create_texture( depth_texture_creation );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type( RenderPassType::Swapchain ).set_name( "Swapchain" );
    swapchain_pass = create_render_pass( swapchain_pass_creation );

    // Recreate command buffers for each frame
    for ( size_t i = 0; i < k_max_frames; i++ ) {
        render_frames[i].on_resize();
    }

    // Reset high level command buffers
    for ( size_t i = 0; i < command_buffers.size; i++ ) {
        CommandBuffer* command_buffer = (CommandBuffer*)command_buffers.access_resource( i );
        command_buffer->reset();
    }
    command_buffers.free_all_resources();

    vkDeviceWaitIdle( vulkan_device );
}

static void vulkan_resize_texture( Device& gpu, TextureVulkan* v_texture, u16 width, u16 height, u16 depth ) {

    // Destroy image first
    vkDestroyImageView( gpu.vulkan_device, v_texture->vk_image_view, gpu.vulkan_allocation_callbacks );
    vmaDestroyImage( gpu.vma_allocator, v_texture->vk_image, v_texture->vma_allocation );

    // Re-create image
    TextureCreation tc;
    tc.set_flags( v_texture->mipmaps, v_texture->flags ).set_format_type( v_texture->format, v_texture->type ).set_name( v_texture->name ).set_size( width, height, depth );
    vulkan_create_texture( gpu, tc, v_texture->handle, v_texture );
}

void Device::resize_output_textures( RenderPassHandle render_pass, u16 width, u16 height ) {

    RenderPassVulkan* vk_render_pass = access_render_pass( render_pass );
    if ( vk_render_pass ) {
        // No need to resize!
        if ( !vk_render_pass->resize ) {
            return;
        }

        // Calculate new width and height based on render pass sizing informations.
        u16 new_width = ( u16 )( width * vk_render_pass->scale_x );
        u16 new_height = ( u16 )( height * vk_render_pass->scale_y );

        // Resize textures
        const u32 rts = vk_render_pass->num_render_targets;
        for ( u32 i = 0; i < rts ; ++i ) {
            TextureHandle texture = vk_render_pass->output_textures[ i ];
            TextureVulkan* vk_texture = access_texture( texture );

            vulkan_resize_texture( *this, vk_texture, new_width, new_height, 1 );
        }

        if ( vk_render_pass->output_depth.handle != k_invalid_handle ) {
            TextureVulkan* vk_texture = access_texture( vk_render_pass->output_depth );

            vulkan_resize_texture( *this, vk_texture, new_width, new_height, 1 );
        }

        // Recreate framebuffer
        vkDestroyFramebuffer( vulkan_device, vk_render_pass->vk_frame_buffer, vulkan_allocation_callbacks );

        vk_render_pass->width = new_width;
        vk_render_pass->height = new_height;

        vulkan_create_framebuffer( *this, vk_render_pass, vk_render_pass->output_textures, vk_render_pass->num_render_targets, vk_render_pass->output_depth );
    }
}

void Device::fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier ) {

    RenderPassVulkan* vk_render_pass = access_render_pass( render_pass );

    out_barrier.num_image_barriers = 0;

    if ( vk_render_pass ) {
        const u32 rts = vk_render_pass->num_render_targets;
        for ( u32 i = 0; i < rts; ++i ) {
            out_barrier.image_barriers[ out_barrier.num_image_barriers++ ].texture = vk_render_pass->output_textures[ i ];
        }

        if ( vk_render_pass->output_depth.handle != k_invalid_handle ) {
            out_barrier.image_barriers[ out_barrier.num_image_barriers++ ].texture = vk_render_pass->output_depth;
        }
    }
}

void Device::present() {

    DeviceRenderFrame& render_frame = render_frames[current_frame];

    vkWaitForFences( vulkan_device, 1, &render_frame.vulkan_in_flight_fence, VK_TRUE, UINT64_MAX );

    VkResult result = vkAcquireNextImageKHR( vulkan_device, vulkan_swapchain, UINT64_MAX, render_frame.vulkan_image_available_semaphore, VK_NULL_HANDLE, &vulkan_image_index );
    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        resize_swapchain();
        return;
    }

    vkResetFences( vulkan_device, 1, &render_frame.vulkan_in_flight_fence );


    // Copy all commands
    VkCommandBuffer enqueued_command_buffers[4];
    for ( uint32_t c = 0; c < num_queued_command_buffers; c++ ) {

        CommandBuffer* command_buffer = queued_command_buffers[c];

        enqueued_command_buffers[c] = command_buffer->vk_command_buffer;

        if ( command_buffer->is_recording && command_buffer->current_pipeline && (command_buffer->current_render_pass->type != RenderPassType::Compute) )
            vkCmdEndRenderPass( command_buffer->vk_command_buffer );

        vkEndCommandBuffer( command_buffer->vk_command_buffer );
    }

    
    
    VkSemaphore wait_semaphores[] = { render_frame.vulkan_image_available_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = num_queued_command_buffers;
    submit_info.pCommandBuffers = enqueued_command_buffers;

    VkSemaphore signal_semaphore[] = { render_frame.vulkan_render_finished_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphore;

    vkQueueSubmit( vulkan_queue, 1, &submit_info, render_frame.vulkan_in_flight_fence );

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphore;

    VkSwapchainKHR swap_chains[] = { vulkan_swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &vulkan_image_index;
    present_info.pResults = nullptr; // Optional
    result = vkQueuePresentKHR( vulkan_queue, &present_info );

    num_queued_command_buffers = 0;

    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized ) {
        resized = false;
        resize_swapchain();

        current_frame = 0;
    }

    //hydra::print_format( "Index %u, %u, %u\n", current_frame, previous_frame, vulkan_image_index );

    // Search next highest free frame
    uint32_t free_fence_index = 0;
    for ( size_t i = 0; i < 3; i++ ) {
        if ( vkGetFenceStatus( vulkan_device, render_frames[i].vulkan_in_flight_fence ) == VK_SUCCESS  ) {
            free_fence_index = i;
            break;
        }
    }

    //
    // GPU Timestamp resolve
    if ( timestamps_enabled ) 
    {
        if ( gpu_timestamp_manager->current_query ) {
        // Query GPU for all timestamps.
            const u32 query_offset = ( current_frame * gpu_timestamp_manager->queries_per_frame ) * 2;
            const u32 query_count = gpu_timestamp_manager->current_query * 2;
            vkGetQueryPoolResults( vulkan_device, vulkan_timestamp_query_pool, query_offset, query_count,
                                   sizeof( uint64_t ) * query_count * 2, &gpu_timestamp_manager->timestamps_data[ query_offset ],
                                   sizeof( gpu_timestamp_manager->timestamps_data[ 0 ] ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );

            // Calculate and cache the elapsed time
            for ( size_t i = 0; i < gpu_timestamp_manager->current_query; i++ ) {
                uint32_t index = ( current_frame * gpu_timestamp_manager->current_query ) + i;

                GPUTimestamp& timestamp = gpu_timestamp_manager->timestamps[ index ];

                double start = ( double )gpu_timestamp_manager->timestamps_data[ ( index * 2 ) ];
                double end = ( double )gpu_timestamp_manager->timestamps_data[ ( index * 2 ) + 1 ];
                double range = end - start;
                double elapsed_time = range * gpu_timestamp_frequency;

                timestamp.elapsed_ms = elapsed_time;
                timestamp.frame_index = absolute_frame;

                //print_format( "%s: %2.3f d(%u) - ", timestamp.name, elapsed_time, timestamp.depth );
            }
            //print_format( "\n" );
        }

        gpu_timestamp_manager->reset();
        gpu_timestamp_reset = true;
    }
    else {
        gpu_timestamp_reset = false;
    }


    previous_frame = current_frame;
    current_frame = free_fence_index;

    // TODO: improve
    // Resource deletion
    uint32_t num_deleted = 0;
    for ( size_t i = 0; i < num_deletion_queue; i++ ) {
        ResourceDeletion& resource_deletion = resource_deletion_queue[ i ];

        if ( resource_deletion.current_frame == current_frame ) {

            switch ( resource_deletion.type ) {

                case ResourceDeletionType::Buffer:
                {
                    destroy_buffer_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Pipeline:
                {
                    destroy_pipeline_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::RenderPass:
                {
                    destroy_render_pass_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ResourceList:
                {
                    destroy_resource_list_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ResourceListLayout:
                {
                    destroy_resource_list_layout_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Sampler:
                {
                    destroy_sampler_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ShaderState:
                {
                    destroy_shader_state_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Texture:
                {
                    destroy_texture_instant( resource_deletion.handle );
                    break;
                }
            }

            // Mark resource as free
            ++num_deleted;
            resource_deletion.current_frame = -1;
        }
    }

    // Reduce deletion queue
    for ( size_t i = 0; i < num_deletion_queue; i++ ) {
        ResourceDeletion& resource_deletion = resource_deletion_queue[ i ];
        if ( resource_deletion.current_frame == -1 ) {

            resource_deletion_queue[ i ] = resource_deletion_queue[ num_deletion_queue - 1 ];
            --num_deletion_queue;

            --num_deleted;

            if ( num_deleted == 0 )
                break;
        }
    }

    // New frame
    render_frames[current_frame].new_frame();
    ++absolute_frame;
}

void Device::link_texture_sampler( TextureHandle texture, SamplerHandle sampler ) {

    TextureVulkan* texture_vk = access_texture( texture );
    SamplerVulkan* sampler_vk = access_sampler( sampler );

    texture_vk->sampler = sampler_vk;
}

u32 Device::get_gpu_timestamps( GPUTimestamp* out_timestamps ) {
    return gpu_timestamp_manager->resolve( previous_frame, out_timestamps );

}

void Device::push_gpu_timestamp( CommandBuffer* command_buffer, const char* name ) {
    if ( !timestamps_enabled )
        return;

    u32 query_index = gpu_timestamp_manager->push( current_frame, name );
    vkCmdWriteTimestamp( command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkan_timestamp_query_pool, query_index );
}

void Device::pop_gpu_timestamp( CommandBuffer* command_buffer ) {
    if ( !timestamps_enabled )
        return;

    u32 query_index = gpu_timestamp_manager->pop( current_frame );
    vkCmdWriteTimestamp( command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkan_timestamp_query_pool, query_index );
}


// CommandBuffer ////////////////////////////////////////////////////////////////

void CommandBuffer::reset() {
    
    is_recording = false;
    current_render_pass = nullptr;
    current_pipeline = nullptr;
    current_command = 0;
}


void CommandBuffer::init( QueueType::Enum type_, uint32_t buffer_size_, uint32_t submit_size, bool baked_ ) {
    this->type = type_;
    this->buffer_size = buffer_size_;
    this->baked = baked_;

    reset();
}

void CommandBuffer::terminate() {

    is_recording = false;
}

void CommandBuffer::bind_pass( uint64_t sort_key, RenderPassHandle handle ) {
    
    //if ( !is_recording ) 
    {
        is_recording = true;

        RenderPassVulkan* render_pass = device->access_render_pass( handle );

        // Begin/End render pass are valid only for graphics render passes.
        if ( current_render_pass && ( current_render_pass->type != RenderPassType::Compute ) && ( render_pass != current_render_pass ) ) {
            vkCmdEndRenderPass( vk_command_buffer );
        }

        if ( render_pass != current_render_pass && ( render_pass->type != RenderPassType::Compute ) ) {
            VkRenderPassBeginInfo render_pass_begin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            render_pass_begin.framebuffer = render_pass->type == RenderPassType::Swapchain ? device->vulkan_swapchain_framebuffers[device->vulkan_image_index] : render_pass->vk_frame_buffer;
            render_pass_begin.renderPass = render_pass->vk_render_pass;

            render_pass_begin.renderArea.offset = { 0, 0 };
            render_pass_begin.renderArea.extent = { render_pass->width, render_pass->height };

            render_pass_begin.clearValueCount = 2;
            render_pass_begin.pClearValues = clears;

            vkCmdBeginRenderPass( vk_command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE );
        }

        // Cache render pass
        current_render_pass = render_pass;
    }
}

void CommandBuffer::bind_pipeline( uint64_t sort_key, PipelineHandle handle ) {
    
    PipelineVulkan* pipeline = device->access_pipeline( handle );
    vkCmdBindPipeline( vk_command_buffer, pipeline->vk_bind_point, pipeline->vk_pipeline );
    
    // Cache pipeline
    current_pipeline = pipeline;
}

void CommandBuffer::bind_vertex_buffer( uint64_t sort_key, BufferHandle handle, uint32_t binding, uint32_t offset ) {
    
    BufferVulkan* buffer = device->access_buffer( handle );
    VkDeviceSize offsets[] = { offset };
    vkCmdBindVertexBuffers( vk_command_buffer, binding, 1, &buffer->vk_buffer, offsets );
}

void CommandBuffer::bind_index_buffer( uint64_t sort_key, BufferHandle handle ) {
    
    BufferVulkan* buffer = device->access_buffer( handle );
    vkCmdBindIndexBuffer( vk_command_buffer, buffer->vk_buffer, 0, VkIndexType::VK_INDEX_TYPE_UINT16 );
}

void CommandBuffer::bind_resource_list( uint64_t sort_key, ResourceListHandle* handles, uint32_t num_lists, uint32_t* offsets, uint32_t num_offsets ) {
    for ( uint32_t l = 0; l < num_lists; ++l ) {
        ResourceListVulkan* resource_list = device->access_resource_list( handles[l] );
        vk_descriptor_sets[l] = resource_list->vk_descriptor_set;
    }
    
    const uint32_t k_first_set = 0;
    vkCmdBindDescriptorSets( vk_command_buffer, current_pipeline->vk_bind_point, current_pipeline->vk_pipeline_layout, k_first_set,
                             num_lists, vk_descriptor_sets, num_offsets, offsets);
}

void CommandBuffer::set_viewport( uint64_t sort_key, const Viewport* viewport ) {

    VkViewport vk_viewport;

    if ( viewport ) {
        vk_viewport.x = viewport->rect.x;
        vk_viewport.width = viewport->rect.width;
        // Invert Y with negative height and proper offset - Vulkan has unique Clipping Y.
        vk_viewport.y = viewport->rect.height - viewport->rect.y;
        vk_viewport.height = -viewport->rect.height;
        vk_viewport.minDepth = viewport->min_depth;
        vk_viewport.maxDepth = viewport->max_depth;
    }
    else {
        vk_viewport.x = 0;

        if ( current_render_pass ) {
            vk_viewport.width = current_render_pass->width;
            // Invert Y with negative height and proper offset - Vulkan has unique Clipping Y.
            vk_viewport.y = current_render_pass->height;
            vk_viewport.height = -current_render_pass->height;
        }
        else {
            vk_viewport.width = device->swapchain_width;
            // Invert Y with negative height and proper offset - Vulkan has unique Clipping Y.
            vk_viewport.y = device->swapchain_height;
            vk_viewport.height = -device->swapchain_height;
        }
        vk_viewport.minDepth = 0.0f;
        vk_viewport.maxDepth = 1.0f;
    }

    vkCmdSetViewport( vk_command_buffer, 0, 1, &vk_viewport);
}

void CommandBuffer::set_scissor( uint64_t sort_key, const Rect2DInt* rect ) {

    VkRect2D vk_scissor;

    if ( rect ) {
        vk_scissor.offset.x = rect->x;
        vk_scissor.offset.y = rect->y;
        vk_scissor.extent.width = rect->width;
        vk_scissor.extent.height = rect->height;
    }
    else {
        vk_scissor.offset.x = 0;
        vk_scissor.offset.y = 0;
        vk_scissor.extent.width = device->swapchain_width;
        vk_scissor.extent.height = device->swapchain_height;
    }

    vkCmdSetScissor( vk_command_buffer, 0, 1, &vk_scissor );
}

void CommandBuffer::clear( uint64_t sort_key, float red, float green, float blue, float alpha ) {
    clears[0].color = { red, green, blue, alpha };
}

void CommandBuffer::clear_depth_stencil( uint64_t sort_key, float depth, uint8_t value ) {
    clears[1].depthStencil.depth = depth;
    clears[1].depthStencil.stencil = value;
}

void CommandBuffer::draw( uint64_t sort_key, TopologyType::Enum topology, uint32_t first_vertex, uint32_t vertex_count, uint32_t first_instance, uint32_t instance_count ) {
    vkCmdDraw( vk_command_buffer, vertex_count, instance_count, first_vertex, first_instance );
}

void CommandBuffer::draw_indexed( uint64_t sort_key, TopologyType::Enum topology, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance ) {
    vkCmdDrawIndexed( vk_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance );
}

void CommandBuffer::dispatch( uint64_t sort_key, uint32_t group_x, uint32_t group_y, uint32_t group_z ) {
    vkCmdDispatch( vk_command_buffer, group_x, group_y, group_z );
}

void CommandBuffer::barrier( const ExecutionBarrier& barrier ) {

    if ( current_render_pass && ( current_render_pass->type != RenderPassType::Compute ) ) {
        vkCmdEndRenderPass( vk_command_buffer );

        current_render_pass = nullptr;
    }

    VkImageLayout new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout new_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAccessFlags source_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags source_depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkAccessFlags destination_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags destination_depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    switch ( barrier.destination_pipeline_stage ) {

        case PipelineStage::FragmentShader:
        {
            //new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            break;
        }

        case PipelineStage::ComputeShader:
        {
            new_layout = VK_IMAGE_LAYOUT_GENERAL;

            break;
        }

        case PipelineStage::RenderTarget:
        {
            new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            new_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            destination_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destination_depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            break;
        }
    }

    switch ( barrier.source_pipeline_stage ) {

        case PipelineStage::FragmentShader:
        {

            break;
        }

        case PipelineStage::ComputeShader:
        {

            break;
        }

        case PipelineStage::RenderTarget:
        {
            source_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            source_depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            break;
        }
    }

    bool has_depth = false;
    static VkImageMemoryBarrier image_barriers[8];
    for ( uint32_t i = 0; i < barrier.num_image_barriers; ++i ) {

        TextureVulkan* texture_vulkan = device->access_texture( barrier.image_barriers[i].texture );

        VkImageMemoryBarrier& vk_barrier = image_barriers[i];
        vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        const bool is_color = !TextureFormat::has_depth_or_stencil( texture_vulkan->format );
        has_depth = has_depth || !is_color;

        vk_barrier.image = texture_vulkan->vk_image;
        vk_barrier.subresourceRange.aspectMask = is_color ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        vk_barrier.subresourceRange.baseMipLevel = 0;
        vk_barrier.subresourceRange.levelCount = 1;
        vk_barrier.subresourceRange.baseArrayLayer = 0;
        vk_barrier.subresourceRange.layerCount = 1;

        vk_barrier.oldLayout = texture_vulkan->vk_image_layout;

        // Transition to...
        vk_barrier.newLayout = is_color ? new_layout : new_depth_layout;

        vk_barrier.srcAccessMask = is_color ? source_access_mask : source_depth_access_mask;
        vk_barrier.dstAccessMask = is_color ? destination_access_mask : destination_depth_access_mask;

        texture_vulkan->vk_image_layout = vk_barrier.newLayout;
    }

    VkPipelineStageFlags source_stage_mask = to_vk_pipeline_stage( (PipelineStage::Enum)barrier.source_pipeline_stage );
    VkPipelineStageFlags destination_stage_mask = to_vk_pipeline_stage( ( PipelineStage::Enum )barrier.destination_pipeline_stage );

    if ( has_depth ) {

        source_stage_mask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        destination_stage_mask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    vkCmdPipelineBarrier( vk_command_buffer, source_stage_mask, destination_stage_mask, 0, 0, nullptr, 0, nullptr, barrier.num_image_barriers, image_barriers );
}

void CommandBuffer::push_marker( const char* name ) {

    device->push_gpu_timestamp( this, name );

    if ( !device->debug_utils_extension_present )
        return;

    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    label.color[ 0 ] = 1.0f;
    label.color[ 1 ] = 1.0f;
    label.color[ 2 ] = 1.0f;
    label.color[ 3 ] = 1.0f;
    pfnCmdBeginDebugUtilsLabelEXT( vk_command_buffer, &label );   
}

void CommandBuffer::pop_marker() {

    device->pop_gpu_timestamp( this );

    if ( !device->debug_utils_extension_present )
        return;

    pfnCmdEndDebugUtilsLabelEXT( vk_command_buffer );
}

// DeviceRenderFrame //////////////////////////////////////////////////////////////////

void DeviceRenderFrame::init( Device* gpu_device_, uint32_t thread_count_ ) {
    thread_count = thread_count_;
    gpu_device = gpu_device_;

    //////// Create command buffers
    VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    cmd_pool_info.queueFamilyIndex = gpu_device->vulkan_queue_family;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // Allocate the arrays
    vulkan_command_pools = (VkCommandPool*)malloc( sizeof( VkCommandPool ) * thread_count );
    vulkan_command_buffers = (VkCommandBuffer*)malloc( sizeof( VkCommandBuffer ) * 16 );
    command_buffers = (CommandBuffer**)malloc( sizeof( CommandBuffer* ) * 16 );

    for ( size_t i = 0; i < thread_count; i++ ) {
        check( vkCreateCommandPool( gpu_device->vulkan_device, &cmd_pool_info, gpu_device->vulkan_allocation_callbacks, &vulkan_command_pools[i] ) );

        // Create 16 command buffers
        VkCommandBufferAllocateInfo cmd = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        cmd.commandPool = vulkan_command_pools[i];
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 16;
        check( vkAllocateCommandBuffers( gpu_device->vulkan_device, &cmd, vulkan_command_buffers ) );

        for ( size_t c = 0; c < 16; c++ ) {

            uint32_t handle = gpu_device->command_buffers.obtain_resource();
            if ( handle != k_invalid_handle ) {
                command_buffers[c] = (CommandBuffer*)gpu_device->command_buffers.access_resource( handle );
                command_buffers[c]->vk_command_buffer = vulkan_command_buffers[c];
                command_buffers[c]->device = gpu_device;
                command_buffers[c]->reset();
            }
        }
    }

    //////// Create semaphores
    VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateSemaphore( gpu_device->vulkan_device, &semaphore_info, gpu_device->vulkan_allocation_callbacks, &vulkan_image_available_semaphore );
    vkCreateSemaphore( gpu_device->vulkan_device, &semaphore_info, gpu_device->vulkan_allocation_callbacks, &vulkan_render_finished_semaphore );

    vkCreateFence( gpu_device->vulkan_device, &fenceInfo, gpu_device->vulkan_allocation_callbacks, &vulkan_in_flight_fence );
}

void DeviceRenderFrame::terminate() {

    for ( size_t i = 0; i < thread_count; i++ ) {
        vkDestroyCommandPool( gpu_device->vulkan_device, vulkan_command_pools[ i ], gpu_device->vulkan_allocation_callbacks );
    }

    vkDestroySemaphore( gpu_device->vulkan_device, vulkan_image_available_semaphore, gpu_device->vulkan_allocation_callbacks );
    vkDestroySemaphore( gpu_device->vulkan_device, vulkan_render_finished_semaphore, gpu_device->vulkan_allocation_callbacks );

    vkDestroyFence( gpu_device->vulkan_device, vulkan_in_flight_fence, gpu_device->vulkan_allocation_callbacks );
}

CommandBuffer* DeviceRenderFrame::get_command_buffer( uint32_t thread_index, bool begin ) {

    CommandBuffer* command_buffer = command_buffers[thread_index];

    command_buffer->reset();

    if ( begin ) {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );
    }

    return command_buffer;
}

void DeviceRenderFrame::on_resize() {

    for ( size_t i = 0; i < thread_count; i++ ) {
        vkFreeCommandBuffers( gpu_device->vulkan_device, vulkan_command_pools[i], 16, vulkan_command_buffers );

        VkCommandBufferAllocateInfo cmd = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        cmd.commandPool = vulkan_command_pools[i];
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 16;
        check( vkAllocateCommandBuffers( gpu_device->vulkan_device, &cmd, vulkan_command_buffers ) );

        for ( size_t c = 0; c < 16; c++ ) {
            command_buffers[c]->vk_command_buffer = vulkan_command_buffers[c];
            command_buffers[c]->reset();
        }
    }
}

void DeviceRenderFrame::new_frame() {
    
    for ( size_t i = 0; i < thread_count; i++ ) {
        vkResetCommandPool( gpu_device->vulkan_device, vulkan_command_pools[i], 0 );
    }
}

// Utility methods //////////////////////////////////////////////////////////////

void check( VkResult result ) {
    if ( result == VK_SUCCESS ) {
        return;
    }

    HYDRA_LOG( "Vulkan error: code(%u)", result );
    if ( result < 0 ) {
        HYDRA_ASSERT( false, "Vulkan error: aborting." );
    }
}

#else
static_assert(false, "No platform was selected!");

#endif // HYDRA_VULKAN


} // namespace graphics
} // namespace hydra
