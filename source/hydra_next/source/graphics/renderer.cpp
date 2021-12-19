
//  Hydra Rendering - v0.46

#include "graphics/renderer.hpp"

#include "kernel/numerics.hpp"
#include "kernel/file.hpp"
#include "kernel/blob_serialization.hpp"

#include "graphics/hydra_shaderfx.h"
#include "graphics/command_buffer.hpp"
#include "graphics/camera.hpp"

#include "imgui/imgui.h"

#include <cmath>

#include "external/stb_image.h"

#define HYDRA_RENDERING_VERBOSE

namespace hydra {
namespace gfx {


// Resource Loaders ///////////////////////////////////////////////////////

struct TextureLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Resource*                       create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) override;

    Renderer*                       renderer;
}; // struct TextureLoader

struct BufferLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Renderer*                       renderer;
}; // struct BufferLoader

struct SamplerLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Renderer*                       renderer;
}; // struct SamplerLoader

struct StageLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Renderer*                       renderer;
}; // struct StageLoader

struct ShaderLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Resource*                       create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) override;

    Renderer*                       renderer;
}; // struct ShaderLoader

struct MaterialLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Resource*                       create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) override;

    Renderer*                       renderer;
}; // struct MaterialLoader

struct RenderViewLoader : public hydra::ResourceLoader {

    Resource*                       get( cstring name ) override;
    Resource*                       get( u64 hashed_name ) override;

    Resource*                       unload( cstring name ) override;

