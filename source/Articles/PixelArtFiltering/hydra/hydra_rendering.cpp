//
//  Hydra Rendering - v0.33

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
void pipeline_create( Device& gpu, hfx::ShaderEffectFile& hfx, u32 pass_index, const RenderPassOutput& pass_output, PipelineHandle& out_pipeline, ResourceLayoutHandle* out_layouts, u32 num_layouts ) {

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

    //gpu.get_render_pass
    render_pipeline.render_pass = pass_output;
    //render_pipeline.render_pass = pass_handle;

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
    

// ClearData //////////////////////////////////////////////////////////////////////////////////////
void ClearData::bind( u64& sort_key, CommandBuffer* gpu_commands ) {

    if ( needs_color_clear )
        gpu_commands->clear( sort_key++, clear_color[0], clear_color[1], clear_color[2], clear_color[3] );

    if ( needs_depth_clear || needs_stencil_clear )
        gpu_commands->clear_depth_stencil( sort_key++, depth_value, stencil_value );
}

ClearData& ClearData::reset() {
    needs_color_clear = needs_depth_clear = needs_stencil_clear = 0;
    return *this;
}

ClearData& ClearData::set_color( vec4s color ) {
    clear_color[ 0 ] = color.x;
    clear_color[ 1 ] = color.y;
    clear_color[ 2 ] = color.z;
    clear_color[ 3 ] = color.w;

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


// Renderer /////////////////////////////////////////////////////////////////////
static const u32 k_dynamic_memory_size = 256 * 1024;
static const bool k_use_global_buffer = true;

void Renderer::init( const RendererCreation& creation ) {

   gpu = creation.gpu;

   width = gpu->swapchain_width;
   height = gpu->swapchain_height;

   textures.init( 128, sizeof( Texture ) );
   buffers.init( 128, sizeof( Buffer ) );
   samplers.init( 128, sizeof( Sampler ) );
   stages.init( 128, sizeof( RenderStage ) );
   shaders.init( 128, sizeof( Shader ) );
   materials.init( 128, sizeof( Material ) );


   if ( k_use_global_buffer ) {
       BufferCreation bc;
       bc.set( BufferType::Constant, ResourceUsageType::Dynamic, k_dynamic_memory_size * 2 ).set_name( "dynamic_constants" );

       dynamic_constants = nullptr;

       u32 index = buffers.obtain_resource();
       if ( index != k_invalid_index ) {
           BufferHandle handle = gpu->create_buffer( bc );
           Buffer* buffer = ( Buffer* )buffers.access_resource( index );
           buffer->handle = handle;
           buffer->index = index;
           gpu->query_buffer( handle, buffer->desc );

           dynamic_constants = buffer;
       }
   }
}

void Renderer::terminate() {

    if ( k_use_global_buffer ) {
        destroy_buffer( dynamic_constants );
    }

    textures.terminate();
    buffers.terminate();
    samplers.terminate();
    stages.terminate();
    shaders.terminate();
    materials.terminate();
}

void Renderer::begin_frame() {
    if ( k_use_global_buffer ) {
        MapBufferParameters cb_map = { dynamic_constants->handle, 0, k_dynamic_memory_size };
        dynamic_mapped_memory = ( u8* )gpu->map_buffer( cb_map );

        dynamic_allocated_size = k_dynamic_memory_size * gpu->current_frame;
    }
}

void Renderer::end_frame() {
    if ( k_use_global_buffer ) {
        MapBufferParameters cb_map = { dynamic_constants->handle, 0, 0 };
        gpu->unmap_buffer( cb_map );
    }
}

void Renderer::on_resize( u32 width_, u32 height_ ) {
    gpu->resize( (u16)width_, (u16)height_ );

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;
}

f32 Renderer::aspect_ratio() const {
    return gpu->swapchain_width * 1.f / gpu->swapchain_height;
}

Buffer* Renderer::create_buffer( const BufferCreation& creation ) {

    u32 index = buffers.obtain_resource();
    if ( index != k_invalid_index ) {
        BufferCreation creation_updated = creation;

        if ( creation.usage == ResourceUsageType::Dynamic && creation.type == BufferType::Constant && k_use_global_buffer ) {
            creation_updated.parent_buffer = dynamic_constants->handle;
        }
        else {
            creation_updated.parent_buffer = k_invalid_buffer;
        }

        BufferHandle handle = gpu->create_buffer( creation_updated );
        Buffer* buffer = ( Buffer* )buffers.access_resource( index );
        buffer->handle = handle;
        buffer->index = index;
        gpu->query_buffer( handle, buffer->desc );

        return buffer;
    }
    return nullptr;
}

Texture* Renderer::create_texture( const TextureCreation& creation ) {
    u32 index = textures.obtain_resource();
    if ( index != k_invalid_index ) {
        TextureHandle handle = gpu->create_texture( creation );
        Texture* texture = ( Texture* )textures.access_resource( index );
        texture->handle = handle;
        texture->index = index;
        gpu->query_texture( handle, texture->desc );

        return texture;
    }
    return nullptr;
}

Texture* Renderer::create_texture( cstring filename ) {
    u32 index = textures.obtain_resource();
    if ( index != k_invalid_index ) {
        TextureHandle handle = create_texture_from_file( *gpu, filename );
        Texture* texture = ( Texture* )textures.access_resource( index );
        texture->handle = handle;
        texture->index = index;
        gpu->query_texture( handle, texture->desc );

        return texture;
    }
    return nullptr;
}

Sampler* Renderer::create_sampler( const SamplerCreation& creation ) {
    u32 index = samplers.obtain_resource();
    if ( index != k_invalid_index ) {
        SamplerHandle handle = gpu->create_sampler( creation );
        Sampler* sampler = ( Sampler* )samplers.access_resource( index );
        sampler->handle = handle;
        sampler->index = index;
        gpu->query_sampler( handle, sampler->desc );

        return sampler;
    }
    return nullptr;
}

RenderStage* Renderer::create_stage( const RenderStageCreation& creation ) {
    u32 index = stages.obtain_resource();
    if ( index != k_invalid_index ) {
        RenderStage aa;
        RenderStage* stage = ( RenderStage* )stages.access_resource( index );

        array_init( stage->features );
        stage->index = index;
        stage->name = creation.name;
        stage->type = creation.type;
        stage->resize = creation.resize;
        stage->clear = creation.clear;
        stage->num_render_targets = creation.num_render_targets;

        for ( u32 i = 0; i < creation.num_render_targets; ++i ) {
            stage->output_textures[ i ] = creation.output_textures[ i ];
        }
        stage->depth_stencil_texture = creation.depth_stencil_texture;

        if ( creation.type != RenderPassType::Swapchain ) {
            // Create RenderPass
            RenderPassCreation rpc;
            rpc.reset().set_name( creation.name ).set_scaling( creation.resize.scale_x, creation.resize.scale_y, creation.resize.resize ).set_type( creation.type );
            rpc.set_depth_stencil_texture( creation.depth_stencil_texture ? creation.depth_stencil_texture->handle : k_invalid_texture );

            for ( u32 i = 0; i < creation.num_render_targets; ++i ) {
                rpc.add_render_texture( creation.output_textures[ i ]->handle );
            }
            stage->render_pass = gpu->create_render_pass( rpc );

            // TODO: can be optimized
            gpu->fill_barrier( stage->render_pass, stage->barrier );

            stage->output_width = creation.output_textures[ 0 ]->desc.width;
            stage->output_height = creation.output_textures[ 0 ]->desc.height;
            stage->output_depth = creation.output_textures[ 0 ]->desc.depth;
            stage->output = gpu->get_render_pass_output( stage->render_pass );
        } else {
            stage->render_pass = gpu->get_swapchain_pass();
            stage->output_width = gpu->swapchain_width;
            stage->output_height = gpu->swapchain_height;
            stage->output_depth = 1;
            stage->output = gpu->get_swapchain_output();
        }
        
        return stage;
    }
    return nullptr;
}

Shader* Renderer::create_shader( const ShaderCreation& creation ) {
    u32 index = shaders.obtain_resource();
    if ( index != k_invalid_index ) {
        Shader* shader = ( Shader* )shaders.access_resource( index );
        // Copy hfx header.
        shader->hfx = *creation.hfx;
        shader->index = index;

        const u32 passes = shader->hfx.header->num_passes;
        // First create arrays
        array_init( shader->pipelines );
        array_init( shader->resource_layouts );

        array_set_length( shader->pipelines, passes );
        array_set_length( shader->resource_layouts, passes );

        if ( creation.num_outputs != passes ) {
            assert( "Missing render outputs!" && false );
        }

        for ( uint32_t i = 0; i < passes; ++i ) {
            // TODO: only 1 resource list allowed for now ?
            pipeline_create( *gpu, shader->hfx, i, creation.outputs[ i ], shader->pipelines[ i ], &shader->resource_layouts[ i ], 1 );
        }
        return shader;
    }
    return nullptr;
}

Material* Renderer::create_material( const MaterialCreation& creation ) {
    u32 index = materials.obtain_resource();
    if ( index != k_invalid_index ) {
        Material* mat = ( Material* )materials.access_resource( index );
        mat->index = index;
        mat->shader = creation.shader;
        mat->num_passes = mat->shader->hfx.header->num_passes;
        // First create arrays
        array_init( mat->pipelines );
        array_init( mat->resource_lists );
        array_init( mat->compute_dispatches );

        array_set_length( mat->pipelines, mat->num_passes );
        array_set_length( mat->resource_lists, mat->num_passes );
        array_set_length( mat->compute_dispatches, mat->num_passes );

        // Cache pipelines and resources
        for ( uint32_t i = 0; i < mat->num_passes; ++i ) {
            mat->pipelines[ i ] = mat->shader->pipelines[ i ];
            // Set layout internally
            creation.resource_lists[ i ].set_layout( mat->shader->resource_layouts[ i ] );
            mat->resource_lists[ i ] = gpu->create_resource_list( creation.resource_lists[ i ] );

            hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( mat->shader->hfx.memory, i );
            mat->compute_dispatches[ i ].x = pass_header->compute_dispatch.x;
            mat->compute_dispatches[ i ].y = pass_header->compute_dispatch.y;
            mat->compute_dispatches[ i ].z = pass_header->compute_dispatch.z;
        }

        return mat;
    }
    return nullptr;
}

void Renderer::destroy_buffer( Buffer* buffer ) {
    gpu->destroy_buffer( buffer->handle );
    buffers.release_resource( buffer->index );
}

void Renderer::destroy_texture( Texture* texture ) {
    gpu->destroy_texture( texture->handle );
    textures.release_resource( texture->index );
}

void Renderer::destroy_sampler( Sampler* sampler ) {
    gpu->destroy_sampler( sampler->handle );
    samplers.release_resource( sampler->index );
}

void Renderer::destroy_stage( RenderStage* stage ) {
    if ( stage->type != RenderPassType::Swapchain )
        gpu->destroy_render_pass( stage->render_pass );
    stages.release_resource( stage->index );
}

void Renderer::destroy_shader( Shader* shader ) {
    const u32 passes = shader->hfx.header->num_passes;
    for ( uint32_t i = 0; i < passes; ++i ) {
        gpu->destroy_pipeline( shader->pipelines[ i ] );
        gpu->destroy_resource_layout( shader->resource_layouts[ i ] );
    }
    hfx::shader_effect_shutdown( shader->hfx );
    shaders.release_resource( shader->index );
}

void Renderer::destroy_material( Material* material ) {
    for ( uint32_t i = 0; i < material->num_passes; ++i ) {
        gpu->destroy_resource_list( material->resource_lists[ i ] );
    }
    materials.release_resource( material->index );
}

// TODO:
static size_t pad_uniform_buffer_size( size_t originalSize ) {
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = 256;// _gpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if ( minUboAlignment > 0 ) {
        alignedSize = ( alignedSize + minUboAlignment - 1 ) & ~( minUboAlignment - 1 );
    }
    return alignedSize;
}

void* Renderer::map_buffer( Buffer* buffer, u32 offset, u32 size ) {

    if ( buffer->desc.parent_handle.index != k_invalid_index && k_use_global_buffer ) {
        gpu->set_buffer_global_offset( buffer->handle, dynamic_allocated_size );

        if ( size == 0 ) {
            size = buffer->desc.size;
        }

        void* mapped_memory = dynamic_mapped_memory + dynamic_allocated_size + offset;
        dynamic_allocated_size += pad_uniform_buffer_size( size );
        return mapped_memory;
    }
    else {
        MapBufferParameters cb_map = { buffer->handle, offset, size };
        return gpu->map_buffer( cb_map );
    }
    
}

void Renderer::unmap_buffer( Buffer* buffer ) {

    if ( buffer->desc.parent_handle.index == k_invalid_index ) {
        MapBufferParameters cb_map = { buffer->handle, 0, 0 };
        gpu->unmap_buffer( cb_map );
    }
}

void Renderer::resize( RenderStage* stage ) {

    if ( !stage->resize.resize )
        return;

    if ( stage->type != RenderPassType::Swapchain ) {
        gpu->resize_output_textures( stage->render_pass, width, height );
    }
        
    stage->output_width = roundu16( width * stage->resize.scale_x );
    stage->output_height = roundu16( height * stage->resize.scale_y );

    // Update texture sizes
    for ( u32 i = 0; i < stage->num_render_targets; ++i ) {
        Texture* t = stage->output_textures[ i ];
        gpu->query_texture( t->handle, t->desc );
    }
    if ( stage->depth_stencil_texture ) {
        gpu->query_texture( stage->depth_stencil_texture->handle, stage->depth_stencil_texture->desc );
    }
}

void Renderer::draw_material( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands, Material* material, u32 pass_index ) {

    gpu_commands->push_marker( stage->name );

    switch ( stage->type ) {
        case RenderPassType::Standard:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::RenderTarget );
            gpu_commands->barrier( stage->barrier );

            stage->clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );
            // Fullscreen tri
            gpu_commands->bind_pipeline( sort_key++, material->pipelines[ pass_index ] );
            gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ pass_index ], 1, 0, 0 );
            gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );

            stage->barrier.set( PipelineStage::RenderTarget, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );

            break;
        }

        case RenderPassType::Compute:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::ComputeShader );
            gpu_commands->barrier( stage->barrier );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->bind_pipeline( sort_key++, material->pipelines[ pass_index ] );
            gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ pass_index ], 1, 0, 0 );

            const ComputeDispatch& dispatch = material->compute_dispatches[ pass_index ];
            gpu_commands->dispatch( sort_key++, ceilu32( stage->output_width * 1.f / dispatch.x ), ceilu32( stage->output_height * 1.f / dispatch.y ), ceilu32( stage->output_depth * 1.f / dispatch.z ) );

            stage->barrier.set( PipelineStage::ComputeShader, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );
            break;
        }

        case RenderPassType::Swapchain:
        {
            stage->clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, gpu->get_swapchain_pass() );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            gpu_commands->bind_pipeline( sort_key++, material->pipelines[ pass_index ] );
            gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[ pass_index ], 1, 0, 0 );
            gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );

            break;
        }
    }

    gpu_commands->pop_marker();
}


