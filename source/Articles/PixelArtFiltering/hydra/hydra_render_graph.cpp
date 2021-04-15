//
//  Hydra Render Graph - v0.01

#include "hydra_render_graph.h"

#include "imgui/imgui.h"
#include "imnodes/ImNodesEz.h"

#include "hydra/hydra_imgui.h"
#include "hydra/hydra_shaderfx.h"

#include "stb_image.h"
#include "json.hpp"


namespace hydra {
namespace graphics{


//
//
static void render_stage_init_as_swapchain( Device& gpu, RenderStage2& out_stage, ClearData& clear, const char* name ) {
    out_stage.clear = clear;
    out_stage.name = name;
    out_stage.type = RenderPassType::Swapchain;
    out_stage.output = gpu.get_swapchain_output();
}


// ShaderEffect2 //////////////////////////////////////////////////////////////////////////////////
void ShaderEffect2::init( Device& gpu, hfx::ShaderEffectFile* hfx, RenderPassOutput* render_passes ) {

    hfx_binary = hfx;

    const u32 passes = hfx->header->num_passes;
    // First create arrays
    array_init( pipelines );
    array_init( resource_layouts );

    array_set_length( pipelines, passes );
    array_set_length( resource_layouts, passes );

    // TODO:
    if ( !render_passes )
        return;

    for ( uint32_t i = 0; i < passes; ++i ) {
        // TODO: only 1 resource list allowed for now ?
        pipeline_create( gpu, *hfx, i, render_passes[ i ], pipelines[ i ], &resource_layouts[ i ], 1 );
    }

}

void ShaderEffect2::shutdown( Device& gpu ) {

    const u32 passes = hfx_binary->header->num_passes;
    for ( uint32_t i = 0; i < passes; ++i ) {

        gpu.destroy_pipeline( pipelines[ i ] );
        gpu.destroy_resource_layout( resource_layouts[ i ] );
    }
}

u32 ShaderEffect2::pass_index( const char* name ) {
    return hfx::shader_effect_get_pass_index( *hfx_binary, name );
}

// Material2 //////////////////////////////////////////////////////////////////////////////////////
void Material2::init( Device& gpu, ShaderEffect2* shader_, ResourceListCreation* resource_lists_ ) {

    shader = shader_;
    num_passes = shader->hfx_binary->header->num_passes;
    // First create arrays
    array_init( pipelines );
    array_init( resource_lists );
    array_init( compute_dispatches );

    array_set_length( pipelines, num_passes );
    array_set_length( resource_lists, num_passes );
    array_set_length( compute_dispatches, num_passes );

    // Cache pipelines and resources
    for ( uint32_t i = 0; i < num_passes; ++i ) {
        pipelines[ i ] = shader->pipelines[ i ];
        // Set layout internally
        resource_lists_[ i ].set_layout( shader->resource_layouts[ i ] );
        resource_lists[ i ] = gpu.create_resource_list( resource_lists_[ i ] );

        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( shader->hfx_binary->memory, i );
        compute_dispatches[ i ].x = pass_header->compute_dispatch.x;
        compute_dispatches[ i ].y = pass_header->compute_dispatch.y;
        compute_dispatches[ i ].z = pass_header->compute_dispatch.z;
    }
}

void Material2::shutdown( Device& gpu ) {

    for ( uint32_t i = 0; i < num_passes; ++i ) {
        gpu.destroy_resource_list( resource_lists[ i ] );
    }
}

void Material2::reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list ) {
    gpu.destroy_resource_list( resource_lists[ index ] );

    // Set layout internally
    resource_list.set_layout( shader->resource_layouts[ index ] );
    resource_lists[ index ] = gpu.create_resource_list( resource_list );
}

// RenderStage2 ///////////////////////////////////////////////////////////////////////////////////
void RenderStage2::init( Device& gpu, RenderStage2Creation& creation ) {

    // Cache from creation.
    clear = creation.clear;
    name = creation.render_pass_creation.name;
    type = creation.render_pass_creation.type;
    material = creation.material;
    material_pass_index = creation.material_pass_index;

    array_init( features );

    if ( type != RenderPassType::Swapchain ) {

        render_pass = gpu.create_render_pass( creation.render_pass_creation );
        output = gpu.get_render_pass_output( render_pass );

        // TODO: can be optimized
        gpu.fill_barrier( render_pass, barrier );

        TextureDescription output_desc;
        gpu.query_texture( creation.render_pass_creation.output_textures[ 0 ], output_desc );

        output_width = output_desc.width;
        output_height = output_desc.height;
        output_depth = output_desc.depth;
    } else {
        render_pass = gpu.get_swapchain_pass();
    }
}

void RenderStage2::shutdown( Device& gpu ) {

    if ( type != RenderPassType::Swapchain )
        gpu.destroy_render_pass( render_pass );
}

void RenderStage2::resize( Device& gpu, u16 width, u16 height ) {
    // Resize only non swapchain textures.
    // Compute could be resized as well ?
    if ( type != RenderPassType::Swapchain )
        gpu.resize_output_textures( render_pass, width, height );

    output_width = width;
    output_height = height;
}

void RenderStage2::set_material( Material2* material_, u16 index ) {
    material = material_;
    material4 = nullptr;
    material_pass_index = index;
}

void RenderStage2::set_material4( Material4* material_, u16 index ) {
    material = nullptr;
    material4 = material_;
    material_pass_index = index;
}

void RenderStage2::add_render_feature( RenderFeature* feature ) {
    array_push( features, feature );
}

void RenderStage2::render( Renderer& renderer, u64& sort_key, CommandBuffer* gpu_commands ) {

    gpu_commands->push_marker( name );

    switch ( type ) {
        case RenderPassType::Standard:
        {
            barrier.set( PipelineStage::FragmentShader, PipelineStage::RenderTarget );
            gpu_commands->barrier( barrier );

            clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            const sizet features_count = array_size( features );
            if ( features_count ) {
                for ( sizet i = 0; i < features_count; ++i ) {
                    features[ i ]->render( renderer, sort_key, gpu_commands );
                }
            } else if ( material ) {
                // Fullscreen tri
                gpu_commands->bind_pipeline( sort_key++, material->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ material_pass_index ], 1, 0, 0 );
                gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );
            }

            barrier.set( PipelineStage::RenderTarget, PipelineStage::FragmentShader );
            gpu_commands->barrier( barrier );

            break;
        }

