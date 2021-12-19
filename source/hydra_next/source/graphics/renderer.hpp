#pragma once

//  Hydra Rendering - v0.46
//
//  High level rendering implementation based on Hydra Graphics library.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/12/11, 18.18
//
// Files /////////////////////////////////
//
// camera.hpp/.cpp, debug_renderer.hpp/.cpp, renderer.hpp/.cpp, render_graph.hpp/.cpp, sprite_batch.hpp/.cpp
//
// Revision history //////////////////////
//
//      0.39 (2021/11/07): + Added ShaderPass and MaterialPass to remove too many arrays in Shaders and Materials.
//      0.38 (2021/10/23): + Added buffer, shader and material new creation methods with just parameter for convenience.
//      0.37 (2021/10/21): + Added methods to remove the need to access GPUDevice directly.
//      0.36 (2021/09/29): + Added support for HFX version 2. + Some code cleanup.
//      0.35 (2021/08/15): + Removed global buffer usage.
//      0.34 (2021/06/13): + Moved to Hydra Next libraries.
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

#include "kernel/array.hpp"
#include "kernel/resource_manager.hpp"
#include "kernel/color.hpp"

#include "graphics/gpu_device.hpp"
#include "graphics/hydra_shaderfx.h"

#include "cglm/types-struct.h"

namespace hfx {
    struct ShaderEffectFile;

    typedef hydra::FlatHashMap<char*, u16> NameToIndex;
} // namespace hfx

namespace hydra {
namespace gfx {

// General methods //////////////////////////////////////////////////////////////


struct Camera;
struct ClearData;
struct Renderer;
struct RenderFeature;
struct RenderStage;
struct RenderView;


// High level resources /////////////////////////////////////////////////////////

//
//
struct Buffer : public hydra::Resource {

    BufferHandle                    handle;
    u32                             pool_index;
    BufferDescription               desc;

    static constexpr cstring        k_type = "hydra_buffer_type";
    static u64                      k_type_hash;

}; // struct Buffer

//
//
struct Sampler : public hydra::Resource {

    SamplerHandle                   handle;
    u32                             pool_index;
    SamplerDescription              desc;

    static constexpr cstring        k_type = "hydra_sampler_type";
    static u64                      k_type_hash;

}; // struct Sampler

// Texture //////////////////////////////////////////////////////////////////////
//
// 
//
struct Texture : public hydra::Resource {

    TextureHandle                   handle;
    u32                             pool_index;
    TextureDescription              desc;

    static constexpr cstring        k_type = "hydra_texture_type";
    static u64                      k_type_hash;

}; // struct Texture

//
//
struct SubTexture {
    //vec2s                           uv0;
    //vec2s                           uv1;
}; // struct SubTexture

//
//
struct TextureRegion {

   // vec2s                           uv0;
    //vec2s                           uv1;

    hydra::gfx::TextureHandle  texture;
}; // struct TextureRegion 

//
//
struct TextureAtlas {

    //array_type( SubTexture )        regions     = nullptr;
    Texture                         texture;

}; // struct TextureAtlas


//
//
struct ComputeDispatch {
    u16                             x = 0;
    u16                             y = 0;
    u16                             z = 0;
}; // struct ComputeDispatch

// Material/Shaders /////////////////////////////////////////////////////////////

//
//
struct ShaderCreation {

    ShaderCreation&                 reset();
    ShaderCreation&                 set_shader_binary( hfx::ShaderEffectFile* hfx );
    ShaderCreation&                 set_shader_binary_v2( hfx::ShaderEffectBlueprint* hfx );
    ShaderCreation&                 set_outputs( const RenderPassOutput* outputs, u32 num_outputs );


    hfx::ShaderEffectFile*          hfx_            = nullptr;
    hfx::ShaderEffectBlueprint*     hfx_blueprint   = nullptr;  // New version of the binary.
    const RenderPassOutput*         outputs;
    u32                             num_outputs;

}; // struct ShaderCreation

//
//
struct ShaderPass {

    PipelineHandle                  pipeline;
    ResourceLayoutHandle            resource_layout;
}; // struct ShaderPass

//
//
struct Shader : public hydra::Resource {

    void                            get_compute_dispatches( u32 pass_index, hydra::gfx::ComputeDispatch& out_dispatch );
    u32                             get_num_passes() const;

    hfx::ShaderEffectFile*          hfx_binary      = nullptr;
    hfx::ShaderEffectBlueprint*     hfx_binary_v2   = nullptr;

    Array<ShaderPass>               passes;

    u32                             pool_index;

    static constexpr cstring        k_type = "hydra_shader_type";
    static u64                      k_type_hash;

}; // struct Shader

//
//
struct MaterialCreation {

