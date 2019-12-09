//
//  Hydra Graphics - v0.042
//  3D API wrapper around Vulkan/Direct3D12/OpenGL.
//  Mostly based on the amazing Sokol library (https://github.com/floooh/sokol), but with a different target (wrapping Vulkan/Direct3D12).
//
//      Source code     : https://www.github.com/jorenjoestar/
//      Version         : 0.042
//
//

#include "hydra_graphics.h"

#include <string.h>
#include <malloc.h>

#if defined (HYDRA_SDL)
#include <SDL.h>
#include <SDL_vulkan.h>
#endif // HYDRA_SDL

// Malloc/free leak checking library
#include "stb_leakcheck.h"

// Defines //////////////////////////////////////////////////////////////////////


// Use shared libraries to enhance different functions.
#define HYDRA_LIB
#if defined(HYDRA_LIB)
#include "hydra_lib.h"

#define HYDRA_LOG                                           hydra::print_format

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

    void clear() {
        current_size = 0;
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

void ResourcePool::init( uint32_t pool_size, uint32_t resource_size ) {

    this->size = pool_size;
    this->resource_size = resource_size;

    memory = (uint8_t*)hy_malloc( pool_size * resource_size );

    // Allocate and add free indices
    free_indices = (uint32_t*)hy_malloc( pool_size * sizeof( uint32_t ) );
    free_indices_head = 0;

    for ( uint32_t i = 0; i < pool_size; ++i ) {
        free_indices[i] = i;
    }
}

void ResourcePool::terminate() {

    hy_free( memory );
    hy_free( free_indices );
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

    //HYDRA_ASSERT( shaders, "Device: shader memory should be initialized!" );
}

void Device::terminate() {
    
    backend_terminate();
    
    s_string_buffer.terminate();

}

CommandBuffer* Device::create_command_buffer( QueueType::Enum type, uint32_t size, bool baked ) {
    CommandBuffer* commands = (CommandBuffer*)malloc( sizeof( CommandBuffer ) );
    commands->init( type, size, 256, baked );

    return commands;
}

void Device::destroy_command_buffer( CommandBuffer* command_buffer ) {
    command_buffer->terminate();
    free( command_buffer );
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
}

// CommandBuffer ////////////////////////////////////////////////////////////////

void CommandBuffer::init( QueueType::Enum type, uint32_t buffer_size, uint32_t submit_size, bool baked ) {
    this->type = type;
    this->buffer_size = buffer_size;
    this->baked = baked;

    data = (uint8_t*)malloc( buffer_size );
    read_offset = write_offset = 0;

    this->max_submits = submit_size;
    this->num_submits = 0;

    this->submit_commands = (SubmitCommand*)malloc( sizeof(SubmitCommand) * submit_size );
}

void CommandBuffer::terminate() {

    free( data );
    free( submit_commands );

    read_offset = write_offset = buffer_size = 0;
    max_submits = num_submits = 0;
}

void CommandBuffer::reset() {
    read_offset = 0;

    // Reset all writing properties.
    if ( !baked ) {
        write_offset = 0;
        num_submits = 0;
    }
}

void CommandBuffer::begin_submit( uint64_t sort_key ) {

    current_submit_command.key = sort_key;
    current_submit_command.data = data + write_offset;

    // Allocate space for size and sentinel 
    current_submit_header = (commands::SubmitHeader*)current_submit_command.data;
    current_submit_header->sentinel = k_submit_header_sentinel;
    write_offset += sizeof( commands::SubmitHeader );
}

void CommandBuffer::end_submit() {

    current_submit_header = (commands::SubmitHeader*)current_submit_command.data;
    // Calculate final submit packed size - removing the additional header.
    current_submit_header->data_size = (data + write_offset) - current_submit_command.data - sizeof( commands::SubmitHeader );

    submit_commands[num_submits++] = current_submit_command;

    current_submit_command.key = 0xffffffffffffffff;
    current_submit_command.data = nullptr;
}

void CommandBuffer::begin_pass( RenderPassHandle handle ) {
    commands::BeginPass* bind = write_command<commands::BeginPass>();
    bind->handle = handle;
}

void CommandBuffer::end_pass() {
    // No arguments -- empty.
    write_command<commands::EndPass>();
}

void CommandBuffer::bind_pipeline( PipelineHandle handle ) {
    commands::BindPipeline* bind = write_command<commands::BindPipeline>();
    bind->handle = handle;
}

void CommandBuffer::bind_vertex_buffer( BufferHandle handle ) {
    commands::BindVertexBuffer* bind = write_command< commands::BindVertexBuffer>();
    bind->buffer = handle;
}

void CommandBuffer::bind_index_buffer( BufferHandle handle ) {
    commands::BindIndexBuffer* bind = write_command< commands::BindIndexBuffer>();
    bind->buffer = handle;
}

void CommandBuffer::bind_resource_list( ResourceListHandle* handle, uint32_t num_lists ) {
    commands::BindResourceList* bind = write_command<commands::BindResourceList>();
    
    for ( uint32_t l = 0; l < num_lists; ++l ) {
        bind->handles[l] = handle[l];
    }

    bind->num_lists = num_lists;
}

void CommandBuffer::set_viewport( const Viewport& viewport ) {
    commands::SetViewport* set = write_command<commands::SetViewport>();
    set->viewport = viewport;
}

void CommandBuffer::set_scissor( const Rect2D& rect ) {
    commands::SetScissor* set = write_command<commands::SetScissor>();
    set->rect = rect;
}

void CommandBuffer::clear( float red, float green, float blue, float alpha ) {
    commands::Clear* clear = write_command<commands::Clear>();
    clear->clear_color[0] = red;
    clear->clear_color[1] = green;
    clear->clear_color[2] = blue;
    clear->clear_color[3] = alpha;
}

void CommandBuffer::draw( TopologyType::Enum topology, uint32_t start, uint32_t count ) {
    commands::Draw* draw_command = write_command<commands::Draw>();

    draw_command->topology = topology;
    draw_command->first_vertex = start;
    draw_command->vertex_count = count;
}

void CommandBuffer::drawIndexed( TopologyType::Enum topology, uint32_t index_count, uint32_t instance_count,
                                 uint32_t first_index, int32_t vertex_offset, uint32_t first_instance ) {
    commands::DrawIndexed* draw_command = write_command<commands::DrawIndexed>();

    draw_command->topology = topology;
    draw_command->index_count = index_count;
    draw_command->instance_count = instance_count;
    draw_command->first_index = first_index;
    draw_command->vertex_offset = vertex_offset;
    draw_command->first_instance = first_instance;
}

void CommandBuffer::dispatch( uint32_t group_x, uint32_t group_y, uint32_t group_z ) {
    commands::Dispatch* command = write_command<commands::Dispatch>();
    command->group_x = (uint16_t)group_x;
    command->group_y = (uint16_t)group_y;
    command->group_z = (uint16_t)group_z;
}


// API Specific /////////////////////////////////////////////////////////////////


// OpenGL ///////////////////////////////////////////////////////////////////////

#if defined (HYDRA_OPENGL)

// Defines
// TODO: until enums are not exported anymore, add + 1, max is not count!

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
static GLuint to_gl_min_filter_type( TextureFilter::Enum filter, TextureFilter::Enum mipmap ) {
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
// Float, Float2, Float3, Float4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Count
static GLuint to_gl_components( VertexComponentFormat::Enum format ) {
    static GLuint s_gl_components[] = { 1, 2, 3, 4, 1, 4, 1, 4, 2, 2, 4, 4 };
    return s_gl_components[format];
}

static GLenum to_gl_vertex_type( VertexComponentFormat::Enum format ) {
    static GLenum s_gl_vertex_type[] = { GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_BYTE, GL_BYTE, GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_SHORT, GL_SHORT, GL_SHORT };
    return s_gl_vertex_type[format];
}

static GLboolean to_gl_vertex_norm( VertexComponentFormat::Enum format ) {
    static GLboolean s_gl_vertex_norm[] = { GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE };
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
    GLuint                          gl_vao_handle       = 0;        // Special case for Vertex Arrays. They need both this AND a buffer.

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

static const uint32_t               k_max_vertex_streams = 4;
static const uint32_t               k_max_vertex_attributes = 16;

struct VertexInputGL {

    uint32_t                        num_streams         = 0;
    uint32_t                        num_attributes      = 0;
    VertexStream                    vertex_streams[k_max_vertex_streams];
    VertexAttribute                 vertex_attributes[k_max_vertex_attributes];

}; // struct VertexInputGL

//
//
struct PipelineGL {

    ShaderHandle                    shader_state;
    GLuint                          gl_program_cached   = 0;

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


    void                            set() const;

}; // struct ResourceListGL

//
// Holds all the states necessary to render.
struct DeviceStateGL {

    GLuint                          fbo_handle          = 0;
    GLuint                          vb_handle           = 0;
    GLuint                          vao_handle          = 0;
    GLuint                          ib_handle           = 0;

    const Viewport*                 viewport            = nullptr;
    const Rect2D*                   scissor             = nullptr;
    const PipelineGL*               pipeline            = nullptr;
    const ResourceListGL*           resource_lists[k_max_resource_layouts];
    uint32_t                        num_lists           = 0;

    float                           clear_color[4];
    bool                            clear_color_flag    = false;
    bool                            swapchain_flag      = false;
    bool                            end_pass_flag       = false;    // End pass after last draw/dispatch.

    void                            apply();

}; // struct DeviceStateGL

// Methods //////////////////////////////////////////////////////////////////////

// Forward declarations
static GLuint                       compile_shader( GLuint stage, const char* source );
static bool                         get_compile_info( GLuint shader, GLuint status );
static bool                         get_link_info( GLuint shader, GLuint status );

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

    // During init, enable debug output
    glEnable( GL_DEBUG_OUTPUT );
    glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glDebugMessageCallback( gl_message_callback, 0 );

    // Disable notification messages.
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE );

    device_state = (DeviceStateGL*)malloc( sizeof( DeviceStateGL ) );

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
    swapchain_pass_creation.is_swapchain = true;
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

    pipelines.terminate();
    buffers.terminate();
    shaders.terminate();
    textures.terminate();
    samplers.terminate();
    resource_list_layouts.terminate();
    resource_lists.terminate();
    render_passes.terminate();
}

// Resource Access //////////////////////////////////////////////////////////////
ShaderStateGL* Device::access_shader( ShaderHandle shader ) {
    return (ShaderStateGL*)shaders.access_resource( shader.handle );
}

const ShaderStateGL* Device::access_shader( ShaderHandle shader ) const {
    return (const ShaderStateGL*)shaders.access_resource( shader.handle );
}

TextureGL* Device::access_texture( TextureHandle texture ) {
    return (TextureGL*)textures.access_resource( texture.handle );
}

const TextureGL * Device::access_texture( TextureHandle texture ) const {
    return (const TextureGL*)textures.access_resource( texture.handle );
}

BufferGL* Device::access_buffer( BufferHandle buffer ) {
    return (BufferGL*)buffers.access_resource( buffer.handle );
}

const BufferGL* Device::access_buffer( BufferHandle buffer ) const {
    return (const BufferGL*)buffers.access_resource( buffer.handle );
}

PipelineGL* Device::access_pipeline( PipelineHandle pipeline ) {
    return (PipelineGL*)pipelines.access_resource( pipeline.handle );
}

const PipelineGL* Device::access_pipeline( PipelineHandle pipeline ) const {
    return (const PipelineGL*)pipelines.access_resource( pipeline.handle );
}

SamplerGL* Device::access_sampler( SamplerHandle sampler ) {
    return (SamplerGL*)samplers.access_resource( sampler.handle );
}

const SamplerGL* Device::access_sampler( SamplerHandle sampler ) const {
    return (const SamplerGL*)samplers.access_resource( sampler.handle );
}

ResourceListLayoutGL* Device::access_resource_list_layout( ResourceListLayoutHandle resource_layout ) {
    return (ResourceListLayoutGL*)resource_list_layouts.access_resource( resource_layout.handle );
}

const ResourceListLayoutGL* Device::access_resource_list_layout( ResourceListLayoutHandle resource_layout ) const {
    return (const ResourceListLayoutGL*)resource_list_layouts.access_resource( resource_layout.handle );
}

ResourceListGL* Device::access_resource_list( ResourceListHandle resource_list ) {
    return (ResourceListGL*)resource_lists.access_resource( resource_list.handle );
}

const ResourceListGL* Device::access_resource_list( ResourceListHandle resource_list ) const {
    return (const ResourceListGL*)resource_lists.access_resource( resource_list.handle );
}

RenderPassGL* Device::access_render_pass( RenderPassHandle render_pass ) {
    return (RenderPassGL*)render_passes.access_resource( render_pass.handle );
}

const RenderPassGL* Device::access_render_pass( RenderPassHandle render_pass ) const {
    return (const RenderPassGL*)render_passes.access_resource( render_pass.handle );
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
        texture->render_target = creation.render_target;

        texture->gl_handle = gl_handle;
        texture->gl_target = gl_target;

        texture->name = creation.name;

        texture->handle = handle;
    }
    
    return handle;
}

