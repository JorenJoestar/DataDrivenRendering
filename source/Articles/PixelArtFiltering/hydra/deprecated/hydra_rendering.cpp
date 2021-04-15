//
//  Hydra Rendering - v0.26

#include "hydra_rendering.h"

#include "hydra_shaderfx.h"
#include "imgui\imgui.h"

#include <math.h>

#if defined (HYDRA_RENDERING_HIGH_LEVEL) || defined(HYDRA_RENDERING_CAMERA)
    #include "cglm/struct/mat4.h"
    #include "cglm/struct/cam.h"
    #include "cglm/struct/affine.h"
    #include "cglm/struct/quat.h"
    #include "cglm/struct/project.h"
#endif // HYDRA_RENDERING_HIGH_LEVEL

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define HYDRA_RENDERING_VERBOSE

namespace hydra {
namespace graphics {

    
//
// 64 Distinct Colors. Used for graphs and anything that needs random colors.
// 
static const uint32_t k_distinct_colors[] = { 
    0xFF000000, 0xFF00FF00, 0xFFFF0000, 0xFF0000FF, 0xFFFEFF01, 0xFFFEA6FF, 0xFF66DBFF, 0xFF016400,
    0xFF670001, 0xFF3A0095, 0xFFB57D00, 0xFFF600FF, 0xFFE8EEFF, 0xFF004D77, 0xFF92FB90, 0xFFFF7600,
    0xFF00FFD5, 0xFF7E93FF, 0xFF6C826A, 0xFF9D02FF, 0xFF0089FE, 0xFF82477A, 0xFFD22D7E, 0xFF00A985,
    0xFF5600FF, 0xFF0024A4, 0xFF7EAE00, 0xFF3B3D68, 0xFFFFC6BD, 0xFF003426, 0xFF93D3BD, 0xFF17B900,
    0xFF8E009E, 0xFF441500, 0xFF9F8CC2, 0xFFA374FF, 0xFFFFD001, 0xFF544700, 0xFFFE6FE5, 0xFF318278,
    0xFFA14C0E, 0xFFCBD091, 0xFF7099BE, 0xFFE88A96, 0xFF0088BB, 0xFF2C0043, 0xFF74FFDE, 0xFFC6FF00,
    0xFF02E5FF, 0xFF000E62, 0xFF9C8F00, 0xFF52FF98, 0xFFB14475, 0xFFFF00B5, 0xFF78FF00, 0xFF416EFF,
    0xFF395F00, 0xFF82686B, 0xFF4EAD5F, 0xFF4057A7, 0xFFD2FFA5, 0xFF67B1FF, 0xFFFF9B00, 0xFFBE5EE8
};

u32 ColorUint::get_distinct_color( u32 index ) {
    return k_distinct_colors[ index % 64 ];
}

//
//
void pipeline_create( Device& gpu, hfx::ShaderEffectFile& hfx, u32 pass_index, const RenderPassHandle& pass_handle, PipelineHandle& out_pipeline, ResourceLayoutHandle* out_layouts, u32 num_layouts ) {

    hydra::graphics::PipelineCreation render_pipeline;
    hfx::shader_effect_get_pipeline( hfx, pass_index, render_pipeline );

    hydra::graphics::ResourceLayoutCreation rll_creation{};

    for ( u32 i = 0; i < num_layouts; i++ ) {

        hfx::shader_effect_get_resource_list_layout( hfx, pass_index, i, rll_creation );
        out_layouts[i] = gpu.create_resource_layout( rll_creation );

        // TODO: improve
        // num active layout is already set to the max, so using add_rll breaks.
        render_pipeline.resource_layout[ i ] = out_layouts[ i ];
    }

    render_pipeline.render_pass = pass_handle;

    out_pipeline = gpu.create_pipeline( render_pipeline );
}

//
//
TextureHandle create_texture_from_file( Device& gpu, cstring filename ) {
    if ( filename ) {
        int comp, width, height;
        uint8_t* image_data = stbi_load( filename, &width, &height, &comp, 4 );
        TextureCreation creation;
        creation.set_data( image_data ).set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, 0 )
                .set_size( ( u16 )width, ( u16 )height, 1 );

        return gpu.create_texture( creation );
    }
    
