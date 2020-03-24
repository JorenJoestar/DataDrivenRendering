#pragma once

//
//  Hydra Application v0.10
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/24, 21.50
//
//
// Revision history //////////////////////
//
//      0.10 (2020/03/09) + Major overhaul. Added different type of apps: command line, sdl+hydra
//      0.01 (2019/09/24) + Initial implementation.
//
// API Documentation /////////////////////////
//
//
// Customization /////////////////////////
//
//      HYDRA_CMD_APP
//      Creates a command line application.
//
//      HYDRA_SDL_APP
//      Creates a barebone SDL + OpenGL application.
//
//      HYDRA_IMGUI_APP
//      Adds support for ImGui.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
namespace enki {
    class TaskScheduler;
}

struct SDL_Window;

#define HYDRA_SDL_APP
#define HYDRA_IMGUI_APP

namespace hydra {

    // Forward declarations
    namespace graphics {
        struct Device;
        struct CommandBuffer;
    } // namespace graphics

    enum ApplicationRootTaskType {
        ApplicationRootTask_SingleExecution,
        ApplicationRootTask_SDL,
        ApplicationRootTask_Custom
    };

    struct ApplicationConfiguration {

        void*                           root_task;
        uint32_t                        window_width;
        uint32_t                        window_height;

        ApplicationRootTaskType         root_task_type;

        const char*                     window_title;
    };

    struct Application {

#if defined (HYDRA_CMD_APP)

        void                            main_loop( const ApplicationConfiguration& configuration );

#elif defined (HYDRA_SDL_APP)

        void                            main_loop( const ApplicationConfiguration& configuration );

        // Overridable methods for a custom application.
        virtual void                    app_init() {}
        virtual void                    app_terminate() {}

        virtual void                    app_update() {}

        virtual void                    app_resize( uint32_t width, uint32_t height ) {}
        virtual bool                    app_window_event( void* event ) { return false; }   // Returns true if client wants to close the application.

        // Internal methods

        // ImGui handling
        void                            imgui_init();
        void                            imgui_terminate();

        void                            imgui_new_frame();
        void                            imgui_collect_draw_data();
        void                            imgui_render();

        // General
        void                            present();


        SDL_Window*                     window          = nullptr;
        void*                           gl_context      = nullptr;

        enki::TaskScheduler*            task_scheduler  = nullptr;

        hydra::graphics::Device*        gfx_device      = nullptr;
        hydra::graphics::CommandBuffer* gfx_commands    = nullptr;

#endif // HYDRA_CMD_APP

    }; // struct Application

} // namespace hydra
