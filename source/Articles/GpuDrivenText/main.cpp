#include "kernel/primitive_types.hpp"

#include "application/game_application.hpp"
#include "application/window.hpp"
#include "application/hydra_imgui.hpp"

#include "imgui/imgui.h"

#include "graphics/command_buffer.hpp"
#include "graphics/hydra_shaderfx.h"
#include "graphics/sprite_batch.hpp"
#include "graphics/camera.hpp"
#include "graphics/gpu_profiler.hpp"
#include "graphics/animation.hpp"

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

    hfx::hfx_compile( "..//data//articles//GpuDrivenText//pixel_art.hfx", "..//bin//data//pixel_art.bhfx2", hfx::CompileOptions_VulkanStandard, "..//source//Articles//GpuDrivenText//generated", force_compilation );
    hfx::hfx_compile( "..//data//articles//GpuDrivenText//debug_gpu_text.hfx", "..//bin//data//debug_gpu_text.bhfx2", hfx::CompileOptions_VulkanStandard, "..//source//Articles//GpuDrivenText//generated", force_compilation );
}

// Sprite /////////////////////////////////////////////////////////////////

//
//
struct Sprite {

    hydra::gfx::Material*           shared_material;
    hydra::gfx::Texture*            texture;
    hydra::gfx::SpriteGPUData       gpu_data;
    hydra::AnimationState*          animation_state;
    hydra::AnimationHandle          animation;

    f32                             movement_time = 0.0f;
    f32                             random_delta_speed = 1.0f;

}; // struct Sprite

//
//
struct InfiniteCloudsSystem {

    void                            update( f32 delta_time );

    Sprite*                         cloud_sprites[8];
};

// ShaderManager //////////////////////////////////////////////////////////
//
//
struct ShaderManager {

    void                            init( hydra::Allocator* allocator, hydra::gfx::Renderer* renderer );
    void                            shutdown();

    hydra::gfx::Shader*             create_shader( cstring path, hydra::gfx::RenderPassOutput* outputs, u32 num_outputs );
    hydra::gfx::Shader*             load_shader( cstring name );

    hydra::gfx::Renderer*           renderer;
    hydra::Allocator*               allocator;
    hydra::FlatHashMap<u64, hydra::gfx::Shader*>    shaders;

}; // struct ShaderManager


// GameApplication ////////////////////////////////////////////////////////

struct hg04 : public hydra::GameApplication {

    void                            create( const hydra::ApplicationConfiguration& configuration ) override;
    void                            destroy() override;

    bool                            main_loop() override;

    void                            load_sprites();

    ShaderManager                   shader_manager;
    hydra::gfx::SpriteBatch         sprite_batch;
    hydra::gfx::Camera              main_camera;
    hydra::gfx::GPUProfiler         gpu_profiler;
    hydra::AnimationSystem          animation_system;
    InfiniteCloudsSystem            cloud_system;

    hydra::gfx::Shader*             pixel_art_shader;

    hydra::gfx::Shader*             debug_gpu_font_shader;
    hydra::gfx::Material*           debug_gpu_font_material;

    hydra::gfx::Material*           shared_sprite_material;

    hydra::Array<Sprite>            sprites;

    hydra::gfx::Buffer*             pixel_art_local_constants_cb;
    hydra::gfx::Buffer*             debug_gpu_font_ub;
    hydra::gfx::Buffer*             debug_gpu_font_entries_ub;
    hydra::gfx::Buffer*             debug_gpu_font_dispatches_ub;
    hydra::gfx::Buffer*             debug_gpu_font_indirect_buffer;

    hydra::gfx::RenderStage*        forward_stage;
    hydra::gfx::RenderStage*        gpu_font_dispatch_stage;

    hydra::gfx::Texture*            main_texture;
    hydra::gfx::Texture*            main_depth;

    hydra::gfx::Texture*            dither_texture_4x4;
    hydra::gfx::Texture*            dither_texture_8x8;

}; // struct HG04

