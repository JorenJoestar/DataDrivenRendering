#pragma once

//
//  Hydra Rendering - v0.27
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
struct RenderStage2;
struct ClearData;

void                                render_stage_init_as_swapchain( Device& gpu, RenderStage2& out_stage, ClearData& clear, const char* name );
    
void                                pipeline_create( Device& gpu, hfx::ShaderEffectFile& hfx, u32 pass_index, const RenderPassHandle& pass_handle, PipelineHandle& out_pipeline, ResourceLayoutHandle* out_layouts, u32 num_layouts );

TextureHandle                       create_texture_from_file( Device& gpu, cstring filename );

//
// Material/Shaders /////////////////////////////////////////////////////////////

//
// Struct used to retrieve textures and buffers.
//
struct ShaderResourcesDatabase {

    struct BufferStringMap {
        char*                       key;
        BufferHandle                value;
    }; // struct BufferStringMap

    struct TextureStringMap {
        char*                       key;
        TextureHandle               value;
    }; // struct TextureStringMap

    struct SamplerStringMap {
        char* key;
        SamplerHandle               value;
    }; // struct SamplerStringMap

    BufferStringMap*                name_to_buffer = nullptr;
    TextureStringMap*               name_to_texture = nullptr;
    SamplerStringMap*               name_to_sampler = nullptr;

    void                            init();
    void                            terminate();

    void                            register_buffer( char* name, BufferHandle buffer );
    void                            register_texture( char* name, TextureHandle texture );
    void                            register_sampler( char* name, SamplerHandle sampler );

    BufferHandle                    find_buffer( char* name );
    TextureHandle                   find_texture( char* name );
    SamplerHandle                   find_sampler( char* name );

}; // struct ShaderResourcesDatabase

//
// Struct to link between a Shader Binding Name and a Resource. Used both in Pipelines and Materials.
//
struct ShaderResourcesLookup {

    enum Specialization {
        Frame, Pass, View, Shader
    }; // enum Specialization

    struct NameMap {
        char*                       key;
        char*                       value;
    }; // struct NameMap

    struct SpecializationMap {
        char*                       key;
        Specialization              value;
    }; // struct SpecializationMap

    NameMap*                        binding_to_resource = nullptr;
    SpecializationMap*              binding_to_specialization = nullptr;
    NameMap*                        binding_to_sampler = nullptr;

    void                            init();
    void                            terminate();

    void                            add_binding_to_resource( char* binding, char* resource );
    void                            add_binding_to_specialization( char* binding, Specialization specialization );
    void                            add_binding_to_sampler( char* binding, char* sampler );

    char*                           find_resource( char* binding );
    Specialization                  find_specialization( char* binding );
    char*                           find_sampler( char* binding );

    void                            specialize( char* pass, char* view, ShaderResourcesLookup& final_lookup );

}; // struct ShaderResourcesLookup

//
// 
//
struct Texture {

    TextureHandle                   handle;
    const char*                     filename;
    uint32_t                        pool_id;

    TextureDescription              description;

    void                            init( Device& gpu, const TextureCreation& creation );
    void                            init( Device& gpu, cstring filename );
    void                            shutdown( Device& gpu );

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

//
//
//
struct ShaderEffectPass {

    PipelineCreation                pipeline_creation;
    char                            name[32];

    PipelineHandle                  pipeline_handle;
    uint32_t                        pool_id;

}; // struct ShaderEffectPass

//
//
//
struct NameDataMap {

    char*                           key;
    void*                           value;

}; // struct NameDataMap

//
// 
//
struct ShaderEffect {

    void                            init( const hfx::ShaderEffectFile& shader_effect_file );

    array_type(ShaderEffectPass)    passes;

    uint16_t                        num_passes                      = 0;
    uint16_t                        num_properties                  = 0;
    uint32_t                        local_constants_size            = 0;

    char*                           local_constants_default_data    = nullptr;
    char*                           properties_data                 = nullptr;

    string_hash(NameDataMap)        name_to_property                = nullptr;

    char                            name[32];
    uint32_t                        pool_id                         = 0;

}; // struct ShaderEffect
//
////
////
//struct ShaderEditorData {
//
//    
//
//    char                            name[32];
//
//}; // struct ShaderEditorData
//
////
////
//struct Shader {
//
//    array(PipelineHandle)           pipelines;
//
//}; // struct Shader

static const char* s_local_constants_name = "LocalConstants";

//
//
//
struct MaterialFile {

