#pragma once

//
//  Hydra Rendering - v0.33
//
//  High level rendering implementation based on Hydra Graphics library.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/12/11, 18.18
//
//
// Revision history //////////////////////
//
//      0.33 (2021/03/22): + Fixed buffer mapping bug when size is not passed as parameter and defaults to 0.
//      0.32 (2021/01/19): + Removed all temporary high level rendering structs.
//      0.31 (2021/01/18): + Implemented global dynamic uniform buffer.
//      0.30 (2021/01/17): + Implemented update of resource list without need to list all items.
//      0.29 (2021/01/26): + Added RenderPassOutput to RenderStages as aid for Pipeline creation.
//      0.28 (2021/01/23): + Finished new Renderer interface and new structs.
//      0.27 (2021/01/22): + Rewrote Renderer, Shader, Material and Stage structs.
//      0.26 (2021/01/20): + Added TextureAtlas struct.
//      0.25 (2021/01/17): + Improved texture struct and added init.
//      0.24 (2021/01/01): + Added RenderManager2 to RenderStage2 for custom rendering.
//      0.23 (2020/12/30): + Added new implementation of RenderStage, Material and ShaderEffect.
//      0.22 (2020/12/28): + Reworked Camera class.
//      0.21 (2020/12/27): + Improved GPU Profiler UI.
//      0.20 (2020/12/23): + Added GPU Profiler class.
//      0.19 (2020/12/21): + Added utility method to create a pipeline from HFX file.
//      0.18 (2020/12/18): + Camera is now quaternion based.
//      0.17 (2020/12/16): + Separated camera code from other high level structs.
//      0.16 (2020/09/15): + Update resource list and layouts to new hydra_graphics interface.
//      0.15 (2020/04/14): + Implemented ShaderEffect creation.
//      0.14 (2020/04/12): + Renamed RenderPipeline in RenderFrame. + Added initial Renderer class. + Renamed ShaderInstance to MaterialPass. + Moved load resources method from MaterialPass to Material.
//      0.13 (2020/04/05): + Updated to new Hydra Graphics API. + Added separation with high-level rendering (camera, models, ...).
//      0.12 (2020/02/05): + Added Ray class. + Added Color Uint class. + Added Ray/Box intersection.
//      0.11 (2020/02/04): + Moved all math to CGLM using structs. Removed HandmadeMath.
//      0.10 (2020/02/02): + Fixed lighting + Fixed translation component in view matrix
//      0.09 (2020/01/31): + Added sampler support
//      0.08 (2020/01/30): + Added lighting manager
//      0.07 (2020/01/28): + Fixed cbuffer declarations + Fixed automatic resource usage
//      0.06 (2020/01/18): + Added RenderManager interface. + Added SceneManager to render geometry. + Added clear depth and stencil commands
//                         + Added instanced draw commands.
//      0.05 (2020/01/13): + Added GLTF meshes loading + Added simple scene+render node classes.
//      0.04 (2020/01/11): + Added simple FPS camera. + Added first 2D lines rendering.
//      0.03 (2020/01/09): + Added HandmadeMath library. + Added basic camera class. + Added initial lines.hfx.
//      0.02 (2019/12/17): + Code cleanup. Adding some utility methods to classes.
//      0.01 (2019/12/11): + Initial file writing.

#include "hydra_lib.h"
#include "hydra_graphics.h"
#include "hydra_shaderfx.h"

#define HYDRA_RENDERING_CAMERA
//#define HYDRA_RENDERING_HIGH_LEVEL

#include "cglm/types-struct.h"

namespace hfx {
    struct ShaderEffectFile;
    struct NameToIndex;
} // namespace hfx

namespace hydra {
namespace graphics {

// General methods //////////////////////////////////////////////////////////////

struct RenderFeature;
struct ClearData;
struct Renderer;

void                                pipeline_create( Device& gpu, hfx::ShaderEffectFile& hfx, u32 pass_index, const RenderPassOutput& pass_output, PipelineHandle& out_pipeline, ResourceLayoutHandle* out_layouts, u32 num_layouts );

TextureHandle                       create_texture_from_file( Device& gpu, cstring filename );


//
// Color class that embeds color in a uint32.
//
struct ColorUint {
    uint32_t                        abgr;

    void                            set( float r, float g, float b, float a )               { abgr = uint8_t( r * 255.f ) | (uint8_t( g * 255.f ) << 8) | (uint8_t( b * 255.f ) << 16) | (uint8_t( a * 255.f ) << 24); }

    static uint32_t                 from_u8( uint8_t r, uint8_t g, uint8_t b, uint8_t a )   { return (r | (g << 8) | (b << 16) | (a << 24)); }