void hg04::create( const hydra::ApplicationConfiguration& configuration ) {

    GameApplication::create( configuration );

    hydra::Allocator* allocator = &hydra::MemoryService::instance()->system_allocator;

    shader_manager.init( allocator, renderer );
    gpu_profiler.init( allocator, 100 );
    animation_system.init( allocator );
    sprites.init( allocator, 8 );
    sprite_batch.init( renderer, allocator );


    using namespace hydra::gfx;
    // Create constant buffer
    pixel_art_local_constants_cb = renderer->create_buffer( BufferType::Constant_mask, ResourceUsageType::Dynamic, sizeof( pixel_art::sprite_forward::vert::LocalConstants ), nullptr, "pixel_art_local_constants_cb" );
    debug_gpu_font_ub = renderer->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_ub" );
    debug_gpu_font_entries_ub = renderer->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_entries_ub" );
    debug_gpu_font_dispatches_ub = renderer->create_buffer( BufferType::Structured_mask, ResourceUsageType::Dynamic, 1024 * 16, nullptr, "gpu_font_dispatches" );
    debug_gpu_font_indirect_buffer = renderer->create_buffer( (BufferType::Mask)(BufferType::Indirect_mask | BufferType::Structured_mask), ResourceUsageType::Dynamic, sizeof(f32) * 8, nullptr, "gpu_font_indirect" );

    // Forward resources
    TextureCreation rt{};
    rt.set_size( renderer->width, renderer->height, 1 ).set_format_type( TextureFormat::B8G8R8A8_UNORM, TextureType::Texture2D ).set_flags( 1, TextureFlags::RenderTarget_mask ).set_name( "Main RT" );
    main_texture = renderer->create_texture( rt );

    rt.set_format_type( TextureFormat::D32_FLOAT_S8X24_UINT, TextureType::Texture2D ).set_name( "Main Depth" );
    main_depth = renderer->create_texture( rt );

    // Stage Creation
    RenderStageCreation rsc;
    rsc.reset().set_type( RenderPassType::Geometry ).set_name( "forward" ).add_render_texture( main_texture ).set_depth_stencil_texture( main_depth );
    rsc.clear.reset().set_depth( 1 );
    rsc.clear.color_operation = RenderPassOperation::Clear;
    forward_stage = renderer->create_stage( rsc );

    rsc.reset().set_type( RenderPassType::Compute ).set_name( "gpu_font" );
    gpu_font_dispatch_stage = renderer->create_stage( rsc );

    RenderPassOutput so[] = { renderer->gpu->swapchain_output, gpu_font_dispatch_stage->output, renderer->gpu->swapchain_output, renderer->gpu->swapchain_output };
    debug_gpu_font_shader = shader_manager.create_shader( "..//bin//data//debug_gpu_text.bhfx2", so, ArraySize( so ) );
    if ( debug_gpu_font_shader ) {
        using namespace hydra::gfx;

        gpu_text::fullscreen::table().reset().set_DebugGpuFontEntries( debug_gpu_font_entries_ub ).set_DebugGpuFontBuffer( debug_gpu_font_ub );
        gpu_text::calculate_dispatch::table().reset().set_DebugGpuFontBuffer( debug_gpu_font_ub ).set_DebugGPUFontDispatch( debug_gpu_font_dispatches_ub )
            .set_DebugGpuFontEntries( debug_gpu_font_entries_ub ).set_DebugGPUIndirect( debug_gpu_font_indirect_buffer );

        gpu_text::sprite::table().reset().set_Local( pixel_art_local_constants_cb ).set_DebugGpuFontBuffer( debug_gpu_font_ub )
            .set_DebugGPUFontDispatch( debug_gpu_font_dispatches_ub ).set_DebugGpuFontEntries( debug_gpu_font_entries_ub );
        
        gpu_text::through::table().reset().set_albedo( main_texture );
        

        debug_gpu_font_material = renderer->create_material( debug_gpu_font_shader, gpu_text::tables, gpu_text::pass_count, "debug_gpu_material" );
    }

    RenderPassOutput so2[] = { forward_stage->output, forward_stage->output };
    pixel_art_shader = shader_manager.create_shader( "..//bin//data//pixel_art.bhfx2", so2, ArraySize(so2) );

    load_sprites();

    main_camera.init_orthographic( 0.01f, 40.f, renderer->width, renderer->height, 1.f );
    //main_camera.set_zoom( 1 / 2.0f );

    main_camera.position.z = 30;
}