        case RenderPassType::Compute:
        {
            barrier.set( PipelineStage::FragmentShader, PipelineStage::ComputeShader );
            gpu_commands->barrier( barrier );

            gpu_commands->bind_pass( sort_key++, render_pass );
            gpu_commands->bind_pipeline( sort_key++, material->pipelines[ material_pass_index ] );
            gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ material_pass_index ], 1, 0, 0 );

            const ComputeDispatch& dispatch = material->compute_dispatches[ material_pass_index ];
            gpu_commands->dispatch( sort_key++, ceilu32( output_width * 1.f / dispatch.x ), ceilu32( output_height * 1.f / dispatch.y ), ceilu32( output_depth * 1.f / dispatch.z ) );

            barrier.set( PipelineStage::ComputeShader, PipelineStage::FragmentShader );
            gpu_commands->barrier( barrier );
            break;
        }

        case RenderPassType::Swapchain:
        {
            clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, renderer.gpu->get_swapchain_pass() );

            if ( material ) {
                gpu_commands->set_scissor( sort_key++, nullptr );
                gpu_commands->set_viewport( sort_key++, nullptr );

                gpu_commands->bind_pipeline( sort_key++, material->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ material_pass_index ], 1, 0, 0 );
                gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );
            }

            break;
        }
    }

    gpu_commands->pop_marker();
}

}
}