void Renderer::draw( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands ) {

    gpu_commands->push_marker( stage->name );

    switch ( stage->type ) {
        case RenderPassType::Standard:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::RenderTarget );
            gpu_commands->barrier( stage->barrier );

            stage->clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            const sizet features_count = array_size( stage->features );
            if ( features_count ) {
                for ( sizet i = 0; i < features_count; ++i ) {
                    stage->features[ i ]->render( *this, sort_key, gpu_commands );
                }
            }

            if ( features_count == 0 ) {
                hydra::print_format( "Error: trying to render a stage with 0 features. Nothing will be rendered." );
            }

            stage->barrier.set( PipelineStage::RenderTarget, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );

            break;
        }

        case RenderPassType::Compute:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::ComputeShader );
            gpu_commands->barrier( stage->barrier );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );

            // WRONG CALL!
            assert( false );

            stage->barrier.set( PipelineStage::ComputeShader, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );
            break;
        }

        case RenderPassType::Swapchain:
        {
            stage->clear.bind( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, gpu->get_swapchain_pass() );

            const sizet features_count = array_size( stage->features );
            if ( features_count ) {
                for ( sizet i = 0; i < features_count; ++i ) {
                    stage->features[ i ]->render( *this, sort_key, gpu_commands );
                }
            }
            break;
        }
    }

    gpu_commands->pop_marker();
}

