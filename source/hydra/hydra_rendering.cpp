//
//  Hydra Rendering - v0.12

#include "hydra_rendering.h"

#include "ShaderCodeGenerator.h"

#include "cglm/struct/mat4.h"
#include "cglm/struct/cam.h"

#define HYDRA_RENDERING_VERBOSE

namespace hydra {
namespace graphics {

    
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

// ShaderEffect /////////////////////////////////////////////////////////////////

void ShaderEffect::init( const hfx::ShaderEffectFile& shader_effect_file ) {

    memcpy( name, shader_effect_file.header->name, 32 );
    memcpy( pipeline_name, shader_effect_file.header->pipeline_name, 32 );

    local_constants_size = shader_effect_file.local_constants_size;
    local_constants_default_data = shader_effect_file.local_constants_default_data;
    num_properties = shader_effect_file.num_properties;
    properties_data = shader_effect_file.properties_data;
    num_passes = shader_effect_file.header->num_passes;

    array_init( passes );
    array_set_length( passes, num_passes );
}

// ShaderInstance ///////////////////////////////////////////////////////////////

void ShaderInstance::load_resources( const PipelineCreation& pipeline_creation, PipelineHandle pipeline_handle, ShaderResourcesDatabase& database, ShaderResourcesLookup& lookup, Device& device ) {
    
    using namespace hydra::graphics;
    ResourceListCreation::Resource resources_handles[k_max_resources_per_list];

    for ( uint32_t l = 0; l < pipeline_creation.num_active_layouts; ++l ) {
            // Get resource layout description
        ResourceListLayoutDescription layout;
        device.query_resource_list_layout( pipeline_creation.resource_list_layout[l], layout );

        // For each resource
        for ( uint32_t r = 0; r < layout.num_active_bindings; r++ ) {
            const ResourceBinding& binding = layout.bindings[r];

            // Find resource name
            // Copy string_buffer 
            char* resource_name = lookup.find_resource( (char*)binding.name );

            switch ( binding.type ) {
                case hydra::graphics::ResourceType::Constants:
                case hydra::graphics::ResourceType::Buffer:
                {
#if defined (HYDRA_RENDERING_VERBOSE)
                    if ( !resource_name ) {
                        hydra::print_format( "Missing resource lookup for binding %s. Using dummy resource.\n", binding.name );
                        resources_handles[r].handle = device.get_dummy_constant_buffer().handle;
                    }
                    else {
                        BufferHandle handle = database.find_buffer( resource_name );
                        if ( handle.handle == 0 ) {
                            hydra::print_format( "Missing buffer for resource %s, binding %s.\n", resource_name, binding.name );
                            handle = device.get_dummy_constant_buffer();
                        }

                        resources_handles[r].handle = handle.handle;
                    }
#else
                    BufferHandle handle = resource_name ? database.find_buffer( resource_name ) : device.get_dummy_constant_buffer();
                    resources_handles[r].handle = handle.handle;
#endif // HYDRA_RENDERING_VERBOSE

                    break;
                }

                case hydra::graphics::ResourceType::Texture:
                case hydra::graphics::ResourceType::TextureRW:
                {
#if defined (HYDRA_RENDERING_VERBOSE)
                    if ( !resource_name ) {
                        hydra::print_format( "Missing resource lookup for binding %s. Using dummy resource.\n", binding.name );
                        resources_handles[r].handle = device.get_dummy_texture().handle;
                    }
                    else {
                        TextureHandle handle = database.find_texture( resource_name );
                        if ( handle.handle == 0 ) {
                            hydra::print_format( "Missing texture for resource %s, binding %s.\n", resource_name, binding.name );
                            handle = device.get_dummy_texture();
                        }

                        char* sampler_name = lookup.find_sampler( (char*)binding.name );
                        if ( sampler_name ) {
                            SamplerHandle sampler_handle = database.find_sampler( sampler_name );
                            // Set sampler, opengl only!
                            // TODO:
#if defined (HYDRA_OPENGL)
                            device.link_texture_sampler( handle, sampler_handle );
#endif // HYDRA_OPENGL
                        }

                        resources_handles[r].handle = handle.handle;
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

        ResourceListCreation creation = { pipeline_creation.resource_list_layout[l], resources_handles, layout.num_active_bindings };
        resource_lists[l] = device.create_resource_list( creation );
    }

    num_resource_lists = pipeline_creation.num_active_layouts;
    pipeline = pipeline_handle;
}

// Material /////////////////////////////////////////////////////////////////////

void Material::load_resources( ShaderResourcesDatabase& db, Device& device ) {

    for ( size_t i = 0; i < num_instances; ++i ) {
        const ShaderEffectPass& shader_pass = effect->passes[i];

        shader_instances[i].load_resources( shader_pass.pipeline_creation, shader_pass.pipeline_handle, db, lookups, device );
    }
}

// RenderPipeline ///////////////////////////////////////////////////////////////

void RenderPipeline::init( ShaderResourcesDatabase* initial_db ) {

    string_hash_init_arena( name_to_stage );
    string_hash_init_arena( name_to_texture );

    resource_database.init();
    resource_lookup.init();

    if ( initial_db ) {
        for ( size_t i = 0; i < string_hash_length( initial_db->name_to_buffer ); i++ ) {
            ShaderResourcesDatabase::BufferStringMap& buffer = initial_db->name_to_buffer[i];
            resource_database.register_buffer( buffer.key, buffer.value );
        }

        for ( size_t i = 0; i < string_hash_length( initial_db->name_to_texture ); i++ ) {
            ShaderResourcesDatabase::TextureStringMap& texture = initial_db->name_to_texture[i];
            resource_database.register_texture( texture.key, texture.value );
        }

        for ( size_t i = 0; i < string_hash_length( initial_db->name_to_sampler ); i++ ) {
            ShaderResourcesDatabase::SamplerStringMap& sampler = initial_db->name_to_sampler[i];
            resource_database.register_sampler( sampler.key, sampler.value );
        }
    }
}

void RenderPipeline::terminate( Device& device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->terminate();
    }

    for ( size_t i = 0; i < string_hash_length( name_to_texture ); i++ ) {
        TextureHandle texture = name_to_texture[i].value;
        device.destroy_texture( texture );
    }
}

void RenderPipeline::update() {
}

void RenderPipeline::render( Device& device, CommandBuffer* commands ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->begin( device, commands );
        stage->render( device, commands );
        stage->end( device, commands );
    }
}

void RenderPipeline::load_resources( Device& device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->load_resources( resource_database, device );
    }
}

void RenderPipeline::resize( uint16_t width, uint16_t height, Device& device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->resize( width, height, device );
    }
}

// RenderStage //////////////////////////////////////////////////////////////////

void RenderStage::init() {

    array_init( render_managers );

    render_pass.handle = k_invalid_handle;
}

void RenderStage::terminate() {
    // Destroy render pass
}

void RenderStage::begin( Device& device, CommandBuffer* commands ) {
    // Render Pass Begin
    commands->begin_submit( 0 );
    commands->begin_pass( render_pass );
    commands->set_viewport( { 0, 0, (float)current_width, (float)current_height, 0.0f, 1.0f } );
    
    if ( clear_rt ) {
        commands->clear( clear_color[0], clear_color[1], clear_color[2], clear_color[3] );
    }

    if ( clear_depth ) {
        commands->clear_depth( clear_depth_value );
    }

    if ( clear_stencil ) {
        commands->clear_stencil( clear_stencil_value );
    }
    
    commands->end_submit();
    // Set render stage states (depth, alpha, ...)
}

void RenderStage::render( Device& device, CommandBuffer* commands ) {
    // For each manager, render

    // TODO: for now use the material and the pass specified
    if ( material ) {
        ShaderInstance& shader_instance = material->shader_instances[pass_index];
        switch ( type ) {

            case Post:
            {
                commands->begin_submit( pass_index );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists, nullptr, 0 );
                //commands->bind_vertex_buffer( device.get_fullscreen_vertex_buffer() );
                commands->draw( graphics::TopologyType::Triangle, 0, 3 );
                commands->end_submit();
                break;
            }

            case PostCompute:
            {
                commands->begin_submit( 0 );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists, nullptr, 0 );
                commands->dispatch( (uint8_t)ceilf( current_width / 32.0f ), (uint8_t)ceilf( current_height / 32.0f ), 1 );
                commands->end_submit();
                break;
            }

            case Swapchain:
            {
                commands->begin_submit( pass_index );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists, nullptr, 0 );
                //commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
                commands->draw( graphics::TopologyType::Triangle, 0, 3 );
                commands->end_submit();
                break;
            }
        }
    }
    else if ( type == Geometry ) {
        // Go through all visible elements in render view and draw them using their respective managers.

        // Theoretically should sort all render scenes per manager and then submit them.
        uint32_t render_scenes = array_length( render_view->visible_render_scenes );
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
    }
}

