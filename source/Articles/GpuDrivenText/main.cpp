#include "kernel/primitive_types.hpp"

#include "application/game_application.hpp"
#include "application/window.hpp"
#include "application/hydra_imgui.hpp"

#include "imgui/imgui.h"

#include "graphics/command_buffer.hpp"
#include "graphics/hydra_shaderfx.h"
#include "graphics/sprite_batch.hpp"
#include "graphics/camera.hpp"

#include "kernel/blob_serialization.hpp"
#include "kernel/file.hpp"
#include "kernel/memory.hpp"
#include "kernel/numerics.hpp"

#include "cglm/struct/mat4.h"

#define uint uint32_t
#define uvec4 vec4
#include "generated/pixel_art.bhfx2.h"
#include "generated/debug_gpu_text.bhfx2.h"

// Compiler ///////////////////////////////////////////////////////////////
static void compile_resources( cstring root, bool force_compilation ) {

    hydra::directory_change( root );
    hydra::Directory directory;
    hydra::directory_current( &directory );

    hprint( "Executing from path %s\n", directory.path );

    hfx::hfx_compile( "..//data//articles//GpuDrivenText//pixel_art.hfx", "..//bin//data//pixel_art.bhfx2", hfx::CompileOptions_VulkanStandard, "..//source//Articles//GpuDrivenText//generated", force_compilation);
    hfx::hfx_compile( "..//data//articles//GpuDrivenText//debug_gpu_text.hfx", "..//bin//data//debug_gpu_text.bhfx2", hfx::CompileOptions_VulkanStandard, "..//source//Articles//GpuDrivenText//generated", force_compilation );
}

// GameApplication ////////////////////////////////////////////////////////

struct hg04 : public hydra::GameApplication {

    void            create( const hydra::ApplicationConfiguration& configuration ) override;
    void            destroy() override;

    bool            main_loop() override;

    hydra::gfx::SpriteBatch         sprite_batch;
    hydra::gfx::Camera              main_camera;

    hydra::gfx::Shader*             pixel_art_shader;

    hydra::gfx::Shader*             debug_gpu_font_shader;
    hydra::gfx::Material*           debug_gpu_font_material;

    hydra::gfx::Material*           nightmare_sprite_material;
    hydra::gfx::Texture*            nightmare_sprite_texture;

    hydra::gfx::Buffer*             pixel_art_local_constants_cb;
    hydra::gfx::Buffer*             debug_gpu_font_ub;
    hydra::gfx::Buffer*             debug_gpu_font_entries_ub;
    hydra::gfx::Buffer*             debug_gpu_font_dispatches_ub;
    hydra::gfx::Buffer*             debug_gpu_font_indirect_buffer;

    hydra::gfx::RenderStage*        forward_stage;
    hydra::gfx::RenderStage*        gpu_font_dispatch_stage;

    hydra::gfx::Texture*            main_texture;
    hydra::gfx::Texture*            main_depth;
}; // struct HG04

//
//
struct ShaderManager {

    void                            init( hydra::Allocator* allocator, hydra::gfx::Renderer* gfx );
    void                            shutdown();

    hydra::gfx::Shader*             create_shader( cstring path, hydra::gfx::RenderPassOutput* outputs, u32 num_outputs );
    hydra::gfx::Shader*             load_shader( cstring name );

    hydra::gfx::Renderer*                           gfx;
    hydra::Allocator*                               allocator;
    hydra::FlatHashMap<u64, hydra::gfx::Shader*>    shaders;

}; // struct ShaderManager

ShaderManager shader_manager;
hydra::gfx::GPUProfiler gpu_profiler;

