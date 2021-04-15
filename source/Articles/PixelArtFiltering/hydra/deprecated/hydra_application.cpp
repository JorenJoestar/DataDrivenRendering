//  Hydra Application v0.19

#include "hydra_application.h"
#include "hydra_graphics.h"
#include "hydra_rendering.h"
#include "hydra_input.h"

//#define HYDRA_PROFILER
//#define HYDRA_MULTITHREADING

#if defined (HYDRA_PROFILER)
    #include "optick/optick.h"
#else
    #define OPTICK_EVENT(a)
    #define OPTICK_TAG(a, b)
    #define OPTICK_FRAME(a)
#endif // HYDRA_PROFILER

#if defined (HYDRA_MULTITHREADING)
#include "enkits/TaskScheduler.h"
#endif // HYDRA_MULTITHREADING


#include <SDL.h>

#if defined (HYDRA_OPENGL)
    #include <GL/glew.h>
    #define SDL_GL
    #define IMGUI_HYDRA
#elif defined (HYDRA_VULKAN)
    #include <SDL_vulkan.h>
    #define SDL_VULKAN
    #define IMGUI_HYDRA
#else
    #include <GL/glew.h>
    #define SDL_GL
    #define IMGUI_GL
#endif // HYDRA_OPENGL

#if defined (HYDRA_IMGUI_APP)
    #include "imgui.h"
    #include "imgui_impl_sdl.h"
    
    #if defined (IMGUI_GL)
        #include "imgui_impl_opengl3.h"
    #elif defined(IMGUI_HYDRA)
        #include "hydra_imgui.h"
    #endif // IMGUI_GL

#endif // HYDRA_IMGUI_APP


#if defined(HYDRA_APPLICATION_CAMERA)
    #include "cglm/struct/mat4.h"
    #include "cglm/struct/cam.h"
    #include "cglm/struct/affine.h"
    #include "cglm/struct/quat.h"

    #include "SDL_scancode.h"
    #include "SDL_mouse.h"
#endif // HYDRA_APPLICATION_CAMERA


#if 0
#define HYDRA_LOG                                           hydra::print_format

#define HYDRA_ASSERT( condition, message, ... )             assert( condition )

#define HYDRA_MALLOC( size )                                hydra::hy_malloc( size )
#define HYDRA_FREE( pointer )                               hydra::hy_free( pointer )

#define HYDRA_DUMP_MEMORY_LEAKS                             stb_leakcheck_dumpmem()
#else

#define HYDRA_LOG                                           printf
#define HYDRA_ASSERT( condition, message, ... )             assert( condition )

#define HYDRA_MALLOC( size )                                malloc( size )
#define HYDRA_FREE( pointer )                               free( pointer )

#define HYDRA_DUMP_MEMORY_LEAKS
#endif // 


namespace hydra {

// Tasks
static const uint32_t               k_main_thread_index;

// Single execution task ////////////////////////////////////////////////////////

#if defined (HYDRA_MULTITHREADING)
//
//
struct SingleExecutionTask : enki::IPinnedTask {

                                    SingleExecutionTask();

    void                            Execute() override;

}; // struct SingleExecutionTask

//
//
SingleExecutionTask::SingleExecutionTask()
    : enki::IPinnedTask( k_main_thread_index ) {
}

//
//
void SingleExecutionTask::Execute() {
    OPTICK_FRAME( "MainThread" );
    printf( "Executed!\n" );
}

#else

struct SingleExecutionTask {
                                    SingleExecutionTask();

    void                            Execute();
};

SingleExecutionTask::SingleExecutionTask() {
}

//
//
void SingleExecutionTask::Execute() {
    printf( "Executed!\n" );
}

#endif // HYDRA_MULTITHREADING

// SDL Loop ////////////////////////////////////////////////////////

//
//
#if defined(HYDRA_MULTITHREADING)
struct SDLMainLoopTask : enki::IPinnedTask {
#else
struct SDLMainLoopTask {
#endif // HYDRA_MULTITHREADING

    SDLMainLoopTask( hydra::Application* application );

#if defined(HYDRA_MULTITHREADING)
    void                            Execute() override;
#else
    void                            Execute( hydra::ApplicationUpdate& update );
#endif // HYDRA_MULTITHREADING