namespace hydra {
namespace graphics {

ShaderEffect3Creation& ShaderEffect3Creation::reset() {
    num_passes = 0;
    return *this;
}

ShaderEffect3Creation& ShaderEffect3Creation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

ShaderEffect3Creation& ShaderEffect3Creation::set_hfx( cstring source, cstring binary, u32 options ) {
    hfx_source = source;
    hfx_binary = binary;
    hfx_options = options;
    return *this;
}

ShaderEffect3Creation& ShaderEffect3Creation::pass( cstring name_, cstring stage ) {
    stages[ num_passes ] = stage;
    render_passes[ num_passes++ ] = name_;
    return *this;
}

u8 ShaderEffect3Creation::get_pass_index( cstring name_ ) {
    for ( u32 i = 0; i < num_passes; ++i ) {
        if ( strcmp( render_passes[ i ], name_ ) == 0 ) {
            return ( u8 )i;
        }
    }
    return u8_max;
}


Material3Creation& Material3Creation::reset() {
    num_resources = num_passes = 0;
    return *this;
}

Material3Creation& Material3Creation::pass( cstring name_ ) {
    // Save current resource count
    pass_names[ num_passes ] = name_;
    resources_offset[ num_passes++ ] = ( u8 )num_resources;
    return *this;
}

Material3Creation& Material3Creation::add_buffer( cstring name_ ) {
    resources[ num_resources++ ] = name_;
    return *this;
}

Material3Creation& Material3Creation::add_texture_and_sampler( cstring texture, cstring sampler ) {
    resources[ num_resources++ ] = texture;
    resources[ num_resources++ ] = sampler;
    return *this;
}

Material3Creation& Material3Creation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

Material3Creation& Material3Creation::set_shader( cstring name_ ) {
    shader_effect = name_;
    return *this;
}

void Material3Creation::get_pass_data( cstring pass_name, u32& resource_offset, u32& resource_count ) {
    u8 pass_index = get_pass_index( pass_name );
    resource_offset = get_pass_resource_offset( pass_index );
    resource_count = get_pass_resource_count( pass_index );
}

// TODO: improve
u8 Material3Creation::get_pass_index( cstring name_ ) {
    for ( u32 i = 0; i < num_passes; ++i ) {
        if ( strcmp( pass_names[ i ], name_ ) == 0 ) {
            return ( u8 )i;
        }
    }
    return u8_max;
}

u32 Material3Creation::get_pass_resource_count( u8 pass_index ) {
    return ( pass_index < num_passes - 1 ) ? resources_offset[ pass_index + 1 ] - resources_offset[ pass_index ] : num_resources - resources_offset[ pass_index ];
}

u32 Material3Creation::get_pass_resource_offset( u8 pass_index ) {
    return resources_offset[ pass_index ];
}


RenderStage3Creation& RenderStage3Creation::reset() {
    num_outputs = 0;
    output_depth = nullptr;
    alias_output_name = nullptr;
    material_name = pass_name = nullptr;
    return *this;
}

RenderStage3Creation& RenderStage3Creation::add_render_texture( cstring name_ ) {
    outputs[ num_outputs++ ] = name_;
    return *this;
}
RenderStage3Creation& RenderStage3Creation::set_scaling( f32 scale_x_, f32 scale_y_, u8 resize_ ) {
    scale_x = scale_x_;
    scale_y = scale_y_;
    resize = resize_;
    return *this;
}
RenderStage3Creation& RenderStage3Creation::set_depth_stencil_texture( cstring name_ ) {
    output_depth = name_;
    return *this;
}

RenderStage3Creation& RenderStage3Creation::set_type( RenderPassType::Enum type_ ) {
    type = type_;
    return *this;
}

RenderStage3Creation& RenderStage3Creation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

RenderStage3Creation& RenderStage3Creation::set_material_and_pass( cstring material, cstring pass ) {
    material_name = material;
    pass_name = pass;
    return *this;
}



Texture3Creation& Texture3Creation::reset() {
    alias = nullptr;
    file = nullptr;
    return *this;
}

Texture3Creation& Texture3Creation::init_from_file( cstring file_, cstring name_ ) {
    file = file_;
    c.name = name_;
    return *this;
}

TextureCreation& Texture3Creation::init() {
    return c;
}




// ShaderEffect3 ////////////////////////////////////////////////////////////////
void ShaderEffect3::init( Device& gpu, hfx::ShaderEffectFile* hfx ) {

    hfx_binary = hfx;

    // First create arrays
    array_init( pipelines );
    array_init( resource_layouts );

    const u32 passes = hfx->header->num_passes;
    array_set_length( pipelines, passes );
    array_set_length( resource_layouts, passes );

    // Init
    for ( u32 i = 0; i < passes; ++i ) {
        pipelines[ i ] = k_invalid_pipeline;
        resource_layouts[ i ] = k_invalid_layout;
    }
}

void ShaderEffect3::shutdown( Device& gpu ) {
    const u32 passes = hfx_binary->header->num_passes;
    for ( u32 i = 0; i < passes; ++i ) {
        if ( pipelines[ i ].index != k_invalid_pipeline.index ) {
            gpu.destroy_pipeline( pipelines[ i ] );
        }

        if ( resource_layouts[ i ].index != k_invalid_layout.index ) {
            gpu.destroy_resource_layout( resource_layouts[ i ] );
        }
    }
}

u32 ShaderEffect3::pass_index( const char* name ) {
        // TODO: improve
    const u32 passes = hfx_binary->header->num_passes;
    for ( u32 i = 0; i < passes; ++i ) {

        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( hfx_binary->memory, i );
        if ( strcmp( name, pass_header->name ) == 0 ) {

            return i;
        }
    }

    return u32_max;
}

// Material3 ////////////////////////////////////////////////////////////////////
void Material3::init( Device& gpu, ShaderEffect3* shader_, ResourceListCreation* resource_lists_ ) {
}

void Material3::shutdown( Device& gpu ) {
}

void Material3::reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list ) {
}


// RenderGraph //////////////////////////////////////////////////////////////////

//
//
void RenderGraph::init() {
    array_init( render_stage_creations );
    array_init( material_creations );
    array_init( shader_creations );
    array_init( texture_creations );
    array_init( buffer_creations );
    array_init( sampler_creations );

    array_init( active_stages );
    array_init( active_materials );
    array_init( active_shaders );
    array_init( active_hfx );
    array_init( active_buffers );
    array_init( active_textures );
    array_init( active_samplers );

    array_init( resources_to_create );

    string_hash_init_arena( name_to_node );
    string_hash_set_default( name_to_node, u16_max );
}

//
//
void RenderGraph::init( Device& gpu ) {
    // Write any resource that needs to be created.
    // Save input/outputs as indices inside the master array.
    string_hash_init_arena( name_to_node );
    string_hash_set_default( name_to_node, u16_max );

    // Pass 1: cache all dependencies to be created.
    // TODO: It starts from the render stages, what about other material_creations/systems ?
    // NOTE: after every push memory can change, so using pointers can cause crashes.
    sizet count = array_size( render_stage_creations );
    for ( sizet i = 0; i < count; ++i ) {
        RenderStage3Creation& rsc = render_stage_creations[ i ];

        u16 stage_node_id = string_hash_get( name_to_node, rsc.name );
        // Stage node not found
        if ( stage_node_id == u16_max ) {
            // Add Stage Resource Node                

            // Create actual node
            ResourceNode stage_node = { Stage, ( u32 )( i ) };
            stage_node.reset();
            stage_node_id = add_resource_node( stage_node, rsc.name );
        }


        // Add Texture Output Nodes
        const u32 to = rsc.num_outputs;
        for ( u32 t = 0; t < to; ++t ) {
            ResourceCreation& res = string_hash_get( name_to_resource_creation, rsc.outputs[ t ] );
            if ( res.type == Texture ) {
                Texture3Creation& texture = texture_creations[ res.creation ];
                ResourceNode texture_node{ Texture, res.creation };
                texture_node.reset().add_input( stage_node_id );
                // Retrieve index
                u16 texture_id = add_resource_node( texture_node, texture.c.name );
                // Cache texture id into stage node output
                resources_to_create[ stage_node_id ].add_output( texture_id );
            }
        }

        if ( rsc.output_depth ) {
            ResourceCreation& res = string_hash_get( name_to_resource_creation, rsc.output_depth );
            if ( res.type == Texture ) {
                Texture3Creation& texture = texture_creations[ res.creation ];
                ResourceNode texture_node{ Texture, res.creation };
                texture_node.reset().add_input( stage_node_id );
                // Retrieve index
                u16 texture_id = add_resource_node( texture_node, texture.c.name );
                // Cache texture id into stage node output
                resources_to_create[ stage_node_id ].add_output( texture_id );
            }
        }

        // TODO: separate loaded passes ?
        // Add Material Nodes
        if ( rsc.material_name ) {
            // Retrieve material node
            ResourceCreation& material_res = string_hash_get( name_to_resource_creation, rsc.material_name );
            Material3Creation* material_creation = &material_creations[ material_res.creation ];

            u16 material_id = string_hash_get( name_to_node, rsc.material_name );
            // Material node not found
            if ( material_id == u16_max ) {
                // Add Material node both to creation and lookup
                ResourceNode material_node{ Material, material_res.creation };
                material_id = add_resource_node( material_node, rsc.material_name );

                // Retrieve shader
                ResourceCreation& shader_res = string_hash_get( name_to_resource_creation, material_creation->shader_effect );

                // If not present in the resource creation list, add it
                u16 shader_id = string_hash_get( name_to_node, material_creation->shader_effect );
                if ( shader_id == u16_max ) {
                    ResourceNode shader_node{ ShaderEffect, shader_res.creation };
                    shader_id = add_resource_node( shader_node, material_creation->shader_effect );
                }

                resources_to_create[ shader_id ].add_output( material_id );
                material_node.add_input( shader_id );
                // Add material node
                //
                resources_to_create[ material_id ] = material_node;
            }

            // Add dependencies from resource list
            if ( rsc.pass_name ) {
                u32 res_offset, res_count;
                material_creation->get_pass_data( rsc.pass_name, res_offset, res_count );

                for ( u32 ir = 0; ir < res_count; ++ir ) {
                    cstring resource_name = material_creation->resources[ res_offset + ir ];
                    ResourceCreation& resource = string_hash_get( name_to_resource_creation, resource_name );
                    u16 resource_id = string_hash_get( name_to_node, resource_name );
                    if ( resource_id == u16_max ) {

                        switch ( resource.type ) {
                            case Texture:
                            {
                                ResourceNode resource_node{ Texture, resource.creation };
                                resource_id = add_resource_node( resource_node, resource_name );
                                resources_to_create[ resource_id ].add_output( stage_node_id );
                                break;
                            }

                            case Buffer:
                            {
                                ResourceNode resource_node{ Buffer, resource.creation };
                                resource_id = add_resource_node( resource_node, resource_name );
                                resources_to_create[ resource_id ].add_output( stage_node_id );
                                break;
                            }

                            case Sampler:
                            {
                                ResourceNode resource_node{ Sampler, resource.creation };
                                resource_id = add_resource_node( resource_node, resource_name );
                                resources_to_create[ resource_id ].add_output( stage_node_id );
                                break;
                            }

                            default:
                            {
                                print_format( "STOLTO!\n" );
                                break;
                            }
                        }
                    }
                    // Add mutual dependencies between resource and stage for easy dependency walking.
                    if ( resource_id != u16_max ) {
                        resources_to_create[ resource_id ].add_output( stage_node_id );
                        resources_to_create[ stage_node_id ].add_input( resource_id );
                    }
                }
            }

            // Add material dependency
            resources_to_create[ stage_node_id ].add_input( material_id );
        }
    }

    // Pass 2: create resources and generate execution graph
    // Pass 2.1: first create graphics resources without low-level dependencies, like texture_creations, buffer_creations and render passes.
    count = array_size( resources_to_create );
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        switch ( node.type ) {
            case Texture:
            {
                Texture3Creation& t = texture_creations[ node.creation ];
                create_texture( gpu, node, t );

                break;
            }

            case Buffer:
            {
                BufferCreation& b = buffer_creations[ node.creation ];
                create_buffer( gpu, node, b );
                break;
            }

            case Sampler:
            {
                SamplerCreation& s = sampler_creations[ node.creation ];
                create_sampler( gpu, node, s );
                break;
            }

            default:
            {
                break;
            }
        }
    }

