#pragma once

#include "kernel/primitive_types.hpp"

#include "graphics/gpu_enum.hpp"


namespace hydra {

struct Allocator;

namespace gfx {

static const u32                    k_invalid_index = 0xffffffff;

typedef u32                         ResourceHandle;

struct BufferHandle {
    ResourceHandle                  index;
}; // struct BufferHandle

struct TextureHandle {
    ResourceHandle                  index;
}; // struct TextureHandle

struct ShaderStateHandle {
    ResourceHandle                  index;
}; // struct ShaderStateHandle

struct SamplerHandle {
    ResourceHandle                  index;
}; // struct SamplerHandle

struct ResourceLayoutHandle {
    ResourceHandle                  index;
}; // struct ResourceLayoutHandle

struct ResourceListHandle {
    ResourceHandle                  index;
}; // struct ResourceListHandle

struct PipelineHandle {
    ResourceHandle                  index;
}; // struct PipelineHandle

struct RenderPassHandle {
	ResourceHandle                  index;
}; // struct RenderPassHandle

// Invalid handles
static BufferHandle                 k_invalid_buffer    { k_invalid_index };
static TextureHandle                k_invalid_texture   { k_invalid_index };
static ShaderStateHandle            k_invalid_shader    { k_invalid_index };
static SamplerHandle                k_invalid_sampler   { k_invalid_index };
static ResourceLayoutHandle         k_invalid_layout    { k_invalid_index };
static ResourceListHandle           k_invalid_list      { k_invalid_index };
static PipelineHandle               k_invalid_pipeline  { k_invalid_index };
static RenderPassHandle             k_invalid_pass      { k_invalid_index };



// Consts ///////////////////////////////////////////////////////////////////////

static const u8                     k_max_image_outputs = 8;        // Maximum number of images/render_targets/fbo attachments usable.
static const u8                     k_max_resource_layouts = 8;        // Maximum number of layouts in the pipeline.
static const u8                     k_max_shader_stages = 5;        // Maximum simultaneous shader stages. Applicable to all different type of pipelines.
static const u8                     k_max_resources_per_list = 16;       // Maximum list elements for both resource list layout and resource lists.
static const u8                     k_max_vertex_streams = 16;
static const u8                     k_max_vertex_attributes = 16;

static const u32                    k_submit_header_sentinel = 0xfefeb7ba;
static const u32                    k_max_resource_deletions = 64;
static const u32                    k_bindless_count = 1000;

// Resource creation structs ////////////////////////////////////////////////////

//
//
struct Rect2D {
    f32                             x = 0.0f;
    f32                             y = 0.0f;
    f32                             width = 0.0f;
    f32                             height = 0.0f;
}; // struct Rect2D

//
//
struct Rect2DInt {
    i16                             x = 0;
    i16                             y = 0;
    u16                             width = 0;
    u16                             height = 0;
}; // struct Rect2D

//
//
struct Viewport {
    Rect2DInt                       rect;
    f32                             min_depth = 0.0f;
    f32                             max_depth = 0.0f;
}; // struct Viewport

//
//
struct ViewportState {
    u32                             num_viewports = 0;
    u32                             num_scissors = 0;

    Viewport* viewport = nullptr;
    Rect2DInt* scissors = nullptr;
}; // struct ViewportState

//
//
struct StencilOperationState {

    StencilOperation::Enum          fail = StencilOperation::Keep;
    StencilOperation::Enum          pass = StencilOperation::Keep;
    StencilOperation::Enum          depth_fail = StencilOperation::Keep;
    ComparisonFunction::Enum        compare = ComparisonFunction::Always;
    u32                             compare_mask = 0xff;
    u32                             write_mask = 0xff;
    u32                             reference = 0xff;

}; // struct StencilOperationState

//
//
struct DepthStencilCreation {

    StencilOperationState           front;
    StencilOperationState           back;
    ComparisonFunction::Enum        depth_comparison = ComparisonFunction::Always;

    u8                              depth_enable        : 1;
    u8                              depth_write_enable  : 1;
    u8                              stencil_enable      : 1;
    u8                              pad                 : 5;

