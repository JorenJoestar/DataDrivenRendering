//  Hydra Application v0.10

#include "hydra_application.h"
#include "hydra_graphics.h"

#include "optick/optick.h"
#include "enkits/TaskScheduler.h"

#if defined (HYDRA_CMD_APP)

#elif defined (HYDRA_SDL_APP)
    
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

#endif // HYDRA_CMD_APP


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

// SDL Loop ////////////////////////////////////////////////////////

// Some helper methods to clean the code from all the options permutations.

void Application::imgui_init() {
}

void Application::imgui_terminate() {

}


void Application::imgui_new_frame() {
    
    // 1. ImGui Rendering Backend
    // 2. SDL update
    // 3. ImGui general update

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_NewFrame();
#elif defined(IMGUI_HYDRA)
    hydra::imgui_new_frame();

    gfx_commands = gfx_device->get_command_buffer( hydra::graphics::QueueType::Graphics, 1000, false );
#endif // IMGUI_GL

    // SDL is always present
    ImGui_ImplSDL2_NewFrame( window );

#if defined (HYDRA_IMGUI_APP)
    ImGui::NewFrame();
#endif // HYDRA_IMGUI_APP
}

void Application::imgui_collect_draw_data() {
    
    ImGui::Render();

#if defined (IMGUI_HYDRA)
    hydra::imgui_collect_draw_data( ImGui::GetDrawData(), *gfx_device, *gfx_commands );
#endif // HYDRA_IMGUI_APP
}

void Application::imgui_render() {

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
#elif defined (IMGUI_HYDRA)
    gfx_device->queue_command_buffer( gfx_commands );
#endif // IMGUI_GL

}

void Application::present() {
#if defined (IMGUI_GL)
    
#elif defined (IMGUI_HYDRA)
    gfx_device->present();
#endif // HYDRA_OPENGL

    SDL_GL_SwapWindow( window );
}


//
//
struct SDLMainLoopTask : enki::IPinnedTask {

                                    SDLMainLoopTask( hydra::Application* application );

    void                            Execute() override;

    hydra::Application*             application;
}; // struct SDLMainLoopTask

//
//
SDLMainLoopTask::SDLMainLoopTask( hydra::Application* application )
    : enki::IPinnedTask( k_main_thread_index ), application(application) {
}

//
//
void SDLMainLoopTask::Execute() {
    

    if ( !application ) {
        HYDRA_LOG( "Application null - program will end.\n" );
        return;
    }

    bool done = false;
    while ( !done )
    {
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

                            //gfx_device.resize( new_width, new_height );
                            application->app_resize( new_width, new_height );

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

            // Application window event
            done = done || application->app_window_event( &event );
        }

        application->imgui_new_frame();

        application->app_update();

        application->imgui_collect_draw_data();

#if defined (SDL_GL)
        SDL_GL_MakeCurrent( application->window, application->gl_context );
#endif // HYDRA_OPENGL
       
        application->imgui_render();

        application->present();
    }
    printf( "Quitting.\n" );
}

    
#if defined (HYDRA_CMD_APP)

void Application::main_loop( const ApplicationConfiguration& configuration ) {

    enki::TaskScheduler task_scheduler;
    task_scheduler.Initialize();

    enki::IPinnedTask* pinned_task = (enki::IPinnedTask*)configuration.root_task;
    task_scheduler.AddPinnedTask( pinned_task );

    task_scheduler.RunPinnedTasks();
    task_scheduler.WaitforTask( pinned_task );
}

#elif defined (HYDRA_SDL_APP)

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

    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags );

    // Init the gfx_device device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    device_creation.width = (uint16_t)window_width;
    device_creation.height = (uint16_t)window_height;
    gfx_device.init( device_creation );

    SDL_Vulkan_GetDrawableSize( window, &window_width, &window_height );
    gfx_device.resize( window_width, window_height );

    // Setup Platform/Renderer bindings

    ImGui_ImplSDL2_InitForVulkan( window );
    hydra_Imgui_Init( gfx_device );
#endif

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
#endif // SDL_GL

    // Init the gfx_device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    
    gfx_device = new hydra::graphics::Device();
    gfx_device->init( device_creation );

    int window_width, window_height;
    SDL_GL_GetDrawableSize( window, &window_width, &window_height );
    gfx_device->resize( window_width, window_height );
    
    hydra::imgui_init( *gfx_device );

    // ImGui OpenGL init
#if defined (HYDRA_IMGUI_APP)
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
#endif // HYDRA_IMGUI_APP

#if defined (IMGUI_GL)
    ImGui_ImplOpenGL3_Init();
#endif // SDL_GL

    // Init Task Scheduler
    task_scheduler = new enki::TaskScheduler();
    task_scheduler->Initialize();

    // Internal Init
    app_init();

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

    app_terminate();

#if defined(IMGUI_GL)
    ImGui_ImplOpenGL3_Shutdown();
#endif // IMGUI_GL

#if defined (HYDRA_IMGUI_APP)
    hydra::imgui_shutdown( *gfx_device );
    gfx_device->terminate();

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
#endif // HYDRA_IMGUI_APP

#if defined (SDL_GL)
    SDL_GL_DeleteContext( gl_context );
#endif // SDL_GL

    SDL_DestroyWindow( window );
    SDL_Quit();

    HYDRA_LOG( "Exiting application\n\n" );
    HYDRA_DUMP_MEMORY_LEAKS;
}

#endif // HYDRA_CMD_APP

} // namespace hydra