void hg04::create( const hydra::ApplicationConfiguration& configuration ) {

    GameApplication::create( configuration );

    hydra::Allocator* allocator = &hydra::MemoryService::instance()->system_allocator;

    shader_manager.init( allocator, gfx );
    gpu_profiler.init( allocator, 100 );

    using namespace hydra::gfx;
    // Create constant buffer
    pixel_art_local_constants_cb = gfx->create_buffer( BufferType::Constant_mask, ResourceUsageType::Dynamic, sizeof( pixel_art::fat_sprite::frag::LocalConstants ), nullptr, "pixel_art_local_constants_cb" );
    debug_gpu_font_ub = gfx->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_ub" );
    debug_gpu_font_entries_ub = gfx->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_entries_ub" );
    debug_gpu_font_dispatches_ub = gfx->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_dispatches" );
    debug_gpu_font_indirect_buffer = gfx->create_buffer( (BufferType::Mask)(BufferType::Indirect_mask | BufferType::Structured_mask), ResourceUsageType::Dynamic, sizeof(f32) * 8, nullptr, "gpu_font_indirect" );

    // Forward resources
    TextureCreation rt{};
    rt.set_size( gfx->width, gfx->height, 1 ).set_format_type( TextureFormat::D32_FLOAT_S8X24_UINT, TextureType::Texture2D ).set_flags( 1, TextureCreationFlags::RenderTarget_mask ).set_name( "Main Depth" );
    main_depth = gfx->create_texture( rt );

    rt.set_format_type( TextureFormat::B8G8R8A8_UNORM, TextureType::Texture2D ).set_name( "Main RT" );
    main_texture = gfx->create_texture( rt );


    // Stage Creation
    RenderStageCreation rsc;
    rsc.reset().set_type( RenderPassType::Standard ).set_name( "forward" ).add_render_texture( main_texture ).set_depth_stencil_texture( main_depth );
    rsc.clear.reset().set_depth( 1 );
    rsc.clear.color_operation = RenderPassOperation::Clear;
    forward_stage = gfx->create_stage( rsc );

    rsc.reset().set_type( RenderPassType::Compute ).set_name( "gpu_font" );
    gpu_font_dispatch_stage = gfx->create_stage( rsc );

    RenderPassOutput so[] = { gfx->gpu->swapchain_output, gpu_font_dispatch_stage->output, gfx->gpu->swapchain_output, gfx->gpu->swapchain_output };
    debug_gpu_font_shader = shader_manager.create_shader( "..//bin//data//debug_gpu_text.bhfx2", so, 4 );
    if ( debug_gpu_font_shader ) {
        using namespace hydra::gfx;
        
        gpu_text::fullscreen::table().reset().set_DebugGpuFontEntries( debug_gpu_font_entries_ub ).set_DebugGpuFontBuffer( debug_gpu_font_ub );
        gpu_text::calculate_dispatch::table().reset().set_DebugGpuFontBuffer( debug_gpu_font_ub ).set_DebugGPUFontDispatch( debug_gpu_font_dispatches_ub )
            .set_DebugGpuFontEntries( debug_gpu_font_entries_ub ).set_DebugGPUIndirect( debug_gpu_font_indirect_buffer );

        gpu_text::sprite::table().reset().set_Local( pixel_art_local_constants_cb ).set_DebugGpuFontBuffer( debug_gpu_font_ub )
            .set_DebugGPUFontDispatch( debug_gpu_font_dispatches_ub ).set_DebugGpuFontEntries( debug_gpu_font_entries_ub );
        
        gpu_text::through::table().reset().set_albedo( main_texture );

        debug_gpu_font_material = gfx->create_material( debug_gpu_font_shader, gpu_text::tables, gpu_text::pass_count );
    }

    RenderPassOutput so2[] = { forward_stage->output };
    pixel_art_shader = shader_manager.create_shader( "..//bin//data//pixel_art.bhfx2", so2, 1 );
    if ( pixel_art_shader ) {
        // Load texture
        nightmare_sprite_texture = gfx->create_texture( "..//data//articles//GpuDrivenText//nightmare-galloping.png" );

        pixel_art::fat_sprite::table().reset().set_Local( pixel_art_local_constants_cb ).
            set_albedo( nightmare_sprite_texture ).set_DebugGpuFontBuffer( debug_gpu_font_ub )
            .set_DebugGpuFontEntries( debug_gpu_font_entries_ub );

        nightmare_sprite_material = gfx->create_material( pixel_art_shader, pixel_art::tables, pixel_art::pass_count );
    }

    sprite_batch.init( *gfx, allocator );

    main_camera.init_orthographic( 0.01f, 40.f, gfx->width, gfx->height, 1.f );
    //main_camera.set_zoom( 1 / 2.0f );

    main_camera.position.z = 30;
}