    Renderer*                       renderer;
}; // struct RenderViewLoader
 
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
static TextureHandle create_texture_from_file( Device& gpu, cstring filename, cstring name ) {

    if ( filename ) {
        int comp, width, height;
        uint8_t* image_data = stbi_load( filename, &width, &height, &comp, 4 );
        if ( !image_data ) {
            hprint( "Error loading texture %s", filename );
            return k_invalid_texture;
        }

        TextureCreation creation;
        creation.set_data( image_data ).set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, 0 ).set_size( ( u16 )width, ( u16 )height, 1 ).set_name( name );

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

ClearData& ClearData::set_color( Color color ) {
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

u64 Texture::k_type_hash = 0;
u64 Buffer::k_type_hash = 0;
u64 Sampler::k_type_hash = 0;
u64 RenderStage::k_type_hash = 0;
u64 Shader::k_type_hash = 0;
u64 Material::k_type_hash = 0;
u64 RenderView::k_type_hash = 0;

static TextureLoader s_texture_loader;
static BufferLoader s_buffer_loader;
static SamplerLoader s_sampler_loader;
static StageLoader s_stage_loader;
static ShaderLoader s_shader_loader;
static MaterialLoader s_material_loader;
static RenderViewLoader s_view_loader;

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

   resource_cache.init( creation.allocator );

   // Init resource hashes
   Texture::k_type_hash = hash_calculate( Texture::k_type );
   Buffer::k_type_hash = hash_calculate( Buffer::k_type );
   Sampler::k_type_hash = hash_calculate( Sampler::k_type );
   RenderStage::k_type_hash = hash_calculate( RenderStage::k_type );
   Shader::k_type_hash = hash_calculate( Shader::k_type );
   Material::k_type_hash = hash_calculate( Material::k_type );
   RenderView::k_type_hash = hash_calculate( RenderView::k_type );   

   s_texture_loader.renderer = this;
   s_buffer_loader.renderer = this;
   s_material_loader.renderer = this;
   s_sampler_loader.renderer = this;
   s_shader_loader.renderer = this;
   s_stage_loader.renderer = this;
   s_view_loader.renderer = this;
}

void Renderer::shutdown() {

    resource_cache.shutdown( this );

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

void Renderer::set_loaders( hydra::ResourceManager* manager ) {

    manager->set_loader( Texture::k_type, &s_texture_loader );
    manager->set_loader( Buffer::k_type, &s_buffer_loader );
    manager->set_loader( Sampler::k_type, &s_sampler_loader );
    manager->set_loader( RenderStage::k_type, &s_stage_loader );
    manager->set_loader( Shader::k_type, &s_shader_loader );
    manager->set_loader( Material::k_type, &s_material_loader );
    manager->set_loader( RenderView::k_type, &s_view_loader );
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
        buffer->name = creation.name;
        gpu->query_buffer( handle, buffer->desc );

        if ( creation.name != nullptr ) {
            resource_cache.buffers.insert( hash_calculate( creation.name ), buffer );
        }

        buffer->references = 1;

        return buffer;
    }
    return nullptr;
}

Buffer* Renderer::create_buffer( BufferType::Mask type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name ) {
    BufferCreation creation{ type, usage, size, data, name };
    return create_buffer( creation );
}

static const u32 k_dynamic_buffer_permutation = hydra::gfx::BufferType::Vertex_mask | hydra::gfx::BufferType::Index_mask | hydra::gfx::BufferType::Constant_mask;

Buffer* Renderer::create_dynamic_buffer( BufferType::Mask type, u32 size, cstring name ) {

    if ( ( type & k_dynamic_buffer_permutation ) == 0 ) {
        hprint( "Error creating dynamic buffer, it can be only vertex, index or constants type.\n" );
        return nullptr;
    }

    BufferCreation creation{ type, ResourceUsageType::Dynamic, size, nullptr, name };
    return create_buffer( creation );
}

Texture* Renderer::create_texture( const TextureCreation& creation ) {
    Texture* texture = textures.obtain();

    if ( texture ) {
        TextureHandle handle = gpu->create_texture( creation );
        texture->handle = handle;
        texture->name = creation.name;
        gpu->query_texture( handle, texture->desc );

        if ( creation.name != nullptr ) {
            resource_cache.textures.insert( hash_calculate( creation.name ), texture );
        }

        texture->references = 1;

        return texture;
    }
    return nullptr;
}

Texture* Renderer::create_texture( cstring name, cstring filename ) {
    Texture* texture = textures.obtain();

    if ( texture ) {
        TextureHandle handle = create_texture_from_file( *gpu, filename, name );
        texture->handle = handle;
        gpu->query_texture( handle, texture->desc );
        texture->references = 1;
        texture->name = name;

        resource_cache.textures.insert( hash_calculate( name ), texture );

        return texture;
    }
    return nullptr;
}

Sampler* Renderer::create_sampler( const SamplerCreation& creation ) {
    Sampler* sampler = samplers.obtain();
    if ( sampler ) {
        SamplerHandle handle = gpu->create_sampler( creation );
        sampler->handle = handle;
        sampler->name = creation.name;
        gpu->query_sampler( handle, sampler->desc );

        if ( creation.name != nullptr ) {
            resource_cache.samplers.insert( hash_calculate( creation.name ), sampler );
        }

        sampler->references = 1;

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
        stage->name_hash = hydra::hash_calculate( creation.name );
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

        if ( creation.name != nullptr ) {
            resource_cache.stages.insert( hash_calculate( creation.name ), stage );
        }

        stage->references = 1;
        
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
        shader->name = creation.hfx_blueprint->name.c_str();

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

        if ( creation.hfx_blueprint->name.c_str() != nullptr ) {
            resource_cache.shaders.insert( hash_calculate( creation.hfx_blueprint->name.c_str() ), shader );
        }

        shader->references = 1;

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
        material->name = creation.name;
        
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

        if ( creation.name != nullptr ) {
            resource_cache.materials.insert( hash_calculate( creation.name ), material );
        }

        material->references = 1;

        return material;
    }
    return nullptr;
}

Material* Renderer::create_material( Shader* shader, ResourceListCreation* resource_lists, u32 num_lists, cstring name ) {
    MaterialCreation creation{ shader, resource_lists, name, num_lists };
    return create_material( creation );
}

RenderView* Renderer::create_render_view( Camera* camera, cstring name, u32 width_, u32 height_, RenderStage** stages_, u32 num_stages ) {
    RenderView* render_view = render_views.obtain();

    render_view->camera = camera;
    render_view->name = name;
    render_view->width = u16( width_ );
    render_view->height = u16( height_ );
    render_view->dependant_render_stages.init( gpu->allocator, num_stages + 2, stages_ ? num_stages : 0 );
    render_view->references = 1;

    if ( stages_ ) {
        memcpy( render_view->dependant_render_stages.data, stages_, num_stages * sizeof( RenderStage* ) );
    }

    if ( name != nullptr ) {
        resource_cache.render_views.insert( hash_calculate( name ), render_view );
    }

    return render_view;
}

void Renderer::destroy_buffer( Buffer* buffer ) {
    if ( !buffer ) {
        return;
    }

    buffer->remove_reference();
    if ( buffer->references ) {
        return;
    }

    resource_cache.buffers.remove( hash_calculate( buffer->desc.name ) );
    gpu->destroy_buffer( buffer->handle );
    buffers.release( buffer );
}

void Renderer::destroy_texture( Texture* texture ) {
    if ( !texture ) {
        return;
    }

    texture->remove_reference();
    if ( texture->references ) {
        return;
    }

    resource_cache.textures.remove( hash_calculate( texture->desc.name ) );
    gpu->destroy_texture( texture->handle );
    textures.release( texture );
}

void Renderer::destroy_sampler( Sampler* sampler ) {
    if ( !sampler ) {
        return;
    }

    sampler->remove_reference();
    if ( sampler->references ) {
        return;
    }

    resource_cache.samplers.remove( hash_calculate( sampler->desc.name ) );
    gpu->destroy_sampler( sampler->handle );
    samplers.release( sampler );
}

void Renderer::destroy_stage( RenderStage* stage ) {
    if ( !stage ) {
        return;
    }

    stage->remove_reference();
    if ( stage->references ) {
        return;
    }

    if ( stage->type != RenderPassType::Swapchain )
        gpu->destroy_render_pass( stage->render_pass );

    stage->features.shutdown();

    resource_cache.stages.remove( hash_calculate( stage->name ) );
    stages.release( stage );
}

void Renderer::destroy_shader( Shader* shader ) {
    if ( !shader ) {
        return;
    }

    shader->remove_reference();
    if ( shader->references ) {
        return;
    }

    const u32 passes = shader->get_num_passes();

    for ( uint32_t i = 0; i < passes; ++i ) {
        ShaderPass& pass = shader->passes[ i ];
        gpu->destroy_pipeline( pass.pipeline );
        gpu->destroy_resource_layout( pass.resource_layout );
    }

    shader->passes.shutdown();

    resource_cache.shaders.remove( hash_calculate( shader->hfx_binary_v2->name.c_str() ) );
    
    hfree( shader->hfx_binary_v2, gpu->allocator );
    shaders.release( shader );
}

void Renderer::destroy_material( Material* material ) {
    if ( !material ) {
        return;
    }

    material->remove_reference();
    if ( material->references ) {
        return;
    }

    for ( uint32_t i = 0; i < material->passes.size; ++i ) {
        MaterialPass& pass = material->passes[ i ];
        gpu->destroy_resource_list( pass.resource_list );
    }

    material->passes.shutdown();
    
    resource_cache.materials.remove( hash_calculate( material->name ) );
    materials.release( material );
}

void Renderer::destroy_render_view( RenderView* render_view ) {
    if ( !render_view ) {
        return;
    }

    render_view->remove_reference();
    if ( render_view->references ) {
        return;
    }

    render_view->dependant_render_stages.shutdown();

    resource_cache.render_views.remove( hash_calculate( render_view->name ) );
    render_views.release( render_view );
}

void* Renderer::dynamic_allocate( Buffer* buffer ) {
    MapBufferParameters cb_map = { buffer->handle, 0, 0 };
    return gpu->map_buffer( cb_map );
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

bool Renderer::resize_stage( RenderStage* stage, u32 new_width, u32 new_height ) {

    if ( !stage->resize.resize )
        return false;

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

    return true;
}

bool Renderer::resize_view( RenderView* view, u32 new_width, u32 new_height ) {
    if ( new_width == view->width && new_height == view->height ) {
        return false;
    }

    bool resized = false;

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

        const bool stage_resized = resize_stage( stage, new_width, new_height );
        resized = resized || stage_resized;
    }

    return resized;
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
            stage->barrier.new_barrier_experimental = 1;
            stage->barrier.load_operation = stage->clear.color_operation == hydra::gfx::RenderPassOperation::Load;
            gpu_commands->barrier( stage->barrier );

            stage->clear.set( sort_key, gpu_commands );

            gpu_commands->bind_pass( sort_key++, stage->render_pass );
            gpu_commands->set_scissor( sort_key++, nullptr );
            gpu_commands->set_viewport( sort_key++, nullptr );

            const u32 features_count = stage->features.size;
            if ( features_count ) {
                for ( u32 i = 0; i < features_count; ++i ) {
                    stage->features[ i ]->render( *this, sort_key, gpu_commands, *stage->render_view, stage->name_hash );
                }
            }

            if ( features_count == 0 ) {
                hprint( "Error: trying to render a stage with 0 features. Nothing will be rendered.\n" );
            }

            stage->barrier.load_operation = 0;
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
                    stage->features[ i ]->render( *this, sort_key, gpu_commands, *stage->render_view, stage->name_hash );
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
    name = nullptr;
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

MaterialCreation& MaterialCreation::set_name( cstring name_ ) {
    name = name_;
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

// Resource Loaders ///////////////////////////////////////////////////////
// Texture Loader /////////////////////////////////////////////////////////
Resource* TextureLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.textures.get( hashed_name );
}

Resource* TextureLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.textures.get( hashed_name );
}

Resource* TextureLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    Texture* texture = renderer->resource_cache.textures.get( hashed_name );
    if ( texture ) {
        renderer->destroy_texture( texture );
    }
    return nullptr;
}