    MaterialCreation&               reset();
    MaterialCreation&               set_shader( Shader* shader );
    MaterialCreation&               set_resource_lists( ResourceListCreation* lists, u32 num_lists );
    MaterialCreation&               set_name( cstring name );

    Shader*                         shader          = nullptr;
    ResourceListCreation*           resource_lists  = nullptr;
    cstring                         name            = nullptr;
    u32                             num_resource_list;

}; // struct MaterialCreation

//
//
struct MaterialPass {

    PipelineHandle                  pipeline;   // Cached from ShaderPass
    ResourceListHandle              resource_list;
    ComputeDispatch                 compute_dispatch;

}; // struct MaterialPass

//
//
struct Material : public hydra::Resource {
    
    Shader*                         shader;

    Array<MaterialPass>             passes;

    u32                             pool_index;

    static constexpr cstring        k_type = "hydra_material_type";
    static u64                      k_type_hash;

}; // struct Material

//
//
struct RenderFeatureResourceContext {

    Renderer*                       renderer;
    hydra::ResourceManager*         resource_manager;
    Allocator*                      allocator;

}; // struct RenderFeatureResourceContext

//
//
struct RenderFeature {

    virtual void                    init( RenderFeatureResourceContext& context ) { }
    virtual void                    shutdown( RenderFeatureResourceContext& context ) { }
    virtual void                    reload( RenderFeatureResourceContext& context ) { }

    virtual void                    update( Renderer& renderer, f32 delta_time ) { }
    virtual void                    render( Renderer& renderer, u64& sort_key, CommandBuffer* commands, RenderView& render_view, u64 stage_name_hash ) { }

    virtual void                    resize( Renderer& renderer, RenderView& render_view ) {}

}; // struct RenderFeature

// Render Stage /////////////////////////////////////////////////////////////////

//
// A RenderView is a combination of a Camera and all the stages and objects visible
// from that camera.
// It guides resizing chains and in practice gives a sectorization of rendering.
//
struct RenderView : public hydra::Resource {

    Camera*                         camera      = nullptr;

    Array<RenderStage*>             dependant_render_stages;

    u16                             width       = 1;
    u16                             height      = 1;

    u32                             pool_index  = 0;

    static constexpr cstring        k_type = "hydra_render_view_type";
    static u64                      k_type_hash;

}; // struct RenderView

//
//
struct ClearData {

    // Draw utility
    void                            set( u64& sort_key, CommandBuffer* gpu_commands );

    // Set methods
    ClearData&                      reset();
    ClearData&                      set_color( vec4s color );
    ClearData&                      set_color( Color color );
    ClearData&                      set_depth( f32 depth );
    ClearData&                      set_stencil( u8 stencil );

    f32                             clear_color[4];
    f32                             depth_value;
    u8                              stencil_value;

    RenderPassOperation::Enum       color_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       depth_operation         = RenderPassOperation::DontCare;
    RenderPassOperation::Enum       stencil_operation       = RenderPassOperation::DontCare;

}; // struct ClearData

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

    RenderStageCreation&            reset();

    RenderStageCreation&            add_render_texture( Texture* texture );
    RenderStageCreation&            set_depth_stencil_texture( Texture* texture );

    RenderStageCreation&            set_scaling( f32 scale_x, f32 scale_y, u8 resize );
    RenderStageCreation&            set_name( cstring name );
    RenderStageCreation&            set_type( RenderPassType::Enum type );
    RenderStageCreation&            set_render_view( RenderView* view );


    ClearData                       clear;
    ResizeData                      resize;

    u16                             num_render_targets  = 0;
    RenderPassType::Enum            type                = RenderPassType::Geometry;

    Texture*                        output_textures[k_max_image_outputs];
    Texture*                        depth_stencil_texture;
    RenderView*                     render_view;

    cstring                         name                = nullptr;

}; // RenderStageCreation

//
//
struct RenderStage : public hydra::Resource {

    RenderPassOutput                output;
    ExecutionBarrier                barrier;
    ClearData                       clear;
    ResizeData                      resize;
    RenderPassHandle                render_pass;
    RenderPassType::Enum            type;

    Texture*                        output_textures[k_max_image_outputs];
    Texture*                        depth_stencil_texture;

    RenderView*                     render_view;
    Array<RenderFeature*>           features;

    u64                             name_hash;

    u16                             num_render_targets = 0;
    u32                             pool_index;
    u16                             output_width;
    u16                             output_height;
    u16                             output_depth;

    static constexpr cstring        k_type = "hydra_render_stage_type";
    static u64                      k_type_hash;
}; // struct RenderStage

// ResourceCache ////////////////////////////////////////////////////////////////

//
//
struct ResourceCache {