    static const uint32_t           red                                 = 0xff0000ff;
    static const uint32_t           green                               = 0xff00ff00;
    static const uint32_t           blue                                = 0xffff0000;
    static const uint32_t           black                               = 0xff000000;
    static const uint32_t           white                               = 0xffffffff;
    static const uint32_t           transparent                         = 0x00000000;

    static u32                      get_distinct_color( u32 index );

}; // struct ColorUint



// High level resources /////////////////////////////////////////////////////////

//
//
struct Buffer {

    BufferHandle                    handle;
    u32                             index;      // Resource Pool Index
    BufferDescription               desc;

}; // struct Buffer

//
//
struct Sampler {

    SamplerHandle                   handle;
    u32                             index;
    SamplerDescription              desc;

}; // struct Sampler

// Texture //////////////////////////////////////////////////////////////////////
//
// 
//
struct Texture {

    TextureHandle                   handle;
    u32                             index;
    TextureDescription              desc;

}; // struct Texture

//
//
struct SubTexture {
    vec2s                           uv0;
    vec2s                           uv1;
}; // struct SubTexture

//
//
struct TextureRegion {

    vec2s                           uv0;
    vec2s                           uv1;

    hydra::graphics::TextureHandle  texture;
}; // struct TextureRegion 

//
//
struct TextureAtlas {

    array_type( SubTexture )        regions     = nullptr;
    Texture                         texture;

}; // struct TextureAtlas


// Material/Shaders /////////////////////////////////////////////////////////////

//
//
struct ClearData {

    // Draw utility
    void                                bind( u64& sort_key, CommandBuffer* gpu_commands );

    // Set methods
    ClearData& reset();
    ClearData& set_color( vec4s color );
    ClearData& set_depth( f32 depth );
    ClearData& set_stencil( u8 stencil );

    f32                                 clear_color[4];
    f32                                 depth_value;
    u8                                  stencil_value;

    u8                                  needs_color_clear = 0;
    u8                                  needs_depth_clear = 0;
    u8                                  needs_stencil_clear = 0;

}; // struct ClearData

//
//
struct ComputeDispatch {
    u16                             x = 0;
    u16                             y = 0;
    u16                             z = 0;
}; // struct ComputeDispatch

//
//
struct ResizeData {

    f32                             scale_x     = 1.f;
    f32                             scale_y     = 1.f;
    u8                              resize      = 1;
};

//
//
struct RenderStageCreation {

    ClearData                       clear;
    ResizeData                      resize;

    u16                             num_render_targets  = 0;
    RenderPassType::Enum            type                = RenderPassType::Standard;

    Texture*                        output_textures[k_max_image_outputs];
    Texture*                        depth_stencil_texture;


    const char*                     name                = nullptr;

    RenderStageCreation&            reset();

    RenderStageCreation&            add_render_texture( Texture* texture );
    RenderStageCreation&            set_depth_stencil_texture( Texture* texture );

    RenderStageCreation&            set_scaling( f32 scale_x, f32 scale_y, u8 resize );
    RenderStageCreation&            set_name( const char* name );
    RenderStageCreation&            set_type( RenderPassType::Enum type );
}; // RenderStageCreation

//
//
struct RenderStage {

    RenderPassOutput                output;
    ExecutionBarrier                barrier;
    ClearData                       clear;
    ResizeData                      resize;
    RenderPassHandle                render_pass;
    RenderPassType::Enum            type;

    Texture*                        output_textures[k_max_image_outputs];
    Texture*                        depth_stencil_texture;

    array_type( RenderFeature* )    features;

    cstring                         name;

    u16                             num_render_targets = 0;
    u32                             index;
    u16                             output_width;
    u16                             output_height;
    u16                             output_depth;
}; // struct RenderStage


//
//
struct ShaderCreation {

    hfx::ShaderEffectFile*          hfx             = nullptr;
    const RenderPassOutput*         outputs;
    u32                             num_outputs;

    ShaderCreation&                 reset();
    ShaderCreation&                 set_shader_binary( hfx::ShaderEffectFile* hfx );
    ShaderCreation&                 set_outputs( const RenderPassOutput* outputs, u32 num_outputs );

}; // struct ShaderCreation

//
//
struct Shader {

    hfx::ShaderEffectFile           hfx;

    array_type( PipelineHandle )    pipelines;
    array_type( ResourceLayoutHandle )  resource_layouts;

    u32                             index;

}; // struct Shader

//
//
struct MaterialCreation {

    Shader*                         shader          = nullptr;
    ResourceListCreation*           resource_lists  = nullptr;
    u32                             num_resource_list;

    MaterialCreation&               reset();
    MaterialCreation&               set_shader( Shader* shader );
    MaterialCreation&               set_resource_lists( ResourceListCreation* lists, u32 num_lists );

}; // struct MaterialCreation

//
//
struct Material {
    
    Shader*                         shader;

    array_type( PipelineHandle )    pipelines;      // Caches from ShaderEffect
    array_type( ResourceListHandle ) resource_lists;
    array_type( ComputeDispatch )   compute_dispatches;