void hg04::destroy() {

    hydra::Allocator* allocator = &hydra::MemoryService::instance()->system_allocator;

    sprite_batch.shutdown( *gfx );

    hfree( pixel_art_shader->hfx_binary_v2, allocator );
    hfree( debug_gpu_font_shader->hfx_binary_v2, allocator );


    gfx->destroy_material( debug_gpu_font_material );
    gfx->destroy_material( nightmare_sprite_material );
    gfx->destroy_texture( nightmare_sprite_texture );
    gfx->destroy_texture( main_texture );
    gfx->destroy_texture( main_depth );
    gfx->destroy_buffer( pixel_art_local_constants_cb );
    gfx->destroy_buffer( debug_gpu_font_ub );
    gfx->destroy_buffer( debug_gpu_font_entries_ub );
    gfx->destroy_buffer( debug_gpu_font_dispatches_ub );
    gfx->destroy_buffer( debug_gpu_font_indirect_buffer );
    gfx->destroy_shader( pixel_art_shader );
    gfx->destroy_shader( debug_gpu_font_shader );
    gfx->destroy_stage( forward_stage );
    gfx->destroy_stage( gpu_font_dispatch_stage );

    shader_manager.shutdown();
    gpu_profiler.shutdown();

    GameApplication::destroy();
}
;
bool hg04::main_loop() {

    while ( !window->requested_exit ) {
        window->handle_os_messages();
        gfx->begin_frame();

        // Handle resize
        if ( window->resized ) {
            gfx->on_resize( window->width, window->height );
            on_resize( window->width, window->height );
            window->resized = false;
        }

        imgui->new_frame( window->platform_handle );

        // IMGUI
        static bool s_use_fullscreen_gpu_font = false;
        if ( ImGui::Begin( "HG04" ) ) {
            ImGui::Checkbox( "Use Fullscreen GPU Font(slow)", &s_use_fullscreen_gpu_font );
            //ImGui::Image( &main_texture->handle, { 800, 500 } );
        }
        ImGui::End();

        if ( ImGui::Begin( "GPU" ) ) {
            gpu_profiler.draw_ui();

        }
        ImGui::End();

        hydra::MemoryService::instance()->debug_ui();

        
        main_camera.update();

        pixel_art::fat_sprite::frag::LocalConstants* constants = ( pixel_art::fat_sprite::frag::LocalConstants* )gfx->map_buffer( pixel_art_local_constants_cb );
        if ( constants ) {
            memcpy( constants->view_projection_matrix, main_camera.view_projection.raw, sizeof( mat4s ) );
            main_camera.get_projection_ortho_2d( constants->projection_matrix_2d );

            gfx->unmap_buffer( pixel_art_local_constants_cb );
        }


        // Reset gpu font ubo indices
        u32* gpu_font_indices = ( u32* )gfx->map_buffer( debug_gpu_font_ub );
        if ( gpu_font_indices ) {
            memset( gpu_font_indices, 0, sizeof( u32 ) * 16 );
            gfx->unmap_buffer( debug_gpu_font_ub );
        }

        static f32 timer = 0.f;


        timer += 0.016f;

        // Draw moving sprite
        hydra::gfx::SpriteGPUData sprite;
        sprite.uv_offset = { 0, 0 };
        sprite.uv_size = { 1, 1 };
        sprite.screen_space_flag = 0;
        sprite.lighting_flag = 1;
        sprite.size = { nightmare_sprite_texture->desc.width * 2.f, nightmare_sprite_texture->desc.height * 2.f };
        sprite.position = { 20 + sinf(timer) * 10, 20 + cosf(timer) * 10, 0, 1 };
        sprite.albedo_id = nightmare_sprite_texture->handle.index;

        sprite_batch.begin( *gfx, main_camera );
        sprite_batch.set( nightmare_sprite_material->pipelines[ 0 ], nightmare_sprite_material->resource_lists[ 0 ] );
        sprite_batch.add( sprite );
        sprite_batch.end( *gfx );

        hydra::gfx::CommandBuffer* cb = gfx->get_command_buffer( hydra::gfx::QueueType::Graphics, true );

        u64 sort_key = 0;
        cb->push_marker( "Frame" );
        cb->clear( sort_key, .1f, .1f, .1f, 1.f );
        cb->clear_depth_stencil( sort_key++, 1.0f, 0 );

        // Draw the sprites! ////////////////////////////        
        cb->push_marker( "Sprites" );
        // Barrier: previously used as read from shader, now it will be rendered into.
        cb->barrier( forward_stage->barrier.set( hydra::gfx::PipelineStage::FragmentShader, hydra::gfx::PipelineStage::RenderTarget ) );
        cb->bind_pass( sort_key++, forward_stage->render_pass );
        cb->set_scissor( sort_key++, nullptr );
        cb->set_viewport( sort_key++, nullptr );

        sprite_batch.draw( cb, sort_key );

        cb->pop_marker();

        // Compute gpu text dispatches
        cb->bind_pipeline( sort_key++, debug_gpu_font_material->pipelines[ gpu_text::pass_calculate_dispatch ] );
        cb->bind_resource_list( sort_key++, &debug_gpu_font_material->resource_lists[ gpu_text::pass_calculate_dispatch ], 1, 0, 0 );
        // Still need to add buffers into per-stage barrier, so do it manually.
        hydra::gfx::ExecutionBarrier barrier;
        barrier.reset().add_memory_barrier( { debug_gpu_font_indirect_buffer->handle } );
        // Barrier: previously used as draw indirect, now to read/write into compute.
        barrier.set( hydra::gfx::PipelineStage::DrawIndirect, hydra::gfx::PipelineStage::ComputeShader );

        cb->barrier( barrier );
        cb->bind_pass( sort_key++, gpu_font_dispatch_stage->render_pass );
        const hydra::gfx::ComputeDispatch& dispatch = debug_gpu_font_material->compute_dispatches[ gpu_text::pass_calculate_dispatch ];
        // Dispatch!
        cb->dispatch( sort_key++, hydra::ceilu32( 1.f / dispatch.x ), hydra::ceilu32( 1.f / dispatch.y ), hydra::ceilu32( 1.f / dispatch.z ) );
        // Barrier reverse: it will be used for draw indirect.
        barrier.set( hydra::gfx::PipelineStage::ComputeShader, hydra::gfx::PipelineStage::DrawIndirect );
        cb->barrier( barrier );

        // Pass through from main rt to swapchain
        cb->push_marker( "GPU text" );

        cb->push_marker( "Apply Main" );
        cb->barrier( forward_stage->barrier.set( hydra::gfx::PipelineStage::RenderTarget, hydra::gfx::PipelineStage::FragmentShader ) );
        cb->bind_pass( sort_key++, gfx->gpu->swapchain_pass );
        
        cb->bind_pipeline( sort_key++, debug_gpu_font_material->pipelines[ gpu_text::pass_through ] );
        cb->bind_resource_list( sort_key++, &debug_gpu_font_material->resource_lists[ gpu_text::pass_through ], 1, 0, 0 );
        // Use first_instance to retrieve texture ID for bindless use.
        cb->draw( sort_key++, hydra::gfx::TopologyType::Triangle, 0, 3, main_texture->handle.index, 1 );
        cb->pop_marker();

        // Draw fullscreen debug text or sprite based
        cb->push_marker( "Write GPU text" );
        if ( s_use_fullscreen_gpu_font ) {
            cb->bind_pipeline( sort_key++, debug_gpu_font_material->pipelines[ gpu_text::pass_fullscreen ] );
            cb->bind_resource_list( sort_key++, &debug_gpu_font_material->resource_lists[ gpu_text::pass_fullscreen ], 1, 0, 0 );
            cb->draw( sort_key++, hydra::gfx::TopologyType::Triangle, 0, 3, 0, 1 );
        }
        else {
            cb->bind_pipeline( sort_key++, debug_gpu_font_material->pipelines[ gpu_text::pass_sprite ] );
            cb->bind_resource_list( sort_key++, &debug_gpu_font_material->resource_lists[ gpu_text::pass_sprite ], 1, 0, 0 );
            cb->draw_indirect( sort_key++, debug_gpu_font_indirect_buffer->handle, 0, sizeof( u32 ) * 4 );
        }
        cb->pop_marker();
        cb->pop_marker();
        cb->pop_marker();

        gpu_profiler.update( *gfx->gpu );

        imgui->render( gfx, *cb );
        gfx->queue_command_buffer( cb );
        gfx->end_frame();
    }

    return true;
}