    return k_invalid_texture;
}

//
//
void render_stage_init_as_swapchain( Device& gpu, RenderStage2& out_stage, ClearData& clear, const char* name ) {
    out_stage.clear = clear;
    out_stage.name = name;
    out_stage.type = RenderPassType::Swapchain;
}

    
// ShaderResourcesDatabase //////////////////////////////////////////////////////
void ShaderResourcesDatabase::init() {
    string_hash_init_arena( name_to_buffer );
    string_hash_init_arena( name_to_texture );
    string_hash_init_arena( name_to_sampler );
}

void ShaderResourcesDatabase::terminate() {
    string_hash_free( name_to_buffer );
    string_hash_free( name_to_texture );
    string_hash_free( name_to_sampler );
}

void ShaderResourcesDatabase::register_buffer( char* name, BufferHandle buffer ) {
    string_hash_put( name_to_buffer, name, buffer );
}

void ShaderResourcesDatabase::register_texture( char* name, TextureHandle texture ) {
    string_hash_put( name_to_texture, name, texture );
}

void ShaderResourcesDatabase::register_sampler( char* name, SamplerHandle sampler ) {
    string_hash_put( name_to_sampler, name, sampler );
}

BufferHandle ShaderResourcesDatabase::find_buffer( char* name ) {

    return string_hash_get( name_to_buffer, name );
}

TextureHandle ShaderResourcesDatabase::find_texture( char* name ) {

    return string_hash_get( name_to_texture, name );
}

SamplerHandle ShaderResourcesDatabase::find_sampler( char* name ) {
    return string_hash_get( name_to_sampler, name );
}

// ShaderResourcesLookup ////////////////////////////////////////////////////////

void ShaderResourcesLookup::init() {
    string_hash_init_arena( binding_to_resource );
    string_hash_init_arena( binding_to_specialization );
    string_hash_init_arena( binding_to_sampler );
}

void ShaderResourcesLookup::terminate() {
    string_hash_free( binding_to_resource );
    string_hash_free( binding_to_specialization );
    string_hash_free( binding_to_sampler );
}

void ShaderResourcesLookup::add_binding_to_resource( char* binding, char* resource ) {
    string_hash_put( binding_to_resource, binding, resource );
}

void ShaderResourcesLookup::add_binding_to_specialization( char* binding, Specialization specialization ) {
    string_hash_put( binding_to_specialization, binding, specialization );
}

void ShaderResourcesLookup::add_binding_to_sampler( char* binding, char* sampler ) {
    string_hash_put( binding_to_sampler, binding, sampler );
}

char* ShaderResourcesLookup::find_resource( char* binding ) {
    return string_hash_get( binding_to_resource, binding );
}

ShaderResourcesLookup::Specialization ShaderResourcesLookup::find_specialization( char* binding ) {
    return string_hash_get( binding_to_specialization, binding );
}

char* ShaderResourcesLookup::find_sampler( char* binding ) {
    return string_hash_get( binding_to_sampler, binding );
}

void ShaderResourcesLookup::specialize( char* pass, char* view, ShaderResourcesLookup& final_lookup ) {

    // TODO
    final_lookup.init();
    // Copy hash maps ?
}

// ShaderEffect4Creation ////////////////////////////////////////////////////////

ShaderEffect4Creation& ShaderEffect4Creation::reset() {
    num_passes = 0;
    return *this;
}

ShaderEffect4Creation& ShaderEffect4Creation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

ShaderEffect4Creation& ShaderEffect4Creation::set_hfx( cstring source, cstring binary, u32 options ) {
    hfx_source = source;
    hfx_binary = binary;
    hfx_options = options;
    return *this;
}

ShaderEffect4Creation& ShaderEffect4Creation::pass( cstring name_, RenderPassHandle stage ) {
    stages[ num_passes ] = stage;
    render_passes[ num_passes++ ] = name_;
    return *this;
}

// Material4Creation ////////////////////////////////////////////////////////////
Material4Creation& Material4Creation::start( ShaderEffect4* shader ) {
    num_resources = num_passes = 0;
    shader_effect = shader;
    return *this;
}

Material4Creation& Material4Creation::pass( cstring name_ ) {
    // Save current resource count
    pass_names[ num_passes ] = name_;
    resources_offset[ num_passes++ ] = ( u8 )num_resources;
    // Move num_resources to the max amount of resources per current pass
    num_resources += shader_effect->get_max_resources_per_pass( name_ );
    return *this;
}

Material4Creation& Material4Creation::set_buffer( cstring name_, BufferHandle buffer ) {
    // Search buffer index
    u16 resource_index = shader_effect->resource_index( pass_names[ num_passes - 1 ], name_ );
    // Resources are allocated with the max num resources per pass.
    u8 resource_offset = resources_offset[ num_passes - 1 ];
    resources[ resource_offset + resource_index ] = buffer.index;
    return *this;
}

Material4Creation& Material4Creation::set_texture_and_sampler( cstring texture_name, TextureHandle texture, cstring sampler_name, SamplerHandle sampler ) {
    u16 resource_index = shader_effect->resource_index( pass_names[ num_passes - 1 ], texture_name );
    u8 resource_offset = resources_offset[ num_passes - 1 ];
    resources[ resource_offset + resource_index ] = texture.index;
    
    return *this;
}

Material4Creation& Material4Creation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

// ShaderEffect4 ////////////////////////////////////////////////////////////////
void ShaderEffect4::init( Device& gpu, ShaderEffect4Creation& creation ) {

    hfx_binary = ( hfx::ShaderEffectFile* )malloc( sizeof( hfx::ShaderEffectFile ) );
    hfx::hfx_compile( creation.hfx_source, creation.hfx_binary, creation.hfx_options, hfx_binary );
    // Allocate for all shaders declared in the HFX file.
    const u32 passes = hfx_binary->header->num_passes;
    // First create arrays
    array_init( pipelines );
    array_init( resource_layouts );

    array_set_length( pipelines, passes );
    array_set_length( resource_layouts, passes );

    // Init hash map containing both pass and all resources indices.
    string_hash_init_arena( name_to_index );
    string_hash_set_default( name_to_index, u16_max );

    char buffer[ 64 ];
    // Iterate over available passes
    for ( u32 p = 0; p < creation.num_passes; ++p ) {
        // Add pass index to map
        u16 pass_index = hfx::shader_effect_get_pass_index( *hfx_binary, creation.render_passes[p] );
        sprintf( buffer, "pass_%s", creation.render_passes[ p ] );
        string_hash_put( name_to_index, buffer, pass_index );

        hydra::graphics::PipelineCreation render_pipeline;
        hfx::shader_effect_get_pipeline( *hfx_binary, p, render_pipeline );

        hydra::graphics::ResourceLayoutCreation rll_creation{};

        for ( u32 i = 0; i < 1; i++ ) {

            hfx::shader_effect_get_resource_layout( *hfx_binary, pass_index, i, rll_creation, name_to_index );
            resource_layouts[ pass_index ] = gpu.create_resource_layout( rll_creation );

            // TODO: improve
            // num active layout is already set to the max, so using add_rll breaks.
            render_pipeline.resource_layout[ i ] = resource_layouts[ pass_index ];
        }

        render_pipeline.render_pass = creation.stages[p];

        pipelines[ pass_index ] = gpu.create_pipeline( render_pipeline );
    }
}

void ShaderEffect4::shutdown( Device& gpu ) {

    free( hfx_binary );
}

u32 ShaderEffect4::pass_index( cstring name_ ) {
    return hfx::shader_effect_get_pass_index( *hfx_binary, name_ );
}

u16 ShaderEffect4::resource_index( cstring pass_name, cstring resource_name ) {
    char buffer[ 64 ];
    sprintf( buffer, "%s_%s", pass_name, resource_name );
    return string_hash_get(name_to_index, buffer);
}

u16 ShaderEffect4::get_max_resources_per_pass( cstring pass_name ) {
    u16 pass_index = hfx::shader_effect_get_pass_index( *hfx_binary, pass_name );
    hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( hfx_binary->memory, pass_index );

    u8 num_bindings;
    const hydra::graphics::ResourceLayoutCreation::Binding* bindings = hfx::shader_effect_pass_get_layout_bindings( pass_header, 0, num_bindings );
    return num_bindings;
}

// Material4 ////////////////////////////////////////////////////////////////////

void Material4::init( Device& gpu, const Material4Creation& creation ) {
    shader = creation.shader_effect;
    num_passes = shader->hfx_binary->header->num_passes;
    // First create arrays
    array_init( pipelines );
    array_init( resource_lists );
    array_init( compute_dispatches );

    array_set_length( pipelines, num_passes );
    array_set_length( resource_lists, num_passes );
    array_set_length( compute_dispatches, num_passes );

    // Cache pipelines and resources
    for ( uint32_t i = 0; i < creation.num_passes; ++i ) {
        u16 pass_index = shader->pass_index( creation.pass_names[ i ] );
        u16 resource_offset = creation.resources_offset[ i ];
        pipelines[ pass_index ] = shader->pipelines[ pass_index ];
        // Set layout internally
        ResourceListCreation rlc;
        u32 num_resources = (i == creation.num_passes - 1) ? creation.num_resources - creation.resources_offset[ i ] :  creation.resources_offset[ i + 1 ] - creation.resources_offset[ i ];
        for ( u32 r = 0; r < num_resources; ++r ) {
            rlc.resources[ r ] = creation.resources[ resource_offset + r ];
            rlc.samplers[ r ] = k_invalid_sampler;
        }
        rlc.num_resources = num_resources;
        rlc.set_layout( shader->resource_layouts[ pass_index ] );
        resource_lists[ pass_index ] = gpu.create_resource_list( rlc );

        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( shader->hfx_binary->memory, pass_index );
        compute_dispatches[ pass_index ].x = pass_header->compute_dispatch.x;
        compute_dispatches[ pass_index ].y = pass_header->compute_dispatch.y;
        compute_dispatches[ pass_index ].z = pass_header->compute_dispatch.z;
    }

}

void Material4::shutdown( Device& gpu ) {
    for ( uint32_t i = 0; i < num_passes; ++i ) {
        gpu.destroy_resource_list( resource_lists[ i ] );
    }
}

void Material4::reload_resource_list( Device& gpu, u32 index, ResourceListCreation& resource_list ) {
    gpu.destroy_resource_list( resource_lists[ index ] );

    // Set layout internally
    resource_list.set_layout( shader->resource_layouts[ index ] );
    resource_lists[ index ] = gpu.create_resource_list( resource_list );
}

// ShaderEffect /////////////////////////////////////////////////////////////////

void ShaderEffect::init( const hfx::ShaderEffectFile& shader_effect_file ) {

    memcpy( name, shader_effect_file.header->name, 32 );

    local_constants_size = shader_effect_file.local_constants_size;
    local_constants_default_data = shader_effect_file.local_constants_default_data;
    num_properties = shader_effect_file.num_properties;
    properties_data = shader_effect_file.properties_data;
    num_passes = (u16)shader_effect_file.header->num_passes;

    array_init( passes );
    array_set_length( passes, num_passes );
}

// Material /////////////////////////////////////////////////////////////////////

void Material::load_resources( ShaderResourcesDatabase& db, Device& device ) {

    using namespace hydra::graphics;

    for ( size_t i = 0; i < array_size(passes); ++i ) {

        const ShaderEffectPass& shader_pass = effect->passes[i];
        const PipelineCreation& pipeline_creation = shader_pass.pipeline_creation;
        MaterialPass& material_pass = passes[i];
        
        ResourceHandle resources_handles[k_max_resources_per_list];

        for ( u32 l = 0; l < pipeline_creation.num_active_layouts; ++l ) {
            // Get resource layout description
            ResourceLayoutDescription layout;
            device.query_resource_layout( pipeline_creation.resource_layout[l], layout );

            // For each resource
            for ( uint32_t r = 0; r < layout.num_active_bindings; r++ ) {
                const ResourceBinding& binding = layout.bindings[r];

                // Find resource name
                // Copy string_buffer 
                char* resource_name = lookups.find_resource( (char*)binding.name );

                switch ( binding.type ) {
                    case hydra::graphics::ResourceType::Constants:
                    case hydra::graphics::ResourceType::Buffer:
                    {
#if defined (HYDRA_RENDERING_VERBOSE)
                        if ( !resource_name ) {
                            hydra::print_format( "Missing resource lookup for binding %s. Using dummy resource.\n", binding.name );
                            resources_handles[r] = device.get_dummy_constant_buffer().index;
                        } else {
                            BufferHandle handle = db.find_buffer( resource_name );
                            if ( handle.index == 0 ) {
                                hydra::print_format( "Missing buffer for resource %s, binding %s.\n", resource_name, binding.name );
                                handle = device.get_dummy_constant_buffer();
                            }

                            resources_handles[r] = handle.index;
                        }
#else
                        BufferHandle handle = resource_name ? database.find_buffer( resource_name ) : device.get_dummy_constant_buffer();
                        resources_handles[r].index = handle.index;
#endif // HYDRA_RENDERING_VERBOSE

                        break;
                    }

                    case hydra::graphics::ResourceType::Texture:
                    case hydra::graphics::ResourceType::Image:
                    {
#if defined (HYDRA_RENDERING_VERBOSE)
                        if ( !resource_name ) {
                            hydra::print_format( "Missing resource lookup for binding %s. Using dummy resource.\n", binding.name );
                            resources_handles[r] = device.get_dummy_texture().index;
                        } else {
                            TextureHandle handle = db.find_texture( resource_name );
                            if ( handle.index == 0 ) {
                                hydra::print_format( "Missing texture for resource %s, binding %s.\n", resource_name, binding.name );
                                handle = device.get_dummy_texture();
                            }

                            char* sampler_name = lookups.find_sampler( (char*)binding.name );
                            if ( sampler_name ) {
                                SamplerHandle sampler_handle = db.find_sampler( sampler_name );
                                // Set sampler, opengl only!
                                // TODO:
#if defined (HYDRA_OPENGL)
                                device.link_texture_sampler( handle, sampler_handle );
#endif // HYDRA_OPENGL
                            }

                            resources_handles[r] = handle.index;
                        }
#else
                        TextureHandle handle = resource_name ? database.find_texture( resource_name ) : device.get_dummy_texture();
                        resources_handles[r].handle = handle.handle;
#endif // HYDRA_RENDERING_VERBOSE

                        break;
                    }

                    // In OpenGL this is useless
                    //case hydra::graphics::ResourceType::Sampler:
                    //{
                    //    SamplerHandle handle = database.find_sampler( resource_name );
                    //    resources_handles[r].handle = handle.handle;
                    //    hydra::print_format( "Missing sampler for resource %s, binding %s.\n", resource_name, binding.name );
                    //    break;
                    //}

                    default:
                    {
                        break;
                    }
                }
            }

            // Create ResourceList
            ResourceListCreation creation {};
            creation.set_layout( pipeline_creation.resource_layout[l] ).set_name(this->name);
            for ( u32 rh = 0; rh < layout.num_active_bindings; ++rh ) {
                creation.resources[rh] = resources_handles[ rh ];
            }
            material_pass.resource_lists[l] = device.create_resource_list( creation );
        }

        material_pass.num_resource_lists = pipeline_creation.num_active_layouts;
        material_pass.pipeline = shader_pass.pipeline_handle;
    }
}

// TODO: IN DEVELOPMENT


// ShaderEffect2 //////////////////////////////////////////////////////////////////////////////////
void ShaderEffect2::init( Device& gpu, hfx::ShaderEffectFile* hfx, RenderPassHandle* render_passes ) {

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


// ClearData //////////////////////////////////////////////////////////////////////////////////////
void ClearData::bind( u64& sort_key, CommandBuffer* gpu_commands ) {

    if ( needs_color_clear )
        gpu_commands->clear( sort_key++, clear_color.x, clear_color.y, clear_color.z, clear_color.w );

    if ( needs_depth_clear || needs_stencil_clear )
        gpu_commands->clear_depth_stencil( sort_key++, depth_value, stencil_value );
}

ClearData& ClearData::reset() {
    needs_color_clear = needs_depth_clear = needs_stencil_clear = 0;
    return *this;
}

ClearData& ClearData::set_color( vec4s color ) {
    clear_color = color;
    needs_color_clear = 1;
    return *this;
}

ClearData& ClearData::set_depth( f32 depth ) {
    depth_value = depth;
    needs_depth_clear = 1;
    return *this;
}

ClearData& ClearData::set_stencil( u8 stencil ) {
    stencil_value = stencil;
    needs_stencil_clear = 1;
    return *this;
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

        // TODO: can be optimized
        gpu.fill_barrier( render_pass, barrier );

        TextureDescription output_desc;
        gpu.query_texture( creation.render_pass_creation.output_textures[ 0 ], output_desc );

        output_width = output_desc.width;
        output_height = output_desc.height;
        output_depth = output_desc.depth;
    }
    else {
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

void RenderStage2::render( Device& gpu, u64& sort_key, CommandBuffer* gpu_commands ) {

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
                    features[ i ]->render( gpu, sort_key, gpu_commands );
                }
            } else if ( material ) {
                // Fullscreen tri
                gpu_commands->bind_pipeline( sort_key++, material->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ material_pass_index ], 1, 0, 0 );
                gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );
            }
            else if ( material4 ) {
                // Fullscreen tri
                gpu_commands->bind_pipeline( sort_key++, material4->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material4->resource_lists[ material_pass_index ], 1, 0, 0 );
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

            gpu_commands->bind_pass( sort_key++, gpu.get_swapchain_pass() );

            if ( material ) {
                gpu_commands->set_scissor( sort_key++, nullptr );
                gpu_commands->set_viewport( sort_key++, nullptr );

                gpu_commands->bind_pipeline( sort_key++, material->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ material_pass_index ], 1, 0, 0 );
                gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );
            }
            else if ( material4 ) {
                gpu_commands->set_scissor( sort_key++, nullptr );
                gpu_commands->set_viewport( sort_key++, nullptr );

                gpu_commands->bind_pipeline( sort_key++, material4->pipelines[ material_pass_index ] );
                gpu_commands->bind_resource_list( sort_key++, &material4->resource_lists[ material_pass_index ], 1, 0, 0 );
                gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );
            }

            break;
        }
    }

    gpu_commands->pop_marker();
}

