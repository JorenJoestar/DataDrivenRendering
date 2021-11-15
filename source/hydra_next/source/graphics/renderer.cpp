
//  Hydra Rendering - v0.42

#include "graphics/renderer.hpp"

#include "kernel/numerics.hpp"

#include "graphics/hydra_shaderfx.h"
#include "graphics/command_buffer.hpp"
#include "graphics/camera.hpp"

#include "imgui/imgui.h"

#include <cmath>


#include "external/stb_image.h"

#define HYDRA_RENDERING_VERBOSE

namespace hydra {
namespace gfx {

    
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
void pipeline_create( Device& gpu, hfx::ShaderEffectFile* hfx, hfx::ShaderEffectBlueprint* hfx_blueprint, u32 pass_index, const RenderPassOutput& pass_output, PipelineHandle& out_pipeline, ResourceLayoutHandle* out_layouts, u32 num_layouts ) {

    hydra::gfx::PipelineCreation render_pipeline;

    // Default to new hfx v2
    if ( hfx_blueprint ) {
        hfx::ShaderPassBlueprint& pass = hfx_blueprint->passes[ pass_index ];

        pass.fill_pipeline( render_pipeline );

        // TODO: future test to check files differences.
        //if ( memcmp( &render_pipeline, &render_pipeline2, sizeof( hydra::gfx::PipelineCreation ) ) != 0 )
            //hprint( "What ?\n" );

        hydra::gfx::ResourceLayoutCreation rll_creation{};

        for ( u32 i = 0; i < num_layouts; i++ ) {

            pass.fill_resource_layout( rll_creation, i );
            out_layouts[ i ] = gpu.create_resource_layout( rll_creation );

            // TODO: improve
            // num active layout is already set to the max, so using add_rll breaks.
            render_pipeline.resource_layout[ i ] = out_layouts[ i ];
        }

        // Cache render pass output
        render_pipeline.render_pass = pass_output;

        out_pipeline = gpu.create_pipeline( render_pipeline );
    }
    else {
#if defined (HFX_V2)
        hy_assertm( false, "Trying to use old HFX binary!" );
#else
        // If needed, use the old hfx binary format.
        hfx::shader_effect_get_pipeline( *hfx, pass_index, render_pipeline );

        hydra::gfx::ResourceLayoutCreation rll_creation{};

        for ( u32 i = 0; i < num_layouts; i++ ) {

            hfx::shader_effect_get_resource_list_layout( *hfx, pass_index, i, rll_creation );
            out_layouts[ i ] = gpu.create_resource_layout( rll_creation );

            // TODO: improve
            // num active layout is already set to the max, so using add_rll breaks.
            render_pipeline.resource_layout[ i ] = out_layouts[ i ];
        }

        // Cache render pass output
        render_pipeline.render_pass = pass_output;

        out_pipeline = gpu.create_pipeline( render_pipeline );
#endif // HFX_V2
    }
}

//
//
static TextureHandle create_texture_from_file( Device& gpu, cstring filename ) {

    if ( filename ) {
        int comp, width, height;
        uint8_t* image_data = stbi_load( filename, &width, &height, &comp, 4 );
        if ( !image_data ) {
            hprint( "Error loading texture %s", filename );
            return k_invalid_texture;
        }

        TextureCreation creation;
        creation.set_data( image_data ).set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, 0 ).set_size( ( u16 )width, ( u16 )height, 1 );

        hydra::gfx::TextureHandle new_texture = gpu.create_texture( creation );

        // IMPORTANT:
        // Free memory loaded from file, it should not matter!
        free( image_data );

        return new_texture;
    }

