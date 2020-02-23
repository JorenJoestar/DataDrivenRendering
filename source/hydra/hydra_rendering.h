#pragma once

//
//  Hydra Rendering - v0.12
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

#include "cglm/types-struct.h"


namespace hfx {
    struct ShaderEffectFile;
} // namespace hfx

namespace hydra {
namespace graphics {

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

}; // struct Texture

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

    array(ShaderEffectPass)         passes;

    uint16_t                        num_passes                      = 0;
    uint16_t                        num_properties                  = 0;
    uint32_t                        local_constants_size            = 0;

    char*                           local_constants_default_data    = nullptr;
    char*                           properties_data                 = nullptr;

    string_hash(NameDataMap)        name_to_property                = nullptr;

    char                            name[32];
    char                            pipeline_name[32];
    uint32_t                        pool_id                         = 0;

}; // struct ShaderEffect

//
//
//
struct ShaderInstance {

    void                            load_resources( const PipelineCreation& pipeline, PipelineHandle pipeline_handle, ShaderResourcesDatabase& database, ShaderResourcesLookup& lookup, Device& device );

    PipelineHandle                  pipeline;
    ResourceListHandle              resource_lists[k_max_resource_layouts];

    uint32_t                        num_resource_lists;
}; // struct ShaderInstance

// Instances

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
struct Material {

    void                            load_resources( ShaderResourcesDatabase& db, Device& device );

    // Runtime part
    ShaderInstance*                 shader_instances                    = nullptr;
    uint32_t                        num_instances                       = 0;

    // Loading/Editing part
    ShaderResourcesLookup           lookups;                            // Per-pass resource lookup. Same count as shader instances.
    ShaderEffect*                   effect                              = nullptr;

    BufferHandle                    local_constants_buffer;
    char*                           local_constants_data                = nullptr;

    const char*                     name                                = nullptr;
    StringBuffer                    loaded_string_buffer;               // TODO: replace with global string repository!

    uint32_t                        num_textures                        = 0;
    uint32_t                        pool_id                             = 0;

    Texture**                       textures                            = nullptr;

}; // struct Material


// Render Pipeline //////////////////////////////////////////////////////////////

struct RenderManager;
struct RenderView;

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

    uint8_t                         pass_index                          = 0;

    Type                            type                                = Count;
    uint32_t                        pool_id                             = 0xffffffff;

    uint64_t                        geometry_stage_mask;                // Used to send render objects to the proper stage. Not used by compute or postprocess stages.

    array( RenderManager* )         render_managers;

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

//
// A full frame of rendering using RenderStages.
//
struct RenderPipeline {

    struct StageMap {
        char*                       key;
        RenderStage*                value;
    };

    struct TextureMap {
        char*                       key;
        TextureHandle               value;
    };

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

}; // struct RenderPipeline

//
//
//
struct PipelineMap {
    
    char* key;
    hydra::graphics::RenderPipeline* value;

}; // struct PipelineMap

//
//
//
struct RenderViewMap {

    char* key;
    hydra::graphics::RenderView*    value;
};

// Geometry/Math/Utils //////////////////////////////////////////////////////////

//
// Color class that embeds color in a uint32.
//
struct ColorUint {
    uint32_t                        abgr;

    void                            set( float r, float g, float b, float a )   { abgr = uint8_t( r * 255.f ) | (uint8_t( g * 255.f ) << 8) | (uint8_t( b * 255.f ) << 16) | (uint8_t( a * 255.f ) << 24); }

    static const uint32_t           red                                 = 0xff0000ff;
    static const uint32_t           green                               = 0xff00ff00;
    static const uint32_t           blue                                = 0xffff0000;
    static const uint32_t           black                               = 0xff000000;
    static const uint32_t           white                               = 0xffffffff;
    static const uint32_t           transparent                         = 0x00000000;

}; // struct ColorUint

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
// Camera struct - can be both perspective and orthographic.
//
struct Camera {

    void                            init( bool perspective, float near_plane, float far_plane );

    void                            update( hydra::graphics::Device& gfx_device );

    mat4s                           view;
    mat4s                           projection;
    mat4s                           view_projection;

    vec3s                           position;
    vec3s                           right;
    vec3s                           direction;
    vec3s                           up;

    float                           yaw;
    float                           pitch;

    float                           near_plane;
    float                           far_plane;

    bool                            perspective;
    bool                            update_projection;

}; // struct Camera

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

} // namespace graphics
} // namespace hydra
