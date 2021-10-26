#include "game_application.hpp"

#include "kernel/service_manager.hpp"
#include "kernel/memory.hpp"
#include "kernel/log.hpp"

#include "application/window.hpp"
#include "application/hydra_input.hpp"
#include "application/hydra_imgui.hpp"

#include "graphics/gpu_device.hpp"
#include "graphics/renderer.hpp"
#include "graphics/command_buffer.hpp"

#include "cglm/util.h"

#include "imgui/imgui.h"

namespace hydra {

static hydra::Window s_window;

// Callbacks
static void input_os_messages_callback( void* os_event, void* user_data ) {
    hydra::InputService* input = ( hydra::InputService* )user_data;
    input->on_event( os_event );
}


// GameApplication ////////////////////////////////////////////////////////////////

void GameApplication::create( const hydra::ApplicationConfiguration& configuration ) {

    using namespace hydra;
    // Init services
    MemoryService::instance()->init( nullptr );

    service_manager = ServiceManager::instance;
    service_manager->init( &MemoryService::instance()->system_allocator );

    // Base create ////////////////////////////////////////////////////////
    
    // window
    WindowConfiguration wconf{ configuration.width, configuration.height, configuration.name, &MemoryService::instance()->system_allocator };    
    window = &s_window;
    window->init( &wconf );
    
    // input
    input = service_manager->get<hydra::InputService>();
    input->init( &MemoryService::instance()->system_allocator );

    // graphics
    hydra::gfx::DeviceCreation dc;
    dc.set_window( window->width, window->height, window->platform_handle ).set_allocator( &MemoryService::instance()->system_allocator );

    hydra::gfx::Device* gpu = service_manager->get<hydra::gfx::Device>();
    gpu->init( dc );

    // Callback register
    window->register_os_messages_callback( input_os_messages_callback, input );

    // App specific create ////////////////////////////////////////////////
    gfx = service_manager->get<hydra::gfx::Renderer>();

    hydra::gfx::RendererCreation rc{ gpu, &hydra::MemoryService::instance()->system_allocator };
    gfx->init( rc );

    // imgui backend
    imgui = service_manager->get<hydra::ImGuiService>();
    imgui->init( gfx );

    hprint( "GameApplication created successfully!\n" );
}

void GameApplication::destroy() {

    hprint( "GameApplication shutdown\n" );

    // Remove callbacks
    window->unregister_os_messages_callback( input_os_messages_callback );

    // Shutdown services
    imgui->shutdown();
    input->shutdown();
    gfx->shutdown();
    window->shutdown();

    service_manager->shutdown();

    hydra::MemoryService::instance()->shutdown();
}

bool GameApplication::main_loop() {

    // Fix your timestep
    accumulator = current_time = 0.0;

    // Main loop
    while ( !window->requested_exit ) {
        // New frame
        if ( !window->minimized ) {
            gfx->begin_frame();
        }
        input->new_frame();

        window->handle_os_messages();

        if ( window->resized ) {
            gfx->on_resize( window->width, window->height );
            on_resize( window->width, window->height );
            window->resized = false;
        }
        // This MUST be AFTER os messages!
        imgui->new_frame( window->platform_handle );

        // TODO: frametime
        f32 delta_time = ImGui::GetIO().DeltaTime;

        //hprint( "Dt %f\n", delta_time );
        delta_time = glm_clamp( delta_time, 0.0f, 0.25f );

        accumulator += delta_time;

        // Various updates
        input->update( delta_time );

        while ( accumulator >= step ) {
            fixed_update( step );

            accumulator -= step;
        }

        variable_update( delta_time );

        if ( !window->minimized ) {
            // Draw debug UIs
            hydra::MemoryService::instance()->debug_ui();

            hydra::gfx::CommandBuffer* gpu_commands = gfx->get_command_buffer( hydra::gfx::QueueType::Graphics, true );
            gpu_commands->push_marker( "Frame" );

            const f32 interpolation_factor = glm_clamp( (f32)(accumulator / step), 0.0f, 1.0f );
            render( interpolation_factor );

            imgui->render( gfx, *gpu_commands );

            gpu_commands->pop_marker();

            // Send commands to GPU
            gfx->queue_command_buffer( gpu_commands );

            gfx->end_frame();
        }
        else {
            ImGui::Render();
        }
        
        // Prepare for next frame if anything must be done.
        frame_end();
    }

    return true;
}

void GameApplication::fixed_update( f32 delta ) {
    
}

void GameApplication::variable_update( f32 delta ) {
    
}

void GameApplication::render( f32 interpolation ) {
    
}

void GameApplication::on_resize( u32 new_width, u32 new_height ) {
    
}

void GameApplication::frame_begin() {

}

void GameApplication::frame_end() {
    
}

} // namespace hydra