    hydra::Application*             application;
}; // struct SDLMainLoopTask

//
//
SDLMainLoopTask::SDLMainLoopTask( hydra::Application* application )
#if defined(HYDRA_MULTITHREADING)
    : enki::IPinnedTask( k_main_thread_index ), application( application )
#else
    : application( application )
#endif // HYDRA_MULTITHREADING
{

}

//
//
void SDLMainLoopTask::Execute( hydra::ApplicationUpdate& update ) {


    if ( !application ) {
        HYDRA_LOG( "Application null - program will end.\n" );
        return;
    }

    bool done = false;
    while ( !done ) {
        OPTICK_FRAME( "MainLoopThread" );

        SDL_Event event;
        while ( SDL_PollEvent( &event ) ) {

            ImGui_ImplSDL2_ProcessEvent( &event );

            switch ( event.type ) {
                case SDL_QUIT:
                {
                    done = true;
                    break;
                }

                case SDL_WINDOWEVENT:
                {
                    switch ( event.window.event ) {
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                        case SDL_WINDOWEVENT_RESIZED:
                        {
                            uint32_t new_width = (uint32_t)(event.window.data1 % 2 == 0 ? event.window.data1 : event.window.data1 - 1);
                            uint32_t new_height = (uint32_t)(event.window.data2 % 2 == 0 ? event.window.data2 : event.window.data2 - 1);

                            // Update only if needed.
                            if ( new_width != update.gpu_device->swapchain_width || new_height != update.gpu_device->swapchain_height ) {
                                update.gpu_device->resize( new_width, new_height );
                                application->imgui_resize( new_width, new_height );
                                application->app_resize( new_width, new_height );
                            }

                            break;
                        }

                        case SDL_WINDOWEVENT_CLOSE:
                        {
                            done = true;
                            printf( "Window close event received.\n" );
                            break;
                        }
                    }
                    break;
                }
            }

            if ( application->input ) {
                application->input->on_event( &event );
            }

            // Application window event
            done = done || application->app_window_event( &event );
        }

        application->input->update( ImGui::GetIO().DeltaTime );


        application->imgui_new_frame();

        hydra::graphics::CommandBuffer* gpu_commands = update.gpu_device->get_command_buffer( hydra::graphics::QueueType::Graphics, false, true );
        gpu_commands->push_marker( "Frame" );

        hydra::ApplicationUpdate task_update{ update.gpu_device, gpu_commands, update.renderer };
        application->app_update( task_update );

        application->imgui_collect_draw_data( gpu_commands );

#if defined (SDL_GL)
        SDL_GL_MakeCurrent( application->window, application->gl_context );
#endif // HYDRA_OPENGL

        application->imgui_render( gpu_commands );

        gpu_commands->pop_marker();
        application->present();
    }
    printf( "Quitting.\n" );
}


// Some helper methods to clean the code from all the options permutations.

void hydra::Application::app_reload() {
    ApplicationReload load{ gpu_device, ApplicationState_Reload };
    app_unload_resources( load );
    app_load_resources( load );
}

void hydra::Application::update_camera( hydra::graphics::Camera& camera ) {

    auto& io = ImGui::GetIO();
    const float window_center_x = gpu_device->swapchain_width / 2.0f;
    const float window_center_y = gpu_device->swapchain_height / 2.0f;

    camera_input.update( camera, window_center_x, window_center_y );
    camera_movement_update.update( camera, camera_input, io.DeltaTime );
    
    center_mouse( window_center_x, window_center_y );

    camera.update();
}

void hydra::Application::center_mouse( uint16_t window_center_x, uint16_t window_center_y ) {
    if ( camera_input.mouse_dragging ) {
        SDL_WarpMouseInWindow( window, window_center_x, window_center_y );
        SDL_SetWindowGrab( window, SDL_TRUE );
    } else {
        SDL_SetWindowGrab( window, SDL_FALSE );
    }
}

void Application::imgui_new_frame() {
    
    // 1. ImGui Rendering Backend
    // 2. SDL update
    // 3. ImGui general update

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_NewFrame();
#elif defined(IMGUI_HYDRA)
    hydra::imgui_new_frame();

#endif // IMGUI_GL

    // SDL is always present
    ImGui_ImplSDL2_NewFrame( window );

#if defined (HYDRA_IMGUI_APP)
    ImGui::NewFrame();
#endif // HYDRA_IMGUI_APP
}

void Application::imgui_collect_draw_data( hydra::graphics::CommandBuffer* gpu_commands ) {
    
    ImGui::Render();

#if defined (IMGUI_HYDRA)
    hydra::imgui_collect_draw_data( ImGui::GetDrawData(), *gpu_device, *gpu_commands );
#endif // HYDRA_IMGUI_APP
}

void Application::imgui_render( hydra::graphics::CommandBuffer* gpu_commands ) {

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
#elif defined (IMGUI_HYDRA)
    gpu_device->queue_command_buffer( gpu_commands );
#endif // IMGUI_GL

}

void hydra::Application::imgui_resize( u32 width, u32 height ) {
    imgui_on_resize( *gpu_device, width, height );
}

void Application::present() {
#if defined (IMGUI_GL)
    
#elif defined (IMGUI_HYDRA)
    gpu_device->present();
#endif // HYDRA_OPENGL

    SDL_GL_SwapWindow( window );
}

void Application::main_loop( const ApplicationConfiguration& configuration ) {

    // Init SDL library
    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        HYDRA_LOG( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );
    
    // Init ImGui
#if defined (HYDRA_IMGUI_APP)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
#endif // HYDRA_IMGUI_APP

#if defined (HYDRA_VULKAN)

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, configuration.window_width, configuration.window_height, window_flags );