void RenderStage::end( Device& device, CommandBuffer* commands ) {
    // TODO: Post render (for always submitting managers)
    for ( uint32_t i = 0; i < array_length( render_managers ); ++i ) {
        RenderManager* render_manager = render_managers[i];

        RenderManager::RenderContext render_context = { &device, render_view, commands, render_view ? render_view->visible_render_scenes : nullptr, 0, 0, 0 };
        render_manager->render( render_context );
    }

    // Render Pass End
    commands->begin_submit( 0 );
    commands->end_pass();
    commands->end_submit();
}

void RenderStage::load_resources( ShaderResourcesDatabase& db, Device& device ) {

    // Create render pass
    if ( render_pass.handle == k_invalid_handle ) {
        RenderPassCreation creation = {};
        switch ( type ) {
            case Geometry:
            {
                break;
            }

            case Post:
            {
                creation.is_compute_post = 0;
                creation.is_swapchain = 0;
                break;
            }

            case PostCompute:
            {
                creation.is_swapchain = 0;
                break;
            }

            case Swapchain:
            {
                creation.is_compute_post = 0;
                creation.is_swapchain = 1;
                break;
            }
        }

        creation.num_render_targets = num_output_textures;
        creation.output_textures = output_textures;
        creation.depth_stencil_texture = depth_texture;

        render_pass = device.create_render_pass( creation );
    }

    if ( resize_output ) {
        current_width = device.swapchain_width * scale_x;
        current_height = device.swapchain_height * scale_y;
    }

    if ( material ) {
        material->load_resources( db, device );
    }
}

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

