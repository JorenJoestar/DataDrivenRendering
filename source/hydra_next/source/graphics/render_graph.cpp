#include "graphics/render_graph.hpp"
#include "graphics/renderer.hpp"

#include "kernel/file.hpp"
#include "kernel/time.hpp"
#include "kernel/blob_serialization.hpp"

namespace hydra {
namespace gfx {


void RenderGraphBuilder::init( hydra::Allocator* allocator_, cstring file_path ) {
    allocator = allocator_;

    registered_resources.init( allocator, 16 );
    executed_nodes.init( allocator, 16 );
    name_to_node.init( allocator, 16 );
    name_to_node.set_default_value( u16_max );
    stages_to_execute.init( allocator, 16 );
    render_views.init( allocator, 8 );

    using namespace hydra::gfx;

    i64 time_start = time_now();

    hydra::FileReadResult frr = hydra::file_read_binary( file_path, allocator );
    if ( frr.data ) {
        hydra::BlobSerializer blob;
        rgb = blob.read<RenderGraphBlob>( allocator, RenderGraphBlob::k_version, frr.size, frr.data );
        blob.shutdown();
    }

    f64 execution_time = time_from_milliseconds( time_start );
    hprint( "RenderGraphBuilder: file loading took %fms.\n", execution_time );

    time_start = time_now();


    // Register stages
    for ( u32 is = 0; is < rgb->stages.size; ++is ) {
        const RenderStageBlueprint& render_stage_blueprint = rgb->stages[ is ];

        // Search or add node
        u16 stage_node_index = name_to_node.get( render_stage_blueprint.name_hash );
        if ( stage_node_index == u16_max ) {
            RenderGraphNode node;
            node.reset().type = RenderGraphNodeType_Stage;
            node.blueprint_index = u16( is );

            stage_node_index = add_node( node );

            name_to_node.insert( render_stage_blueprint.name_hash, u16( registered_resources.size - 1 ) );
        }

        // Add Texture Output Nodes
        for ( u32 t = 0; t < render_stage_blueprint.outputs.size; ++t ) {
            
            // TODO: add buffer support
            u16 texture_blueprint_index = render_stage_blueprint.outputs[ t ];
            const RenderGraphTextureBlueprint& texture = rgb->textures[ texture_blueprint_index ];

            // Search texture node
            u16 texture_node_index = name_to_node.get( texture.name_hash );
            if ( texture_node_index == u16_max ) {
                RenderGraphNode node;
                node.reset().type = RenderGraphNodeType_Texture;
                node.blueprint_index = u16( texture_blueprint_index );

                texture_node_index = add_node( node );

                name_to_node.insert( texture.name_hash, u16( registered_resources.size - 1 ) );
            }
            // Add output to the stage
            registered_resources[ stage_node_index ].add_output( texture_node_index );
            // Add input to the texture
            registered_resources[ texture_node_index ].add_input( stage_node_index );
        }

        // Add DepthStencil Output Node
        if ( render_stage_blueprint.output_ds_index != u16_max ) {
            const RenderGraphTextureBlueprint& texture = rgb->textures[ render_stage_blueprint.output_ds_index ];

            // Search texture node
            u16 texture_node_index = name_to_node.get( texture.name_hash );
            if ( texture_node_index == u16_max ) {
                RenderGraphNode node;
                node.reset().type = RenderGraphNodeType_Texture;
                node.blueprint_index = u16( render_stage_blueprint.output_ds_index );

                texture_node_index = add_node( node );

                name_to_node.insert( texture.name_hash, u16( registered_resources.size - 1 ) );
            }
            // Add output
            registered_resources[ stage_node_index ].add_output( texture_node_index );
            // Add input to the texture
            registered_resources[ texture_node_index ].add_input( stage_node_index );
        }

        // Add Texture Input Nodes
        for ( u32 t = 0; t < render_stage_blueprint.inputs.size; ++t ) {

            // TODO: add buffer support. Encode in the index with 1 bit ?
            u16 texture_blueprint_index = render_stage_blueprint.inputs[ t ];
            const RenderGraphTextureBlueprint& texture = rgb->textures[ texture_blueprint_index ];

            // Search texture node
            u16 texture_node_index = name_to_node.get( texture.name_hash );
            if ( texture_node_index == u16_max ) {
                RenderGraphNode node;
                node.reset().type = RenderGraphNodeType_Texture;
                node.blueprint_index = u16( texture_blueprint_index );

                texture_node_index = add_node( node );

                name_to_node.insert( texture.name_hash, u16( registered_resources.size - 1 ) );
            }
            // Add output
            registered_resources[ stage_node_index ].add_input( texture_node_index );
            // Add input to the texture
            registered_resources[ texture_node_index ].add_output( stage_node_index );
        }
    }

    // Record time of registration
    execution_time = time_from_milliseconds( time_start );
    hprint( "RenderGraph Building Done in %fms!\n", execution_time );
}

void RenderGraphBuilder::shutdown() {

    hfree( rgb, allocator );
    //TODO
    stages_to_execute.shutdown();
    render_views.shutdown();

    registered_resources.shutdown();
    executed_nodes.shutdown();
    name_to_node.shutdown();
}

void RenderGraphBuilder::setup( hydra::gfx::Renderer* gfx, RenderFeature** features, u32 count ) {


    // Debug print all registered resources
    for ( u32 i = 0; i < registered_resources.size; ++i ) {
        const RenderGraphNode& node = registered_resources[ i ];
        switch ( node.type ) {
            case RenderGraphNodeType_Stage:
            {
                const RenderStageBlueprint& render_stage_blueprint = rgb->stages[ node.blueprint_index ];
                hprint( "Stage %s\n", render_stage_blueprint.name.c_str() );
                break;
            }
            case RenderGraphNodeType_Texture:
            {
                const RenderGraphTextureBlueprint& texture_blueprint = rgb->textures[ node.blueprint_index ];
                hprint( "Texture %s\n", texture_blueprint.name.c_str() );
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Cull passes
    // Generate execution graph
    // Start from "swapchain" node and backtrack until no inputs are present.
    i64 time_start = time_now();

    executed_nodes.clear();

    // TODO: proper graph traversal.
    // For now just follow the order of declaration of stages.
    for ( i32 i = registered_resources.size - 1; i >= 0; i-- ) {
        RenderGraphNode& node = registered_resources[ i ];
        if ( node.type == RenderGraphNodeType_Stage ) {
            const RenderStageBlueprint& render_stage_blueprint = rgb->stages[ node.blueprint_index ];
            executed_nodes.push( &node );
        }
    }

    /*u16 swapchain_node_index = name_to_node.get( hydra::hash_calculate( "swapchain" ) );
    if ( swapchain_node_index < name_to_node.size ) {
        RenderGraphNode* node_to_execute = &registered_resources[ swapchain_node_index ];

        executed_nodes.push( node_to_execute );

    } else {
        hy_assertm( false, "Cannot find node 'swapchain'. Cannot create execution graph.\n" );
    }*/

    i64 execution_time = time_from_milliseconds( time_start );
    hprint( "RenderGraph Execution Graph Done in %fms.\n", execution_time );

    using namespace hydra::gfx;

    // Create resources used by the executed nodes
    // Find resources lifetime
    time_start = time_now();


    for ( u32 iv = 0; iv < rgb->views.size; ++iv ) {
        const RenderViewBlueprint& view_blueprint = rgb->views[ iv ];

        RenderView* view = gfx->create_render_view( nullptr, view_blueprint.name.c_str(), gfx->width, gfx->height, nullptr, 2 );
        //gfx_resource_manager->register_view( view_blueprint.name.c_str(), view );

        render_views.push( view );
    }

    for ( u32 is = 0; is < executed_nodes.size; ++is ) {
        // Reverse iterate
        RenderGraphNode* node = executed_nodes[ executed_nodes.size - 1 - is ];

        // Prepare stage creation
        const RenderStageBlueprint& render_stage_blueprint = rgb->stages[ node->blueprint_index ];
        RenderStageCreation rsc;
        rsc.reset().set_type( render_stage_blueprint.type ).set_name( render_stage_blueprint.name.c_str() ).clear.reset();
        rsc.set_render_view( render_stage_blueprint.render_view_index != u16_max ? render_views[ render_stage_blueprint.render_view_index ] : nullptr );
        rsc.resize.resize = render_stage_blueprint.resize;

        if ( render_stage_blueprint.needs_clear_color ) {
            Color clear_color{ render_stage_blueprint.clear_color };
            rsc.clear.set_color( clear_color );
        }
        else if ( render_stage_blueprint.load_color ) {
            rsc.clear.color_operation = RenderPassOperation::Load;
        }

        if ( render_stage_blueprint.needs_clear_depth ) {
            rsc.clear.set_depth( render_stage_blueprint.clear_depth );
        }
        else if ( render_stage_blueprint.load_depth ) {
            rsc.clear.depth_operation = RenderPassOperation::Load;
        }

        // Create output textures
        for ( u32 io = 0; io < node->num_outputs; ++io ) {
            u16 output_node_index = node->outputs[ io ];
            RenderGraphNode* output_node = &registered_resources[ output_node_index ];
            const RenderGraphTextureBlueprint& texture_blueprint = rgb->textures[ output_node->blueprint_index ];

            Texture* texture = gfx->resource_cache.textures.get( hash_calculate( texture_blueprint.name.c_str() ) );
            if ( !texture ) {
                TextureCreation tc;
                
                tc.set_format_type( texture_blueprint.format, TextureType::Texture2D ).set_flags( 1, TextureFlags::RenderTarget_mask );
                tc.set_name( texture_blueprint.name.c_str() ).set_size( gfx->width, gfx->height, 1 );

                texture = gfx->create_texture( tc );
                //gfx_resource_manager->register_texture( tc.name, texture );
            }
            
            if ( !TextureFormat::has_depth_or_stencil( texture->desc.format ) ) {
                rsc.add_render_texture( texture );
            } else {
                rsc.set_depth_stencil_texture( texture );
            }
        }

        // Create stage
        RenderStage* stage = gfx->create_stage( rsc );

        //gfx_resource_manager->register_render_stage( rsc.name, stage );

        //TODO
        stages_to_execute.push( stage );

        // Register stage to render view
        u16 view_index = render_stage_blueprint.render_view_index;
        if ( view_index < render_views.size ) {
            RenderView* view = render_views[ view_index ];
            view->dependant_render_stages.push( stage );
        }

    }

    execution_time = time_from_milliseconds( time_start );
    hprint( "RenderGraph Resource Creation Done in %fms.\n", execution_time );
}

void RenderGraphBuilder::fill_render_graph( RenderGraph* graph ) {

    graph->stages.clear();

    for ( u32 is = 0; is < stages_to_execute.size; ++is ) {
        RenderStage* stage = stages_to_execute[ is ];
        graph->stages.push( stage );
    }
}

u16 RenderGraphBuilder::add_node( RenderGraphNode& node ) {
    registered_resources.push( node );
    return u16( registered_resources.size - 1 );
}

// RenderGraph ////////////////////////////////////////////////////////////
void RenderGraph::init( hydra::Allocator* allocator_ ) {
    allocator = allocator_;
    stages.init( allocator, 8 );
}

void RenderGraph::shutdown() {
    stages.shutdown();
}

void RenderGraph::render( hydra::gfx::Renderer* gfx, u64 sort_key, hydra::gfx::CommandBuffer* command_buffer ) {

    for ( u32 is = 0; is < stages.size; ++is ) {
        RenderStage* stage = stages[ is ];
        gfx->draw( stage, sort_key, command_buffer );
    }
}


} // namespace gfx
} // namespace hydra