    struct Property {
        char                        name[64];
        char                        data[192];
    };

    struct Binding {
        char                        name[64];
        char                        value[64];
    };

    struct Header {
        uint8_t                     num_properties;
        uint8_t                     num_bindings;
        uint8_t                     num_textures;
        uint8_t                     num_sampler_bindings;
        char                        name[64];
        char                        hfx_filename[192];
    };

    Header*                         header;
    Property*                       property_array;
    Binding*                        binding_array;
    Binding*                        sampler_binding_array;

}; // struct MaterialFile


//
//
//
struct MaterialPass {

    PipelineHandle                  pipeline;
    ResourceListHandle              resource_lists[k_max_resource_layouts];

    uint32_t                        num_resource_lists;
}; // struct MaterialPass

//
//
//
struct Material {

    void                            load_resources( ShaderResourcesDatabase& db, Device& device );

    // Runtime part
    array_type(MaterialPass)        passes                              = nullptr;

    // Loading/Editing part
    ShaderResourcesLookup           lookups;                            // Per-pass resource lookup. Same count as passes.
    ShaderEffect*                   effect                              = nullptr;

    BufferHandle                    local_constants_buffer;
    char*                           local_constants_data                = nullptr;

    const char*                     name                                = nullptr;
    StringBuffer                    loaded_string_buffer;               // TODO: replace with global string repository!

    uint32_t                        num_textures                        = 0;
    uint32_t                        pool_id                             = 0;

    Texture**                       textures                            = nullptr;

}; // struct Material


// TODO: IN DEVELOPMENT ///////////////////////////////////////////////////////////////////////////

//
//
struct ComputeDispatch {
    u16                             x = 0;
    u16                             y = 0;
    u16                             z = 0;
}; // struct ComputeDispatch

//
//
struct ShaderEffect2 {

    void                                init( Device& gpu, hfx::ShaderEffectFile* hfx, RenderPassHandle* render_passes );
    void                                shutdown( Device& gpu );

    u32                                 pass_index( const char* name );

    array_type( PipelineHandle )        pipelines;
    array_type( ResourceLayoutHandle )  resource_layouts;

    hfx::ShaderEffectFile*              hfx_binary;

}; // struct ShaderEffect2

//
//
struct Material2 {

    void                                init( Device& gpu, ShaderEffect2* shader, ResourceListCreation* resource_lists );
    void                                shutdown( Device& gpu );

    void                                reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list );

    ShaderEffect2*                      shader;

    array_type( PipelineHandle )        pipelines;      // Caches from ShaderEffect
    array_type( ResourceListHandle )    resource_lists;
    array_type( ComputeDispatch )       compute_dispatches;

    u32                                 num_passes;

}; // struct Material2

//
//
struct ClearData {

    // Draw utility
    void                                bind( u64& sort_key, CommandBuffer* gpu_commands );

    // Set methods
    ClearData&                          reset();
    ClearData&                          set_color( vec4s color );
    ClearData&                          set_depth( f32 depth );
    ClearData&                          set_stencil( u8 stencil );

    vec4s                               clear_color;
    f32                                 depth_value;
    u8                                  stencil_value;

    u8                                  needs_color_clear       = 0;
    u8                                  needs_depth_clear       = 0;
    u8                                  needs_stencil_clear     = 0;

}; // struct ClearData

//
//
struct RenderStage2Creation {

    RenderPassCreation                  render_pass_creation;
    Material2*                          material                = nullptr;
    u16                                 material_pass_index     = UINT16_MAX;

    ClearData                           clear;

}; // struct RenderStage2Creation

struct Material4;

//
//
struct RenderStage2 {

    void                                init( Device& gpu, RenderStage2Creation& creation );
    void                                shutdown( Device& gpu );
    void                                resize( Device& gpu, u16 width, u16 height );
    void                                render( Device& gpu, u64& sort_key, CommandBuffer* gpu_commands );

    void                                set_material( Material2* material, u16 index );
    void                                set_material4( Material4* material4, u16 index );
    void                                add_render_feature( RenderFeature* render_feature );
    
    ExecutionBarrier                    barrier;
    ClearData                           clear;
    RenderPassHandle                    render_pass;
    RenderPassType::Enum                type;