// ShaderManager //////////////////////////////////////////////////////////
void ShaderManager::init( hydra::Allocator* allocator_, hydra::gfx::Renderer* gfx_ ) {
    allocator = allocator_;
    gfx = gfx_;

    shaders.init( allocator, 16 );
}

void ShaderManager::shutdown() {

    shaders.shutdown();
}

hydra::gfx::Shader* ShaderManager::create_shader( cstring path, hydra::gfx::RenderPassOutput* outputs, u32 num_outputs ) {

    hydra::BlobSerializer bs;
    hydra::FileReadResult frr = hydra::file_read_binary( path, allocator );
    if ( frr.size ) {
        hfx::ShaderEffectBlueprint* hfx = bs.read<hfx::ShaderEffectBlueprint>( allocator, hfx::ShaderEffectBlueprint::k_version, frr.size, frr.data );
        
        hydra::gfx::Shader* shader = gfx->create_shader( hfx, outputs, num_outputs );

        u64 hashed_name = hydra::hash_calculate( hfx->name.c_str() );
        shaders.insert( hashed_name, shader );
        return shader;
    }
    return nullptr;
}

hydra::gfx::Shader* ShaderManager::load_shader( cstring name ) {
    u64 hashed_name = hydra::hash_calculate( name );
    return shaders.get( hashed_name );
}

// Main ///////////////////////////////////////////////////////////////////

static bool force_compilation = true;

int main( int argc, char** argv ) {

    // Run application
    hprint( "Running application\n" );

    compile_resources( ".", force_compilation );

    hg04 app;
    hydra::ApplicationConfiguration conf;
    conf.w( 1600 ).h( 1000 ).name_( "HG04" );
    app.create( conf );
    app.main_loop();
    app.destroy();

    return 0;
}