    // Default constructor
    DepthStencilCreation() : depth_enable( 0 ), depth_write_enable( 0 ), stencil_enable( 0 ) {
    }

    DepthStencilCreation&           set_depth( bool write, ComparisonFunction::Enum comparison_test );

}; // struct DepthStencilCreation

struct BlendState {

    Blend::Enum                     source_color        = Blend::One;
    Blend::Enum                     destination_color   = Blend::One;
    BlendOperation::Enum            color_operation     = BlendOperation::Add;

    Blend::Enum                     source_alpha        = Blend::One;
    Blend::Enum                     destination_alpha   = Blend::One;
    BlendOperation::Enum            alpha_operation     = BlendOperation::Add;

    ColorWriteEnabled::Mask         color_write_mask    = ColorWriteEnabled::All_mask;

    u8                              blend_enabled   : 1;
    u8                              separate_blend  : 1;
    u8                              pad             : 6;


    BlendState() : blend_enabled( 0 ), separate_blend( 0 ) {
    }

    BlendState&                     set_color( Blend::Enum source_color, Blend::Enum destination_color, BlendOperation::Enum color_operation );
    BlendState&                     set_alpha( Blend::Enum source_color, Blend::Enum destination_color, BlendOperation::Enum color_operation );
    BlendState&                     set_color_write_mask( ColorWriteEnabled::Mask value );

}; // struct BlendState

struct BlendStateCreation {

    BlendState                      blend_states[ k_max_image_outputs ];
    u32                             active_states = 0;

    BlendStateCreation&             reset();
    BlendState&                     add_blend_state();

}; // BlendStateCreation

//
//
struct RasterizationCreation {

    CullMode::Enum                  cull_mode = CullMode::None;
    FrontClockwise::Enum            front = FrontClockwise::False;
    FillMode::Enum                  fill = FillMode::Solid;
}; // struct RasterizationCreation

//
//
struct BufferCreation {

    BufferType::Mask                type    = BufferType::Vertex_mask;
    ResourceUsageType::Enum         usage   = ResourceUsageType::Immutable;
    u32                             size    = 0;
    void*                           initial_data = nullptr;

    const char*                     name    = nullptr;
    BufferHandle                    parent_buffer = k_invalid_buffer;

    BufferCreation&                 set( BufferType::Mask type, ResourceUsageType::Enum usage, u32 size );
    BufferCreation&                 set_data( void* data );
    BufferCreation&                 set_name( const char* name );

}; // struct BufferCreation

//
//
struct TextureCreation {

    void*                           initial_data    = nullptr;
    u16                             width           = 1;
    u16                             height          = 1;
    u16                             depth           = 1;
    u8                              mipmaps         = 1;
    u8                              flags           = 0;    // TextureCreationFlags bitmasks

    TextureFormat::Enum             format          = TextureFormat::UNKNOWN;
    TextureType::Enum               type            = TextureType::Texture2D;

    const char*                     name            = nullptr;

    TextureCreation&                set_size( u16 width, u16 height, u16 depth );
    TextureCreation&                set_flags( u8 mipmaps, u8 flags );
    TextureCreation&                set_format_type( TextureFormat::Enum format, TextureType::Enum type );
    TextureCreation&                set_name( const char* name );
    TextureCreation&                set_data( void* data );

}; // struct TextureCreation

//
//
struct SamplerCreation {

    TextureFilter::Enum             min_filter = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w = TextureAddressMode::Repeat;

    const char*                     name = nullptr;

    SamplerCreation&                set_min_mag_mip( TextureFilter::Enum min, TextureFilter::Enum mag, TextureMipFilter::Enum mip );
    SamplerCreation&                set_address_mode_u( TextureAddressMode::Enum u );
    SamplerCreation&                set_address_mode_uv( TextureAddressMode::Enum u, TextureAddressMode::Enum v );
    SamplerCreation&                set_address_mode_uvw( TextureAddressMode::Enum u, TextureAddressMode::Enum v, TextureAddressMode::Enum w );
    SamplerCreation&                set_name( const char* name );

}; // struct SamplerCreation

//
//
struct ShaderStateCreation {

    struct Stage {

