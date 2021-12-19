#pragma once

#include "kernel/primitive_types.hpp"
#include "kernel/array.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/relative_data_structures.hpp"
#include "kernel/blob.hpp"

#include "graphics/gpu_enum.hpp"

namespace hydra {
namespace gfx {

struct CommandBuffer;
struct Renderer;
struct RenderFeature;
struct RenderGraphNode;
struct RenderGraph;
struct RenderGraphBlob;
struct RenderStage;
struct ResourceManager;
struct RenderView;


enum RenderGraphNodeType : u8 {
    RenderGraphNodeType_Stage = 0,
    RenderGraphNodeType_Texture,
    RenderGraphNodeType_Buffer,
    RenderGraphNodeType_Shader,
    RenderGraphNodeType_Material,
    RenderGraphNodeType_Sampler
}; // enum RenderGraphNodeType

//
//
struct RenderGraphNode {

    RenderGraphNodeType         type;
    u32                         blueprint_index;

    // Indices to other nodes.
    u16                         inputs[ 16 ];
    u16                         outputs[ 16 ];

    u32                         num_inputs = 0;
    u32                         num_outputs = 0;

    RenderGraphNode&            reset() {
        num_inputs = num_outputs = 0;
        return *this;
    }

    RenderGraphNode&            add_input( u16 input ) {
        inputs[ num_inputs++ ] = input;
        return *this;
    }

    RenderGraphNode&            add_output( u16 output ) {
        outputs[ num_outputs++ ] = output;
        return *this;
    }

}; // struct RenderGraphNode


//
//
struct RenderGraphBuilder {

    void                        init( hydra::Allocator* allocator, cstring file_path );
    void                        shutdown();

    void                        setup( hydra::gfx::Renderer* gfx, RenderFeature** features, u32 count );

    void                        fill_render_graph( RenderGraph* graph );

    u16                         add_node( RenderGraphNode& node );

    hydra::Allocator*           allocator   = nullptr;
    RenderGraphBlob*            rgb         = nullptr;

    hydra::Array<RenderGraphNode>   registered_resources;
    hydra::Array<RenderGraphNode*>  executed_nodes;
    hydra::FlatHashMap<u64, u16>    name_to_node;
    hydra::Array<RenderStage*>      stages_to_execute;
    hydra::Array<RenderView*>       render_views;

}; // struct RenderGraphBuilder

//
//
struct RenderGraph {

    void                        init( hydra::Allocator* allocator );
    void                        shutdown();

    void                        render( Renderer* gfx, u64 sort_key, CommandBuffer* command_buffer );

    hydra::Array<RenderStage*>  stages;
    hydra::Allocator*           allocator   = nullptr;

}; // struct RenderGraph

// Blueprints ////////////////////////////////////////////////////////////

//
//
struct RenderGraphTextureBlueprint {

    u64                         name_hash;
    TextureFormat::Enum         format;
    hydra::RelativeString       name;

}; // struct RenderGraphTextureBlueprint

//
//
struct RenderStageBlueprint {

    u64                         name_hash;

    hydra::RelativeArray<u16>   inputs;
    hydra::RelativeArray<u16>   outputs;

    u16                         render_view_index;
    u16                         output_ds_index;

    u32                         clear_color;
    f32                         clear_depth;

    RenderPassType::Enum        type;

    u8                          clear_stencil;
    u8                          needs_clear_color;
    u8                          needs_clear_depth;
    u8                          needs_clear_stencil;
    u8                          load_color;
    u8                          load_depth;
    u8                          load_stencil;
    u8                          resize;

    hydra::RelativeString       name;

}; // struct RenderStageBlueprint

//
//
struct RenderViewBlueprint {

    u64                         name_hash;

    hydra::RelativeString       name;

}; // struct RenderViewBlueprint

//
//
struct RenderGraphBlob : public hydra::Blob {

    u64                         name_hash;
    hydra::RelativeString       name;

    hydra::RelativeArray<RenderGraphTextureBlueprint> textures;
    hydra::RelativeArray<RenderStageBlueprint> stages;
    hydra::RelativeArray<RenderViewBlueprint> views;

    static constexpr u32        k_version = 0;

}; // struct RenderGraphBlob


} // namespace gfx
} // namespace hydra