    u32                             num_passes;
    u32                             index;

}; // struct Material


//
//
struct RenderFeature {

    virtual void                    load_resources( Renderer& renderer, bool init, bool reload ) { }
    virtual void                    unload_resources( Renderer& renderer, bool shutdown, bool reload ) { }

    virtual void                    update( Renderer& renderer, f32 delta_time ) { }
    virtual void                    render( Renderer& renderer, u64& sort_key, CommandBuffer* commands ) { }

    virtual void                    resize( Renderer& renderer, u32 width, u32 height ) {}

}; // struct RenderFeature


// Utils ////////////////////////////////////////////////////////////////////////

// Renderer /////////////////////////////////////////////////////////////////////


struct RendererCreation {

    hydra::graphics::Device*    gpu;

}; // struct RendererCreation

//
// Main class responsible for handling all high level resources
//
struct Renderer {

    void                        init( const RendererCreation& creation );
    void                        terminate();

    void                        begin_frame();
    void                        end_frame();

    void                        on_resize( u32 width, u32 height );

    f32                         aspect_ratio() const;

    // Creation/destruction
    Buffer*                     create_buffer( const BufferCreation& creation );
    Texture*                    create_texture( const TextureCreation& creation );
    Texture*                    create_texture( cstring filename );
    Sampler*                    create_sampler( const SamplerCreation& creation );
    RenderStage*                create_stage( const RenderStageCreation& creation );
    Shader*                     create_shader( const ShaderCreation& creation );
    Material*                   create_material( const MaterialCreation& creation );

    void                        destroy_buffer( Buffer* buffer );
    void                        destroy_texture( Texture* texture );
    void                        destroy_sampler( Sampler* sampler );
    void                        destroy_stage( RenderStage* stage );
    void                        destroy_shader( Shader* shader );
    void                        destroy_material( Material* material );

    // Update resources
    void*                       map_buffer( Buffer* buffer, u32 offset = 0, u32 size = 0 );
    void                        unmap_buffer( Buffer* buffer );

    void                        resize( RenderStage* stage );
    void                        reload_resource_list( Material* material, u32 index );

    // Draw
    void                        draw( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands ); // Draw using render features

    void                        draw_material( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands,
                                               Material* material, u32 material_pass_index );           // Draw using materials
    
    ResourcePool                textures;
    ResourcePool                buffers;
    ResourcePool                samplers;
    ResourcePool                stages;
    ResourcePool                shaders;
    ResourcePool                materials;

    hydra::graphics::Device*    gpu;
    Buffer*                     dynamic_constants;
    u8*                         dynamic_mapped_memory;
    u32                         dynamic_allocated_size;

    u16                         width;
    u16                         height;

}; // struct Renderer

// GPUProfiler //////////////////////////////////////////////////////

struct GPUProfiler {

    void                        init( u32 max_frames );
    void                        shutdown();

    void                        update( Device& gpu );

    void                        draw_ui();

    GPUTimestamp*               timestamps;
    u16*                        per_frame_active;

    u32                         max_frames;
    u32                         current_frame;

    f32                         max_time;
    f32                         min_time;
    f32                         average_time;

    f32                         max_duration;
    bool                        paused;

}; // struct GPUProfiler

#if defined(HYDRA_RENDERING_CAMERA)
//
// Camera struct - can be both perspective and orthographic.
//
struct Camera {

    void                        init_perpective( f32 near_plane, f32 far_plane, f32 fov_y, f32 aspect_ratio );
    void                        init_orthographic( f32 near_plane, f32 far_plane, f32 viewport_width, f32 viewport_height, f32 zoom );

    void                        reset();

    void                        set_viewport_size( f32 width, f32 height );
    void                        set_zoom( f32 zoom );
    void                        set_aspect_ratio( f32 aspect_ratio );
    void                        set_fov_y( f32 fov_y );

    void                        update();
    void                        rotate( f32 delta_pitch, f32 delta_yaw );

    // Project/unproject
    vec3s                       unproject( const vec3s& screen_coordinates );


    static void                 yaw_pitch_from_direction( const vec3s& direction, f32 & yaw, f32& pitch );

    mat4s                       view;
    mat4s                       projection;
    mat4s                       view_projection;

    vec3s                       position;
    vec3s                       right;
    vec3s                       direction;
    vec3s                       up;

    f32                         yaw;
    f32                         pitch;

    f32                         near_plane;
    f32                         far_plane;

    f32                         field_of_view_y;
    f32                         aspect_ratio;

    f32                         zoom;
    f32                         viewport_width;
    f32                         viewport_height;

    bool                        perspective;
    bool                        update_projection;

}; // struct Camera


#endif // HYDRA_RENDERING_CAMERA

} // namespace graphics
} // namespace hydra