Resource* TextureLoader::create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) {
    return renderer->create_texture( name, filename );
}

// BufferLoader //////////////////////////////////////////////////////////
Resource* BufferLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.buffers.get( hashed_name );
}

Resource* BufferLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.buffers.get( hashed_name );
}

Resource* BufferLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    Buffer* buffer = renderer->resource_cache.buffers.get( hashed_name );
    if ( buffer ) {
        renderer->destroy_buffer( buffer );
    }
        
    return nullptr;
}

// SamplerLoader /////////////////////////////////////////////////////////
Resource* SamplerLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.samplers.get( hashed_name );
}

Resource* SamplerLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.samplers.get( hashed_name );
}

Resource* SamplerLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    Sampler* sampler = renderer->resource_cache.samplers.get( hashed_name );
    if ( sampler ) {
        renderer->destroy_sampler( sampler );
    }
    return nullptr;
}

// StageLoader ///////////////////////////////////////////////////////////
Resource* StageLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.stages.get( hashed_name );
}

Resource* StageLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.stages.get( hashed_name );
}

Resource* StageLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    RenderStage* stage = renderer->resource_cache.stages.get( hashed_name );
    if ( stage ) {
        renderer->destroy_stage( stage );
    }
    return nullptr;
}

// ShaderLoader //////////////////////////////////////////////////////////
Resource* ShaderLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.shaders.get( hashed_name );
}