void hg04::destroy() {

    hydra::Allocator* allocator = &hydra::MemoryService::instance()->system_allocator;

    sprite_batch.shutdown( renderer );

    for ( u32 i = 0; i < sprites.size; ++i ) {
        renderer->destroy_texture( sprites[ i ].texture );
        // For shared texture, setting this to nullptr avoids double deletion.
        sprites[ i ].texture = nullptr;

        if ( sprites[ i ].animation_state )
            animation_system.destroy_animation_state( sprites[ i ].animation_state );

        if ( sprites[ i ].animation != u32_max )
            animation_system.destroy_animation( sprites[ i ].animation );
    }

    sprites.shutdown();

    renderer->destroy_material( debug_gpu_font_material );
    renderer->destroy_material( shared_sprite_material );
    renderer->destroy_texture( main_texture );
    renderer->destroy_texture( main_depth );
    renderer->destroy_texture( dither_texture_4x4 );
    renderer->destroy_texture( dither_texture_8x8 );
    renderer->destroy_buffer( pixel_art_local_constants_cb );
    renderer->destroy_buffer( debug_gpu_font_ub );
    renderer->destroy_buffer( debug_gpu_font_entries_ub );
    renderer->destroy_buffer( debug_gpu_font_dispatches_ub );
    renderer->destroy_buffer( debug_gpu_font_indirect_buffer );
    renderer->destroy_shader( pixel_art_shader );
    renderer->destroy_shader( debug_gpu_font_shader );
    renderer->destroy_stage( forward_stage );
    renderer->destroy_stage( gpu_font_dispatch_stage );

    animation_system.shutdown();
    shader_manager.shutdown();
    gpu_profiler.shutdown();

    GameApplication::destroy();
}
;
bool hg04::main_loop() {

    while ( !window->requested_exit ) {
        handle_begin_frame();

        // Logic //////////////////////////////////////////////////////////
        delta_time = glm_clamp( delta_time, 0.0f, 0.25f );

        static f32 timer = 0.f;
        static bool pause_animation = false;
        timer += delta_time;

        main_camera.update();

        // Update animations and sprites uv
        if ( !pause_animation ) {
            for ( u32 i = 0; i < sprites.size; ++i ) {
                if ( sprites[ i ].animation_state ) {
                    Sprite& sprite = sprites[ i ];
                    animation_system.update_animation( *sprite.animation_state, delta_time );

                    hydra::gfx::SpriteGPUData& gpu_sprite = sprite.gpu_data;
                    gpu_sprite.uv_offset = sprite.animation_state->uv_offset;
                    gpu_sprite.uv_size = sprite.animation_state->uv_size;

                    gpu_sprite.position.x = sinf( sprite.movement_time ) * 100;

                    sprite.movement_time += delta_time * sprite.random_delta_speed;
                }
            }

            // Every 4 seconds recalculate the random delta speed
            static f32 change_timer = 4.0f;
            change_timer -= delta_time;

            if ( change_timer < 0.0f ) {
                change_timer = 4.0f;
                for ( u32 i = 0; i < sprites.size; ++i ) {
                    sprites[ i ].random_delta_speed = 1.0f + ( ( rand() % 10 ) * 0.5f - 5.0f ) * 0.1f;
                }
            }
        }

        cloud_system.update( delta_time );
        
        // IMGUI //////////////////////////////////////////////////////////
        static bool s_use_fullscreen_gpu_font = false;
        if ( ImGui::Begin( "HG04" ) ) {
            ImGui::Checkbox( "Use Fullscreen GPU Font(slow)", &s_use_fullscreen_gpu_font );
            ImGui::Checkbox( "Pause animation", &pause_animation );
            if ( ImGui::Button( "Reload shader" ) ) {

                // Destroy resources
                renderer->destroy_shader( pixel_art_shader );
                renderer->destroy_material( shared_sprite_material );

                compile_resources( ".", true );

                hydra::gfx::RenderPassOutput so2[] = { forward_stage->output, forward_stage->output };
                pixel_art_shader = shader_manager.create_shader( "..//bin//data//pixel_art.bhfx2", so2, ArraySize( so2 ) );

                pixel_art::sprite_forward::table().reset().set_Local( pixel_art_local_constants_cb )
                            .set_albedo( dither_texture_4x4 ).set_DebugGpuFontBuffer( debug_gpu_font_ub )
                            .set_DebugGpuFontEntries( debug_gpu_font_entries_ub );
                pixel_art::sky_color::table().reset();

                shared_sprite_material = renderer->create_material( pixel_art_shader, pixel_art::tables, pixel_art::pass_count, "sprite_shared_material" );

                for ( u32 i = 0; i < sprites.size; ++i ) {
                    sprites[ i ].shared_material = shared_sprite_material;
                }
            }
        }
        ImGui::End();

        if ( ImGui::Begin( "GPU" ) ) {
            gpu_profiler.imgui_draw();
        }
        ImGui::End();

        hydra::MemoryService::instance()->imgui_draw();

        pixel_art::sprite_forward::vert::LocalConstants* constants = ( pixel_art::sprite_forward::vert::LocalConstants* )renderer->map_buffer( pixel_art_local_constants_cb );
        if ( constants ) {
            memcpy( constants->view_projection_matrix, main_camera.view_projection.raw, sizeof( mat4s ) );
            main_camera.get_projection_ortho_2d( constants->projection_matrix_2d );

            renderer->unmap_buffer( pixel_art_local_constants_cb );
        }

        // Collect sprites.
        sprite_batch.begin( *renderer, main_camera );
        // Set common material. Texture is the only thing changing,
        // but it is encoded in the sprite instance data.
        sprite_batch.set( shared_sprite_material->passes[ 0 ] );

        for ( u32 i = 0; i < sprites.size; ++i ) {
            hydra::gfx::SpriteGPUData& sprite = sprites[ i ].gpu_data;
            sprite_batch.add( sprite );
        }
        sprite_batch.end( *renderer );

        // Rendering /////////////////////////////////////////////////////
        hydra::gfx::CommandBuffer* cb = renderer->get_command_buffer( hydra::gfx::QueueType::Graphics, true );

        u64 sort_key = 0;
        cb->push_marker( "Frame" );
        cb->clear( sort_key, .1f, .1f, .1f, 1.f );
        cb->clear_depth_stencil( sort_key++, 1.0f, 0 );
        cb->fill_buffer( debug_gpu_font_ub->handle, 0, 64, 0 );

        // Draw the sprites and the background! ////////////////////////////        
        {
            cb->push_marker( "Sprites" );
            hydra::gfx::ExecutionBarrier barrier;
            barrier.reset().add_memory_barrier( { debug_gpu_font_ub->handle } );
            barrier.set( hydra::gfx::PipelineStage::ComputeShader, hydra::gfx::PipelineStage::VertexShader );
            cb->barrier( barrier );

            // Barrier: previously used as read from shader, now it will be rendered into.
            cb->barrier( forward_stage->barrier.set( hydra::gfx::PipelineStage::FragmentShader, hydra::gfx::PipelineStage::RenderTarget ) );
            cb->bind_pass( sort_key++, forward_stage->render_pass );
            cb->set_scissor( sort_key++, nullptr );
            cb->set_viewport( sort_key++, nullptr );

            hydra::gfx::MaterialPass& sky_pass = shared_sprite_material->passes[ pixel_art::pass_sky_color ];
            cb->bind_pipeline( sort_key++, sky_pass.pipeline );
            cb->bind_resource_list( sort_key++, &sky_pass.resource_list, 1, 0, 0 );
            cb->draw( sort_key++, hydra::gfx::TopologyType::Triangle, 0, 3, dither_texture_4x4->handle.index, 1 );

            sprite_batch.draw( cb, sort_key );

            barrier.set( hydra::gfx::PipelineStage::VertexShader, hydra::gfx::PipelineStage::ComputeShader );
            cb->barrier( barrier );

            cb->pop_marker();
        }

        // Compute gpu text dispatches
        {
            cb->bind_pipeline( sort_key++, debug_gpu_font_material->passes[ gpu_text::pass_calculate_dispatch ].pipeline );
            cb->bind_resource_list( sort_key++, &debug_gpu_font_material->passes[ gpu_text::pass_calculate_dispatch ].resource_list, 1, 0, 0 );
            // Still need to add buffers into per-stage barrier, so do it manually.
            hydra::gfx::ExecutionBarrier barrier;
            barrier.reset().add_memory_barrier( { debug_gpu_font_indirect_buffer->handle } );
            // Barrier: previously used as draw indirect, now to read/write into compute.
            barrier.set( hydra::gfx::PipelineStage::DrawIndirect, hydra::gfx::PipelineStage::ComputeShader );

            cb->barrier( barrier );
            cb->bind_pass( sort_key++, gpu_font_dispatch_stage->render_pass );
            const hydra::gfx::ComputeDispatch& dispatch = debug_gpu_font_material->passes[ gpu_text::pass_calculate_dispatch ].compute_dispatch;
            // Dispatch!
            cb->dispatch( sort_key++, hydra::ceilu32( 1.f / dispatch.x ), hydra::ceilu32( 1.f / dispatch.y ), hydra::ceilu32( 1.f / dispatch.z ) );
            // Barrier reverse: it will be used for draw indirect.
            barrier.set( hydra::gfx::PipelineStage::ComputeShader, hydra::gfx::PipelineStage::DrawIndirect );
            cb->barrier( barrier );
        }

        {
            // Pass through from main rt to swapchain
            cb->push_marker( "Apply Main" );
            cb->barrier( forward_stage->barrier.set( hydra::gfx::PipelineStage::RenderTarget, hydra::gfx::PipelineStage::FragmentShader ) );
            cb->bind_pass( sort_key++, renderer->gpu->swapchain_pass );

            cb->bind_pipeline( sort_key++, debug_gpu_font_material->passes[ gpu_text::pass_through ].pipeline );
            cb->bind_resource_list( sort_key++, &debug_gpu_font_material->passes[ gpu_text::pass_through ].resource_list, 1, 0, 0 );
            // Use first_instance to retrieve texture ID for bindless use.
            cb->draw( sort_key++, hydra::gfx::TopologyType::Triangle, 0, 3, main_texture->handle.index, 1 );
            cb->pop_marker();
        }

        // Draw fullscreen debug text or sprite based
        {
            cb->push_marker( "Write GPU text" );
            if ( s_use_fullscreen_gpu_font ) {
                cb->bind_pipeline( sort_key++, debug_gpu_font_material->passes[ gpu_text::pass_fullscreen ].pipeline );
                cb->bind_resource_list( sort_key++, &debug_gpu_font_material->passes[ gpu_text::pass_fullscreen ].resource_list, 1, 0, 0 );
                cb->draw( sort_key++, hydra::gfx::TopologyType::Triangle, 0, 3, 0, 1 );
            } else {
                cb->bind_pipeline( sort_key++, debug_gpu_font_material->passes[ gpu_text::pass_sprite ].pipeline );
                cb->bind_resource_list( sort_key++, &debug_gpu_font_material->passes[ gpu_text::pass_sprite ].resource_list, 1, 0, 0 );
                cb->draw_indirect( sort_key++, debug_gpu_font_indirect_buffer->handle, 0, sizeof( u32 ) * 4 );
            }
            cb->pop_marker();
        }
        cb->pop_marker();   // Frame

        gpu_profiler.update( *renderer->gpu );

        imgui->render( renderer, *cb );
        renderer->queue_command_buffer( cb );
        renderer->end_frame();
    }

    return true;
}

