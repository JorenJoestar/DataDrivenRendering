/////////////////////////////////////////////////////////////////////////////////
//  
//  Hydra Graphics: 3D API wrapper around Vulkan/Direct3D12/OpenGL.
//
//  Mostly based on the amazing Sokol library (https://github.com/floooh/sokol),
//  but with a different target (wrapping Vulkan/Direct3D12).
//
//  Source code     : https://www.github.com/jorenjoestar/HydraGraphics
//
//  Date            : 22/05/2019, 18.50
//  Version         : 0.020
//
/////////////////////////////////////////////////////////////////////////////////

#include "hydra_graphics.h"

#include <string.h>
#include <malloc.h>

#if defined (HYDRA_SDL)
#include <SDL.h>
#include <SDL_vulkan.h>
#endif // HYDRA_SDL

/////////////////////////////////////////////////////////////////////////////////
// Defines
/////////////////////////////////////////////////////////////////////////////////

// Use shared libraries to enhance different functions.
#define HYDRA_LIB
#if defined(HYDRA_LIB)
#include "hydra_lib.h"

#define HYDRA_LOG                                           hydra::PrintFormat

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

struct StringBufferGfx {

    void init( uint32_t size ) {
        if ( data ) {
            free( data );
        }

        data = (char*)malloc( size );
        buffer_size = size;
        current_size = 0;
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

static const uint32_t               k_invalid_handle = 0xffffffff;

/////////////////////////////////////////////////////////////////////////////////
// Resource Pool
/////////////////////////////////////////////////////////////////////////////////
void ResourcePool::init( uint32_t pool_size, uint32_t resource_size ) {

    this->size = pool_size;
    this->resource_size = resource_size;

    memory = (uint8_t*)malloc( pool_size * resource_size );

    // Allocate and add free indices
    free_indices = (uint32_t*)malloc( pool_size * sizeof( uint32_t ) );
    free_indices_head = 0;

    for ( uint32_t i = 0; i < pool_size; ++i ) {
        free_indices[i] = i;
    }
}

void ResourcePool::terminate() {

    free( memory );
    free( free_indices );
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


/////////////////////////////////////////////////////////////////////////////////
// Device
/////////////////////////////////////////////////////////////////////////////////
HYDRA_STRINGBUFFER s_string_buffer;

void Device::init( const DeviceCreation& creation ) {
    // 1. Perform common code
    s_string_buffer.init( 1024 * 10 );

    // 2. Perform backend specific code
    backend_init( creation );

    //HYDRA_ASSERT( shaders, "Device: shader memory should be initialized!" );
}

void Device::terminate() {
}

CommandBuffer* Device::get_command_buffer( QueueType::Enum type, uint32_t size ) {
    CommandBuffer* commands = new CommandBuffer();
    commands->init( type, size );

    return commands;
}

BufferHandle Device::get_fullscreen_vertex_buffer() const {
    return fullscreen_vertex_buffer;
}


/////////////////////////////////////////////////////////////////////////////////
// CommandBuffer
/////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::init( QueueType::Enum type, uint32_t size ) {
    this->type = type;
    this->buffer_size = size;

    data = (uint8_t*)malloc( size );
    read_offset = write_offset = 0;
}

void CommandBuffer::bind_pipeline( PipelineHandle handle ) {
    commands::BindPipeline* bind = write_command<commands::BindPipeline>();
    bind->handle = handle;
}

void CommandBuffer::bind_vertex_buffer( BufferHandle handle ) {
    commands::BindVertexBuffer* bind = write_command< commands::BindVertexBuffer>();
    bind->buffer = handle;
}

void CommandBuffer::bind_resource_set( ResourceSetHandle handle ) {
    commands::BindResourceSet* bind = write_command<commands::BindResourceSet>();
    bind->handle = handle;
}

void CommandBuffer::draw( TopologyType::Enum topology, uint32_t start, uint32_t count ) {
    commands::Draw* draw_command = write_command<commands::Draw>();

    draw_command->topology = topology;
    draw_command->first_vertex = start;
    draw_command->vertex_count = count;
}

void CommandBuffer::dispatch( uint8_t group_x, uint8_t group_y, uint8_t group_z ) {
    commands::Dispatch* command = write_command<commands::Dispatch>();
    command->group_x = group_x;
    command->group_y = group_y;
    command->group_z = group_z;
}


/////////////////////////////////////////////////////////////////////////////////
// API Specific
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// OpenGL
/////////////////////////////////////////////////////////////////////////////////
#if defined (HYDRA_OPENGL)

// Defines
// TODO: until enums are not exported anymore, add + 1, max is not count!

// Enum translations. Use tables or switches depending on the case.
static GLuint translate_gl_target( TextureType::Enum type ) {
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
    static GLuint s_gl_buffer_types[BufferType::Count] = { GL_ARRAY_BUFFER, GL_INDEX_ARRAY, GL_UNIFORM_BUFFER, GL_DRAW_INDIRECT_BUFFER };
    return s_gl_buffer_types[type];
}

//
//
//
static GLuint to_gl_buffer_usage( ResourceUsageType::Enum type ) {
    static GLuint s_gl_buffer_types[ResourceUsageType::Count] = { GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_DYNAMIC_DRAW };
    return s_gl_buffer_types[type];
}


// Structs
//
//
struct ShaderStateGL : public ShaderState {

    GLuint                          gl_program          = 0;

}; // struct ShaderState

//
//
struct TextureGL : public Texture {

    GLuint                          gl_handle           = 0;
    GLuint                          gl_target           = 0;

}; // struct TextureGL

//
//
struct BufferGL : public Buffer {

    GLuint                          gl_handle           = 0;
    GLuint                          gl_type             = 0;
    GLuint                          gl_usage            = 0;

}; // struct BufferGL

//
//
struct PipelineGL : public Pipeline {

    GLuint                          gl_program_cached   = 0;

    const ResourceSetLayoutGL*      resource_set_layout = nullptr;
    // Blend state
    // Depth state
    // Rasterization state

}; // struct PipelineGL

//
//
struct SamplerGL : public Sampler {

}; // struct SamplerGL


//
//
struct ResourceBindingGL : public ResourceBinding {

    GLuint                          gl_block_index      = 0;
    GLint                           gl_block_binding    = 0;
};

//
//
struct ResourceSetLayoutGL : public ResourceSetLayout {

    ResourceBindingGL*              bindings            = nullptr;
    uint32_t                        num_bindings        = 0;

}; // struct ResourceSetLayoutGL

//
//
struct ResourceSetGL : public ResourceSet {

    const ResourceSetLayoutGL*      layout              = nullptr;

    void                            set() const {

        if ( layout == nullptr ) {
            return;
        }

        // TODO: this is the first version. Just sets textures.
        for ( uint32_t r = 0; r < num_resources; ++r ) {
            const ResourceBindingGL& binding = layout->bindings[r];

            if ( binding.gl_block_binding == -1 )
                continue;

            const uint32_t resource_type = binding.type;
            ResourceHandle handle = (ResourceHandle)resources[r].data;

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

}; // struct ResourceSetGL

// Methods

// Forward declarations
static GLuint                       compile_shader( GLuint stage, const char* source );
static bool                         get_compile_info( GLuint shader, GLuint status );
static bool                         get_link_info( GLuint shader, GLuint status );

static void                         cache_resource_bindings( GLuint shader, const ResourceSetLayoutGL* resource_set_layout );

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
    resource_layouts.init( 128, sizeof( ResourceSetLayoutGL ) );
    resource_sets.init( 128, sizeof( ResourceSetGL ) );

    // During init, enable debug output
    glEnable( GL_DEBUG_OUTPUT );
    glDebugMessageCallback( gl_message_callback, 0 );

#if defined (HYDRA_GRAPHICS_TEST)
    test_texture_creation( *this );
    test_pool( *this );
    test_command_buffer( *this );
#endif // HYDRA_GRAPHICS_TEST

    //
    // Init primitive resources
    // Fullscreen
    fullscreen_vertex_buffer.handle = buffers.obtain_resource();
    if ( fullscreen_vertex_buffer.handle == k_invalid_handle ) {
        HYDRA_ASSERT( false, "Error in creation of 1 buffer. Quitting" );
    }

    BufferGL* fullscree_vb = access_buffer( fullscreen_vertex_buffer );
    glGenVertexArrays( 1, &fullscreen_vertex_buffer.handle );
}

void Device::backend_terminate() {
    glDisable( GL_DEBUG_OUTPUT );

    pipelines.terminate();
    buffers.terminate();
    shaders.terminate();
    textures.terminate();
    samplers.terminate();
    resource_layouts.terminate();
    resource_sets.terminate();
}

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

ResourceSetLayoutGL* Device::access_resource_set_layout( ResourceSetLayoutHandle resource_layout ) {
    return (ResourceSetLayoutGL*)resource_layouts.access_resource( resource_layout.handle );
}

const ResourceSetLayoutGL* Device::access_resource_set_layout( ResourceSetLayoutHandle resource_layout ) const {
    return (const ResourceSetLayoutGL*)resource_layouts.access_resource( resource_layout.handle );
}

ResourceSetGL* Device::access_resource_set( ResourceSetHandle resource_set ) {
    return (ResourceSetGL*)resource_sets.access_resource( resource_set.handle );
}

const ResourceSetGL* Device::access_resource_set( ResourceSetHandle resource_set ) const {
    return (const ResourceSetGL*)resource_sets.access_resource( resource_set.handle );
}

TextureHandle Device::create_texture( const TextureCreation& creation ) {

    uint32_t resource_index = textures.obtain_resource();
    TextureHandle handle = { resource_index };
    if ( resource_index == k_invalid_handle ) {
        return handle;
    }

    GLuint gl_handle;
    glGenTextures( 1, &gl_handle );
    const GLuint gl_target = translate_gl_target(creation.type);

    glBindTexture( gl_target, gl_handle );

    // For some unknown reasons, not setting any parameter results in an unusable texture.
    glTexParameteri( gl_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( gl_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    const GLuint gl_internal_format = to_gl_internal_format(creation.format);
    const GLuint gl_format = to_gl_format(creation.format);
    const GLuint gl_type = to_gl_format_type(creation.format);

    switch ( creation.type ) {
        case TextureType::Texture2D:
        {
            GLint level = 0;
            GLint border = 0;
            glTexImage2D( gl_target, level, gl_internal_format, creation.width, creation.height, border, gl_format, gl_type, nullptr );
            break;
        }

        default:
        {
            break;
        }
    }

    GLenum gl_error = glGetError();
    if ( gl_error != 0 ) {
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
    // Link and 
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
    ShaderStateGL* shader_state = access_shader( creation.shader_state );

    pipeline->shader_state = creation.shader_state;
    pipeline->gl_program_cached = shader_state->gl_program;
    pipeline->resource_set_layout = access_resource_set_layout( creation.resource_layout );

    cache_resource_bindings( pipeline->gl_program_cached, pipeline->resource_set_layout );

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

    glGenBuffers( 1, &buffer->gl_handle );
    buffer->gl_type = to_gl_buffer_type( creation.type );
    buffer->gl_usage = to_gl_buffer_usage( creation.usage );

    glBindBuffer( buffer->gl_type, buffer->gl_handle );
    glBufferData( buffer->gl_type, buffer->size, creation.initial_data, buffer->gl_usage );
    glBindBuffer( buffer->gl_type, 0 );

    switch ( creation.type ) {
        case BufferType::Constant:
        {
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

ResourceSetLayoutHandle Device::create_resource_set_layout( const ResourceSetLayoutCreation& creation ) {
    ResourceSetLayoutHandle handle = { resource_layouts.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceSetLayoutGL* resource_layout = access_resource_set_layout( handle );

    // TODO: add support for multiple sets.
    // Create flattened binding list
    resource_layout->num_bindings = creation.num_bindings;
    resource_layout->bindings = (ResourceBindingGL*)malloc( sizeof( ResourceBindingGL ) * creation.num_bindings );

    for ( uint32_t r = 0; r < creation.num_bindings; ++r ) {
        ResourceBindingGL& binding = resource_layout->bindings[r];
        binding.start = r;
        binding.count = 1;
        binding.type = creation.bindings[r].type;
        binding.name = creation.bindings[r].name;
    }

    return handle;
}

ResourceSetHandle Device::create_resource_set( const ResourceSetCreation& creation ) {
    ResourceSetHandle handle = { resource_sets.obtain_resource() };
    if ( handle.handle == k_invalid_handle ) {
        return handle;
    }

    ResourceSetGL* resources = access_resource_set( handle );
    resources->layout = access_resource_set_layout( creation.layout );

    resources->resources = (ResourceSet::Resource*)malloc( sizeof( ResourceSet::Resource ) * creation.num_resources );
    resources->num_resources = creation.num_resources;
    
    // Set all resources
    for ( uint32_t r = 0; r < creation.num_resources; ++r ) {
        ResourceSet::Resource& resource = resources->resources[r];

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
        }
    }

    return handle;
}

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

void Device::destroy_resource_layout( ResourceSetLayoutHandle resource_layout ) {
    if ( resource_layout.handle != k_invalid_handle ) {

        resource_layouts.release_resource( resource_layout.handle );
    }
}

void Device::destroy_resource_set( ResourceSetHandle resource_set ) {
    if ( resource_set.handle != k_invalid_handle ) {

        resource_sets.release_resource( resource_set.handle );
    }
}

const Buffer* Device::query_buffer( BufferHandle buffer ) {
    if ( buffer.handle != k_invalid_handle ) {
        const Buffer* buffer_data = access_buffer( buffer );
        return buffer_data;
    }
    return nullptr;
}

const Texture* Device::query_texture( TextureHandle texture ) {
    if ( texture.handle != k_invalid_handle ) {
        const Texture* texture_data = access_texture( texture );
        return texture_data;
    }
    return nullptr;
}

const ShaderState* Device::query_shader( ShaderHandle shader ) {
    if ( shader.handle != k_invalid_handle ) {
        const ShaderState* shader_state = access_shader( shader );
        return shader_state;
    }
    return nullptr;
}

const Pipeline* Device::query_pipeline( PipelineHandle pipeline ) {
    if ( pipeline.handle != k_invalid_handle ) {
        const Pipeline* pipeline_data = access_pipeline( pipeline );
        return pipeline_data;
    }
    return nullptr;
}

const Sampler* Device::query_sampler( SamplerHandle sampler ) {
    if ( sampler.handle != k_invalid_handle ) {
        const Sampler* sampler_data = access_sampler( sampler );
        return sampler_data;
    }
    return nullptr;
}

const ResourceSetLayout* Device::query_resource_set_layout( ResourceSetLayoutHandle resource_layout ) {
    if ( resource_layout.handle != k_invalid_handle ) {
        const ResourceSetLayout* resource_set_layout = access_resource_set_layout( resource_layout );
        return resource_set_layout;
    }
    return nullptr;
}

const ResourceSet* Device::query_resource_set( ResourceSetHandle resource_set ) {
    if ( resource_set.handle != k_invalid_handle ) {
        const ResourceSet* resource_set_data = access_resource_set( resource_set );
        return resource_set_data;
    }
    return nullptr;
}

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

void Device::execute_command_buffer( CommandBuffer* command_buffer ) {

    while ( !command_buffer->end_of_stream() ) {
        CommandType::Enum command_type = command_buffer->get_command_type();

        switch ( command_type ) {
            case CommandType::BindVertexBuffer:
            {
                const commands::BindVertexBuffer& binding = command_buffer->read_command<commands::BindVertexBuffer>();
                glBindVertexArray( binding.buffer.handle );
                break;
            }

            case CommandType::BindPipeline:
            {
                const commands::BindPipeline& binding = command_buffer->read_command<commands::BindPipeline>();
                const PipelineGL* pipeline = access_pipeline( binding.handle );
                glUseProgram( pipeline->gl_program_cached );

                break;
            }

            case CommandType::Dispatch:
            {
                const commands::Dispatch& dispatch = command_buffer->read_command<commands::Dispatch>();
                glDispatchCompute( dispatch.group_x, dispatch.group_y, dispatch.group_z );

                // TODO: barrier handling.
                glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

                break;
            }

            case CommandType::BindResourceSet:
            {
                const commands::BindResourceSet& binding = command_buffer->read_command<commands::BindResourceSet>();
                const ResourceSetGL* resource_set = access_resource_set( binding.handle );

                resource_set->set();

                break;
            }

            case CommandType::Draw:
            {
                const commands::Draw& draw = command_buffer->read_command<commands::Draw>();
                glDrawArrays( GL_TRIANGLES, draw.first_vertex, draw.vertex_count );
                
                break;
            }

            default:
            {
                break;
            }
        }
    }

    // Once read it can be reset to the beginning for next usage.
    command_buffer->rewind();
}

void Device::present() {
}

//
// Utility methods
//
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

void GLAPIENTRY gl_message_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam ) {
    HYDRA_LOG( "GL Error: %s type = 0x%x, severity = 0x%x, message = %s\n", (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message );
}

void cache_resource_bindings( GLuint shader, const ResourceSetLayoutGL* resource_set_layout ) {

    for ( uint32_t i = 0; i < resource_set_layout->num_bindings; i++ ) {
        ResourceBindingGL& binding = resource_set_layout->bindings[i];

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

// Testing
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

    const Texture* t1_info = device.query_texture( t1 );

    device.destroy_texture( t1 );
    device.destroy_texture( t0 );
    device.destroy_texture( t2 );
}

void test_command_buffer( Device& device ) {
    graphics::CommandBuffer* commands = device.get_command_buffer( graphics::QueueType::Graphics, 1024 );

    commands->draw( graphics::TopologyType::Triangle, 0, 3 );

    const graphics::commands::Draw& draw = commands->read_command<graphics::commands::Draw>();
    HYDRA_ASSERT( draw.first_vertex == 0, "First vertex should be 0 instead of %u", draw.first_vertex );
    HYDRA_ASSERT( draw.vertex_count == 3, "Vertex count should be 3 instead of %u", draw.vertex_count );
    HYDRA_ASSERT( draw.topology == TopologyType::Triangle, "Topology should be triangle instead of %s", TopologyType::ToString(draw.topology) );
    HYDRA_ASSERT( draw.type == CommandType::Draw, "Command should be Draw instead of %s", CommandType::ToString(draw.type) );
    HYDRA_ASSERT( draw.size == sizeof( commands::Draw ), "Size should be %u instead of %u", sizeof( commands::Draw ), draw.size );
}


/////////////////////////////////////////////////////////////////////////////////
// Vulkan
/////////////////////////////////////////////////////////////////////////////////
#elif defined (HYDRA_VULKAN)

#define VULKAN_DEBUG_REPORT

#ifdef VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData )
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    HYDRA_LOG( "[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
    return VK_FALSE;
}
#endif // VULKAN_DEBUG_REPORT


static void Check( VkResult result ) {
    if ( result == VK_SUCCESS ) {
        return;
    }

    HYDRA_LOG( "Vulkan error: code(%u)", result );
    if ( result < 0 ) {
        HYDRA_ASSERT( false, "Vulkan error: aborting." );
    }
}

static VkSurfaceFormatKHR ChooseSurfaceFormat( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space );
static VkPresentModeKHR ChoosePresentMode( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count );


void Device::BackendInit( const DeviceCreation& creation ) {
    
    // init Vulkan instance.
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = creation.extensions_count;
    create_info.ppEnabledExtensionNames = creation.extensions;

    VkResult result;
#ifdef VULKAN_DEBUG_REPORT
    // Enabling multiple validation layers grouped as LunarG standard validation
    const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
    const char** extensions_ext = (const char**)malloc( sizeof( const char* ) * (creation.extensions_count + 1) );
    memcpy( extensions_ext, creation.extensions, creation.extensions_count * sizeof( const char* ) );
    extensions_ext[creation.extensions_count] = "VK_EXT_debug_report";
    create_info.enabledExtensionCount = creation.extensions_count + 1;
    create_info.ppEnabledExtensionNames = extensions_ext;

    // Create Vulkan Instance
    result = vkCreateInstance( &create_info, v_allocation_callbacks, &v_instance );
    Check( result );

    free( extensions_ext );

    // Create debug callback
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( v_instance, "vkCreateDebugReportCallbackEXT" );
    HYDRA_ASSERT( vkCreateDebugReportCallbackEXT != NULL, "" );

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = DebugCallback;
    debug_report_ci.pUserData = NULL;
    result = vkCreateDebugReportCallbackEXT( v_instance, &debug_report_ci, v_allocation_callbacks, &v_debug_callback );
    Check( result );
#else
        // Create Vulkan Instance without any debug feature
    result = vkCreateInstance( &create_info, v_allocation_callbacks, &v_instance );
    Check( result );
#endif

    // Choose physical device
    uint32_t gpu_count;
    result = vkEnumeratePhysicalDevices( v_instance, &gpu_count, NULL );
    Check( result );

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc( sizeof( VkPhysicalDevice ) * gpu_count );
    result = vkEnumeratePhysicalDevices( v_instance, &gpu_count, gpus );
    Check( result );

    // TODO: create a 'isDeviceSuitable' method to choose the proper GPU.
    v_physical_device = gpus[0];
    free( gpus );

    //createLogicalDevice
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( v_physical_device, &queue_family_count, nullptr );

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)malloc( sizeof( VkQueueFamilyProperties ) * queue_family_count );
    vkGetPhysicalDeviceQueueFamilyProperties( v_physical_device, &queue_family_count, queue_families );

    uint32_t family_index = 0;
    for ( ; family_index < queue_family_count; ++family_index ) {
        VkQueueFamilyProperties queue_family = queue_families[family_index];
        if ( queue_family.queueCount > 0 && queue_family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) ) {
            //indices.graphicsFamily = i;
            break;
        }

        //VkBool32 presentSupport = false;
        //vkGetPhysicalDeviceSurfaceSupportKHR( v_physical_device, i, _surface, &presentSupport );
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
    result = vkCreateDevice( v_physical_device, &device_create_info, v_allocation_callbacks, &v_device );
    Check( result );
    vkGetDeviceQueue( v_device, family_index, 0, &v_queue );

    v_queue_family = family_index;

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
    result = vkCreateDescriptorPool( v_device, &pool_info, v_allocation_callbacks, &v_descriptor_pool );
    Check( result );

#if defined (HYDRA_SDL)
    // Create surface
    SDL_Window* window = (SDL_Window*)creation.window;
    if ( SDL_Vulkan_CreateSurface( window, v_instance, &v_window_surface ) == 0 ) {
        printf( "Failed to create Vulkan surface.\n" );
        //return 1;
    }

    // Create Framebuffers
    int window_width, window_height;
    SDL_GetWindowSize( window, &window_width, &window_height );

#endif // HYDRA_SDL

    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR( v_physical_device, v_queue_family, v_window_surface, &surface_supported );
    if ( surface_supported != VK_TRUE ) {
        fprintf( stderr, "Error no WSI support on physical device 0\n" );
    }

    // Select Surface Format
    const VkFormat surface_image_formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    v_surface_format = ChooseSurfaceFormat( v_physical_device, v_window_surface, surface_image_formats, ArrayLength( surface_image_formats ), surface_color_space );

    //VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    v_present_mode = ChoosePresentMode( v_physical_device, v_window_surface, present_modes, ArrayLength( present_modes ) );


    //createSurface();
    //createSwapChain();
}

void Device::BackendTerminate() {


#ifdef VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( v_instance, "vkDestroyDebugReportCallbackEXT" );
    vkDestroyDebugReportCallbackEXT( v_instance, v_debug_callback, v_allocation_callbacks );
#endif // IMGUI_VULKAN_DEBUG_REPORT

    vkDestroyDescriptorPool( v_device, v_descriptor_pool, v_allocation_callbacks );

    vkDestroyDevice( v_device, v_allocation_callbacks );
    vkDestroyInstance( v_instance, v_allocation_callbacks );
}

VkSurfaceFormatKHR ChooseSurfaceFormat( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space )
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

VkPresentModeKHR ChoosePresentMode( VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count )
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
