#pragma once

//
//  Hydra Render Graph - v0.01
//
//  Render Graph implementation using Hydra Rendering.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2021/01/07    18.16
//
//
// Revision history //////////////////////
//
//      0.01 (2021/01/07): + First implementation moved to its own file.

#include "hydra_rendering.h"

namespace hfx {
    struct ShaderEffectFile;
}


namespace hydra {
namespace graphics {

// todo //////////////////////////////////////////////////////////////////
//
//
struct ShaderEffect2 {

    void                                init( Device& gpu, hfx::ShaderEffectFile* hfx, RenderPassOutput* render_passes );
    void                                shutdown( Device& gpu );

    u32                                 pass_index( const char* name );

    array_type( PipelineHandle )        pipelines;
    array_type( ResourceLayoutHandle )  resource_layouts;

    hfx::ShaderEffectFile* hfx_binary;

}; // struct ShaderEffect2

//
//
struct Material2 {

    void                                init( Device& gpu, ShaderEffect2* shader, ResourceListCreation* resource_lists );
    void                                shutdown( Device& gpu );

    void                                reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list );

    ShaderEffect2* shader;

    array_type( PipelineHandle )        pipelines;      // Caches from ShaderEffect
    array_type( ResourceListHandle )    resource_lists;
    array_type( ComputeDispatch )       compute_dispatches;

    u32                                 num_passes;

}; // struct Material2

//
//
struct RenderStage2Creation {

    RenderPassCreation                  render_pass_creation;
    Material2* material = nullptr;
    u16                                 material_pass_index = UINT16_MAX;

    ClearData                           clear;

}; // struct RenderStage2Creation

struct Material4;

//
//
struct RenderStage2 {

    void                                init( Device& gpu, RenderStage2Creation& creation );
    void                                shutdown( Device& gpu );
    void                                resize( Device& gpu, u16 width, u16 height );
    void                                render( Renderer& renderer, u64& sort_key, CommandBuffer* gpu_commands );

    void                                set_material( Material2* material, u16 index );
    void                                set_material4( Material4* material4, u16 index );
    void                                add_render_feature( RenderFeature* render_feature );

    RenderPassOutput                    output;
    ExecutionBarrier                    barrier;
    ClearData                           clear;
    RenderPassHandle                    render_pass;
    RenderPassType::Enum                type;

    const char* name;
    array_type( RenderFeature* )          features;
    Material2* material;
    Material4* material4;
    u16                                 material_pass_index;

    u16                                 output_width;
    u16                                 output_height;
    u16                                 output_depth;

}; // struct RenderStage2

} // namespace graphics
} // namespace hydra


namespace hydra {
namespace graphics {

struct ShaderEffect3Creation {
    cstring                     render_passes[ 8 ];
    cstring                     stages[ 8 ];

    cstring                     name;
    cstring                     hfx_source = nullptr;
    cstring                     hfx_binary = nullptr;
    u32                         hfx_options = 0;
    u32                         num_passes = 0;

    u8                          get_pass_index( cstring name );

    ShaderEffect3Creation&      reset();
    ShaderEffect3Creation&      set_name( cstring name_ );
    ShaderEffect3Creation&      set_hfx( cstring source, cstring binary, u32 options );
    ShaderEffect3Creation&      pass( cstring name_, cstring stage );

}; // struct ShaderEffect3Creation


//
//
struct Material3Creation {
    cstring                     resources[ 64 ];
    cstring                     pass_names[ 16 ];
    u8                          resources_offset[ 64 ];

    cstring                     name = nullptr;
    cstring                     shader_effect = nullptr;

    u32                         num_resources = 0;
    u32                         num_passes = 0;

    void                        get_pass_data( cstring pass_name, u32& resource_offset, u32& resource_count );
    u8                          get_pass_index( cstring name );
    u32                         get_pass_resource_count( u8 pass_index );
    u32                         get_pass_resource_offset( u8 pass_index );

    Material3Creation&          reset();
    Material3Creation&          pass( cstring name );
    Material3Creation&          add_buffer( cstring name );
    Material3Creation&          add_texture_and_sampler( cstring texture, cstring sampler );
    Material3Creation&          set_name( cstring name );
    Material3Creation&          set_shader( cstring name );

}; // struct Material3Creation


struct RenderStage3Creation {

    cstring                     outputs[ k_max_image_outputs ];
    cstring                     output_depth = nullptr;
    cstring                     name = nullptr;

    u32                         num_outputs = 0;

    f32                         scale_x = 1.f;
    f32                         scale_y = 1.f;
    u8                          resize = 1;
    u8                          deactivated = 0;

    ClearData                   clear;

    cstring                     alias_output_name = nullptr;
    cstring                     material_name = nullptr;
    cstring                     pass_name = nullptr;
    u32                         pass_index;

    RenderPassType::Enum        type;

    RenderStage3Creation&       reset();
    RenderStage3Creation&       add_render_texture( cstring name );
    RenderStage3Creation&       set_scaling( f32 scale_x_, f32 scale_y_, u8 resize_ );
    RenderStage3Creation&       set_depth_stencil_texture( cstring name );
    RenderStage3Creation&       set_type( RenderPassType::Enum type_ );
    RenderStage3Creation&       set_name( cstring name_ );
    RenderStage3Creation&       set_material_and_pass( cstring material, cstring pass );

}; // struct RenderStage3Creation


struct Texture3Creation {
    TextureCreation             c;
    cstring                     alias = nullptr;
    cstring                     file = nullptr;
    bool                        is_swapchain = false;