    void                            init( Allocator* allocator );
    void                            shutdown( Renderer* renderer );

    FlatHashMap<u64, Texture*>      textures;
    FlatHashMap<u64, Buffer*>       buffers;
    FlatHashMap<u64, Sampler*>      samplers;
    FlatHashMap<u64, RenderStage*>  stages;
    FlatHashMap<u64, Shader*>       shaders;
    FlatHashMap<u64, Material*>     materials;
    FlatHashMap<u64, RenderView*>   render_views;    

}; // struct ResourceCache

// Renderer /////////////////////////////////////////////////////////////////////


struct RendererCreation {

    hydra::gfx::Device*         gpu;
    Allocator*                  allocator;

}; // struct RendererCreation

//
// Main class responsible for handling all high level resources
//
struct Renderer : public Service {

    hy_declare_service( Renderer );

    void                        init( const RendererCreation& creation );
    void                        shutdown();

    void                        set_loaders( hydra::ResourceManager* manager );

    void                        begin_frame();
    void                        end_frame();

    void                        resize_swapchain( u32 width, u32 height );

    f32                         aspect_ratio() const;

    // Creation/destruction
    Buffer*                     create_buffer( const BufferCreation& creation );
    Buffer*                     create_buffer( BufferType::Mask type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name );
    Buffer*                     create_dynamic_buffer( BufferType::Mask type, u32 size, cstring name );
    
    Texture*                    create_texture( const TextureCreation& creation );
    Texture*                    create_texture( cstring name, cstring filename );
    
    Sampler*                    create_sampler( const SamplerCreation& creation );
    
    RenderStage*                create_stage( const RenderStageCreation& creation );
    
    Shader*                     create_shader( const ShaderCreation& creation );
    Shader*                     create_shader( hfx::ShaderEffectBlueprint* hfx, RenderPassOutput* outputs, u32 num_outputs );
    
    Material*                   create_material( const MaterialCreation& creation );
    Material*                   create_material( Shader* shader, ResourceListCreation* resource_lists, u32 num_lists, cstring name );

    RenderView*                 create_render_view( Camera* camera, cstring name, u32 width, u32 height, RenderStage** stages, u32 num_stages );

    void                        destroy_buffer( Buffer* buffer );
    void                        destroy_texture( Texture* texture );
    void                        destroy_sampler( Sampler* sampler );
    void                        destroy_stage( RenderStage* stage );
    void                        destroy_shader( Shader* shader );
    void                        destroy_material( Material* material );
    void                        destroy_render_view( RenderView* render_view );

    // Update resources
    void*                       dynamic_allocate( Buffer* buffer );     // Used for dynamic buffers, no need to unmap.

    void*                       map_buffer( Buffer* buffer, u32 offset = 0, u32 size = 0 );
    void                        unmap_buffer( Buffer* buffer );

    bool                        resize_stage( RenderStage* stage, u32 new_width, u32 new_height );
    bool                        resize_view( RenderView* view, u32 new_width, u32 new_height );

    void                        reload_resource_list( Material* material, u32 index );

    CommandBuffer*              get_command_buffer( QueueType::Enum type, bool begin )  { return gpu->get_command_buffer( type, begin ); }
    void                        queue_command_buffer( hydra::gfx::CommandBuffer* commands ) { gpu->queue_command_buffer( commands ); }

    // Draw
    void                        draw( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands ); // Draw using render features

    void                        draw_material( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands,
                                               Material* material, u32 material_pass_index );           // Draw using materials
    
    ResourcePoolTyped<Texture>  textures;
    ResourcePoolTyped<Buffer>   buffers;
    ResourcePoolTyped<Sampler>  samplers;
    ResourcePoolTyped<RenderStage> stages;
    ResourcePoolTyped<Shader>   shaders;
    ResourcePoolTyped<Material> materials;
    ResourcePoolTyped<RenderView> render_views;

    ResourceCache               resource_cache;

    hydra::gfx::Device*         gpu;

    u16                         width;
    u16                         height;

    static constexpr cstring    k_name = "hydra_rendering_service";

}; // struct Renderer


//
//
struct BindingBlueprint {

    u64                         name_hash;
    u64                         resource_db_name_hash;
    hydra::RelativeString       name;
    hydra::RelativeString       resource_db_name;
}; // struct BindingBlueprint

//
//
struct MaterialBlob : public hydra::Blob {

    u64                         name_hash;

    hydra::RelativeString       name;
    hydra::RelativeString       hfx_path;

    hydra::RelativeArray<BindingBlueprint> bindings;
    hydra::RelativeArray<u64>   stage_names;

    static constexpr u32        k_version = 0;

}; // struct MaterialBlob


} // namespace gfx
} // namespace hydra