void hg04::load_sprites() {
    
    // Nightmare galloping sprite
    // Load texture
    dither_texture_4x4 = renderer->create_texture( "BayerDither4x4", "..//data//articles//GpuDrivenText//BayerDither4x4.png" );
    dither_texture_8x8 = renderer->create_texture( "BayerDither4x4", "..//data//articles//GpuDrivenText//BayerDither8x8.png" );

    // TODO: this is an API issue. In bindless
    pixel_art::sprite_forward::table().reset().set_Local( pixel_art_local_constants_cb )
        .set_albedo( dither_texture_4x4 ).set_DebugGpuFontBuffer( debug_gpu_font_ub )
        .set_DebugGpuFontEntries( debug_gpu_font_entries_ub );
    pixel_art::sky_color::table().reset();

    shared_sprite_material = renderer->create_material( pixel_art_shader, pixel_art::tables, pixel_art::pass_count, "sprite_shared_material" );

    hydra::AnimationCreation ac{};

    // These sprites are just horizontal sprite sheets animations.
    {
        // Nightmare sprite
        Sprite& sprite = sprites.push_use();
        sprite.movement_time = 0.f;
        sprite.random_delta_speed = 1.f;
        sprite.shared_material = shared_sprite_material;
        sprite.texture = renderer->create_texture( "nightmare-galloping", "..//data//articles//GpuDrivenText//nightmare-galloping.png" );;
        const u32 num_frames = 4;
        f32 random_offset = ( rand() % 4 ) * 0.5f + 2.0f;
        sprite.gpu_data = { {10 + random_offset,0,0,-1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { sprite.texture->desc.width * 2.f / num_frames, sprite.texture->desc.height * 2.f }, 1, sprite.texture->handle.index };

        ac.reset().set_animation( num_frames, num_frames, 8, true, false ).set_offset( 0, 0 ).set_frame_size( sprite.texture->desc.width / num_frames, sprite.texture->desc.height ).set_texture_size( sprite.texture->desc.width, sprite.texture->desc.height );
        sprite.animation = animation_system.create_animation( ac );
        sprite.animation_state = animation_system.create_animation_state();
        animation_system.start_animation( *sprite.animation_state, sprite.animation, true );
    }
    {
        // Wolf sprite
        Sprite& sprite = sprites.push_use();
        sprite.movement_time = 0.f;
        sprite.random_delta_speed = 1.f;
        sprite.shared_material = shared_sprite_material;
        sprite.texture = renderer->create_texture( "wolf-running", "..//data//articles//GpuDrivenText//wolf-runing-cycle.png" );
        const u32 num_frames = 4;
        f32 random_offset = ( rand() % 4 ) * 0.5f + 2.0f;
        sprite.gpu_data = { {-10 + random_offset,-180,0,1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { sprite.texture->desc.width * 2.f / num_frames, sprite.texture->desc.height * 2.f }, 1, sprite.texture->handle.index };

        ac.reset().set_animation( num_frames, num_frames, 8, true, false ).set_offset( 0, 0 ).set_frame_size( sprite.texture->desc.width / num_frames, sprite.texture->desc.height ).set_texture_size( sprite.texture->desc.width, sprite.texture->desc.height );
        sprite.animation = animation_system.create_animation( ac );
        sprite.animation_state = animation_system.create_animation_state();
        animation_system.start_animation( *sprite.animation_state, sprite.animation, true );
    }
    {
        // Hell hound
        Sprite& sprite = sprites.push_use();
        sprite.movement_time = 0.f;
        sprite.random_delta_speed = 1.f;
        sprite.shared_material = shared_sprite_material;
        sprite.texture = renderer->create_texture( "hell-hound-run", "..//data//articles//GpuDrivenText//hell-hound-run.png" );
        const u32 num_frames = 5;
        f32 random_offset = ( rand() % 4 ) * 0.5f + 2.0f;
        sprite.gpu_data = { {0 + random_offset,180,0,-1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { sprite.texture->desc.width * 2.f / num_frames, sprite.texture->desc.height * 2.f }, 1, sprite.texture->handle.index };

        ac.reset().set_animation( num_frames, num_frames, 8, true, false ).set_offset( 0, 0 ).set_frame_size( sprite.texture->desc.width / num_frames, sprite.texture->desc.height ).set_texture_size( sprite.texture->desc.width, sprite.texture->desc.height );
        sprite.animation = animation_system.create_animation( ac );
        sprite.animation_state = animation_system.create_animation_state();
        animation_system.start_animation( *sprite.animation_state, sprite.animation, true );
    }
    {
        // Hero run
        Sprite& sprite = sprites.push_use();
        sprite.movement_time = 0.f;
        sprite.random_delta_speed = 1.f;
        sprite.shared_material = shared_sprite_material;
        sprite.texture = renderer->create_texture( "gothic-hero-run", "..//data//articles//GpuDrivenText//gothic-hero-run.png" );
        const u32 num_frames = 12;
        f32 random_offset = ( rand() % 4 ) * 0.5f + 2.0f;
        sprite.gpu_data = { {-200 + random_offset,-20,0,1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { sprite.texture->desc.width * 2.f / num_frames, sprite.texture->desc.height * 2.f }, 1, sprite.texture->handle.index };

        ac.reset().set_animation( num_frames, num_frames, 12, true, false ).set_offset( 0, 0 ).set_frame_size( sprite.texture->desc.width / num_frames, sprite.texture->desc.height ).set_texture_size( sprite.texture->desc.width, sprite.texture->desc.height );
        sprite.animation = animation_system.create_animation( ac );
        sprite.animation_state = animation_system.create_animation_state();
        animation_system.start_animation( *sprite.animation_state, sprite.animation, true );
    }
    {
        // Clouds: create more sprites
        Sprite& sprite = sprites.push_use();
        sprite.movement_time = 0.f;
        sprite.random_delta_speed = 1.f;
        sprite.shared_material = shared_sprite_material;
        sprite.texture = renderer->create_texture( "night-town-background-clouds", "..//data//articles//GpuDrivenText//night-town-background-clouds.png" );
        f32 random_offset = ( rand() % 4 ) * 0.5f + 2.0f;
        sprite.gpu_data = { {-200,-20,-1,1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { sprite.texture->desc.width * 2.f, sprite.texture->desc.height * 2.f }, 1, sprite.texture->handle.index };
        sprite.animation_state = nullptr;
        sprite.animation = u32_max;

        cloud_system.cloud_sprites[ 0 ] = &sprite;

        Sprite& sprite1 = sprites.push_use();
        sprite1 = sprite;
        sprite1.gpu_data.position.x = 1200;
        sprite1.gpu_data.position.y = 30;
        sprite.texture->add_reference();

        cloud_system.cloud_sprites[ 1 ] = &sprite1;

        Sprite& sprite2 = sprites.push_use();
        sprite2 = sprite;
        sprite2.gpu_data.position.x = 80;
        sprite2.gpu_data.position.y = 150;
        sprite.texture->add_reference();

        cloud_system.cloud_sprites[ 2 ] = &sprite2;

        Sprite& sprite3 = sprites.push_use();
        sprite3 = sprite;
        sprite3.gpu_data.position.x = 600;
        sprite3.gpu_data.position.y = 90;
        sprite.texture->add_reference();

        cloud_system.cloud_sprites[ 3 ] = &sprite3;
    }
}

int main( int argc, char** argv ) {

    if ( (argc >= 2 && strcmp( argv[ 1 ], "compiler" ) == 0)  ) {
        // Compile data
        hprint( "Compiling resources\n" );

        compile_resources( "..//", true );
    }
    else {
        // Run application
        hprint( "Running application\n" );

        compile_resources( ".", true );

        hg04 app;
        hydra::ApplicationConfiguration conf;
        conf.w( 1600 ).h( 1000 ).name_( "HG04" );
        app.create( conf );
        app.main_loop();
        app.destroy();
    }

    return 0;
}

// ShaderManager //////////////////////////////////////////////////////////
void ShaderManager::init( hydra::Allocator* allocator_, hydra::gfx::Renderer* renderer_ ) {
    allocator = allocator_;
    renderer = renderer_;

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
        
        hydra::gfx::Shader* shader = renderer->create_shader( hfx, outputs, num_outputs );

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

// InfiniteCloudsSystem //////////////////////////////////////////////////
void InfiniteCloudsSystem::update( f32 delta_time ) {
    for ( u32 i = 0; i < 4; ++i ) {
        Sprite* sprite = cloud_sprites[ i ];
        sprite->gpu_data.position.x -= 80 * delta_time;

        // If sprite is out of screen, respawn it on the right.
        if ( sprite->gpu_data.position.x + sprite->texture->desc.width / 2.0f < -800 ) {
            sprite->gpu_data.position.x = 800 + sprite->texture->desc.width;
            sprite->gpu_data.position.y = ( rand() % 20 ) * 10.0f;
        }
    }
}