    return k_invalid_texture;
}
    

// ClearData //////////////////////////////////////////////////////////////////////////////////////
void ClearData::set( u64& sort_key, CommandBuffer* gpu_commands ) {

    if ( color_operation == RenderPassOperation::Clear ) {
        gpu_commands->clear( sort_key++, clear_color[ 0 ], clear_color[ 1 ], clear_color[ 2 ], clear_color[ 3 ] );
    }

    if ( depth_operation == RenderPassOperation::Clear || stencil_operation == RenderPassOperation::Clear ) {
        gpu_commands->clear_depth_stencil( sort_key++, depth_value, stencil_value );
    }
}

ClearData& ClearData::reset() {
    color_operation = depth_operation = stencil_operation = RenderPassOperation::DontCare;
    return *this;
}

ClearData& ClearData::set_color( vec4s color ) {
    clear_color[ 0 ] = color.x;
    clear_color[ 1 ] = color.y;
    clear_color[ 2 ] = color.z;
    clear_color[ 3 ] = color.w;

    color_operation = RenderPassOperation::Clear;
    return *this;
}

ClearData& ClearData::set_color( ColorUint color ) {
    clear_color[ 0 ] = color.r();
    clear_color[ 1 ] = color.g();
    clear_color[ 2 ] = color.b();
    clear_color[ 3 ] = color.a();

    color_operation = RenderPassOperation::Clear;
    return *this;
}

ClearData& ClearData::set_depth( f32 depth ) {
    depth_value = depth;
    depth_operation = RenderPassOperation::Clear;
    return *this;
}

ClearData& ClearData::set_stencil( u8 stencil ) {
    stencil_value = stencil;
    stencil_operation = RenderPassOperation::Clear;
    return *this;
}


// Renderer /////////////////////////////////////////////////////////////////////
static const u32 k_dynamic_memory_size = 256 * 1024;
static Renderer s_renderer;

Renderer* Renderer::instance() {
    return &s_renderer;
}

void Renderer::init( const RendererCreation& creation ) {

    hprint( "Renderer init\n" );

   gpu = creation.gpu;

   width = gpu->swapchain_width;
   height = gpu->swapchain_height;

   textures.init( creation.allocator, 128 );
   buffers.init( creation.allocator, 128 );
   samplers.init( creation.allocator, 128 );
   stages.init( creation.allocator, 128 );
   shaders.init( creation.allocator, 128 );
   materials.init( creation.allocator, 128 );
   render_views.init( creation.allocator, 16 );
}

void Renderer::shutdown() {

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();
    stages.shutdown();
    shaders.shutdown();
    materials.shutdown();
    render_views.shutdown();

    hprint( "Renderer shutdown\n" );

    gpu->shutdown();
}

void Renderer::begin_frame() {
    gpu->new_frame();
}

void Renderer::end_frame() {
    // Present
    gpu->present();
}

void Renderer::resize_swapchain( u32 width_, u32 height_ ) {
    gpu->resize( (u16)width_, (u16)height_ );

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;
}

f32 Renderer::aspect_ratio() const {
    return gpu->swapchain_width * 1.f / gpu->swapchain_height;
}

Buffer* Renderer::create_buffer( const BufferCreation& creation ) {

    Buffer* buffer = buffers.obtain();
    if ( buffer ) {
        BufferHandle handle = gpu->create_buffer( creation );
        buffer->handle = handle;
        gpu->query_buffer( handle, buffer->desc );

        return buffer;
    }
    return nullptr;
}

Buffer* Renderer::create_buffer( BufferType::Mask type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name ) {
    BufferCreation creation{ type, usage, size, data, name, k_invalid_buffer };
    return create_buffer( creation );
}

Texture* Renderer::create_texture( const TextureCreation& creation ) {
    Texture* texture = textures.obtain();

    if ( texture ) {
        TextureHandle handle = gpu->create_texture( creation );
        texture->handle = handle;
        gpu->query_texture( handle, texture->desc );

        return texture;
    }
    return nullptr;
}

Texture* Renderer::create_texture( cstring filename ) {
    Texture* texture = textures.obtain();

    if ( texture ) {
        TextureHandle handle = create_texture_from_file( *gpu, filename );
        texture->handle = handle;
        gpu->query_texture( handle, texture->desc );

        return texture;
    }
    return nullptr;
}

Sampler* Renderer::create_sampler( const SamplerCreation& creation ) {
    Sampler* sampler = samplers.obtain();
    if ( sampler ) {
        SamplerHandle handle = gpu->create_sampler( creation );
        sampler->handle = handle;
        gpu->query_sampler( handle, sampler->desc );

        return sampler;
    }
    return nullptr;
}

RenderStage* Renderer::create_stage( const RenderStageCreation& creation ) {
    RenderStage* stage = stages.obtain();
    if ( stage ) {
        // TODO: allocator
        stage->features.init( gpu->allocator, 1 );
        stage->name = creation.name;
        stage->type = creation.type;
        stage->resize = creation.resize;
        stage->clear = creation.clear;
        stage->num_render_targets = creation.num_render_targets;
        stage->render_view = creation.render_view;

        for ( u32 i = 0; i < creation.num_render_targets; ++i ) {
            stage->output_textures[ i ] = creation.output_textures[ i ];
        }
        stage->depth_stencil_texture = creation.depth_stencil_texture;

        if ( creation.type != RenderPassType::Swapchain ) {
            // Create RenderPass
            RenderPassCreation rpc;
            rpc.reset().set_name( creation.name ).set_scaling( creation.resize.scale_x, creation.resize.scale_y, creation.resize.resize ).set_type( creation.type );
            rpc.set_depth_stencil_texture( creation.depth_stencil_texture ? creation.depth_stencil_texture->handle : k_invalid_texture );
            rpc.set_operations( stage->clear.color_operation, stage->clear.depth_operation, stage->clear.stencil_operation );

            for ( u32 i = 0; i < creation.num_render_targets; ++i ) {
                rpc.add_render_texture( creation.output_textures[ i ]->handle );
            }
            stage->render_pass = gpu->create_render_pass( rpc );

            // TODO: can be optimized
            stage->barrier.reset();
            gpu->fill_barrier( stage->render_pass, stage->barrier );

            if ( creation.num_render_targets ) {
                stage->output_width = creation.output_textures[ 0 ]->desc.width;
                stage->output_height = creation.output_textures[ 0 ]->desc.height;
                stage->output_depth = creation.output_textures[ 0 ]->desc.depth;
            }
            
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
    Shader* shader = shaders.obtain();
    if ( shader ) {
        // Copy hfx header.
        shader->hfx_binary = creation.hfx_;
        shader->hfx_binary_v2 = creation.hfx_blueprint;

        const u32 num_passes = shader->hfx_binary ? shader->hfx_binary->header->num_passes : shader->hfx_binary_v2->passes.size;
        // First create arrays
        shader->passes.init( gpu->allocator, num_passes, num_passes );
        
        if ( creation.num_outputs != num_passes ) {
            hy_assert( "Missing render outputs!" && false );
        }

        for ( uint32_t i = 0; i < num_passes; ++i ) {
            ShaderPass& pass = shader->passes[ i ];
            pipeline_create( *gpu, creation.hfx_, creation.hfx_blueprint, i, creation.outputs[ i ], pass.pipeline, &pass.resource_layout, 1 );
        }
        return shader;
    }
    return nullptr;
}

Shader* Renderer::create_shader( hfx::ShaderEffectBlueprint* hfx, RenderPassOutput* outputs, u32 num_outputs ) {
    ShaderCreation sc{ nullptr, hfx, outputs, num_outputs };
    return create_shader( sc );
}

Material* Renderer::create_material( const MaterialCreation& creation ) {
    Material* material = materials.obtain();
    if ( material ) {
        material->shader = creation.shader;
        
        u32 num_passes = material->shader->get_num_passes();
        // First create arrays
        material->passes.init( gpu->allocator, num_passes, num_passes );
        
        // Cache pipelines and resources
        for ( uint32_t i = 0; i < num_passes; ++i ) {
            MaterialPass& pass = material->passes[ i ];
            ShaderPass& shader_pass = material->shader->passes[ i ];
            pass.pipeline = shader_pass.pipeline;
            // Set layout internally
            creation.resource_lists[ i ].set_layout( shader_pass.resource_layout );
            pass.resource_list = gpu->create_resource_list( creation.resource_lists[ i ] );

            material->shader->get_compute_dispatches( i, pass.compute_dispatch );
        }

        return material;
    }
    return nullptr;
}

Material* Renderer::create_material( Shader* shader, ResourceListCreation* resource_lists, u32 num_lists ) {
    MaterialCreation creation{ shader, resource_lists, num_lists };
    return create_material( creation );
}

RenderView* Renderer::create_render_view( Camera* camera, cstring name, u32 width_, u32 height_, RenderStage** stages_, u32 num_stages ) {
    RenderView* render_view = render_views.obtain();

    render_view->camera = camera;
    render_view->name = name;
    render_view->width = u16( width_ );
    render_view->height = u16( height_ );
    render_view->dependant_render_stages.init( gpu->allocator, num_stages + 2, stages_ ? num_stages : 0 );

    if ( stages_ ) {
        memcpy( render_view->dependant_render_stages.data, stages_, num_stages * sizeof( RenderStage* ) );
    }

    return render_view;
}

void Renderer::destroy_buffer( Buffer* buffer ) {
    gpu->destroy_buffer( buffer->handle );
    buffers.release( buffer );
}

void Renderer::destroy_texture( Texture* texture ) {
    gpu->destroy_texture( texture->handle );
    textures.release( texture );
}

void Renderer::destroy_sampler( Sampler* sampler ) {
    gpu->destroy_sampler( sampler->handle );
    samplers.release( sampler );
}

void Renderer::destroy_stage( RenderStage* stage ) {
    if ( stage->type != RenderPassType::Swapchain )
        gpu->destroy_render_pass( stage->render_pass );

    stage->features.shutdown();

    stages.release( stage );
}

void Renderer::destroy_shader( Shader* shader ) {
    const u32 passes = shader->get_num_passes();

    for ( uint32_t i = 0; i < passes; ++i ) {
        ShaderPass& pass = shader->passes[ i ];
        gpu->destroy_pipeline( pass.pipeline );
        gpu->destroy_resource_layout( pass.resource_layout );
    }

    shader->passes.shutdown();

    // TODO: this is handled externally.
    /*hfx::shader_effect_shutdown( shader->hfx_ );*/
    shaders.release( shader );
}

void Renderer::destroy_material( Material* material ) {
    for ( uint32_t i = 0; i < material->passes.size; ++i ) {
        MaterialPass& pass = material->passes[ i ];
        gpu->destroy_resource_list( pass.resource_list );
    }

    material->passes.shutdown();
    
    materials.release( material );
}

void Renderer::destroy_render_view( RenderView* render_view ) {
    render_view->dependant_render_stages.shutdown();

    render_views.release( render_view );
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

    MapBufferParameters cb_map = { buffer->handle, offset, size };
    return gpu->map_buffer( cb_map );   
}

void Renderer::unmap_buffer( Buffer* buffer ) {

    if ( buffer->desc.parent_handle.index == k_invalid_index ) {
        MapBufferParameters cb_map = { buffer->handle, 0, 0 };
        gpu->unmap_buffer( cb_map );
    }
}

void Renderer::resize_stage( RenderStage* stage, u32 new_width, u32 new_height ) {

    if ( !stage->resize.resize )
        return;

    if ( stage->type != RenderPassType::Swapchain ) {
        gpu->resize_output_textures( stage->render_pass, new_width, new_height );
    }
        
    stage->output_width = roundu16( new_width * stage->resize.scale_x );
    stage->output_height = roundu16( new_height * stage->resize.scale_y );

    // Update texture sizes
    for ( u32 i = 0; i < stage->num_render_targets; ++i ) {
        Texture* t = stage->output_textures[ i ];
        gpu->query_texture( t->handle, t->desc );
    }
    if ( stage->depth_stencil_texture ) {
        gpu->query_texture( stage->depth_stencil_texture->handle, stage->depth_stencil_texture->desc );
    }
}

void Renderer::resize_view( RenderView* view, u32 new_width, u32 new_height ) {
    if ( new_width == view->width && new_height == view->height ) {
        return;
    }

    view->width = new_width;
    view->height = new_height;

    if ( view->camera ) {
        view->camera->set_viewport_size( new_width, new_height );
        view->camera->set_aspect_ratio( new_width * 1.f / new_height );
    }

    for ( u32 is = 0; is < view->dependant_render_stages.size; ++is ) {
        RenderStage* stage = view->dependant_render_stages[ is ];
        if ( stage->render_view != view ) {
            continue;
        }

        resize_stage( stage, new_width, new_height );
    }
}

void Renderer::draw_material( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands, Material* material, u32 pass_index ) {

    gpu_commands->push_marker( stage->name );

    MaterialPass& pass = material->passes[ pass_index ];

    switch ( stage->type ) {
        case RenderPassType::Geometry:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::RenderTarget );
            gpu_commands->barrier( stage->barrier );

            stage->clear.set( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );
            // Fullscreen tri
            gpu_commands->bind_pipeline( sort_key++, pass.pipeline );
            gpu_commands->bind_resource_list( sort_key++, &pass.resource_list, 1, 0, 0 );
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
            gpu_commands->bind_pipeline( sort_key++, pass.pipeline );
            gpu_commands->bind_resource_list( sort_key++, &pass.resource_list, 1, 0, 0 );

            const ComputeDispatch& dispatch = pass.compute_dispatch;
            gpu_commands->dispatch( sort_key++, ceilu32( stage->output_width * 1.f / dispatch.x ), ceilu32( stage->output_height * 1.f / dispatch.y ), ceilu32( stage->output_depth * 1.f / dispatch.z ) );

            stage->barrier.set( PipelineStage::ComputeShader, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );
            break;
        }

        case RenderPassType::Swapchain:
        {
            stage->clear.set( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, gpu->get_swapchain_pass() );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            gpu_commands->bind_pipeline( sort_key++, pass.pipeline );
            gpu_commands->bind_resource_list( sort_key++, &pass.resource_list, 1, 0, 0 );
            gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 3, 0, 1 );

            break;
        }
    }