void RenderStage::register_render_manager( RenderManager* manager ) {

    array_push( render_managers, manager );
}


// Camera ///////////////////////////////////////////////////////////////////////

void Camera::init( bool perspective, float near_plane, float far_plane ) {

    position = glms_vec3_zero();
    
    yaw = 0;
    pitch = 0;

    this->perspective = perspective;
    this->near_plane = near_plane;
    this->far_plane = far_plane;

    view = glms_mat4_identity();
    projection = glms_mat4_identity();

    update_projection = true;
}

void Camera::update( hydra::graphics::Device& gfx_device ) {

    // Left for reference.
    // Create rotation quaternion
    //hmm_quaternion rotation = HMM_QuaternionFromAxisAngle( { 1,0,0 }, pitch ) * HMM_QuaternionFromAxisAngle( { 0,1,0 }, yaw );
    // Concatenate rotation and translation to calculate final view matrix.
    //view = HMM_QuaternionToMat4( rotation ) * HMM_Translate( HMM_MultiplyVec3f( position * -1.0f, 1.0f ) );

    // Calculate rotation from yaw and pitch
    direction.x = sinf( ( yaw ) ) * cosf( ( pitch ) );
    direction.y = sinf( ( pitch ) );
    direction.z = cosf( ( yaw ) ) * cosf( ( pitch ) );

    
    direction = glms_vec3_normalize( direction );

    vec3s center = glms_vec3_sub( position, direction );
    vec3s cup{ 0,1,0 };

    // Calculate view matrix
    view = glms_lookat( position, center, cup );
    
    // Update the up vector, used for movement
    up = { view.m01, view.m11, view.m21 };
    right = { view.m00, view.m10, view.m20 };

    if ( update_projection ) {
        update_projection = false;

        if ( perspective ) {
            projection = glms_perspective( glm_rad(60.0f), gfx_device.swapchain_width * 1.0f / gfx_device.swapchain_height, near_plane, far_plane );
        }
        else {
            projection = glms_ortho( -gfx_device.swapchain_width / 2, gfx_device.swapchain_width / 2, -gfx_device.swapchain_height / 2, gfx_device.swapchain_height / 2, near_plane, far_plane );
        }
    }
    
    // Calculate final view projection matrix
    view_projection = glms_mat4_mul( projection, view );
}

// SceneRenderer ////////////////////////////////////////////////////////////////

static void render_mesh( hydra::graphics::CommandBuffer* commands, const hydra::graphics::Mesh& mesh, uint32_t node_id, BufferHandle transformBuffer ) {
    for ( uint32_t i = 0; i < array_length( mesh.sub_meshes ); ++i ) {
        const hydra::graphics::SubMesh& sub_mesh = mesh.sub_meshes[i];

        hydra::graphics::ShaderInstance& shader_instance = sub_mesh.material->shader_instances[0];

        commands->begin_submit( 0 );
        commands->bind_pipeline( shader_instance.pipeline );

        //uint32_t offsets[2] = { 0, node_id * sizeof(hmm_mat4) };
        commands->bind_resource_list( shader_instance.resource_lists, shader_instance.num_resource_lists, 0, 0 );

        for ( uint32_t vb = 0; vb < array_length( sub_mesh.vertex_buffers ); ++vb ) {
            commands->bind_vertex_buffer( sub_mesh.vertex_buffers[vb], vb, sub_mesh.vertex_buffer_offsets[vb] );
        }

        commands->bind_vertex_buffer( transformBuffer, 3, 0 );
        commands->bind_index_buffer( sub_mesh.index_buffer );

        commands->drawIndexed( hydra::graphics::TopologyType::Triangle, sub_mesh.end_index, 1, sub_mesh.start_index, 0, node_id );

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

        ShaderInstance& shader_instance = line_material->shader_instances[3];
        commands->bind_pipeline( shader_instance.pipeline );
        commands->bind_resource_list( shader_instance.resource_lists, shader_instance.num_resource_lists, nullptr, 0 );
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

        ShaderInstance& shader_instance = line_material->shader_instances[4];
        commands->bind_pipeline( shader_instance.pipeline );
        commands->bind_resource_list( shader_instance.resource_lists, shader_instance.num_resource_lists, nullptr, 0 );
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


} // namespace graphics
} // namespace hydra