ShaderHandle Device::create_shader( const ShaderCreation& creation ) {

    ShaderHandle handle = { k_invalid_handle };

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
        const ShaderCreation::Stage& stage = creation.stages[compiled_shaders];
        GLuint gl_shader = compile_shader( to_gl_shader_stage(stage.type), stage.code );

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

        if ( !get_link_info( gl_program, GL_LINK_STATUS ) ) {
            glDeleteProgram( gl_program );
            gl_program = 0;

            creation_failed = true;

            HYDRA_LOG( "Error linking GL shader %s.\n", creation.name );
        }

        ShaderStateGL* shader_state = access_shader( handle );
        shader_state->gl_program = gl_program;
        shader_state->name = creation.name;
    }

    if ( creation_failed ) {
        shaders.release_resource( handle.handle );
        handle.handle = k_invalid_handle;
    }

    return handle;
}

PipelineHandle Device::create_pipeline( const PipelineCreation& creation ) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    PipelineGL* pipeline = access_pipeline( handle );

    // Create all necessary resources
    ShaderHandle shader_state = create_shader( creation.shaders );
    if ( shader_state.handle == k_invalid_handle ) {
        // Shader did not compile.
        pipelines.release_resource( handle.handle );
        handle.handle = k_invalid_handle;

        return handle;
    }

    ShaderStateGL* shader_state_data = access_shader( shader_state );

    pipeline->shader_state = shader_state;
    pipeline->gl_program_cached = shader_state_data->gl_program;
    pipeline->handle = handle;

    if ( !creation.compute ) {
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

        pipeline->graphics_pipeline = true;
    }
    else {
        pipeline->graphics_pipeline = false;
    }

    // Resource List Layout
    for ( uint32_t l = 0; l < creation.num_active_layouts; ++l ) {
        pipeline->resource_list_layout[l] = access_resource_list_layout( creation.resource_list_layout[l] );
        pipeline->resource_list_layout_handle[l] = creation.resource_list_layout[l];

        cache_resource_bindings( pipeline->gl_program_cached, pipeline->resource_list_layout[l] );
    }

    if ( creation.num_active_layouts == 0 ) {
        print_format( "Error in pipeline: no resources layouts are specificed!" );
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
            // Create both a buffer AND a vertex array object.
            glCreateBuffers( 1, &buffer->gl_handle );
            glNamedBufferData( buffer->gl_handle, buffer->size, creation.initial_data, buffer->gl_usage );

            glCreateVertexArrays( 1, &buffer->gl_vao_handle );

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
    resource_layout->bindings = (ResourceBindingGL*)hy_malloc( sizeof( ResourceBindingGL ) * creation.num_bindings );
    resource_layout->handle = handle;

    for ( uint32_t r = 0; r < creation.num_bindings; ++r ) {
        ResourceBindingGL& binding = resource_layout->bindings[r];
        binding.start = r;
        binding.count = 1;
        binding.type = creation.bindings[r].type;
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

    resources->resources = (ResourceData*)hy_malloc( sizeof( ResourceData ) * creation.num_resources );
    resources->num_resources = creation.num_resources;
    
    // Set all resources
    for ( uint32_t r = 0; r < creation.num_resources; ++r ) {
        ResourceData& resource = resources->resources[r];

        const ResourceBindingGL binding = resources->layout->bindings[r];
        
        switch ( binding.type ) {
            case ResourceType::Texture:
            case ResourceType::TextureRW:
            {
                TextureHandle texture_handle = { creation.resources[r].handle };
                TextureGL* texture_data = access_texture( texture_handle );
                resource.data = (void*)texture_data;

                break;
            }

            case ResourceType::Constants:
            {
                BufferHandle buffer_handle = { creation.resources[r].handle };
                BufferGL* buffer = access_buffer( buffer_handle );
                resource.data = (void*)buffer;
                break;
            }

            default:
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

    // Create the FBO only if it actually outputs to textures.
    // Compute post-processes and framebuffer passes do not output to FBOs in OpenGL.
    if ( !creation.is_swapchain && !creation.is_compute_post ) {
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

void Device::destroy_shader( ShaderHandle shader ) {

    if ( shader.handle != k_invalid_handle ) {
        ShaderStateGL* shader_state = access_shader( shader );
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
        hy_free( state->bindings );
        resource_list_layouts.release_resource( resource_layout.handle );
    }
}

void Device::destroy_resource_list( ResourceListHandle resource_list ) {
    if ( resource_list.handle != k_invalid_handle ) {
        ResourceListGL* state = access_resource_list( resource_list );
        hy_free( state->resources );
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

void Device::query_shader( ShaderHandle shader, ShaderStateDescription& out_description ) {
    if ( shader.handle != k_invalid_handle ) {
        const ShaderStateGL* shader_state = access_shader( shader );

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
void Device::resize_output_textures( RenderPassHandle render_pass, uint16_t width, uint16_t height ) {

    RenderPassGL* render_pass_gl = access_render_pass( render_pass );
    if ( render_pass_gl ) {

        for ( size_t i = 0; i < render_pass_gl->num_render_targets; ++i ) {
            TextureGL* texture = render_pass_gl->render_targets[i];
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

            glBindTexture( texture->gl_target, 0 );
        }
    }
}

void Device::queue_command_buffer( CommandBuffer* command_buffer ) {

    queued_command_buffers[num_queued_command_buffers++] = command_buffer;
}

void Device::present() {

    // TODO:
    // 1. Merge and sort all command buffers.
    // 2. Execute command buffers.

    static SubmitCommand merged_commands[128];
    uint32_t num_submits = 0;

    // Copy all commands
    for ( uint32_t c = 0; c < num_queued_command_buffers; c++ ) {

        CommandBuffer* command_buffer = queued_command_buffers[c];
        for ( uint32_t s = 0; s < command_buffer->num_submits; ++s ) {
            merged_commands[num_submits++] = command_buffer->submit_commands[s];
        }

        command_buffer->reset();
    }

    // TODO: missing implementation.
    // Sort them

    // Execute
    // TODO: temporary implementation.
    CommandBuffer command_buffer;

    for ( uint32_t s = 0; s < num_submits; ++s ) {

        const SubmitCommand& submit = merged_commands[s];
        const commands::SubmitHeader& submit_header = *(const commands::SubmitHeader*)submit.data;

        HYDRA_ASSERT(submit_header.sentinel == k_submit_header_sentinel, "");

        // Read data from submit_header
        uint32_t total_data = submit_header.data_size;
        const uint8_t* read_memory = submit.data +sizeof( commands::SubmitHeader );

        // Repurpose command buffer
        command_buffer.data = (uint8_t*)read_memory;
        command_buffer.read_offset = 0;

        while ( command_buffer.read_offset < total_data ) {

            CommandType::Enum command_type = command_buffer.get_command_type();

            switch ( command_type ) {

                case CommandType::BeginPass:
                {
                    // TODO:
                    const commands::BeginPass& begin_pass = command_buffer.read_command<commands::BeginPass>();
                    RenderPassGL* render_pass_gl = access_render_pass( begin_pass.handle );

                    device_state->fbo_handle = render_pass_gl->fbo_handle;
                    device_state->swapchain_flag = render_pass_gl->is_swapchain;
                    device_state->scissor = nullptr;
                    device_state->viewport = nullptr;

                    break;
                }

                case CommandType::EndPass:
                {
                    const commands::EndPass& end_pass = command_buffer.read_command<commands::EndPass>();
                    device_state->end_pass_flag = true;

                    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

                    break;
                }

                case CommandType::BindVertexBuffer:
                {
                    const commands::BindVertexBuffer& binding = command_buffer.read_command<commands::BindVertexBuffer>();
                    BufferGL* buffer = access_buffer( binding.buffer );

                    device_state->vao_handle = buffer->gl_vao_handle;
                    device_state->vb_handle = buffer->gl_handle;

                    break;
                }

                case CommandType::BindIndexBuffer:
                {
                    const commands::BindIndexBuffer& binding = command_buffer.read_command<commands::BindIndexBuffer>();
                    BufferGL* buffer = access_buffer( binding.buffer );

                    device_state->ib_handle = buffer->gl_handle;

                    break;
                }

                case CommandType::SetViewport:
                {
                    const commands::SetViewport& set = command_buffer.read_command<commands::SetViewport>();
                    device_state->viewport = &set.viewport;

                    break;
                }

                case CommandType::SetScissor:
                {
                    const commands::SetScissor& set = command_buffer.read_command<commands::SetScissor>();
                    device_state->scissor = &set.rect;

                    break;
                }

                case CommandType::Clear:
                {
                    const commands::Clear& clear = command_buffer.read_command<commands::Clear>();
                    memcpy( device_state->clear_color, clear.clear_color, sizeof( float ) * 4 );
                    device_state->clear_color_flag = true;

                    break;
                }

                case CommandType::BindPipeline:
                {
                    const commands::BindPipeline& binding = command_buffer.read_command<commands::BindPipeline>();
                    const PipelineGL* pipeline = access_pipeline( binding.handle );

                    device_state->pipeline = pipeline;

                    break;
                }

                case CommandType::BindResourceSet:
                {
                    const commands::BindResourceList& binding = command_buffer.read_command<commands::BindResourceList>();

                    for ( uint32_t l = 0; l < binding.num_lists; ++l ) {
                        const ResourceListGL* resource_list = access_resource_list( binding.handles[l] );
                        device_state->resource_lists[l] = resource_list;
                    }

                    device_state->num_lists = binding.num_lists;

                    break;
                }

                case CommandType::Dispatch:
                {
                    device_state->apply();

                    const commands::Dispatch& dispatch = command_buffer.read_command<commands::Dispatch>();
                    glDispatchCompute( dispatch.group_x, dispatch.group_y, dispatch.group_z );

                    // TODO: barrier handling.
                    glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

                    break;
                }

                case CommandType::Draw:
                {
                    device_state->apply();

                    const commands::Draw& draw = command_buffer.read_command<commands::Draw>();
                    glDrawArrays( GL_TRIANGLES, draw.first_vertex, draw.vertex_count );

                    break;
                }

                case CommandType::DrawIndexed:
                {
                    device_state->apply();

                    const commands::DrawIndexed& draw = command_buffer.read_command<commands::DrawIndexed>();
                    const uint32_t index_buffer_size = 2;
                    const GLuint start = 0;
                    const GLuint start_index_offset = draw.first_index * index_buffer_size;
                    const GLuint end_index_offset = start_index_offset + draw.index_count * index_buffer_size;
                    glDrawRangeElementsBaseVertex( GL_TRIANGLES, start_index_offset, end_index_offset, (GLsizei)draw.index_count, GL_UNSIGNED_SHORT, (void*)start_index_offset, draw.vertex_offset );
                    
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

void ResourceListGL::set() const {

    if ( layout == nullptr ) {
        return;
    }

    // TODO: this is the first version. Just sets textures.
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
                glBindBufferBase( buffer->gl_type, binding.gl_block_binding, buffer->gl_handle );

                break;
            }

            default:
            {
                HYDRA_ASSERT( false, "Resource type not handled, %u" );
                break;
            }
        }
    }
}

// DeviceStateGL ////////////////////////////////////////////////////////////////
void DeviceStateGL::apply()  {

    if ( pipeline->graphics_pipeline ) {
        // Bind FrameBuffer
        if ( !swapchain_flag && fbo_handle > 0 ) {
            glBindFramebuffer( GL_FRAMEBUFFER, fbo_handle );
        }

        // Bind Vertex Buffer
        glBindBuffer( GL_ARRAY_BUFFER, vb_handle );
        glBindVertexArray( vao_handle );

        // Bind Index Buffer
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ib_handle );

        if ( viewport )
            glViewport( viewport->rect.x, viewport->rect.y, viewport->rect.width, viewport->rect.height );

        if ( scissor ) {
            glEnable( GL_SCISSOR_TEST );
            glScissor( scissor->x, scissor->y, scissor->width, scissor->height );
        }

        // Bind shaders
        glUseProgram( pipeline->gl_program_cached );

        if ( num_lists ) {
            for ( uint32_t l = 0; l < num_lists; ++l ) {
                resource_lists[l]->set();
            }
        }    

        glDisable( GL_SCISSOR_TEST );

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

        if ( clear_color_flag ) {
            glClearColor( clear_color[0], clear_color[1], clear_color[2], clear_color[3] );
            glClear( GL_COLOR_BUFFER_BIT );
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
            glFrontFace( rasterization.cull_mode == CullMode::Front ? GL_FRONT : GL_BACK );
        }

        glFrontFace( rasterization.front == FrontClockwise::True ? GL_CW : GL_CCW );

        const VertexInputGL& vertex_input = pipeline->vertex_input;
        for ( uint32_t i = 0; i < vertex_input.num_streams; i++ ) {
            const VertexStream& stream = vertex_input.vertex_streams[i];
            glBindVertexBuffer( stream.binding, vb_handle, 0, stream.stride );
        }

        for ( uint32_t i = 0; i < vertex_input.num_attributes; i++ ) {
            const VertexAttribute& attribute = vertex_input.vertex_attributes[i];
            glEnableVertexAttribArray( attribute.location );
            glVertexAttribFormat( attribute.location, to_gl_components( attribute.format ), to_gl_vertex_type( attribute.format ),
                                  to_gl_vertex_norm( attribute.format ), attribute.offset );
            glVertexAttribBinding( attribute.location, attribute.binding );
        }

        // Reset cached states
        clear_color_flag = false;
    }
    else {

        glUseProgram( pipeline->gl_program_cached );

        if ( num_lists ) {
            for ( uint32_t l = 0; l < num_lists; ++l ) {
                resource_lists[l]->set();
            }
        }
    }

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

    // init depth
    //glgen..
    //glBindRenderbuffer( GL30.GL_RENDERBUFFER, depthbufferHandle );
    //glRenderbufferStorage( GL30.GL_RENDERBUFFER, GL30.GL_DEPTH_COMPONENT, width, height );

    //glBindRenderbuffer( GL30.GL_RENDERBUFFER, stencilbufferHandle );
    //glRenderbufferStorage( GL30.GL_RENDERBUFFER, GL30.GL_STENCIL_INDEX8, width, height );

    //depthStencilPackedBufferHandle = glGenRenderbuffers();
    //hasDepthStencilPackedBuffer = true;
    //glBindRenderbuffer( GL30.GL_RENDERBUFFER, depthStencilPackedBufferHandle );
    //glRenderbufferStorage( GL30.GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height );
    //glBindRenderbuffer( GL30.GL_RENDERBUFFER, 0 );

    //glFramebufferRenderbuffer( GL30.GL_FRAMEBUFFER, GL30.GL_DEPTH_ATTACHMENT, GL30.GL_RENDERBUFFER, depthStencilPackedBufferHandle );
    //glFramebufferRenderbuffer( GL30.GL_FRAMEBUFFER, GL30.GL_STENCIL_ATTACHMENT, GL30.GL_RENDERBUFFER, depthStencilPackedBufferHandle );

    //glBindRenderbuffer( GL30.GL_RENDERBUFFER, 0 );
    //glBindTexture( GL30.GL_TEXTURE_2D, 0 );

    int result = glCheckFramebufferStatus( GL_FRAMEBUFFER );

    //if ( result == GL30.GL_FRAMEBUFFER_UNSUPPORTED && hasDepth && hasStencil
    //     && (Gdx.graphics.supportsExtension( "GL_OES_packed_depth_stencil" ) ||
    //          Gdx.graphics.supportsExtension( "GL_EXT_packed_depth_stencil" )) ) {

    //    deleteDepthStencil();

    //    initPackedDepthStencil();
    //    result = glCheckFramebufferStatus( GL30.GL_FRAMEBUFFER );
    //}

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

    //    if ( hasDepth ) {
    //        glFramebufferRenderbuffer( GL30.GL_FRAMEBUFFER, GL30.GL_DEPTH_ATTACHMENT, GL30.GL_RENDERBUFFER, depthbufferHandle );
    //    }
    //    if ( hasStencil ) {
    //        glFramebufferRenderbuffer( GL30.GL_FRAMEBUFFER, GL30.GL_STENCIL_ATTACHMENT, GL30.GL_RENDERBUFFER, stencilbufferHandle );
    //    }
    }

    // Attach textures
    
    //drawBuffers = BufferUtils.newIntBuffer( textures.size );
    render_pass.num_render_targets = creation.num_render_targets;

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

    //    drawBuffers.put( toGLColorAttachment( i ) );
    }

    //if ( depthTexture != null ) {
    //    int textureHandle = depthTexture.getTextureObjectHandle();
    //    glBindTexture( depthTexture.glTarget, textureHandle );

    //    if ( (depthTexture.glTarget == GL_TEXTURE_CUBE_MAP) || (depthTexture.glTarget == GL_TEXTURE_CUBE_MAP_ARRAY) ) {
    //        glFramebufferTexture( GL30.GL_FRAMEBUFFER, GL30.GL_DEPTH_ATTACHMENT, textureHandle, 0 );
    //    }
    //    else {
    //        glFramebufferTexture2D( GL30.GL_FRAMEBUFFER, GL30.GL_DEPTH_ATTACHMENT, depthTexture.glTarget, textureHandle, 0 );
    //    }

    //    checkFrameBuffer();
    //}
    render_pass.fbo_handle = framebuffer_handle;

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

}

GLuint compile_shader( GLuint stage, const char* source ) {
    GLuint shader = glCreateShader( stage );
    if ( !shader ) {
        HYDRA_LOG( "Error creating GL shader.\n" );
        return shader;
    }

    // Attach source code and compile.
    glShaderSource( shader, 1, &source, 0 );
    glCompileShader( shader );

    if ( !get_compile_info( shader, GL_COMPILE_STATUS ) ) {
        glDeleteShader( shader );
        shader = 0;

        HYDRA_LOG( "Error compiling GL shader.\n" );
    }

    return shader;
}

bool get_compile_info( GLuint shader, GLuint status ) {
    GLint result;
    // Status is either compile (for shaders) or link (for programs).
    glGetShaderiv( shader, status, &result );
    // Compilation failed. Get info log.
    if ( !result ) {
        GLint info_log_length = 0;
        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &info_log_length );
        if ( info_log_length > 0 ) {
            glGetShaderInfoLog( shader, s_string_buffer.buffer_size, &info_log_length, s_string_buffer.data );
            HYDRA_LOG( "%s\n", s_string_buffer.data );
        }
        return false;
    }

    return true;
}

bool get_link_info( GLuint program, GLuint status ) {
    GLint result;
    // Status is either compile (for shaders) or link (for programs).
    glGetProgramiv( program, status, &result );
    // Compilation failed. Get info log.
    if ( !result ) {
        GLint info_log_length = 0;
        glGetProgramiv( program, GL_INFO_LOG_LENGTH, &info_log_length );
        if ( info_log_length > 0 ) {
            glGetShaderInfoLog( program, s_string_buffer.buffer_size, &info_log_length, s_string_buffer.data );
            HYDRA_LOG( "%s\n", s_string_buffer.data );
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
    graphics::CommandBuffer* commands = device.create_command_buffer( graphics::QueueType::Graphics, 1024, false );

    commands->draw( graphics::TopologyType::Triangle, 0, 3 );

    const graphics::commands::Draw& draw = commands->read_command<graphics::commands::Draw>();
    HYDRA_ASSERT( draw.first_vertex == 0, "First vertex should be 0 instead of %u", draw.first_vertex );
    HYDRA_ASSERT( draw.vertex_count == 3, "Vertex count should be 3 instead of %u", draw.vertex_count );
    HYDRA_ASSERT( draw.topology == TopologyType::Triangle, "Topology should be triangle instead of %s", TopologyType::ToString(draw.topology) );
    HYDRA_ASSERT( draw.type == CommandType::Draw, "Command should be Draw instead of %s", CommandType::ToString(draw.type) );
    HYDRA_ASSERT( draw.size == sizeof( commands::Draw ), "Size should be %u instead of %u", sizeof( commands::Draw ), draw.size );
}


// Vulkan ///////////////////////////////////////////////////////////////////////

#elif defined (HYDRA_VULKAN)

static const char* s_wanted_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_MACOS_MVK
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_XLIB_KHR
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_WAYLAND_KHR
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_MIR_KHR || VK_USE_PLATFORM_DISPLAY_KHR
        VK_KHR_DISPLAY_EXTENSION_NAME
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif VK_USE_PLATFORM_IOS_MVK
        VK_MVK_IOS_SURFACE_EXTENSION_NAME
#endif
};

#define VULKAN_DEBUG_REPORT

#ifdef VULKAN_DEBUG_REPORT

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData ) {
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    HYDRA_LOG( "[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
    return VK_FALSE;
}
#endif // VULKAN_DEBUG_REPORT


static void                         check( VkResult result );
static VkSurfaceFormatKHR           choose_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space );
static VkPresentModeKHR             choose_present_mode( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count );


void Device::backend_init( const DeviceCreation& creation ) {
    
    // 1. Init Vulkan instance.
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = 0;
    create_info.ppEnabledExtensionNames = nullptr;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;

    VkResult result;

    // 1.1. Choose extensions
#ifdef VULKAN_DEBUG_REPORT
    // Enabling multiple validation layers grouped as LunarG standard validation
    const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    const char* extensions[] = { "VK_EXT_debug_report" };
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = extensions;

    vulkan_allocation_callbacks = nullptr;

    // Create Vulkan Instance
    result = vkCreateInstance( &create_info, vulkan_allocation_callbacks, &vulkan_instance );
    check( result );

    // Create debug callback
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugReportCallbackEXT" );
    HYDRA_ASSERT( vkCreateDebugReportCallbackEXT != NULL, "" );

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_callback;
    debug_report_ci.pUserData = NULL;
    result = vkCreateDebugReportCallbackEXT( vulkan_instance, &debug_report_ci, vulkan_allocation_callbacks, &vulkan_debug_callback );
    check( result );
#else
    // Create Vulkan Instance without any debug feature
    result = vkCreateInstance( &create_info, vulkan_allocation_callbacks, &vulkan_instance );
    check( result );
#endif

    // 2. Choose physical device
    uint32_t num_physical_device;
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, NULL );
    check( result );

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc( sizeof( VkPhysicalDevice ) * num_physical_device );
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, gpus );
    check( result );

    // TODO: create a 'isDeviceSuitable' method to choose the proper GPU.
    vulkan_physical_device = gpus[0];
    free( gpus );

    //createLogicalDevice
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

    vkGetDeviceQueue( vulkan_device, family_index, 0, &vulkan_queue );

    vulkan_queue_family = family_index;

    // Create pools
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


#if defined (HYDRA_SDL)

    // Create surface
    SDL_Window* window = (SDL_Window*)creation.window;
    if ( SDL_Vulkan_CreateSurface( window, vulkan_instance, &vulkan_window_surface ) == 0 ) {
        printf( "Failed to create Vulkan surface.\n" );
        //return 1;
    }

    // Create Framebuffers
    int window_width, window_height;
    SDL_GetWindowSize( window, &window_width, &window_height );

#endif // HYDRA_SDL

    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, vulkan_queue_family, vulkan_window_surface, &surface_supported );
    if ( surface_supported != VK_TRUE ) {
        fprintf( stderr, "Error no WSI support on physical device 0\n" );
    }

    // Select Surface Format
    const VkFormat surface_image_formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    vulkan_surface_format = choose_surface_format( vulkan_physical_device, vulkan_window_surface, surface_image_formats, ArrayLength( surface_image_formats ), surface_color_space );

    //VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    vulkan_present_mode = choose_present_mode( vulkan_physical_device, vulkan_window_surface, present_modes, ArrayLength( present_modes ) );


    //createSurface();
    //createSwapChain();


#if defined (HYDRA_GRAPHICS_TEST)
    test_texture_creation( *this );
    test_pool( *this );
    test_command_buffer( *this );
#endif // HYDRA_GRAPHICS_TEST

    //// Init pools
    //shaders.init( 128, sizeof( ShaderStateGL ) );
    //textures.init( 128, sizeof( TextureGL ) );
    //buffers.init( 128, sizeof( BufferGL ) );
    //pipelines.init( 128, sizeof( PipelineGL ) );
    //samplers.init( 32, sizeof( SamplerGL ) );
    //resource_list_layouts.init( 128, sizeof( ResourceListLayoutGL ) );
    //resource_lists.init( 128, sizeof( ResourceListGL ) );
    //render_passes.init( 256, sizeof( RenderPassGL ) );

    //
    // Init primitive resources
    BufferCreation fullscreen_vb_creation = { BufferType::Vertex, ResourceUsageType::Immutable, 0, nullptr, "Fullscreen_vb" };
    fullscreen_vertex_buffer = create_buffer( fullscreen_vb_creation );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.is_swapchain = true;
    swapchain_pass = create_render_pass( swapchain_pass_creation );

    // Init Dummy resources
    TextureCreation dummy_texture_creation = { nullptr, 1, 1, 1, 1, 0, TextureFormat::R8_UINT, TextureType::Texture2D };
    dummy_texture = create_texture( dummy_texture_creation );

    BufferCreation dummy_constant_buffer_creation = { BufferType::Constant, ResourceUsageType::Immutable, 16, nullptr, "Dummy_cb" };
    dummy_constant_buffer = create_buffer( dummy_constant_buffer_creation );

    queued_command_buffers = (CommandBuffer**)malloc( sizeof( CommandBuffer* ) * 128 );
}

void Device::backend_terminate() {


    free( queued_command_buffers );
    destroy_buffer( fullscreen_vertex_buffer );
    destroy_render_pass( swapchain_pass );
    destroy_texture( dummy_texture );
    destroy_buffer( dummy_constant_buffer );

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
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkDestroyDebugReportCallbackEXT" );
    vkDestroyDebugReportCallbackEXT( vulkan_instance, vulkan_debug_callback, vulkan_allocation_callbacks );
#endif // IMGUI_VULKAN_DEBUG_REPORT

    vkDestroyDescriptorPool( vulkan_device, vulkan_descriptor_pool, vulkan_allocation_callbacks );

    vkDestroyDevice( vulkan_device, vulkan_allocation_callbacks );
    vkDestroyInstance( vulkan_instance, vulkan_allocation_callbacks );
}


// Resource Creation ////////////////////////////////////////////////////////////
TextureHandle Device::create_texture( const TextureCreation& creation ) {

    uint32_t resource_index = textures.obtain_resource();
    TextureHandle handle = { resource_index };
    if ( resource_index == k_invalid_handle ) {
        return handle;
    }

    return handle;
}

PipelineHandle Device::create_pipeline( const PipelineCreation& creation ) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }


    return handle;
}

BufferHandle Device::create_buffer( const BufferCreation& creation ) {
    BufferHandle handle = { buffers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }


    return handle;
}

SamplerHandle Device::create_sampler( const SamplerCreation& creation ) {
    SamplerHandle handle = { samplers.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }


    return handle;
}

ResourceListLayoutHandle Device::create_resource_list_layout( const ResourceListLayoutCreation& creation ) {
    ResourceListLayoutHandle handle = { resource_list_layouts.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }


    return handle;
}

ResourceListHandle Device::create_resource_list( const ResourceListCreation& creation ) {
    ResourceListHandle handle = { resource_lists.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }


    return handle;
}

RenderPassHandle Device::create_render_pass( const RenderPassCreation& creation ) {
    RenderPassHandle handle = { render_passes.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    return handle;
}

// Resource Destruction /////////////////////////////////////////////////////////

void Device::destroy_buffer( BufferHandle buffer ) {
    if ( buffer.handle != k_invalid_handle ) {
        //BufferGL* gl_buffer = access_buffer( buffer );
        //if ( gl_buffer ) {
        //    glDeleteBuffers( 1, &gl_buffer->gl_handle );
        //}

        //buffers.release_resource( buffer.handle );
    }
}

void Device::destroy_texture( TextureHandle texture ) {

    if ( texture.handle != k_invalid_handle ) {
        //TextureGL* texture_data = access_texture( texture );
        //if ( texture_data ) {
        //    glDeleteTextures( 1, &texture_data->gl_handle );
        //}
        //textures.release_resource( texture.handle );
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
        //ResourceListLayoutGL* state = access_resource_list_layout( resource_layout );
        //free( state->bindings );
        //resource_list_layouts.release_resource( resource_layout.handle );
    }
}

void Device::destroy_resource_list( ResourceListHandle resource_list ) {
    if ( resource_list.handle != k_invalid_handle ) {
        //ResourceListGL* state = access_resource_list( resource_list );
        //free( state->resources );
        //resource_lists.release_resource( resource_list.handle );
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
        //const BufferGL* buffer_data = access_buffer( buffer );

        //out_description.name = buffer_data->name;
        //out_description.size = buffer_data->size;
        //out_description.type = buffer_data->type;
        //out_description.usage = buffer_data->usage;
        //out_description.native_handle = (void*)&buffer_data->gl_handle;
    }
}

void Device::query_texture( TextureHandle texture, TextureDescription& out_description ) {
    if ( texture.handle != k_invalid_handle ) {
        //const TextureGL* texture_data = access_texture( texture );

        //out_description.width = texture_data->width;
        //out_description.height = texture_data->height;
        //out_description.depth = texture_data->depth;
        //out_description.format = texture_data->format;
        //out_description.mipmaps = texture_data->mipmaps;
        //out_description.type = texture_data->type;
        //out_description.render_target = texture_data->render_target;
        //out_description.native_handle = (void*)&texture_data->gl_handle;
    }
}

void Device::query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description ) {
    if ( pipeline.handle != k_invalid_handle ) {
        //const PipelineGL* pipeline_data = access_pipeline( pipeline );

        //out_description.shader = pipeline_data->shader_state;
    }
}

void Device::query_sampler( SamplerHandle sampler, SamplerDescription& out_description ) {
    if ( sampler.handle != k_invalid_handle ) {
        //const SamplerGL* sampler_data = access_sampler( sampler );
    }
}

void Device::query_resource_list_layout( ResourceListLayoutHandle resource_list_layout, ResourceListLayoutDescription& out_description ) {
    if ( resource_list_layout.handle != k_invalid_handle ) {
        //const ResourceListLayoutGL* resource_list_layout_data = access_resource_list_layout( resource_list_layout );

        //const uint32_t num_bindings = resource_list_layout_data->num_bindings;
        //for ( size_t i = 0; i < num_bindings; i++ ) {
        //    out_description.bindings[i].name = resource_list_layout_data->bindings[i].name;
        //    out_description.bindings[i].type = resource_list_layout_data->bindings[i].type;
        //}

        //out_description.num_active_bindings = resource_list_layout_data->num_bindings;
    }
}

void Device::query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description ) {
    if ( resource_list.handle != k_invalid_handle ) {
        //const ResourceListGL* resource_list_data = access_resource_list( resource_list );
    }
}

// Resource Map/Unmap ///////////////////////////////////////////////////////////

void* Device::map_buffer( const MapBufferParameters& parameters ) {
    //if ( parameters.buffer.handle == k_invalid_handle )
        return nullptr;

    
}

void Device::unmap_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.handle == k_invalid_handle )
        return;

    
}