    // Pass 2.2: create stages
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        switch ( node.type ) {

            case Stage:
            {
                RenderStage3Creation& rsc = render_stage_creations[ node.creation ];
                create_stage( gpu, node, rsc );
                break;
            }

            default:
            {
                break;
            }
        }
    }

    // Pass 2.3: create shaders
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        switch ( node.type ) {

            case ShaderEffect:
            {
                ShaderEffect3Creation& sec = shader_creations[ node.creation ];
                create_shader( gpu, node, sec );
                break;
            }

            default:
            {
                break;
            }
        }
    }

    // Pass 2.4: create materials
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        switch ( node.type ) {

            case Material:
            {
                Material3Creation& m = material_creations[ node.creation ];
                create_material( gpu, node, m );
                break;
            }

            default:
            {
                break;
            }
        }
    }

    // TODO: set material to stage
    count = array_size( resources_to_create );
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        if ( node.type == Stage ) {
            RenderStage3Creation* rsc = &render_stage_creations[ node.creation ];
            print_format( "Stage %s\n", rsc->name );

            RenderStage2& stage = active_stages[ node.active_handle ];

            print_format( "\tInputs: " );
            const u32 si = node.num_inputs;
            for ( u32 s = 0; s < si; ++s ) {
                ResourceNode& input_node = resources_to_create[ node.inputs[ s ] ];
                switch ( input_node.type ) {
                    case Material:
                    {
                        Material3Creation* m = ( Material3Creation* )&material_creations[ input_node.creation ];
                        Material2* mat = &active_materials[ input_node.active_handle ];

                        u16 pass_index = mat->shader->pass_index( rsc->pass_name );
                        stage.set_material( mat, pass_index );
                        print_format( "material %s, ", m->name );

                        break;
                    }

                    case Texture:
                    {
                        Texture3Creation* t = ( Texture3Creation* )&texture_creations[ input_node.creation ];
                        print_format( "texture %s, ", t->c.name );

                        break;
                    }

                    case Buffer:
                    {
                        BufferCreation* b = ( BufferCreation* )&buffer_creations[ input_node.creation ];
                        print_format( "buffer %s, ", b->name );

                        break;
                    }
                }
            }

            print_format( "\n\tOutputs: " );
            const u32 to = node.num_outputs;
            for ( u32 t = 0; t < to; ++t ) {
                ResourceNode& texture_node = resources_to_create[ node.outputs[ t ] ];
                Texture3Creation* tc = ( Texture3Creation* )&texture_creations[ texture_node.creation ];

                print_format( "%s - ", tc->c.name );
            }

            print_format( "\n" );
        }
    }
    print_format( "DAJE!\n" );
}