// RenderFrame ///////////////////////////////////////////////////////////////

//
//
void RenderFrame::init( ShaderResourcesDatabase* initial_db ) {

    string_hash_init_arena( name_to_stage );
    string_hash_init_arena( name_to_texture );

    resource_database.init();
    resource_lookup.init();

    if ( initial_db ) {
        for ( size_t i = 0; i < string_hash_size( initial_db->name_to_buffer ); i++ ) {
            ShaderResourcesDatabase::BufferStringMap& buffer = initial_db->name_to_buffer[i];
            resource_database.register_buffer( buffer.key, buffer.value );
        }

        for ( size_t i = 0; i < string_hash_size( initial_db->name_to_texture ); i++ ) {
            ShaderResourcesDatabase::TextureStringMap& texture = initial_db->name_to_texture[i];
            resource_database.register_texture( texture.key, texture.value );
        }

        for ( size_t i = 0; i < string_hash_size( initial_db->name_to_sampler ); i++ ) {
            ShaderResourcesDatabase::SamplerStringMap& sampler = initial_db->name_to_sampler[i];
            resource_database.register_sampler( sampler.key, sampler.value );
        }
    }
}

//
//
void RenderFrame::terminate( Device& device ) {

    for ( size_t i = 0; i < string_hash_size( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->terminate();
    }

    for ( size_t i = 0; i < string_hash_size( name_to_texture ); i++ ) {
        TextureHandle texture = name_to_texture[i].value;
        device.destroy_texture( texture );
    }
}

//
//
void RenderFrame::update() {
}

//
//
void RenderFrame::render( Device& device, CommandBuffer* commands ) {

    for ( size_t i = 0; i < string_hash_size( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->begin( device, commands );
        stage->render( device, commands );
        stage->end( device, commands );
    }
}

//
//
void RenderFrame::load_resources( Device& device ) {

    for ( size_t i = 0; i < string_hash_size( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->load_resources( resource_database, device );
    }
}

//
//
void RenderFrame::resize( uint16_t width, uint16_t height, Device& device ) {

    for ( size_t i = 0; i < string_hash_size( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->resize( width, height, device );
    }
}

// RenderStage //////////////////////////////////////////////////////////////////

//
//
void RenderStage::init() {

    array_init( render_managers );

    render_pass = k_invalid_pass;
}

//
//
void RenderStage::terminate() {
    // Destroy render pass
}

//
//
void RenderStage::begin( Device& device, CommandBuffer* commands ) {
    // Render Pass Begin
    uint64_t sort_key = SortKey::get_key( render_pass_index );

    commands->bind_pass( sort_key++, render_pass );
    hydra::graphics::Viewport viewport{ 0, 0, current_width, current_height, 0.0f, 1.0f };
    commands->set_viewport( sort_key++, &viewport );
    
    if ( clear_rt ) {
        commands->clear( sort_key++, clear_color[0], clear_color[1], clear_color[2], clear_color[3] );
    }

    if ( clear_depth || clear_stencil ) {
        commands->clear_depth_stencil( sort_key++, clear_depth_value, clear_stencil_value );
    }

    current_sort_key = sort_key;
    // Set render stage states (depth, alpha, ...)
}

//
//
void RenderStage::render( Device& device, CommandBuffer* commands ) {
    // For each manager, render
    uint64_t sort_key = current_sort_key;
    // TODO: for now use the material and the pass specified
    if ( material ) {
        MaterialPass& material_pass = material->passes[material_pass_index];
        switch ( type ) {

            case Post:
            {
                commands->bind_pipeline( sort_key++, material_pass.pipeline );
                commands->bind_resource_list( sort_key++, &material_pass.resource_lists[0], material_pass.num_resource_lists, nullptr, 0 );
                //commands->bind_vertex_buffer( device.get_fullscreen_vertex_buffer() );
                commands->draw( sort_key++, graphics::TopologyType::Triangle, 0, 3, 0, 1 );
                break;
            }

            case PostCompute:
            {
                commands->bind_pipeline( sort_key++, material_pass.pipeline );
                commands->bind_resource_list( sort_key++, &material_pass.resource_lists[0], material_pass.num_resource_lists, nullptr, 0 );
                commands->dispatch( sort_key++, (uint8_t)ceilf( current_width / 32.0f ), (uint8_t)ceilf( current_height / 32.0f ), 1 );
                break;
            }

            case Swapchain:
            {
                commands->bind_pipeline( sort_key++, material_pass.pipeline );
                commands->bind_resource_list( sort_key++, &material_pass.resource_lists[0], material_pass.num_resource_lists, nullptr, 0 );
                //commands->bind_vertex_buffer( gpu_device.get_fullscreen_vertex_buffer() );
                commands->draw( sort_key++, graphics::TopologyType::Triangle, 0, 3, 0, 1 );
                break;
            }
        }
    }
    else if ( type == Geometry ) {
        // Go through all visible elements in render view and draw them using their respective managers.
#if defined (HYDRA_RENDERING_HIGH_LEVEL)
        // Theoretically should sort all render scenes per manager and then submit them.
        uint32_t render_scenes = array_size( render_view->visible_render_scenes );
        RenderManager* render_manager = nullptr;
        if ( render_scenes ) {
            if ( render_view->visible_render_scenes[0].stage_mask.value == geometry_stage_mask ) {
                render_manager = render_view->visible_render_scenes[0].render_manager;
            }
        }

        if ( render_manager ) {
            RenderManager::RenderContext render_context = { &device, render_view, commands, render_view->visible_render_scenes, 0, render_scenes, 0 };
            render_manager->render( render_context );
        }
#endif // HYDRA_RENDERING_HIGH_LEVEL
    }
}

//
//
void RenderStage::end( Device& device, CommandBuffer* commands ) {
    // TODO: Post render (for always submitting managers)
    //for ( uint32_t i = 0; i < array_length( render_managers ); ++i ) {
    //    RenderManager* render_manager = render_managers[i];

    //    RenderManager::RenderContext render_context = { &device, render_view, commands, render_view ? render_view->visible_render_scenes : nullptr, 0, 0, 0 };
    //    render_manager->render( render_context );
    //}

    //// Render Pass End
    //commands->begin_submit( 0 );
    //commands->end_pass();
    //commands->end_submit();
}

//
//
void RenderStage::load_resources( ShaderResourcesDatabase& db, Device& device ) {

    // Create render pass
    if ( render_pass.index == k_invalid_index ) {
        RenderPassCreation creation = {};
        switch ( type ) {
            case Geometry:
            {
                break;
            }

            case Post:
            {
                creation.type = RenderPassType::Standard;
                break;
            }

            case PostCompute:
            {
                creation.type = RenderPassType::Compute;
                break;
            }

            case Swapchain:
            {
                creation.type = RenderPassType::Swapchain;
                break;
            }
        }

        creation.num_render_targets = num_output_textures;
        for ( uint32_t rt = 0; rt < num_output_textures; ++rt )
            creation.output_textures[rt] = output_textures[rt];

        creation.depth_stencil_texture = depth_texture;

        render_pass = device.create_render_pass( creation );
    }

    if ( resize_output ) {
        current_width = ceilu16( device.swapchain_width * scale_x );
        current_height = ceilu16( device.swapchain_height * scale_y );
    }

    if ( material ) {
        material->load_resources( db, device );
    }
}

//
//
void RenderStage::resize( uint16_t width, uint16_t height, Device& device ) {

    if ( !resize_output ) {
        return;
    }

    uint16_t new_width = (uint16_t)(width * scale_x);
    uint16_t new_height = (uint16_t)(height * scale_y);

    if ( new_width != current_width || new_height != current_height ) {
        current_width = new_width;
        current_height = new_height;

        device.resize_output_textures( render_pass, new_width, new_height );
    }
}

//
//
void RenderStage::register_render_manager( RenderManager* manager ) {

    array_push( render_managers, manager );
}

// Renderer /////////////////////////////////////////////////////////////////////


void Renderer::init( const RendererCreation& creation ) {

    gpu_device = creation.gpu_device;
    render_frame = nullptr;
}

void Renderer::terminate() {

    if ( render_frame ) {
        destroy_render_frame( render_frame );
    }
}

ShaderEffect* Renderer::create_shader( const ShaderCreation& creation ) {

    return nullptr;
}

Material* Renderer::create_material( const MaterialCreation& creation ) {

    using namespace hydra::graphics;

    return nullptr;
}

RenderFrame* Renderer::create_render_frame( const RenderFrameCreation& creation ) {
    return nullptr;
}

void Renderer::destroy_shader( ShaderEffect* effect ) {

}

void Renderer::destroy_material( Material* material ) {

}

void Renderer::destroy_render_frame( RenderFrame* _render_frame ) {

}

#if defined(HYDRA_RENDERING_CAMERA)

// Camera ///////////////////////////////////////////////////////////////////////


void Camera::init_perpective( f32 near_plane_, f32 far_plane_, f32 fov_y, f32 aspect_ratio_ ) {
    perspective = true;

    near_plane = near_plane_;
    far_plane = far_plane_;
    field_of_view_y = fov_y;
    aspect_ratio = aspect_ratio_;

    reset();
}

void Camera::init_orthographic( f32 near_plane_, f32 far_plane_, f32 viewport_width_, f32 viewport_height_, f32 zoom_ ) {
    perspective = false;

    near_plane = near_plane_;
    far_plane = far_plane_;

    viewport_width = viewport_width_;
    viewport_height = viewport_height_;
    zoom = zoom_;

    reset();
}

void Camera::reset() {
    position = glms_vec3_zero();
    yaw = 0;
    pitch = 0;
    view = glms_mat4_identity();
    projection = glms_mat4_identity();

    update_projection = true;
}

void Camera::set_viewport_size( f32 width_, f32 height_ ) {
    viewport_width = width_;
    viewport_height = height_;

    update_projection = true;
}

void Camera::set_zoom( f32 zoom_ ) {
    zoom = zoom_;

    update_projection = true;
}

void Camera::set_aspect_ratio( f32 aspect_ratio_ ) {
    aspect_ratio = aspect_ratio_;

    update_projection = true;
}

void Camera::set_fov_y( f32 fov_y_ ) {
    field_of_view_y = fov_y_;

    update_projection = true;
}

void Camera::update() {

    // Left for reference.
    // Calculate rotation from yaw and pitch
    /*direction.x = sinf( ( yaw ) ) * cosf( ( pitch ) );
    direction.y = sinf( ( pitch ) );
    direction.z = cosf( ( yaw ) ) * cosf( ( pitch ) );
    direction = glms_vec3_normalize( direction );

    vec3s center = glms_vec3_sub( position, direction );
    vec3s cup{ 0,1,0 };
    
    right = glms_cross( cup, direction );
    up = glms_cross( direction, right );

    // Calculate view matrix
    view = glms_lookat( position, center, up );
    */

    // Quaternion based rotation
    const versors pitch_rotation = glms_quat( pitch, 1, 0, 0 );
    const versors yaw_rotation = glms_quat( yaw, 0, 1, 0 );
    const versors rotation = glms_quat_normalize( glms_quat_mul( pitch_rotation, yaw_rotation ) );

    const mat4s translation = glms_translate_make( glms_vec3_scale( position, -1.f) );
    view = glms_mat4_mul( glms_quat_mat4( rotation ), translation );
    
    // Update the vectors used for movement
    right =     { view.m00, view.m10, view.m20 };
    up =        { view.m01, view.m11, view.m21 };
    direction = { view.m02, view.m12, view.m22 };

    if ( update_projection ) {
        update_projection = false;

        if ( perspective ) {
            projection = glms_perspective( glm_rad( field_of_view_y ), aspect_ratio, near_plane, far_plane );
        }
        else {
            projection = glms_ortho( zoom * -viewport_width / 2.f, zoom * viewport_width / 2.f, zoom * -viewport_height / 2.f, zoom * viewport_height / 2.f, near_plane, far_plane );
        }
    }
    
    // Calculate final view projection matrix
    view_projection = glms_mat4_mul( projection, view );
}

void Camera::rotate( f32 delta_pitch, f32 delta_yaw ) {

    pitch += delta_pitch;
    yaw += delta_yaw;
}

vec3s Camera::unproject( const vec3s& screen_coordinates ) {
    return glms_unproject( screen_coordinates, view_projection, { 0, 0, viewport_width, viewport_height } );
}

void Camera::yaw_pitch_from_direction( const vec3s& direction, f32& yaw, f32& pitch ) {

    yaw = glm_deg( atan2f( direction.z, direction.x ) );
    pitch = glm_deg( asinf( direction.y ) );
}

#endif // HYDRA_RENDERING_CAMERA

#if defined (HYDRA_RENDERING_HIGH_LEVEL)
// SceneRenderer ////////////////////////////////////////////////////////////////

static void render_mesh( hydra::graphics::CommandBuffer* commands, const hydra::graphics::Mesh& mesh, uint32_t node_id, BufferHandle transformBuffer ) {
    for ( uint32_t i = 0; i < array_length( mesh.sub_meshes ); ++i ) {
        const hydra::graphics::SubMesh& sub_mesh = mesh.sub_meshes[i];

        hydra::graphics::MaterialPass& material_pass = sub_mesh.material->shader_instances[0];

        commands->begin_submit( 0 );
        commands->bind_pipeline( material_pass.pipeline );

        //uint32_t offsets[2] = { 0, node_id * sizeof(hmm_mat4) };
        commands->bind_resource_list( material_pass.resource_lists, material_pass.num_resource_lists, 0, 0 );

        for ( uint32_t vb = 0; vb < array_length( sub_mesh.vertex_buffers ); ++vb ) {
            commands->bind_vertex_buffer( sub_mesh.vertex_buffers[vb], vb, sub_mesh.vertex_buffer_offsets[vb] );
        }

        commands->bind_vertex_buffer( transformBuffer, 3, 0 );
        commands->bind_index_buffer( sub_mesh.index_buffer );

        commands->draw_indexed( hydra::graphics::TopologyType::Triangle, sub_mesh.end_index, 1, sub_mesh.start_index, 0, node_id );

        commands->end_submit();
    }
}

static void render_node( hydra::graphics::CommandBuffer* commands, const hydra::graphics::RenderNode& node, BufferHandle transformBuffer ) {

    if ( node.mesh ) {
        render_mesh( commands, *node.mesh, node.node_id, transformBuffer );
    }
}

static void render_scene_nodes( hydra::graphics::CommandBuffer* commands, const hydra::graphics::RenderScene& scene ) {

    const uint32_t node_count = array_length( scene.nodes );
    for ( uint32_t i = 0; i < node_count; ++i ) {
        render_node( commands, scene.nodes[i], scene.node_transforms_buffer );
    }
}

void SceneRenderer::render( RenderContext& render_context ) {

    for ( uint32_t i = render_context.start; i < render_context.count; ++i ) {
        RenderScene& scene = render_context.render_scene_array[i];

        render_scene_nodes( render_context.commands, scene );
    }
}

// LineRenderer /////////////////////////////////////////////////////////////////

struct LinVertex {
    vec3s                           position;
    uint32_t                        color;

    void                            set( float x, float y, float z, uint32_t color );
};

struct LinVertex2D {
    vec2s                           position;
    uint32_t                        color;
};

struct LocalConstants {
    mat4s                           view_projection;
    mat4s                           projection;
    vec4s                           resolution;
};


static const uint32_t k_max_lines = 10000;

static LinVertex s_line_buffer[k_max_lines];
static LinVertex2D s_line_buffer_2d[k_max_lines];

void LineRenderer::init( ShaderResourcesDatabase& db, Device& device ) {

    // Buffer of 3D points (3d position + color)
    BufferCreation vb_creation = { BufferType::Vertex, ResourceUsageType::Dynamic, sizeof(LinVertex) * k_max_lines, nullptr, "VB_Lines" };
    lines_vb = device.create_buffer( vb_creation );
    // Buffer of 2D points
    BufferCreation vb_creation_2d = { BufferType::Vertex, ResourceUsageType::Dynamic, sizeof(LinVertex2D) * k_max_lines, nullptr, "VB_Lines_2d" };
    lines_vb_2d = device.create_buffer( vb_creation_2d );

    BufferCreation cb_creation = { BufferType::Constant, ResourceUsageType::Dynamic, sizeof(LocalConstants), nullptr, "CB_Lines" };
    lines_cb = device.create_buffer( cb_creation );

    db.register_buffer( (char*)cb_creation.name, lines_cb );

    current_line_index = 0;
    current_line_index_2d = 0;
}

void LineRenderer::terminate( Device& device ) {
}

void LineRenderer::line( const vec3s& from, const vec3s& to, uint32_t color0, uint32_t color1 ) {
    if ( current_line_index >= k_max_lines )
        return;

    s_line_buffer[current_line_index++].set( from.x, from.y, from.z, color0 );
    s_line_buffer[current_line_index++].set( to.x, to.y, to.z, color1 );
}

void LineRenderer::line_2d( const vec2s& from, const vec2s& to, uint32_t color0, uint32_t color1 ) {
    if ( current_line_index_2d >= k_max_lines )
        return;

    s_line_buffer_2d[current_line_index_2d++] = { from.x, from.y, color0 };
    s_line_buffer_2d[current_line_index_2d++] = { to.x, to.y, color1 };
}

void LineRenderer::box( const Box& box, uint32_t color ) {
    
    const float x0 = box.min.x;
    const float y0 = box.min.y;
    const float z0 = box.min.z;
    const float x1 = box.max.x;
    const float y1 = box.max.y;
    const float z1 = box.max.z;

    line( {x0, y0, z0}, {x0, y1, z0}, color, color );
    line( {x0, y1, z0}, {x1, y1, z0}, color, color );
    line( {x1, y1, z0}, {x1, y0, z0}, color, color );
    line( {x1, y0, z0}, {x0, y0, z0}, color, color );
    line( {x0, y0, z0}, {x0, y0, z1}, color, color );
    line( {x0, y1, z0}, {x0, y1, z1}, color, color );
    line( {x1, y1, z0}, {x1, y1, z1}, color, color );
    line( {x1, y0, z0}, {x1, y0, z1}, color, color );
    line( {x0, y0, z1}, {x0, y1, z1}, color, color );
    line( {x0, y1, z1}, {x1, y1, z1}, color, color );
    line( {x1, y1, z1}, {x1, y0, z1}, color, color );
    line( {x1, y0, z1}, {x0, y0, z1}, color, color );
}

void LinVertex::set( float x, float y, float z, uint32_t color ) {
    position = { x, y, z };
    this->color = color;
}


void LineRenderer::render( RenderContext& render_context ) {

    Device& device = *render_context.device;

    // Update camera matrix
    const Camera& camera = render_context.render_view->camera;

    MapBufferParameters cb_map = { lines_cb, 0, 0 };
    
    float L = 0, T = 0;
    float R = device.swapchain_width, B = device.swapchain_height;
    const float ortho_projection[4][4] =
    {
        { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
    };

    LocalConstants* cb_data = (LocalConstants*)device.map_buffer( cb_map );
    if ( cb_data ) {
        cb_data->view_projection = camera.view_projection;
        
        memcpy( &cb_data->projection, &ortho_projection, 64 );
        
        cb_data->resolution = { device.swapchain_width * 1.0f, device.swapchain_height * 1.0f, 1.0f / device.swapchain_width, 1.0f / device.swapchain_height };
        device.unmap_buffer( cb_map );
    }

    if ( current_line_index ) {
        const uint32_t mapping_size = sizeof( LinVertex ) * current_line_index;
        MapBufferParameters map_parameters_vb = { lines_vb, 0, mapping_size };
        LinVertex* vtx_dst = (LinVertex*)device.map_buffer( map_parameters_vb );
        
        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer[0], mapping_size );
            
            device.unmap_buffer( map_parameters_vb );
        }

        CommandBuffer* commands = render_context.commands;
        commands->begin_submit( 2 );

        MaterialPass& material_pass = line_material->shader_instances[3];
        commands->bind_pipeline( material_pass.pipeline );
        commands->bind_resource_list( material_pass.resource_lists, material_pass.num_resource_lists, nullptr, 0 );
        commands->bind_vertex_buffer( lines_vb, 0, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        commands->draw( TopologyType::Triangle, 0, num_vertices, current_line_index / 2 );
        commands->end_submit();

        current_line_index = 0;
    }

    if ( current_line_index_2d ) {
        const uint32_t mapping_size = sizeof( LinVertex2D ) * current_line_index_2d;
        MapBufferParameters map_parameters_vb = { lines_vb, 0, mapping_size };
        LinVertex* vtx_dst = (LinVertex*)device.map_buffer( map_parameters_vb );

        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer_2d[0], mapping_size );

            device.unmap_buffer( map_parameters_vb );
        }

        CommandBuffer* commands = render_context.commands;
        commands->begin_submit( 2 );

        MaterialPass& material_pass = line_material->shader_instances[4];
        commands->bind_pipeline( material_pass.pipeline );
        commands->bind_resource_list( material_pass.resource_lists, material_pass.num_resource_lists, nullptr, 0 );
        commands->bind_vertex_buffer( lines_vb, 0, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        commands->draw( TopologyType::Triangle, 0, num_vertices, current_line_index_2d / 2 );
        commands->end_submit();

        current_line_index_2d = 0;
    }
}

// LightingManager //////////////////////////////////////////////////////////////

struct LightingConstants {

    vec3s                           directional_light;
    uint32_t                        use_point_light;

    vec3s                           camera_position;
    float                           pad1;

    float                           depth_constants[2];
    float                           resolution_rcp[2];

    vec3s                           point_light_position;
    float                           point_light_intensity;

    mat4s                           inverse_view_projection;
};

void LightingManager::init( ShaderResourcesDatabase& db, Device& device ) {

    BufferCreation cb_creation = { BufferType::Constant, ResourceUsageType::Dynamic, sizeof(LightingConstants), nullptr, "lighting_constants" };
    lighting_cb = device.create_buffer( cb_creation );

    db.register_buffer( (char*)cb_creation.name, lighting_cb );

    SamplerCreation sampler_creation{ TextureFilter::Linear, TextureFilter::Linear, TextureMipFilter::Linear, TextureAddressMode::Clamp_Border, TextureAddressMode::Clamp_Border, TextureAddressMode::Clamp_Border, "linear" };
    SamplerHandle sampler = device.create_sampler( sampler_creation );

    db.register_sampler( (char*)sampler_creation.name, sampler );

    point_light_position = { 0,0,1 };
    point_light_intensity = 1.0f;

    directional_light = { 0, 0.7, 0.7 };

    use_point_light = false;
}

void LightingManager::terminate( Device& device ) {

}

void LightingManager::render( RenderContext& render_context ) {

    Device& device = *render_context.device;
    MapBufferParameters cb_map = { lighting_cb, 0, 0 };
    LightingConstants* cb_data = (LightingConstants*)device.map_buffer( cb_map );
    if ( cb_data ) {

        cb_data->directional_light = glms_normalize( directional_light );
        cb_data->use_point_light = use_point_light;

        const Camera& camera = render_context.render_view->camera;

        cb_data->camera_position = camera.position;

        cb_data->point_light_position = point_light_position;
        cb_data->point_light_intensity = point_light_intensity;

        //vec2 depth_consts = vec2( 1 - far / near, far / near );
        cb_data->depth_constants[0] = 1 - (camera.far_plane / camera.near_plane);
        cb_data->depth_constants[1] = camera.far_plane / camera.near_plane;

        cb_data->resolution_rcp[0] = 1.0f / render_context.device->swapchain_width;
        cb_data->resolution_rcp[1] = 1.0f / render_context.device->swapchain_height;

        cb_data->inverse_view_projection = glms_mat4_inv( camera.view_projection );
        
        device.unmap_buffer( cb_map );
    }

}

// Math
bool ray_box_intersection( const hydra::graphics::Box& box, const hydra::graphics::Ray& ray, float &t ) {
    // r.dir is unit direction vector of ray
    vec3s dirfrac;
    dirfrac.x = 1.0f / ray.direction.x;
    dirfrac.y = 1.0f / ray.direction.y;
    dirfrac.z = 1.0f / ray.direction.z;
    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (box.min.x - ray.origin.x)*dirfrac.x;
    float t2 = (box.max.x - ray.origin.x)*dirfrac.x;
    float t3 = (box.min.y - ray.origin.y)*dirfrac.y;
    float t4 = (box.max.y - ray.origin.y)*dirfrac.y;
    float t5 = (box.min.z - ray.origin.z)*dirfrac.z;
    float t6 = (box.max.z - ray.origin.z)*dirfrac.z;

    float tmin = glm_max( glm_max( glm_min( t1, t2 ), glm_min( t3, t4 ) ), glm_min( t5, t6 ) );
    float tmax = glm_min( glm_min( glm_max( t1, t2 ), glm_max( t3, t4 ) ), glm_max( t5, t6 ) );

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if ( tmax < 0 )
    {
        t = tmax;
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if ( tmin > tmax )
    {
        t = tmax;
        return false;
    }

    t = tmin;
    return true;
}

#endif // HYDRA_RENDERING_HIGH_LEVEL

// GPU task names to colors
struct NameColorMap {
    char*       key;
    u32         value;
};

NameColorMap*   name_to_color;

static u32      initial_frames_paused = 3;

void GPUProfiler::init( u32 max_frames_ ) {

    max_frames = max_frames_;
    timestamps = ( GPUTimestamp* )malloc( sizeof( GPUTimestamp ) * max_frames * 32 );
    per_frame_active = ( u16* )malloc( sizeof( u16 )* max_frames );

    max_duration = 16.666f;

    memset( per_frame_active, 0, 2 * max_frames );

    string_hash_init_arena( name_to_color );
    string_hash_set_default( name_to_color, 0xffff );
}

void GPUProfiler::shutdown() {

    string_hash_free( name_to_color );

    free( timestamps );
    free( per_frame_active );
}

void GPUProfiler::update( Device& gpu ) {

    gpu.set_gpu_timestamps_enable( !paused );

    if ( initial_frames_paused ) {
        --initial_frames_paused;
        return;
    }

    if ( paused )
        return;

    u32 active_timestamps = gpu.get_gpu_timestamps( &timestamps[ 32 * current_frame ] );
    per_frame_active[ current_frame ] = (u16)active_timestamps;

    // Get colors
    for ( u32 i = 0; i < active_timestamps; ++i ) {
        GPUTimestamp& timestamp = timestamps[ 32 * current_frame + i ];

        u32 color_index = string_hash_get( name_to_color, timestamp.name );
        // No entry found, add new color
        if ( color_index == 0xffff ) {

            color_index = (u32)string_hash_size( name_to_color );
            string_hash_put( name_to_color, timestamp.name, color_index );
        }

        timestamp.color = ColorUint::get_distinct_color( color_index );
    }

    current_frame = ( current_frame + 1 ) % max_frames;

    // Reset Min/Max/Average after few frames
    if ( current_frame == 0 ) {
        max_time = -FLT_MAX;
        min_time = FLT_MAX;
        average_time = 0.f;
    }
}

void GPUProfiler::draw_ui() {

    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        f32 widget_height = canvas_size.y - 100;

        f32 legend_width = 200;
        f32 graph_width = canvas_size.x - legend_width;
        u32 rect_width = ceilu32( graph_width / max_frames );
        i32 rect_x = ceili32(graph_width - rect_width);

        f64 new_average = 0;

        ImGuiIO& io = ImGui::GetIO();

        static char buf[ 128 ];

        const ImVec2 mouse_pos = io.MousePos;

        i32 selected_frame = -1;

        // Draw time reference lines
        sprintf( buf, "%3.4fms", max_duration );
        draw_list->AddText( { cursor_pos.x, cursor_pos.y }, 0xff0000ff, buf );
        draw_list->AddLine( { cursor_pos.x + rect_width, cursor_pos.y }, { cursor_pos.x + graph_width, cursor_pos.y }, 0xff0000ff );

        sprintf( buf, "%3.4fms", max_duration / 2.f );
        draw_list->AddText( { cursor_pos.x, cursor_pos.y + widget_height / 2.f }, 0xff00ffff, buf );
        draw_list->AddLine( { cursor_pos.x + rect_width, cursor_pos.y + widget_height / 2.f }, { cursor_pos.x + graph_width, cursor_pos.y + widget_height / 2.f }, 0xff00ffff );

        // Draw Graph
        for ( u32 i = 0; i < max_frames; ++i ) {
            u32 frame_index = ( current_frame - 1 - i ) % max_frames;

            f32 frame_x = cursor_pos.x + rect_x;
            GPUTimestamp* frame_timestamps = &timestamps[ frame_index * 32 ];
            f32 frame_time = (f32)frame_timestamps[ 0 ].elapsed_ms;
            // Clamp values to not destroy the frame data
            frame_time = glm_clamp( frame_time, 0.00001f, 1000.f );
            // Update timings
            new_average += frame_time;
            min_time = min( min_time, frame_time );
            max_time = max( max_time, frame_time );

            f32 rect_height = frame_time / max_duration * widget_height;
            //drawList->AddRectFilled( { frame_x, cursor_pos.y + rect_height }, { frame_x + rect_width, cursor_pos.y }, 0xffffffff );

            for ( u32 j = 0; j < per_frame_active[ frame_index ]; ++j ) {
                const GPUTimestamp& timestamp = frame_timestamps[ j ];

                /*if ( timestamp.depth != 1 ) {
                    continue;
                }*/

                rect_height = (f32)timestamp.elapsed_ms / max_duration * widget_height;
                draw_list->AddRectFilled( { frame_x, cursor_pos.y + widget_height - rect_height },
                                          { frame_x + rect_width, cursor_pos.y + widget_height }, timestamp.color );
            }

            if ( mouse_pos.x >= frame_x && mouse_pos.x < frame_x + rect_width &&
                 mouse_pos.y >= cursor_pos.y && mouse_pos.y < cursor_pos.y + widget_height ) {
                draw_list->AddRectFilled( { frame_x, cursor_pos.y + widget_height },
                                          { frame_x + rect_width, cursor_pos.y }, 0x0fffffff );

                ImGui::SetTooltip( "(%u): %f", frame_index, frame_time );

                selected_frame = frame_index;
            }

            draw_list->AddLine( { frame_x, cursor_pos.y + widget_height }, { frame_x, cursor_pos.y }, 0x0fffffff );

            // Debug only
            /*static char buf[ 32 ];
            sprintf( buf, "%u", frame_index );
            draw_list->AddText( { frame_x, cursor_pos.y + widget_height - 64 }, 0xffffffff, buf );

            sprintf( buf, "%u", frame_timestamps[0].frame_index );
            drawList->AddText( { frame_x, cursor_pos.y + widget_height - 32 }, 0xffffffff, buf );*/

            rect_x -= rect_width;
        }

        average_time = (f32)new_average / max_frames;

        // Draw legend
        ImGui::SetCursorPosX( cursor_pos.x + graph_width );
        // Default to last frame if nothing is selected.
        selected_frame = selected_frame == -1 ? ( current_frame - 1 ) % max_frames : selected_frame;
        if ( selected_frame >= 0 ) {
            GPUTimestamp* frame_timestamps = &timestamps[ selected_frame * 32 ];

            f32 x = cursor_pos.x + graph_width;
            f32 y = cursor_pos.y;

            for ( u32 j = 0; j < per_frame_active[ selected_frame ]; ++j ) {
                const GPUTimestamp& timestamp = frame_timestamps[ j ];

                /*if ( timestamp.depth != 1 ) {
                    continue;
                }*/

                draw_list->AddRectFilled( { x, y },
                                          { x + 8, y + 8 }, timestamp.color );

                sprintf( buf, "(%d)-%s %2.4f", timestamp.depth, timestamp.name, timestamp.elapsed_ms );
                draw_list->AddText( { x + 12, y }, 0xffffffff, buf );

                y += 16;
            }
        }

        ImGui::Dummy( { canvas_size.x, widget_height } );
    }
    
    ImGui::SetNextItemWidth( 100.f );
    ImGui::LabelText( "", "Max %3.4fms", max_time );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 100.f );
    ImGui::LabelText( "", "Min %3.4fms", min_time );
    ImGui::SameLine();
    ImGui::LabelText( "", "Ave %3.4fms", average_time );

    ImGui::Separator();
    ImGui::Checkbox( "Pause", &paused );
    
    static const char* items[] = { "200ms", "100ms", "66ms", "33ms", "16ms", "8ms", "4ms" };
    static const float max_durations[] = { 200.f, 100.f, 66.f, 33.f, 16.f, 8.f, 4.f };

    static int max_duration_index = 4;
    if ( ImGui::Combo( "Graph Max", &max_duration_index, items, IM_ARRAYSIZE( items ) ) ) {
        max_duration = max_durations[ max_duration_index ];
    }
}

// Texture //////////////////////////////////////////////////////////////////////
void Texture::init( Device& gpu, const TextureCreation& creation ) {

    handle = gpu.create_texture( creation );
    gpu.query_texture( handle, description );
}

void Texture::init( Device& gpu, cstring filename ) {
    handle = create_texture_from_file( gpu, filename );
    gpu.query_texture( handle, description );
}

void Texture::shutdown( Device& gpu ) {
    gpu.destroy_texture( handle );
}

} // namespace graphics
} // namespace hydra
