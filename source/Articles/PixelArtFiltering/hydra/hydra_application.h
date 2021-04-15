#pragma once

//
//  Hydra Application v0.19
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/24, 21.50
//
//
// Revision history //////////////////////
//
//      0.19 (2021/01/11): + Added option to disable camera input.
//      0.18 (2020/12/29): + Fixed resize callback happening only when size is different from before.
//      0.17 (2020/12/28): + Added app_reload method. + Clearer names for app load/unload.
//      0.16 (2020/12/27): + Fixed camera movement taking only one key at the time.
//      0.15 (2020/12/18): + Added reload resources callback.
//      0.14 (2020/12/16): + Added camera input and camera movement classes.
//      0.13 (2020/09/15): + Fixed passing of CommandBuffer to execution context.
//      0.12 (2020/04/12): + Added renderer to application struct. + Added configuration to enable renderer.
//      0.11 (2020/04/05): + Added optional macros to remove strict dependency to enkits and optick.
//      0.10 (2020/03/09): + Major overhaul. Added different type of apps: command line, sdl+hydra
//      0.01 (2019/09/24): + Initial implementation.
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
//
//      HYDRA_APPLICATION_CAMERA
//      Add support for a simple camera system.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "cglm/types-struct.h"

// Forward declarations
struct SDL_Window;

#define HYDRA_SDL_APP
#define HYDRA_IMGUI_APP
#define HYDRA_APPLICATION_CAMERA

namespace hydra {

    // Forward declarations
    namespace graphics {
        struct Device;
        struct CommandBuffer;
        struct Renderer;
        struct Camera;
    } // namespace graphics

    struct InputSystem;

    enum ApplicationService {
        ApplicationService_SDL,
    }; // enum ApplicationService

    enum RenderingService {
        RenderingService_Vulkan,
        RenderingService_OpenGL
    }; // enum RenderingService

#if defined(HYDRA_APPLICATION_CAMERA)

    struct CameraInput {

        float                           target_yaw;
        float                           target_pitch;

        float                           mouse_sensitivity;
        float                           movement_delta;
        uint32_t                        ignore_dragging_frames;

        vec3s                           target_movement;

        bool                            enabled;
        bool                            mouse_dragging;

        void                            init( bool enabled = true );
        void                            reset();

        void                            update( const hydra::graphics::Camera& camera, hydra::InputSystem* input, uint16_t window_center_x, uint16_t window_center_y );

    }; // struct CameraInput


    struct CameraMovementUpdate {

        float                           rotation_speed;
        float                           movement_speed;

        void                            init( float rotation_speed, float movement_speed );

        void                            update( hydra::graphics::Camera& camera, CameraInput& camera_input, float delta_time );
    }; // struct CameraMovementUpdate

#endif // HYDRA_APPLICATION_CAMERA

    //
    //
    struct ApplicationConfiguration {

        uint32_t                        window_width;
        uint32_t                        window_height;

        const char* window_title;

        ApplicationService              application_service;
        RenderingService                rendering_service;

    }; // struct ApplicationConfiguration

    //
    //
    struct ApplicationUpdate {

        hydra::graphics::CommandBuffer* gpu_commands;
        hydra::graphics::Renderer*      renderer;

        float                           delta_time;

    }; // struct ApplicationUpdate

    //
    //
    struct ApplicationReload {
        
        hydra::graphics::Renderer*      renderer;
        uint8_t                         init;
        uint8_t                         shutdown;
        uint8_t                         reload;
        uint8_t                         resize;
    }; // struct ApplicationReload

    //
    //
    struct Application {

        void                            main_loop( const ApplicationConfiguration& configuration );

        // Overridable methods for a custom application.
        virtual void                    app_init() {}
        virtual void                    app_terminate() {}

        virtual void                    app_update( ApplicationUpdate& update ) {}

        virtual void                    app_resize( uint32_t width, uint32_t height ) {}
        virtual void                    app_load_resources( ApplicationReload& load ) {}
        virtual void                    app_unload_resources( ApplicationReload& unload ) {}

        virtual bool                    app_window_event( void* event ) { return false; }   // Returns true if client wants to close the application.

        // Internal methods
        void                            app_reload();
        void                            update_camera( hydra::graphics::Camera& camera, float delta_time );
        void                            center_mouse( uint16_t window_center_x, uint16_t window_center_y );

        // ImGui handling

        void                            imgui_new_frame();
        void                            imgui_collect_draw_data( hydra::graphics::CommandBuffer* gpu_commands );
        void                            imgui_render( hydra::graphics::CommandBuffer* gpu_commands );
        void                            imgui_resize( uint32_t width, uint32_t height );

        // General
        void                            present();
        
        SDL_Window*                     window          = nullptr;
        void*                           gl_context      = nullptr;

        hydra::graphics::Renderer*      renderer        = nullptr;
        hydra::InputSystem*             input           = nullptr;

#if defined(HYDRA_APPLICATION_CAMERA)
        CameraInput                     camera_input;
        CameraMovementUpdate            camera_movement_update;
#endif // HYDRA_APPLICATION_CAMERA

    }; // struct Application

} // namespace hydra