//
//
void RenderGraph::shutdown( Device& gpu ) {

    sizet count = array_size( resources_to_create );
    for ( sizet i = 0; i < count; ++i ) {
        ResourceNode& node = resources_to_create[ i ];

        switch ( node.type ) {
            case Stage:
            {
                RenderStage2& stage = active_stages[ node.active_handle ];
                stage.shutdown( gpu );
                break;
            }

            case Material:
            {
                Material2& material = active_materials[ node.active_handle ];
                material.shutdown( gpu );
                break;
            }

            case Texture:
            {
                gpu.destroy_texture( { node.active_handle } );
                break;
            }

            case Buffer:
            {
                gpu.destroy_buffer( { node.active_handle } );
                break;
            }

            case ShaderEffect:
            {
                ShaderEffect2& shader = active_shaders[ node.active_handle ];
                shader.shutdown( gpu );

                hfx::shader_effect_shutdown( active_hfx[ node.active_handle ] );

                break;
            }

            case Sampler:
            {
                gpu.destroy_sampler( { node.active_handle } );
                break;
            }
        }
    }

    // Free resources to create
    array_free( resources_to_create );
    string_hash_free( name_to_node );

    array_free( active_stages );
    array_free( active_materials );
    array_free( active_shaders );
    array_free( active_hfx );
    array_free( active_buffers );
    array_free( active_textures );
    array_free( active_samplers );
}

//
//
void RenderGraph::resize( Device& gpu, u32 width, u32 height ) {
    const sizet count = array_size( active_stages );

    for ( sizet i = 0; i < count; ++i ) {
        RenderStage2& stage = active_stages[ i ];
        stage.resize( gpu, ( u16 )width, ( u16 )height );
    }

    // Recreate resource lists
    // TODO: understand WHICH material and WHICH pass need this!
    ResourceCreation& resource = string_hash_get( name_to_resource_creation, "pixel_art_material" );
    Material3Creation& creation = material_creations[ resource.creation ];
    Material2* material = get_material( "pixel_art_material" );
    ResourceListCreation rlc;

    for ( u32 i = 0; i < creation.num_passes; ++i ) {

        fill_resource_list( creation, creation.pass_names[ i ], rlc );

        const u32 rtl_index = material->shader->pass_index( creation.pass_names[ i ] );
        material->reload_resource_list( gpu, rtl_index, rlc );
    }
}

//
//
void RenderGraph::render( Renderer& renderer, u64& sort_key, CommandBuffer* gpu_commands ) {
    const sizet count = array_size( active_stages );

    for ( sizet i = 0; i < count; ++i ) {
        RenderStage2& stage = active_stages[ i ];
        stage.render( renderer, sort_key, gpu_commands );
    }
}