        const char*                 code        = nullptr;
        u32                         code_size   = 0;
        ShaderStage::Enum           type        = ShaderStage::Compute;

    }; // struct Stage

    Stage                           stages[ k_max_shader_stages ];

    const char*                     name            = nullptr;

    u32                             stages_count    = 0;
    u32                             spv_input       = 0;

    // Building helpers
    ShaderStateCreation&            reset();
    ShaderStateCreation&            set_name( const char* name );
    ShaderStateCreation&            add_stage( const char* code, u32 code_size, ShaderStage::Enum type );
    ShaderStateCreation&            set_spv_input( bool value );

}; // struct ShaderStateCreation

//
// 
struct ResourceLayoutCreation {

    //
    // A single resource binding. It can be relative to one or more resources of the same type.
    //
    struct Binding {

        ResourceType::Enum          type    = ResourceType::Constants;
        u16                         start   = 0;
        u16                         count   = 0;
        cstring                     name    = nullptr;  // Comes from external memory.
    }; // struct Binding

    Binding                         bindings[ k_max_resources_per_list ];
    u32                             num_bindings = 0;

    const char*                     name = nullptr;

    // Building helpers
    ResourceLayoutCreation&         reset();
    ResourceLayoutCreation&         add_binding( const Binding& binding );
    ResourceLayoutCreation&         set_name( const char* name );

}; // struct ResourceLayoutCreation

//
//
struct ResourceListCreation {

    ResourceHandle                  resources[ k_max_resources_per_list ];
    SamplerHandle                   samplers[ k_max_resources_per_list ];
    u16                             bindings[ k_max_resources_per_list ];

    ResourceLayoutHandle            layout;
    u32                             num_resources   = 0;

    const char*                     name            = nullptr;

    // Building helpers
    ResourceListCreation&           reset();
    ResourceListCreation&           set_layout( ResourceLayoutHandle layout );
    ResourceListCreation&           texture( TextureHandle texture, u16 binding );
    ResourceListCreation&           buffer( BufferHandle buffer, u16 binding );
    ResourceListCreation&           texture_sampler( TextureHandle texture, SamplerHandle sampler, u16 binding );   // TODO: separate samplers from textures
    ResourceListCreation&           set_name( const char* name );

}; // struct ResourceListCreation

//
//
struct ResourceListUpdate {
    ResourceListCreation            creation;
    ResourceListHandle              resource_list;

    u32                             frame_issued = 0;

    // Building helpers
    ResourceListUpdate&             reset();
    ResourceListUpdate&             set_resource_list( ResourceListHandle handle );
    ResourceListUpdate&             texture( TextureHandle texture, u16 binding );
    ResourceListUpdate&             buffer( BufferHandle buffer, u16 binding );
    ResourceListUpdate&             texture_sampler( TextureHandle texture, SamplerHandle sampler, u16 binding );

}; // ResourceListUpdate

//
//
struct VertexAttribute {

    u16                             location = 0;
    u16                             binding = 0;
    u32                             offset = 0;
    VertexComponentFormat::Enum     format = VertexComponentFormat::Count;

}; // struct VertexAttribute

//
//
struct VertexStream {

    u16                             binding = 0;
    u16                             stride = 0;
    VertexInputRate::Enum           input_rate = VertexInputRate::Count;

}; // struct VertexStream

//
//
struct VertexInputCreation {

    u32                             num_vertex_streams = 0;
    u32                             num_vertex_attributes = 0;

    VertexStream                    vertex_streams[ k_max_vertex_streams ];
    VertexAttribute                 vertex_attributes[ k_max_vertex_attributes ];

    VertexInputCreation&            reset();
    VertexInputCreation&            add_vertex_stream( const VertexStream& stream );
    VertexInputCreation&            add_vertex_attribute( const VertexAttribute& attribute );
}; // struct VertexInputCreation

//
//
struct RenderPassOutput {

    TextureFormat::Enum             color_formats[ k_max_image_outputs ];
    TextureFormat::Enum             depth_stencil_format;
    u32                             num_color_formats;

    RenderPassOperation::Enum       color_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       depth_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       stencil_operation       = RenderPassOperation::DontCare;