    Texture3Creation&           reset();
    Texture3Creation&           init_from_file( cstring file_, cstring name_ );
    TextureCreation&            init();

}; // struct Texture3Creation


//
//
struct ShaderEffect3 {

    void                        init( Device& gpu, hfx::ShaderEffectFile* hfx );
    void                        shutdown( Device& gpu );

    u32                         pass_index( const char* name );

    // Lazy initialized resources. Created by Material as needed.
    array_type( PipelineHandle )         pipelines;
    array_type( ResourceLayoutHandle )   resource_layouts;

    hfx::ShaderEffectFile*      hfx_binary;

}; // struct ShaderEffect3

//
//
struct Material3 {

    void                        init( Device& gpu, ShaderEffect3* shader, ResourceListCreation* resource_lists );
    void                        shutdown( Device& gpu );

    void                        reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list );

    ShaderEffect3*              shader;

    array_type( PipelineHandle ) pipelines;      // Caches from ShaderEffect
    array_type( ResourceListHandle ) resource_lists;
    array_type( ComputeDispatch ) compute_dispatches;

}; // struct Material3

// RenderGraph //////////////////////////////////////////////////////////////////
struct RenderGraph {

    struct                      ResourceNode;

    void                        init();
    void                        init( Device& gpu );
    void                        shutdown( Device& gpu );

    void                        resize( Device& gpu, u32 width, u32 height );
    void                        render( Renderer& renderer, u64& sort_key, CommandBuffer* gpu_commands );

    void                        ui_draw( Device& gpu );

    void                        print_resource_name( ResourceNode& node, bool input );
    void                        print_output_dependencies( cstring name );
    void                        print_input_dependencies( cstring name );

    //
    u16                         add_resource_node( ResourceNode node, cstring node_name );
    ResourceNode* get_resource_node( cstring name );

    // Update resources
    void                        set_material_pass_texture_sampler( Device& gpu, cstring material, cstring material_pass, cstring texture, cstring sampler );
    void                        set_stage_material_pass( cstring stage_name, cstring material_pass_name );

    // Building graph methods ////////////////////////////////////////////////////
    void                        add_shader( ShaderEffect3Creation& data );
    void                        add_material( Material3Creation& data );
    void                        add_stage( RenderStage3Creation& data );
    void                        add_texture( Texture3Creation& data );
    void                        add_buffer( BufferCreation& data );
    void                        add_sampler( SamplerCreation& data );

    // Creation methods //////////////////////////////////////////////////////////
    void                        create_texture( Device& gpu, ResourceNode& node, Texture3Creation& creation );
    void                        create_buffer( Device& gpu, ResourceNode& node, BufferCreation& creation );
    void                        create_stage( Device& gpu, ResourceNode& node, RenderStage3Creation& creation );
    void                        create_shader( Device& gpu, ResourceNode& node, ShaderEffect3Creation& creation );
    void                        create_material( Device& gpu, ResourceNode& node, Material3Creation& creation );
    void                        create_sampler( Device& gpu, ResourceNode& node, SamplerCreation& creation );

    TextureHandle               get_texture( cstring name );
    BufferHandle                get_buffer( cstring name );
    Material2*                  get_material( cstring name );
    RenderStage2*               get_stage( cstring name );
    SamplerHandle               get_sampler( cstring name );

    void                        fill_resource_list( Material3Creation& creation, cstring pass_name, ResourceListCreation& rlc );

    enum GraphResourceType {
        ShaderEffect,
        Material,
        Stage,
        Buffer,
        Texture,
        Sampler
    };

    struct ResourceCreation {
        GraphResourceType       type;
        u32                     creation;
    };

    struct NameToResourceCreation {
        char*                   key;
        ResourceCreation        value;
    };

    // 
    struct ResourceNode {
        GraphResourceType       type;
        u32                     creation;

        hydra::graphics::ResourceHandle active_handle; // gfx actual handle

        u16                     inputs[ 16 ];
        u16                     outputs[ 16 ];

        u32                     num_inputs = 0;
        u32                     num_outputs = 0;

        ResourceNode& reset() {
            num_inputs = num_outputs = 0;
            return *this;
        }

        ResourceNode& add_input( u16 input ) {
            inputs[ num_inputs++ ] = input;
            return *this;
        }

        ResourceNode& add_output( u16 output ) {
            outputs[ num_outputs++ ] = output;
            return *this;
        }
    };

    struct NameToResourceNodeIndex {
        char* key;
        u16                     value;
    }; // struct NameToResourceIndex


    // String buffer ?
    // Linear allocator ?

    // Resources dependencies
    array_type( ResourceNode )  resources_to_create;
    string_hash( NameToResourceNodeIndex )  name_to_node;

    string_hash( NameToResourceCreation ) name_to_resource_creation;

    array_type( RenderStage3Creation )  render_stage_creations;
    array_type( Material3Creation )     material_creations;
    array_type( ShaderEffect3Creation ) shader_creations;
    array_type( Texture3Creation )      texture_creations;
    array_type( BufferCreation )        buffer_creations;
    array_type( SamplerCreation )       sampler_creations;

    // Active rendering resources (high and low level)
    array_type( RenderStage2 )          active_stages;
    array_type( Material2 )             active_materials;
    array_type( ShaderEffect2 )         active_shaders;
    array_type( hfx::ShaderEffectFile ) active_hfx;
    array_type( BufferHandle )          active_buffers;
    array_type( TextureHandle )         active_textures;
    array_type( SamplerHandle )         active_samplers;

}; // struct RenderGraph

} // namespace graphics
} // namespace hydra