//
//
void RenderGraph::ui_draw( Device& gpu ) {

    // TODO:
    static ImNodes::CanvasState canvas{};

    ImNodes::BeginCanvas( &canvas );
    static bool selected[ 1000 ];
    static ImVec2 node_pos[ 1000 ];

    static ImNodes::Ez::SlotInfo inputs[] = { {"in_1", 1}, {"in_2", 2}, {"in_3", 3}, {"in_4", 4}, {"in_5", 5}, {"in_6", 6}, {"in_7", 7}, {"in_8", 8} };
    static ImNodes::Ez::SlotInfo outputs[] = { {"out_1", 1}, {"out_2", 2}, {"out_3", 3}, {"out_4", 4}, {"out_5", 5}, {"out_6", 6}, {"out_7", 7}, {"out_8", 8} };

    sizet count = array_size( active_stages );
    for ( sizet i = 0; i < count; ++i ) {
        RenderStage2& stage = active_stages[ i ];
        u16 node_index = string_hash_get( name_to_node, stage.name );
        ResourceNode* node = get_resource_node( stage.name );

        if ( ImNodes::Ez::BeginNode( node, stage.name, &node_pos[ node_index ], &selected[ node_index ] ) ) {

            ImNodes::Ez::InputSlots( inputs, node->num_inputs );
            ImNodes::Ez::OutputSlots( outputs, node->num_outputs );

            // Add output connections
            for ( u32 o = 0; o < node->num_outputs; ++o ) {
                ResourceNode* out_node = &resources_to_create[ node->outputs[ o ] ];
                ImNodes::Connection( out_node, inputs[ o ].title, node, outputs[ o ].title );
            }

        }

        ImNodes::Ez::EndNode();
    }

    count = array_size( active_textures );
    for ( sizet i = 0; i < count; ++i ) {
        TextureDescription td;
        gpu.query_texture( active_textures[ i ], td );

        u16 node_index = string_hash_get( name_to_node, td.name );
        ResourceNode* node = get_resource_node( td.name );
        if ( ImNodes::Ez::BeginNode( node, td.name, &node_pos[ node_index ], &selected[ node_index ] ) ) {

            ImNodes::Ez::InputSlots( inputs, node->num_inputs );
            ImNodes::Ez::OutputSlots( outputs, node->num_outputs );

            for ( u32 o = 0; o < node->num_inputs; ++o ) {
                ResourceNode* out_node = &resources_to_create[ node->inputs[ o ] ];
               // ImNodes::Connection( node, outputs[ o ].title, out_node, inputs[ o ].title );
            }

            for ( u32 o = 0; o < node->num_outputs; ++o ) {
                ResourceNode* out_node = &resources_to_create[ node->outputs[ o ] ];
                ImNodes::Connection( out_node, inputs[ o ].title, node, outputs[ o ].title );
            }
        }

        ImNodes::Ez::EndNode();
    }
    ImNodes::EndCanvas();

    /*
    if ( ImGui::BeginChild( "Active Resources" ) ) {
        if ( ImGui::TreeNode( "Stages" ) ) {
            const sizet count = array_size( active_stages );

            for ( sizet i = 0; i < count; ++i ) {
                RenderStage2& stage = active_stages[ i ];
                if ( ImGui::TreeNode( stage.name ) ) {

                    if ( ImGui::Button( "Print In Dep" ) ) {
                        print_input_dependencies( stage.name );
                    }

                    if ( ImGui::Button( "Print Out Dep" ) ) {
                        print_output_dependencies( stage.name );
                    }

                    ImGui::TreePop();
                }

            }
            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "Textures" ) ) {
            const sizet count = array_size( active_textures );

            for ( sizet i = 0; i < count; ++i ) {
                TextureHandle& handle = active_textures[ i ];
                TextureDescription desc;
                gpu.query_texture( handle, desc );
                if ( ImGui::TreeNode( desc.name ) ) {

                    if ( ImGui::Button( "Print In Dep" ) ) {
                        print_input_dependencies( desc.name );
                    }

                    if ( ImGui::Button( "Print Out Dep" ) ) {
                        print_output_dependencies( desc.name );
                    }

                    ImGui::TreePop();
                }

            }
            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "Materials" ) ) {
            const sizet count = array_size( active_materials );

            for ( sizet i = 0; i < count; ++i ) {
                Material2& material = active_materials[ i ];
                if ( ImGui::TreeNode( &material, "Material %u", i ) ) {

                    ImGui::TreePop();
                }

            }
            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "Shaders" ) ) {
            const sizet count = array_size( active_shaders );

            for ( sizet i = 0; i < count; ++i ) {
                ShaderEffect2& shader = active_shaders[ i ];
                if ( ImGui::TreeNode( &shader, "Shader %u", i ) ) {


                    ImGui::TreePop();
                }

            }
            ImGui::TreePop();
        }

        ImGui::EndChild();
    }

    // Stats
    sizet total_memory = array_size( buffer_creations ) * sizeof(BufferCreation) + array_size( material_creations ) * sizeof(Material3Creation) +
                         array_size( render_stage_creations ) * sizeof(RenderStage3Creation) + array_size( shader_creations) * sizeof(ShaderEffect3Creation) +
                        array_size( sampler_creations ) * sizeof(SamplerCreation) + array_size(texture_creations) * sizeof(Texture3Creation);

    ImGui::Separator();
    ImGui::LabelText( "", "Total memory %lluK", total_memory / 1024 );
    */
}

//
//
void RenderGraph::print_resource_name( ResourceNode& node, bool input ) {
    switch ( node.type ) {
        case RenderGraph::Texture:
        {
            Texture3Creation* rsc = &texture_creations[ node.creation ];
            print_format( "RT %s\n", rsc->c.name );

            if ( input ) {
                print_input_dependencies( rsc->c.name );
            } else {
                print_output_dependencies( rsc->c.name );
            }

            break;
        }

        case RenderGraph::Stage:
        {
            RenderStage3Creation* rsc = &render_stage_creations[ node.creation ];
            print_format( "Stage %s\n", rsc->name );
            if ( input ) {
                print_input_dependencies( rsc->name );
            } else {
                print_output_dependencies( rsc->name );
            }
            break;
        }
    }
}

//
//
void RenderGraph::print_output_dependencies( cstring name ) {

    ResourceNode* node = get_resource_node( name );
    if ( !node ) {
        return;
    }

    print_format( "%s Outputs:\n", name );
    u32 outs = node->num_outputs;
    for ( u32 i = 0; i < outs; ++i ) {
        ResourceNode& out_node = resources_to_create[ node->outputs[ i ] ];
        print_resource_name( out_node, false );
    }
}

//
//
void RenderGraph::print_input_dependencies( cstring name ) {
    ResourceNode* node = get_resource_node( name );
    if ( !node ) {
        return;
    }

    print_format( "%s Inputs:\n", name );
    u32 ins = node->num_inputs;
    for ( u32 i = 0; i < ins; ++i ) {
        ResourceNode& out_node = resources_to_create[ node->inputs[ i ] ];
        print_resource_name( out_node, true );
    }
}

//
//
u16 RenderGraph::add_resource_node( ResourceNode node, cstring node_name ) {
    u16 node_index = ( u16 )array_size( resources_to_create );
    array_push( resources_to_create, node );
    string_hash_put( name_to_node, node_name, node_index );
    return node_index;
}

//
//
RenderGraph::ResourceNode* RenderGraph::get_resource_node( cstring name ) {
    u16 node_index = string_hash_get( name_to_node, name );
    if ( node_index != u16_max ) {
        ResourceNode* resource_node = &resources_to_create[ node_index ];
        return resource_node;
    }
    return nullptr;
}

//
//
void RenderGraph::set_stage_material_pass( cstring stage_name, cstring material_pass_name ) {

    ResourceNode* stage_node = get_resource_node( stage_name );
    if ( stage_node && stage_node->type == Stage ) {
        //RenderStage3Creation* rsc = &render_stage_creations[stage_node->creation];
        RenderStage2& stage = active_stages[ stage_node->active_handle ];
        const u32 si = stage_node->num_inputs;
        for ( u32 s = 0; s < si; ++s ) {
            ResourceNode& input_node = resources_to_create[ stage_node->inputs[ s ] ];
            if ( input_node.type == Material ) {
                Material2* mat = &active_materials[ input_node.active_handle ];

                u16 pass_index = mat->shader->pass_index( material_pass_name );
                if ( pass_index != u16_max ) {
                    stage.set_material( mat, pass_index );

                    return;
                }
            }
        }
    }
}