    RenderPassOutput&               reset();
    RenderPassOutput&               color( TextureFormat::Enum format );
    RenderPassOutput&               depth( TextureFormat::Enum format );
    RenderPassOutput&               set_operations( RenderPassOperation::Enum color, RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil );

}; // struct RenderPassOutput

//
//
struct RenderPassCreation {

    u16                             num_render_targets  = 0;
    RenderPassType::Enum            type                = RenderPassType::Geometry;

    TextureHandle                   output_textures[ k_max_image_outputs ];
    TextureHandle                   depth_stencil_texture;

    f32                             scale_x             = 1.f;
    f32                             scale_y             = 1.f;
    u8                              resize              = 1;

    RenderPassOperation::Enum       color_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       depth_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       stencil_operation       = RenderPassOperation::DontCare;

    const char*                     name                = nullptr;

    RenderPassCreation&             reset();
    RenderPassCreation&             add_render_texture( TextureHandle texture );
    RenderPassCreation&             set_scaling( f32 scale_x, f32 scale_y, u8 resize );
    RenderPassCreation&             set_depth_stencil_texture( TextureHandle texture );
    RenderPassCreation&             set_name( const char* name );
    RenderPassCreation&             set_type( RenderPassType::Enum type );
    RenderPassCreation&             set_operations( RenderPassOperation::Enum color, RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil );

}; // struct RenderPassCreation

//
//
struct PipelineCreation {

    RasterizationCreation           rasterization;
    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    VertexInputCreation             vertex_input;
    ShaderStateCreation             shaders;

    RenderPassOutput                render_pass;
    ResourceLayoutHandle            resource_layout[ k_max_resource_layouts ];
    const ViewportState*            viewport = nullptr;

    u32                             num_active_layouts = 0;

    const char*                     name = nullptr;

    PipelineCreation&               add_resource_layout( ResourceLayoutHandle handle );
    RenderPassOutput&               render_pass_output();

}; // struct PipelineCreation

// API-agnostic structs /////////////////////////////////////////////////////////

//
// Helper methods for texture formats
//
namespace TextureFormat {
                                                                        // D32_FLOAT_S8X24_UINT, D24_UNORM_S8_UINT, D32_FLOAT, D24_UNORM_X8_UINT, D16_UNORM, S8_UINT
    inline bool                     is_depth_stencil( Enum value ) {
        return value == D32_FLOAT_S8X24_UINT || value == D24_UNORM_S8_UINT;
    }
    inline bool                     is_depth_only( Enum value ) {
        return value >= D32_FLOAT && value < S8_UINT;
    }
    inline bool                     is_stencil_only( Enum value ) {
        return value == S8_UINT;
    }

    inline bool                     has_depth( Enum value ) {
        return value >= D32_FLOAT_S8X24_UINT && value < S8_UINT;
    }
    inline bool                     has_stencil( Enum value ) {
        return value == D32_FLOAT_S8X24_UINT || value == D24_UNORM_S8_UINT || value == S8_UINT;
    }
    inline bool                     has_depth_or_stencil( Enum value ) {
        return value >= D32_FLOAT_S8X24_UINT && value <= S8_UINT;
    }

} // namespace TextureFormat

struct ResourceData {

    void* data = nullptr;

}; // struct ResourceData

//
//
struct ResourceBinding {
    u16                             type = 0;    // ResourceType
    u16                             start = 0;
    u16                             count = 0;
    u16                             set = 0;

    const char*                     name = nullptr;
}; // struct ResourceBinding


// API-agnostic descriptions ////////////////////////////////////////////////////

//
//
struct ShaderStateDescription {

    void*                           native_handle = nullptr;
    const char*                     name        = nullptr;

}; // struct ShaderStateDescription

//
//
struct BufferDescription {

    void*                           native_handle = nullptr;

    BufferType::Mask                type        = BufferType::Vertex_mask;
    ResourceUsageType::Enum         usage       = ResourceUsageType::Immutable;
    u32                             size        = 0;
    BufferHandle                    parent_handle;

    const char*                     name        = nullptr;

}; // struct BufferDescription

//
//
struct TextureDescription {