// Other methods ////////////////////////////////////////////////////////////////
void Device::resize_output_textures( RenderPassHandle render_pass, uint16_t width, uint16_t height ) {

}

void Device::queue_command_buffer( CommandBuffer* command_buffer ) {

    queued_command_buffers[num_queued_command_buffers++] = command_buffer;
}

void Device::present() {
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

VkSurfaceFormatKHR choose_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space )
{
    HYDRA_ASSERT( request_formats != NULL, "Format array cannot be null!" );
    HYDRA_ASSERT( request_formats_count > 0, "Format count cannot be 0!" );

    // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
    // Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
    // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
    // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
    uint32_t available_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &available_count, NULL );
    VkSurfaceFormatKHR* available_formats = (VkSurfaceFormatKHR*)malloc( sizeof( VkSurfaceFormatKHR ) * available_count );
    vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &available_count, available_formats );

    // TODO: memory leak!

    // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
    if ( available_count == 1 ) {
        if ( available_formats[0].format == VK_FORMAT_UNDEFINED ) {
            VkSurfaceFormatKHR ret;
            ret.format = request_formats[0];
            ret.colorSpace = request_color_space;
            return ret;
        }
        else {
            // No point in searching another format
            return available_formats[0];
        }
    }
    else {
        // Request several formats, the first found will be used
        for ( int request_i = 0; request_i < request_formats_count; request_i++ ) {
            for ( uint32_t avail_i = 0; avail_i < available_count; avail_i++ ) {
                if ( available_formats[avail_i].format == request_formats[request_i] && available_formats[avail_i].colorSpace == request_color_space )
                    return available_formats[avail_i];
            }
        }

        // If none of the requested image formats could be found, use the first available
        return available_formats[0];
    }
}

VkPresentModeKHR choose_present_mode( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count )
{
    HYDRA_ASSERT( request_modes != NULL, "Requested present mode array cannot be null!" );
    HYDRA_ASSERT( request_modes_count > 0, "Requested mode count cannot be 0!" );

    // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
    uint32_t available_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &available_count, NULL );
    VkPresentModeKHR* available_modes = (VkPresentModeKHR*)malloc( sizeof( VkPresentModeKHR ) * available_count );
    vkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &available_count, available_modes );
    //for (uint32_t avail_i = 0; avail_i < available_count; avail_i++)
    //    printf("[vulkan] avail_modes[%d] = %d\n", avail_i, avail_modes[avail_i]);

    for ( int request_i = 0; request_i < request_modes_count; request_i++ ) {
        for ( uint32_t avail_i = 0; avail_i < available_count; avail_i++ ) {
            if ( request_modes[request_i] == available_modes[avail_i] )
                return request_modes[request_i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // Always available
}
#else
static_assert(false, "No platform was selected!");

#endif // HYDRA_VULKAN


} // namespace gfx_device
} // namespace hydra