void Renderer::reload_resource_list( Material* material, u32 index ) {

    ResourceListUpdate u;
    u.resource_list = material->resource_lists[ index ];
    gpu->update_resource_list_instant( u );
}

// RenderStageCreation //////////////////////////////////////////////////////////

RenderStageCreation& RenderStageCreation::reset() {
    num_render_targets = 0;
    depth_stencil_texture = nullptr;
    resize.resize = 0;
    resize.scale_x = 1.f;
    resize.scale_y = 1.f;

    return *this;
}

RenderStageCreation& RenderStageCreation::add_render_texture( Texture* texture ) {
    output_textures[ num_render_targets++ ] = texture;
    return *this;
}

RenderStageCreation& RenderStageCreation::set_depth_stencil_texture( Texture* texture ) {
    depth_stencil_texture = texture;
    return *this;
}

RenderStageCreation& RenderStageCreation::set_scaling( f32 scale_x_, f32 scale_y_, u8 resize_ ) {
    resize.scale_x = scale_x_;
    resize.scale_y = scale_y_;
    resize.resize = resize_;

    return *this;
}

RenderStageCreation& RenderStageCreation::set_name( const char* name_ ) {
    name = name_;
    return *this;
}

RenderStageCreation& RenderStageCreation::set_type( RenderPassType::Enum type_ ) {
    type = type_;
    return *this;
}

// ShaderCreation ///////////////////////////////////////////////////////////////
ShaderCreation& ShaderCreation::reset() {
    num_outputs = 0;
    hfx = nullptr;
    return *this;
}

ShaderCreation& ShaderCreation::set_shader_binary( hfx::ShaderEffectFile* hfx_ ) {
    hfx = hfx_;
    return *this;
}

ShaderCreation& ShaderCreation::set_outputs( const RenderPassOutput* outputs_, u32 num_outputs_ ) {
    outputs = outputs_;
    num_outputs = num_outputs_;
    return *this;
}

// MaterialCreation /////////////////////////////////////////////////////////////
MaterialCreation& MaterialCreation::reset() {
    num_resource_list = 0;
    shader = nullptr;
    return *this;
}

MaterialCreation& MaterialCreation::set_shader( Shader* shader_ ) {
    shader = shader_;
    return *this;
}

MaterialCreation& MaterialCreation::set_resource_lists( ResourceListCreation* lists, u32 num_lists ) {
    resource_lists = lists;
    num_resource_list = num_lists;
    return *this;
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

} // namespace graphics
} // namespace hydra