    void* native_handle = nullptr;
    const char* name = nullptr;
    u16                             width = 1;
    u16                             height = 1;
    u16                             depth = 1;
    u8                              mipmaps = 1;
    u8                              render_target = 0;

    TextureFormat::Enum             format = TextureFormat::UNKNOWN;
    TextureType::Enum               type = TextureType::Texture2D;

}; // struct Texture

//
//
struct SamplerDescription {

    const char* name = nullptr;

    TextureFilter::Enum             min_filter = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w = TextureAddressMode::Repeat;

}; // struct Sampler

//
//
struct ResourceLayoutDescription {

    ResourceBinding                 bindings[ k_max_resources_per_list ];
    u32                             num_active_bindings = 0;

}; // struct ResourceLayoutDescription

//
//
struct ResourceListDescription {

    ResourceData                    resources[ k_max_resources_per_list ];
    u32                             num_active_resources = 0;

}; // struct ResourceListDescription

//
//
struct PipelineDescription {

    ShaderStateHandle               shader;

}; // struct PipelineDescription


// API-agnostic resource modifications //////////////////////////////////////////

struct MapBufferParameters {
    BufferHandle                    buffer;
    u32                             offset = 0;
    u32                             size = 0;

}; // struct MapBufferParameters

// Synchronization //////////////////////////////////////////////////////////////

//
//
struct ImageBarrier {

    TextureHandle                   texture;

}; // struct ImageBarrier

//
//
struct MemoryBarrier {

    BufferHandle                    buffer;

}; // struct MemoryBarrier

//
//
struct ExecutionBarrier {

    PipelineStage::Enum             source_pipeline_stage;
    PipelineStage::Enum             destination_pipeline_stage;

    u32                             num_image_barriers;
    u32                             num_memory_barriers;

    ImageBarrier                    image_barriers[ 8 ];
    MemoryBarrier                   memory_barriers[ 8 ];

    ExecutionBarrier&               reset();
    ExecutionBarrier&               set( PipelineStage::Enum source, PipelineStage::Enum destination );
    ExecutionBarrier&               add_image_barrier( const ImageBarrier& image_barrier );
    ExecutionBarrier&               add_memory_barrier( const MemoryBarrier& memory_barrier );

}; // struct Barrier

//
//
struct ResourceUpdate {

    ResourceDeletionType::Enum      type;
    ResourceHandle                  handle;
    u32                             current_frame;
}; // struct ResourceUpdate

// API-gnostic resources ////////////////////////////////////////////////////////
/*

#if defined (HYDRA_OPENGL)

struct ShaderStateGL;
struct TextureGL;
struct BufferGL;
struct PipelineGL;
struct SamplerGL;
struct ResourceLayoutGL;
struct ResourceListGL;
struct RenderPassGL;

struct DeviceStateGL;

#define ShaderStateAPIGnostic ShaderStateGL
#define TextureAPIGnostic TextureGL
#define BufferAPIGnostic BufferGL
#define PipelineAPIGnostic PipelineGL
#define SamplerAPIGnostic SamplerGL
#define ResourceLayoutAPIGnostic ResourceLayoutGL
#define ResourceListAPIGnostic ResourceListGL
#define RenderPassAPIGnostic RenderPassGL

#elif defined (HYDRA_VULKAN)*/

static const u32            k_max_swapchain_images = 3;

struct ShaderStateVulkan;
struct TextureVulkan;
struct BufferVulkan;
struct PipelineVulkan;
struct SamplerVulkan;
struct ResourceLayoutVulkan;
struct ResourceListVulkan;
struct RenderPassVulkan;

struct DeviceStateVulkan;

#define ShaderStateAPIGnostic       ShaderStateVulkan
#define TextureAPIGnostic           TextureVulkan
#define BufferAPIGnostic            BufferVulkan
#define PipelineAPIGnostic          PipelineVulkan
#define SamplerAPIGnostic           SamplerVulkan
#define ResourceLayoutAPIGnostic    ResourceLayoutVulkan
#define ResourceListAPIGnostic      ResourceListVulkan
#define RenderPassAPIGnostic        RenderPassVulkan


} // namespace gfx
} // namespace hydra