//
//
void RenderGraph::set_material_pass_texture_sampler( Device& gpu, cstring material_, cstring material_pass, cstring texture, cstring sampler ) {

    ResourceNode* node = get_resource_node( material_ );
    if ( node && node->type == Material ) {
        Material3Creation* mat = &material_creations[ node->creation ];
        Material2* material = &active_materials[ node->active_handle ];

        ResourceListCreation rlc;
        u32 res_offset, res_count;
        mat->get_pass_data( material_pass, res_offset, res_count );

        for ( u32 ir = 0; ir < res_count; ++ir ) {
            cstring resource_name = mat->resources[ res_offset + ir ];
            if ( ResourceNode* resource_node = get_resource_node( resource_name ) ) {
                switch ( resource_node->type ) {
                    case Texture:
                    {
                        TextureHandle t = { resource_node->active_handle };
                        // Search for sampler as well
                        // TODO:
                        // use new sampler
                        if ( strcmp( resource_name, texture ) == 0 ) {
                            mat->resources[ res_offset + ir + 1 ] = sampler;
                        }

                        cstring sampler_name = mat->resources[ res_offset + ir + 1 ];

                        if ( ResourceNode* sampler_node = get_resource_node( sampler_name ) ) {
                            SamplerHandle s = { sampler_node->active_handle };
                            rlc.texture_sampler( t, s, rlc.num_resources );
                        } else {
                            rlc.texture( t, rlc.num_resources );
                        }

                        break;
                    }

                    case Buffer:
                    {
                        BufferHandle b = { resource_node->active_handle };
                        rlc.buffer( b, rlc.num_resources );
                        break;
                    }

                    case Sampler:
                    {
                        break;
                    }
                }
            }
        }

        const u32 rtl_index = material->shader->pass_index( material_pass );
        material->reload_resource_list( gpu, rtl_index, rlc );
    }
}

//
//
void RenderGraph::fill_resource_list( Material3Creation& creation, cstring pass_name, ResourceListCreation& rlc ) {
    u32 res_offset, res_count;
    creation.get_pass_data( pass_name, res_offset, res_count );

    rlc.reset();
    for ( u32 ir = 0; ir < res_count; ++ir ) {
        cstring resource_name = creation.resources[ res_offset + ir ];
        if ( ResourceNode* resource_node = get_resource_node( resource_name ) ) {
            switch ( resource_node->type ) {
                case Texture:
                {
                    TextureHandle t = { resource_node->active_handle };
                    // Search for sampler as well
                    cstring sampler_name = creation.resources[ res_offset + ir + 1 ];
                    if ( ResourceNode* sampler_node = get_resource_node( sampler_name ) ) {
                        SamplerHandle s = { sampler_node->active_handle };
                        rlc.texture_sampler( t, s, rlc.num_resources );
                    } else {
                        rlc.texture( t, rlc.num_resources );
                    }

                    break;
                }

                case Buffer:
                {
                    BufferHandle b = { resource_node->active_handle };
                    rlc.buffer( b, rlc.num_resources );
                    break;
                }

                case Sampler:
                {
                    break;
                }
            }
        }
    }
}