    int window_width, window_height;
    SDL_Vulkan_GetDrawableSize( window, &window_width, &window_height );

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForVulkan( window );
#endif

    hydra::memory_service_init();

#if defined (SDL_GL)

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow( configuration.window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, configuration.window_width, configuration.window_height, window_flags );

    // Creats a vanilla SDL + ImGui application.
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );

    gl_context = SDL_GL_CreateContext( window );

    SDL_GL_SetSwapInterval( 1 ); // Enable vsync

    // Initialize OpenGL loader
    if ( glewInit() != GLEW_OK )
    {
        HYDRA_LOG( "Failed to initialize OpenGL Glew loader!\n" );
        return;
    }

    int window_width, window_height;
    SDL_GL_GetDrawableSize( window, &window_width, &window_height );

#endif // SDL_GL

    // Init the gpu_device
    hydra::graphics::DeviceCreation device_creation = { window, window_width, window_height };
    gpu_device = new hydra::graphics::Device();
    gpu_device->init( device_creation );

    gpu_device->resize( window_width, window_height );

    // Initialize renderer if needed
    if ( configuration.rendering_service > hydra::RenderingService_LowLevelDevice ) {
        renderer = new hydra::graphics::Renderer();

        hydra::graphics::RendererCreation renderer_creation{ gpu_device };
        renderer->init( renderer_creation );
    }
    else {
        renderer = nullptr;
    }
    
    hydra::imgui_init( *gpu_device );

    // Input
    input = new hydra::InputSystem();
    input->init();

    // ImGui OpenGL init
#if defined (HYDRA_IMGUI_APP)
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
#endif // HYDRA_IMGUI_APP

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_Init();
#endif // SDL_GL


#if defined (HYDRA_MULTITHREADING)
    // Init Task Scheduler
    task_scheduler = new enki::TaskScheduler();
    task_scheduler->Initialize();
#endif // HYDRA_MULTITHREADING

    // Internal Init
    app_init();
    // Load resources for startup. Callback defined by user application.
    ApplicationReload load{gpu_device, ApplicationState_Init};
    app_load_resources( load );

#if defined (HYDRA_MULTITHREADING)
    // Main Loop using a task
    enki::IPinnedTask* pinned_task = nullptr;
    // Choose task type based on configuration.
    switch ( configuration.root_task_type ) {
        case ApplicationRootTask_Custom:
        {
            pinned_task = (enki::IPinnedTask*)configuration.root_task;
            break;
        }

        case ApplicationRootTask_SingleExecution:
        {
            pinned_task = new SingleExecutionTask();
            break;
        }

        case ApplicationRootTask_SDL:
        {
            pinned_task = new SDLMainLoopTask( this );
            break;
        }
    }

    if ( pinned_task ) {
        task_scheduler->AddPinnedTask( pinned_task );

        task_scheduler->RunPinnedTasks();
        task_scheduler->WaitforTask( pinned_task );

        // When task is over, the application is quitting.
        delete pinned_task;
    }
    else {
        HYDRA_LOG( "Task is null. Not executing.\n" );
    }

    delete (task_scheduler);
    task_scheduler = nullptr;
#else

    SDLMainLoopTask main_task(this);
    hydra::ApplicationUpdate application_update{ gpu_device, nullptr, renderer };
    main_task.Execute( application_update );

#endif // HYDRA_MULTITHREADING

    load.app_state = ApplicationState_Shutdown;
    app_unload_resources( load );

    if ( input ) {
        input->terminate();
        delete input;
    }

    if ( renderer ) {
        renderer->terminate();
        delete renderer;
    }