Resource* ShaderLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.shaders.get( hashed_name );
}

Resource* ShaderLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    Shader* shader = renderer->resource_cache.shaders.get( hashed_name );
    if ( shader ) {
        renderer->destroy_shader( shader );
    }
    return nullptr;
}

Resource* ShaderLoader::create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) {
    using namespace hydra::gfx;

    hydra::BlobSerializer bs;
    // TODO: allocator
    hydra::FileReadResult frr = hydra::file_read_binary( filename, renderer->gpu->allocator );
    if ( frr.size ) {
        hfx::ShaderEffectBlueprint* hfx = bs.read<hfx::ShaderEffectBlueprint>( renderer->gpu->allocator, hfx::ShaderEffectBlueprint::k_version, frr.size, frr.data );

        RenderPassOutput rpo[ 8 ];

        // TODO: find proper stage!
        RenderStage* stage = renderer->stages.get( 0 );
        for ( u32 p = 0; p < hfx->passes.size; ++p ) {
            rpo[ p ] = stage->output;
        }

        Shader* shader = renderer->create_shader( hfx, rpo, hfx->passes.size );

        bs.shutdown();

        return shader;
    }

    return nullptr;
}

// MaterialLoader ////////////////////////////////////////////////////////
Resource* MaterialLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.materials.get( hashed_name );
}

Resource* MaterialLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.materials.get( hashed_name );
}

Resource* MaterialLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    Material* material = renderer->resource_cache.materials.get( hashed_name );
    if ( material ) {
        renderer->destroy_material( material );
    }
    return nullptr;
}