//
//
void RenderGraph::add_shader( ShaderEffect3Creation& data ) {
    ResourceCreation new_resource{ ShaderEffect, ( u32 )array_size( shader_creations ) };
    array_push( shader_creations, data );
    NameToResourceCreation nn{ ( char* )data.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::add_material( Material3Creation& data ) {
    ResourceCreation new_resource{ Material, ( u32 )array_size( material_creations ) };
    array_push( material_creations, data );
    NameToResourceCreation nn{ ( char* )data.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::add_stage( RenderStage3Creation& data ) {
    ResourceCreation new_resource{ Stage, ( u32 )array_size( render_stage_creations ) };
    array_push( render_stage_creations, data );
    NameToResourceCreation nn{ ( char* )data.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::add_texture( Texture3Creation& data ) {
    ResourceCreation new_resource{ Texture, ( u32 )array_size( texture_creations ) };
    array_push( texture_creations, data );
    NameToResourceCreation nn{ ( char* )data.c.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::add_buffer( BufferCreation& data ) {
    ResourceCreation new_resource{ Buffer, ( u32 )array_size( buffer_creations ) };
    array_push( buffer_creations, data );
    NameToResourceCreation nn{ ( char* )data.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::add_sampler( SamplerCreation& data ) {
    ResourceCreation new_resource{ Sampler, ( u32 )array_size( sampler_creations ) };
    array_push( sampler_creations, data );
    NameToResourceCreation nn{ ( char* )data.name, new_resource };
    string_hash_put_structure( name_to_resource_creation, nn );
}

//
//
void RenderGraph::create_texture( Device& gpu, ResourceNode& node, Texture3Creation& creation ) {

    TextureHandle handle = k_invalid_texture;
    if ( creation.file ) {
        int comp, width, height;
        uint8_t* image_data = stbi_load( creation.file, &width, &height, &comp, 4 );
        creation.c.set_data( image_data ).set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, 0 )
            .set_size( ( u16 )width, ( u16 )height, 1 );

    }
    handle = gpu.create_texture( creation.c );

    node.active_handle = handle.index;
    array_push( active_textures, handle );
}

//
//
void RenderGraph::create_buffer( Device& gpu, ResourceNode& node, BufferCreation& creation ) {
    BufferHandle handle = gpu.create_buffer( creation );
    node.active_handle = handle.index;
    array_push( active_buffers, handle );
}

//
//
void RenderGraph::create_sampler( Device& gpu, ResourceNode& node, SamplerCreation& creation ) {
    SamplerHandle handle = gpu.create_sampler( creation );
    node.active_handle = handle.index;
    array_push( active_samplers, handle );
}

//
//
void RenderGraph::create_stage( Device& gpu, ResourceNode& node, RenderStage3Creation& creation ) {

    u32 index = ( u32 )array_size( active_stages );
    node.active_handle = index;

    array_push( active_stages, RenderStage2() );
    RenderStage2& rs = array_last( active_stages );

    if ( creation.type == RenderPassType::Swapchain ) {
        render_stage_init_as_swapchain( gpu, rs, creation.clear, creation.name );
    } else {
        RenderStage2Creation rsc;
        rsc.render_pass_creation.reset().set_scaling( creation.scale_x, creation.scale_y, creation.resize ).set_name( creation.name );
        rsc.clear = creation.clear;

        // First retrieve eventual depth
        TextureHandle depth_texture = k_invalid_texture;

        if ( creation.output_depth ) {
            depth_texture = get_texture( creation.output_depth );
            rsc.render_pass_creation.set_depth_stencil_texture( depth_texture );
        }
        
        // Node outputs contains also depth, skip it.
        const u32 si = node.num_outputs;
        for ( u32 s = 0; s < si; ++s ) {
            ResourceNode& output_node = resources_to_create[ node.outputs[ s ] ];
            switch ( output_node.type ) {
                case Texture:
                {
                    TextureHandle handle = { output_node.active_handle };
                    // Skip depth texture as render texture.
                    if ( handle.index != depth_texture.index )
                        rsc.render_pass_creation.add_render_texture( handle );

                    break;
                }
            }
        }


        rs.init( gpu, rsc );
    }
}

//
//
void RenderGraph::create_shader( Device& gpu, ResourceNode& node, ShaderEffect3Creation& creation ) {

    // Add HFX and compile
    array_push( active_hfx, hfx::ShaderEffectFile() );
    hfx::ShaderEffectFile* hfx_file = &array_last( active_hfx );
    hfx::hfx_compile( creation.hfx_source, creation.hfx_binary, creation.hfx_options, hfx_file );

    u32 index = ( u32 )array_size( active_shaders );
    node.active_handle = index;
    array_push( active_shaders, ShaderEffect2() );
    ShaderEffect2& shader = array_last( active_shaders );

    // Gather render passes handles
    RenderPassOutput render_passes[ 8 ];
    for ( u32 i = 0; i < creation.num_passes; ++i ) {
        u16 pass_index = hfx::shader_effect_get_pass_index( *hfx_file, creation.render_passes[ i ] );

        if ( ResourceNode* stage_node = get_resource_node( creation.stages[ i ] ) ) {
            RenderStage2& render_stage = active_stages[ stage_node->active_handle ];
            render_passes[ pass_index ] = render_stage.output;
        }
    }
    // get render pass output from render stage
    shader.init( gpu, hfx_file, render_passes );
}

//
//
void RenderGraph::create_material( Device& gpu, ResourceNode& node, Material3Creation& creation ) {

    u32 index = ( u32 )array_size( active_materials );
    node.active_handle = index;
    array_push( active_materials, Material2() );
    Material2& material = array_last( active_materials );

    // Gather shader
    if ( ResourceNode* shader_node = get_resource_node( creation.shader_effect ) ) {
        ShaderEffect2* shader = &active_shaders[ shader_node->active_handle ];

        ResourceListCreation rlc[ 8 ];

        for ( u32 i = 0; i < creation.num_passes; ++i ) {

            const u32 rtl_index = shader->pass_index( creation.pass_names[ i ] );
            fill_resource_list( creation, creation.pass_names[ i ], rlc[ rtl_index ] );
        }

        material.init( gpu, shader, rlc );
    }
}

//
//
TextureHandle RenderGraph::get_texture( cstring name ) {
    const u16 index = string_hash_get( name_to_node, name );
    TextureHandle return_handle = k_invalid_texture;

    if ( index != u16_max ) {
        const RenderGraph::ResourceNode& resource_node = resources_to_create[ index ];
        return_handle.index = resource_node.active_handle;
    }

    return return_handle;
}

//
//
BufferHandle RenderGraph::get_buffer( cstring name ) {
    const u16 index = string_hash_get( name_to_node, name );
    BufferHandle return_handle = k_invalid_buffer;

    if ( index != u16_max ) {
        const RenderGraph::ResourceNode& resource_node = resources_to_create[ index ];
        return_handle.index = resource_node.active_handle;
    }

    return return_handle;
}

Material2* RenderGraph::get_material( cstring name ) {
    const u16 index = string_hash_get( name_to_node, name );
    if ( index != u16_max ) {
        const RenderGraph::ResourceNode& resource_node = resources_to_create[ index ];
        Material2* return_material = &active_materials[ resource_node.active_handle ];
        return return_material;
    }

    return nullptr;
}

RenderStage2* RenderGraph::get_stage( cstring name ) {
    const ResourceNode* node = get_resource_node( name );

    if ( node ) {
        RenderStage2* stage = &active_stages[ node->active_handle ];
        return stage;
    }

    return nullptr;
}

SamplerHandle RenderGraph::get_sampler( cstring name ) {
    const u16 index = string_hash_get( name_to_node, name );
    SamplerHandle return_handle = k_invalid_sampler;

    if ( index != u16_max ) {
        const RenderGraph::ResourceNode& resource_node = resources_to_create[ index ];
        return_handle.index = resource_node.active_handle;
    }

    return return_handle;
}



} // namespace graphics
} // namespace hydra