    const char*                         name;
    array_type(RenderFeature*)          features;
    Material2*                          material;
    Material4*                          material4;
    u16                                 material_pass_index;

    u16                                 output_width;
    u16                                 output_height;
    u16                                 output_depth;

}; // struct RenderStage2

//
struct ShaderEffect4Creation {
    cstring                     render_passes[ 8 ];
    RenderPassHandle            stages[ 8 ];

    cstring                     name;
    cstring                     hfx_source = nullptr;
    cstring                     hfx_binary = nullptr;
    u32                         hfx_options = 0;
    u32                         num_passes = 0;

    ShaderEffect4Creation&      reset();
    ShaderEffect4Creation&      set_name( cstring name );
    ShaderEffect4Creation&      set_hfx( cstring source, cstring binary, u32 options );
    ShaderEffect4Creation&      pass( cstring name, RenderPassHandle stage );

}; // struct ShaderEffect4Creation

//
//
struct NameToIndex {
    char*                       key;
    u16                         value;
};

//
//
struct ShaderEffect4 {

    void                        init( Device& gpu, ShaderEffect4Creation& creation );
    void                        shutdown( Device& gpu );

    u32                         pass_index( cstring name );
    u16                         resource_index( cstring pass_name, cstring resource_name );
    u16                         get_max_resources_per_pass( cstring pass_name );

    array_type( PipelineHandle )         pipelines;
    array_type( ResourceLayoutHandle )   resource_layouts;

    string_hash( hfx::NameToIndex )  name_to_index;  // Contains both pass and per pass resource indices, using prefixes for name search.

    hfx::ShaderEffectFile*      hfx_binary;

}; // struct ShaderEffect4


//
//
struct Material4Creation {
    ResourceHandle              resources[ 64 ];
    cstring                     pass_names[ 16 ];
    u8                          resources_offset[ 64 ];

    cstring                     name            = nullptr;
    ShaderEffect4*              shader_effect   = nullptr;

    u32                         num_resources   = 0;
    u32                         num_passes      = 0;

    Material4Creation&          start( ShaderEffect4* shader );
    Material4Creation&          pass( cstring name );
    Material4Creation&          set_buffer( cstring name, BufferHandle buffer );
    Material4Creation&          set_texture_and_sampler( cstring texture_name, TextureHandle texture, cstring sampler_name, SamplerHandle sampler );
    Material4Creation&          set_name( cstring name );

}; // struct Material4Creation

//
//
struct Material4 {

    void                        init( Device& gpu, const Material4Creation& creation );
    void                        shutdown( Device& gpu );


    void                        reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list );

    ShaderEffect4*              shader;

    array_type( PipelineHandle )        pipelines;      // Caches from ShaderEffect
    array_type( ResourceListHandle )    resource_lists;
    array_type( ComputeDispatch )       compute_dispatches;

    u32                                 num_passes;
};

//
//
struct RenderFeature {

    virtual void                        load_resources( hydra::graphics::Device& gpu, bool init, bool reload ) { }
    virtual void                        unload_resources( hydra::graphics::Device& gpu, bool shutdown, bool reload ) { }

    virtual void                        update( Device& gpu, f32 delta_time ) { }
    virtual void                        render( Device& gpu, u64& sort_key, CommandBuffer* gpu_commands ) { }

}; // struct RenderFeature


// Render Frame /////////////////////////////////////////////////////////////////

struct RenderManager;
struct RenderView;
struct RenderFrame;

//
//
//
struct RenderStageMask {

    uint64_t                        value;
}; // struct RenderStageMask

//
// Encapsulate the rendering of anything that writes to one or ore Render Targets.
//
struct RenderStage {

    enum Type {
        Geometry, Post, PostCompute, Swapchain, Count
    };

    TextureHandle*                  input_textures                      = nullptr;
    TextureHandle*                  output_textures                     = nullptr;

    TextureHandle                   depth_texture;

    float                           scale_x                             = 1.0f;
    float                           scale_y                             = 1.0f;
    uint16_t                        current_width                       = 1;
    uint16_t                        current_height                      = 1;
    uint8_t                         num_input_textures                  = 0;
    uint8_t                         num_output_textures                 = 0;

    RenderPassHandle                render_pass;

    Material*                       material                            = nullptr;
    RenderView*                     render_view                         = nullptr;