    gpu_commands->pop_marker();
}


void Renderer::draw( RenderStage* stage, u64& sort_key, CommandBuffer* gpu_commands ) {

    gpu_commands->push_marker( stage->name );

    switch ( stage->type ) {
        case RenderPassType::Geometry:
        {
            stage->barrier.set( PipelineStage::FragmentShader, PipelineStage::RenderTarget );
            gpu_commands->barrier( stage->barrier );

            stage->clear.set( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            const u32 features_count = stage->features.size;
            if ( features_count ) {
                for ( u32 i = 0; i < features_count; ++i ) {
                    stage->features[ i ]->render( *this, sort_key, gpu_commands, *stage->render_view );
                }
            }

            if ( features_count == 0 ) {
                hprint( "Error: trying to render a stage with 0 features. Nothing will be rendered.\n" );
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
            hy_assert( false );

            stage->barrier.set( PipelineStage::ComputeShader, PipelineStage::FragmentShader );
            gpu_commands->barrier( stage->barrier );
            break;
        }

        case RenderPassType::Swapchain:
        {
            stage->clear.set( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, gpu->get_swapchain_pass() );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            const u32 features_count = stage->features.size;
            if ( features_count ) {
                for ( u32 i = 0; i < features_count; ++i ) {
                    stage->features[ i ]->render( *this, sort_key, gpu_commands, *stage->render_view );
                }
            }
            break;
        }
    }

    gpu_commands->pop_marker();
}

void Renderer::reload_resource_list( Material* material, u32 index ) {

    //ResourceListUpdate u;
    //u.resource_list = material->resource_lists[ index ];
    // TODO:
    //gpu->update_resource_list_instant( u );
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

RenderStageCreation& RenderStageCreation::set_name( cstring name_ ) {
    name = name_;
    return *this;
}

RenderStageCreation& RenderStageCreation::set_type( RenderPassType::Enum type_ ) {
    type = type_;
    return *this;
}

RenderStageCreation& RenderStageCreation::set_render_view( RenderView* view ) {
    render_view = view;
    return *this;
}

// ShaderCreation ///////////////////////////////////////////////////////////////
ShaderCreation& ShaderCreation::reset() {
    num_outputs = 0;
    hfx_ = nullptr;
    hfx_blueprint = nullptr;
    return *this;
}

ShaderCreation& ShaderCreation::set_shader_binary( hfx::ShaderEffectFile* hfx__ ) {
    hfx_ = hfx__;
    return *this;
}

ShaderCreation& ShaderCreation::set_shader_binary_v2( hfx::ShaderEffectBlueprint* hfx ) {
    hfx_blueprint = hfx;
    return *this;
}

ShaderCreation& ShaderCreation::set_outputs( const RenderPassOutput* outputs_, u32 num_outputs_ ) {
    outputs = outputs_;
    num_outputs = num_outputs_;
    return *this;
}

// MaterialCreation ///////////////////////////////////////////////////////
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

// Shader /////////////////////////////////////////////////////////////////
void Shader::get_compute_dispatches( u32 pass_index, hydra::gfx::ComputeDispatch& out_dispatch ) {
    if ( hfx_binary_v2 ) {
        hfx::ComputeDispatch& dispatch = hfx_binary_v2->passes[ pass_index ].compute_dispatch;
        out_dispatch.x = dispatch.x;
        out_dispatch.y = dispatch.y;
        out_dispatch.z = dispatch.z;

    } else {
#if defined (HFX_V2)
        hy_assertm( false, "Trying to use old HFX binary!" );
#else
        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( hfx_binary->memory, pass_index );
        out_dispatch.x = pass_header->compute_dispatch.x;
        out_dispatch.y = pass_header->compute_dispatch.y;
        out_dispatch.z = pass_header->compute_dispatch.z;

#endif // HFX_V2
    }
}

u32 Shader::get_num_passes() const {
    return hfx_binary_v2 ? hfx_binary_v2->passes.size : hfx_binary->header->num_passes;
}

// GPUProfiler ////////////////////////////////////////////////////////////

// GPU task names to colors
hydra::FlatHashMap<u64, u32>   name_to_color;

static u32      initial_frames_paused = 3;

void GPUProfiler::init( Allocator* allocator_, u32 max_frames_ ) {

    allocator = allocator_;
    max_frames = max_frames_;
    timestamps = ( GPUTimestamp* )halloca( sizeof( GPUTimestamp ) * max_frames * 32, allocator );
    per_frame_active = ( u16* )halloca( sizeof( u16 )* max_frames, allocator );

    max_duration = 16.666f;
    current_frame = 0;
    min_time = max_time = average_time = 0.f;
    paused = false;

    memset( per_frame_active, 0, 2 * max_frames );

    name_to_color.init( allocator, 16 );
    name_to_color.set_default_value( u32_max );
}

void GPUProfiler::shutdown() {

    name_to_color.shutdown();

    hfree( timestamps, allocator );
    hfree( per_frame_active, allocator );
}

void GPUProfiler::update( Device& gpu ) {

    gpu.set_gpu_timestamps_enable( !paused );

    if ( initial_frames_paused ) {
        --initial_frames_paused;
        return;
    }

    if ( paused && !gpu.resized )
        return;

    u32 active_timestamps = gpu.get_gpu_timestamps( &timestamps[ 32 * current_frame ] );
    per_frame_active[ current_frame ] = (u16)active_timestamps;

    // Get colors
    for ( u32 i = 0; i < active_timestamps; ++i ) {
        GPUTimestamp& timestamp = timestamps[ 32 * current_frame + i ];

        u64 hashed_name = hydra::hash_calculate( timestamp.name );
        u32 color_index = name_to_color.get( hashed_name );
        // No entry found, add new color
        if ( color_index == u32_max ) {

            color_index = (u32)name_to_color.size;
            name_to_color.insert( hashed_name, color_index );
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

void GPUProfiler::imgui_draw() {
    if ( initial_frames_paused ) {
        return;
    }

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
            frame_time = clamp( frame_time, 0.00001f, 1000.f );
            // Update timings
            new_average += frame_time;
            min_time = hydra::min( min_time, frame_time );
            max_time = hydra::max( max_time, frame_time );

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