Resource* MaterialLoader::create_from_file( cstring name, cstring filename, ResourceManager* resource_manager ) {
    hydra::BlobSerializer bs;
    Allocator* allocator = renderer->gpu->allocator;
    hydra::FileReadResult frr = hydra::file_read_binary( filename, allocator );
    if ( frr.size ) {

        MaterialBlob* blob = bs.read<MaterialBlob>( allocator, MaterialBlob::k_version, frr.size, frr.data );
        // Create shader lookup
        hydra::FlatHashMap<u64, u64> binding_to_resource;
        binding_to_resource.init( allocator, 4 );
        binding_to_resource.set_default_value( u64_max );

        for ( u32 i = 0; i < blob->bindings.size; ++i ) {
            const BindingBlueprint& bb = blob->bindings[ i ];
            binding_to_resource.insert( bb.name_hash, bb.resource_db_name_hash );
        }

        using namespace hydra::gfx;
        ResourceListCreation rlc[ 16 ];
        Shader* shader = resource_manager->load<hydra::gfx::Shader>( blob->hfx_path.c_str() );

        for ( u32 p = 0; p < shader->passes.size; ++p ) {
            ResourceLayoutDescription rld;
            renderer->gpu->query_resource_layout( shader->passes[ p ].resource_layout, rld );

            rlc[ p ].reset();

            for ( u32 i = 0; i < rld.num_active_bindings; ++i ) {
                const ResourceBinding& rb = rld.bindings[ i ];

                u64 resource_hash = binding_to_resource.get( hydra::hash_calculate( rb.name ) );

                switch ( rb.type ) {
                    case ResourceType::Constants:
                    {
                        Buffer* buffer = resource_manager->get<hydra::gfx::Buffer>( resource_hash );
                        if ( buffer ) {
                            rlc[ p ].buffer( buffer->handle, u16( i ) );
                        }
                        else {
                            hprint( "Material Creation Error: material %s, missing buffer %s in db, index %u\n", name, rb.name, i );
                        }
                        break;
                    }

                    case ResourceType::Texture:
                    {
                        Texture* texture = resource_manager->get<hydra::gfx::Texture>( resource_hash );
                        if ( texture ) {
                            rlc[ p ].texture( texture->handle, u16( i ) );
                        }
                        else {
                            hprint( "Material Creation Error: material %s, missing texture %s in db, index %u\n", name, rb.name, i );
                        }
                        break;
                    }

                    default:
                    {
                        hprint( "Material Creation Error: material %s, unsupported resource %s type %s, index %u\n", name, rb.name, ResourceType::ToString((ResourceType::Enum)rb.type), i );
                        break;
                    }
                }
            }
        }

        binding_to_resource.shutdown();
        bs.shutdown();
        hydra::gfx::Material* material = renderer->create_material( shader, rlc, shader->passes.size, blob->name.c_str() );
        hfree( blob, allocator );

        return material;
    }
    return nullptr;
}

// RenderViewLoader //////////////////////////////////////////////////////
Resource* RenderViewLoader::get( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    return renderer->resource_cache.render_views.get( hashed_name );
}

Resource* RenderViewLoader::get( u64 hashed_name ) {
    return renderer->resource_cache.render_views.get( hashed_name );
}

Resource* RenderViewLoader::unload( cstring name ) {
    const u64 hashed_name = hash_calculate( name );
    RenderView* view = renderer->resource_cache.render_views.get( hashed_name );
    if ( view ) {
        renderer->destroy_render_view( view );
    }
    return nullptr;
}

// ResourceCache
void ResourceCache::init( Allocator* allocator ) {
    // Init resources caching
    textures.init( allocator, 16 );
    buffers.init( allocator, 16 );
    samplers.init( allocator, 16 );
    stages.init( allocator, 16 );
    shaders.init( allocator, 16 );
    materials.init( allocator, 16 );
    render_views.init( allocator, 16 );
}

void ResourceCache::shutdown( Renderer* renderer ) {

    hydra::FlatHashMapIterator it = shaders.iterator_begin();

    while ( it.is_valid() ) {
        hydra::gfx::Shader* shader = shaders.get( it );

        // TODO
        //hfree( shader->hfx_binary_v2, allocator );

        renderer->destroy_shader( shader );

        shaders.iterator_advance( it );
    }

    it = materials.iterator_begin();

    while ( it.is_valid() ) {
        hydra::gfx::Material* material = materials.get( it );
        renderer->destroy_material( material );

        materials.iterator_advance( it );
    }

    it = textures.iterator_begin();

    while ( it.is_valid() ) {
        hydra::gfx::Texture* texture = textures.get( it );
        renderer->destroy_texture( texture );

        textures.iterator_advance( it );
    }

    it = stages.iterator_begin();

    while ( it.is_valid() ) {
        hydra::gfx::RenderStage* stage = stages.get( it );
        renderer->destroy_stage( stage );

        stages.iterator_advance( it );
    }

    for ( it = render_views.iterator_begin(); it.is_valid(); render_views.iterator_advance( it ) ) {
        RenderView* view = render_views.get( it );
        renderer->destroy_render_view( view );
    }

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();
    stages.shutdown();
    shaders.shutdown();
    materials.shutdown();
    render_views.shutdown();
}

} // namespace graphics
} // namespace hydra