    float                           clear_color[4];
    float                           clear_depth_value;
    uint8_t                         clear_stencil_value;

    uint8_t                         clear_rt                            : 1;
    uint8_t                         clear_depth                         : 1;
    uint8_t                         clear_stencil                       : 1;
    uint8_t                         resize_output                       : 1;
    uint8_t                         pad                                 : 4;

    uint8_t                         material_pass_index                 = 0;
    uint8_t                         render_pass_index                   = 0;

    Type                            type                                = Count;
    uint32_t                        pool_id                             = UINT32_MAX;

    uint64_t                        current_sort_key;
    uint64_t                        geometry_stage_mask;                // Used to send render objects to the proper stage. Not used by compute or postprocess stages.

    array_type( RenderManager* )    render_managers;

    // Interface
    virtual void                    init();
    virtual void                    terminate();

    virtual void                    begin( Device& device, CommandBuffer* commands );
    virtual void                    render( Device& device, CommandBuffer* commands );
    virtual void                    end( Device& device, CommandBuffer* commands );

    virtual void                    load_resources( ShaderResourcesDatabase& db, Device& device );
    virtual void                    resize( uint16_t width, uint16_t height, Device& device );

    void                            register_render_manager( RenderManager* manager );

}; // struct RenderStage

// Maps ////////////////////////////

//
//
//
struct RenderFrameMap {

    char*                           key;
    RenderFrame*                    value;

}; // struct PipelineMap

//
//
//
struct RenderViewMap {

    char*                           key;
    RenderView*                     value;
};

//
//
//
struct StageMap {
    char*                           key;
    RenderStage*                    value;
};

//
//
//
struct TextureMap {
    char*                           key;
    TextureHandle                   value;
};


//
// A full frame of rendering using RenderStages.
//
struct RenderFrame {

    
    void                            init( ShaderResourcesDatabase* initial_db );
    void                            terminate( Device& device );

    void                            update();
    void                            render( Device& device, CommandBuffer* commands );

    void                            load_resources( Device& device );
    void                            resize( uint16_t width, uint16_t height, Device& device );

    StageMap*                       name_to_stage                       = nullptr;
    TextureMap*                     name_to_texture                     = nullptr;

    ShaderResourcesDatabase         resource_database;
    ShaderResourcesLookup           resource_lookup;

}; // struct RenderFrame

// Geometry/Math/Utils //////////////////////////////////////////////////////////

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


// Renderer /////////////////////////////////////////////////////////////////////


struct ShaderCreation {

    const char*                     shader_filename                     = nullptr;  // Either filename or memory of the COMPILED shader.
    char*                           shader_filename_memory              = nullptr;

}; // struct ShaderCreation

struct MaterialCreation {

}; // struct MaterialCreation

struct RenderFrameCreation {

}; // struct RenderFrameCreation

struct RendererCreation {

    hydra::graphics::Device*    gpu_device;

}; // struct RendererCreation

//
// Main class responsible for handling all high level resources
//
struct Renderer {

    void                        init( const RendererCreation& creation );
    void                        terminate();

    // High level interface
    ShaderEffect*               create_shader( const ShaderCreation& creation );
    Material*                   create_material( const MaterialCreation& creation );
    RenderFrame*                create_render_frame( const RenderFrameCreation& creation );

    void                        destroy_shader( ShaderEffect* effect );
    void                        destroy_material( Material* material );
    void                        destroy_render_frame( RenderFrame* render_frame );

    hydra::graphics::Device*    gpu_device;
    RenderFrame*                render_frame;


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

    void                            init_perpective( f32 near_plane, f32 far_plane, f32 fov_y, f32 aspect_ratio );
    void                            init_orthographic( f32 near_plane, f32 far_plane, f32 viewport_width, f32 viewport_height, f32 zoom );

    void                            reset();

    void                            set_viewport_size( f32 width, f32 height );
    void                            set_zoom( f32 zoom );
    void                            set_aspect_ratio( f32 aspect_ratio );
    void                            set_fov_y( f32 fov_y );

    void                            update();
    void                            rotate( f32 delta_pitch, f32 delta_yaw );

    // Project/unproject
    vec3s                           unproject( const vec3s& screen_coordinates );


    static void                     yaw_pitch_from_direction( const vec3s& direction, f32 & yaw, f32& pitch );

    mat4s                           view;
    mat4s                           projection;
    mat4s                           view_projection;