    app_terminate();

#if defined(IMGUI_GL)
    ImGui_ImplOpenGL3_Shutdown();
#endif // IMGUI_GL

#if defined (HYDRA_IMGUI_APP)
    hydra::imgui_shutdown( *gpu_device );
    gpu_device->terminate();

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
#endif // HYDRA_IMGUI_APP

#if defined (SDL_GL)
    SDL_GL_DeleteContext( gl_context );
#endif // SDL_GL

    hydra::memory_service_terminate();

    SDL_DestroyWindow( window );
    SDL_Quit();

    HYDRA_LOG( "Exiting application\n\n" );
    HYDRA_DUMP_MEMORY_LEAKS;
}

// CameraInput //////////////////////////////////////////////////////////////////
void CameraInput::init( bool enabled_ ) {

    reset();
    enabled = enabled_;
}

void CameraInput::reset() {

    target_yaw = 0.0f;
    target_pitch = 0.0f;

    target_movement = glms_vec3_zero();

    mouse_dragging = false;
    ignore_dragging_frames = 3;
    mouse_sensitivity = 0.005f;
    movement_delta = 0.03f;
}

void CameraInput::update( const hydra::graphics::Camera& camera, uint16_t window_center_x, uint16_t window_center_y ) {

    if ( !enabled )
        return;

    // Ignore first dragging frames for mouse movement waiting the cursor to be placed at the center of the screen.
    auto& input = ImGui::GetIO();

    if ( ImGui::IsMouseDragging( 1 ) && !ImGui::IsAnyItemHovered() ) {

        if ( ignore_dragging_frames == 0 ) {
            target_yaw += ( input.MousePos.x - window_center_x ) * mouse_sensitivity;
            target_pitch += ( input.MousePos.y - window_center_y ) * mouse_sensitivity;
        } else {
            --ignore_dragging_frames;
        }
        mouse_dragging = true;

    } else {
        mouse_dragging = false;

        ignore_dragging_frames = 3;
    }

    vec3s camera_movement{ 0, 0, 0 };
    float camera_movement_delta = movement_delta;

    if ( ImGui::IsKeyDown( SDL_SCANCODE_RSHIFT ) || ImGui::IsKeyDown( SDL_SCANCODE_LSHIFT ) ) {
        camera_movement_delta *= 10.0f;
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_RCTRL ) || ImGui::IsKeyDown( SDL_SCANCODE_LCTRL ) ) {
        camera_movement_delta *= 0.1f;
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_LEFT ) || ImGui::IsKeyDown( SDL_SCANCODE_A ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.right, -camera_movement_delta ) );
    } else if ( ImGui::IsKeyDown( SDL_SCANCODE_RIGHT ) || ImGui::IsKeyDown( SDL_SCANCODE_D ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.right, camera_movement_delta ) );
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_PAGEDOWN ) || ImGui::IsKeyDown( SDL_SCANCODE_E ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.up, -camera_movement_delta ) );
    } else if ( ImGui::IsKeyDown( SDL_SCANCODE_PAGEUP ) || ImGui::IsKeyDown( SDL_SCANCODE_Q ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.up, camera_movement_delta ) );
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_UP ) || ImGui::IsKeyDown( SDL_SCANCODE_W ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.direction, -camera_movement_delta ) );
    } else if ( ImGui::IsKeyDown( SDL_SCANCODE_DOWN ) || ImGui::IsKeyDown( SDL_SCANCODE_S ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.direction, camera_movement_delta ) );
    }

    target_movement = glms_vec3_add( (vec3s&)target_movement, camera_movement );
}

// CameraMovementUpdate /////////////////////////////////////////////////////////

void CameraMovementUpdate::init( float rotation_speed_, float movement_speed_ ) {
    rotation_speed = rotation_speed_;
    movement_speed = movement_speed_;
}

void CameraMovementUpdate::update( hydra::graphics::Camera& camera, CameraInput& camera_input, float delta_time ) {
    // Update camera rotation
    const float tween_speed = rotation_speed * delta_time;
    camera.rotate( ( camera_input.target_pitch - camera.pitch ) * tween_speed,
                   ( camera_input.target_yaw - camera.yaw ) * tween_speed );
    
    // Update camera position
    const float tween_position_speed = movement_speed * delta_time;
    vec3s delta_movement{ camera_input.target_movement };
    delta_movement = glms_vec3_scale( delta_movement, tween_position_speed );

    camera.position = glms_vec3_add( delta_movement, camera.position );

    // Remove delta movement from target movement
    camera_input.target_movement = glms_vec3_sub( camera_input.target_movement, delta_movement );
}


} // namespace hydra