    vec3s                           position;
    vec3s                           right;
    vec3s                           direction;
    vec3s                           up;

    f32                             yaw;
    f32                             pitch;

    f32                             near_plane;
    f32                             far_plane;

    f32                             field_of_view_y;
    f32                             aspect_ratio;

    f32                             zoom;
    f32                             viewport_width;
    f32                             viewport_height;

    bool                            perspective;
    bool                            update_projection;

}; // struct Camera


#endif // HYDRA_RENDERING_CAMERA


#if defined (HYDRA_RENDERING_HIGH_LEVEL)
//
//
struct Box {

    union {
        struct {
            vec3s                   min;
            vec3s                   max;
        };
        vec3s                       box[2];
    };

}; // struct Box


//
//
struct Ray {
    vec3s                           origin;
    vec3s                           direction;
}; // struct Ray


bool                                ray_box_intersection( const hydra::graphics::Box& box, const hydra::graphics::Ray& ray, float &t );

//
// Mesh/models/scene ////////////////////////////////////////////////////////////

//
//
struct SubMesh {
    uint32_t                        start_index;
    uint32_t                        end_index;

    array( BufferHandle )           vertex_buffers;
    array( uint32_t )               vertex_buffer_offsets;
    BufferHandle                    index_buffer;

    Box                             bounding_box;

    Material*                       material;

}; // struct SubMesh

//
//
struct Mesh {

    array( SubMesh )                sub_meshes;

}; // struct Mesh

//
//
struct RenderNode {

    Mesh*                           mesh;

    uint32_t                        node_id;
    uint32_t                        parent_id;

}; // struct RenderNode

//
//
struct RenderScene {

    RenderManager*                  render_manager;
    RenderStageMask                 stage_mask;         // Used to bind the scene to one or more stages.
    BufferHandle                    node_transforms_buffer; // Shared buffers

    array( RenderNode )             nodes;
    array( BufferHandle )           buffers;            // All vertex and index buffers are here. Accessors will reference handles coming from here.

    array( mat4s )                  node_transforms;

}; // struct RenderScene

// 
// Camera/Views

//
// Render view is a 'contextualized' camera - a way of using the camera in the render pipeline.
//
struct RenderView {

    Camera                          camera;
    array( RenderScene )            visible_render_scenes;

}; // struct RenderView

//
// Renderers

// RenderManager ////////////////////////////////////////////////////////////////

//
//
//
struct RenderManager {

    struct RenderContext {
        Device*                     device;

        const RenderView*           render_view;
        CommandBuffer*              commands;
        
        RenderScene*                render_scene_array;
        uint16_t                    start;
        uint16_t                    count;
        
        uint16_t                    stage_index;
    }; // struct RenderContext

    virtual void                    render( RenderContext& render_context ) = 0;

    //virtual void                    reload( Device& device ) = 0;
}; // struct RenderManager

//
//
//
struct SceneRenderer : public RenderManager {

    void                            render( RenderContext& render_context ) override;

    Material*                       material;

}; // struct SceneRenderer

//
//
//
struct LineRenderer : public RenderManager {

    void                            init( ShaderResourcesDatabase& db, Device& device );
    void                            terminate( Device& device );

    void                            line( const vec3s& from, const vec3s& to, uint32_t color0, uint32_t color1 );     // Colors are in ABGR format.
    void                            line_2d( const vec2s& from, const vec2s& to, uint32_t color0, uint32_t color1 );
    void                            box( const Box& box, uint32_t color );

    void                            render( RenderContext& render_context ) override;

    BufferHandle                    lines_vb;
    BufferHandle                    lines_vb_2d;
    BufferHandle                    lines_cb;
    Material*                       line_material;

    uint32_t                        current_line_index;
    uint32_t                        current_line_index_2d;

}; // struct LineRenderer

//
//
//
struct LightingManager : public RenderManager {

    void                            init( ShaderResourcesDatabase& db, Device& device );
    void                            terminate( Device& device );

    void                            render( RenderContext& render_context ) override;

    BufferHandle                    lighting_cb;

    vec3s                           directional_light;

    vec3s                           point_light_position;
    float                           point_light_intensity;

    bool                            use_point_light;

}; // struct LightingManager

#endif // HYDRA_RENDERING_HIGH_LEVEL

} // namespace graphics
} // namespace hydra
