#include "hydra_application.h"
#include "hydra_lib.h"

#include "stb_leakcheck.h"


#include <SDL.h>
#if defined (HYDRA_OPENGL)
#include <GL/glew.h>
#elif defined (HYDRA_VULKAN)
#include <SDL_vulkan.h>
#endif // HYDRA_OPENGL


namespace hydra {
    
void Application::init() {

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        printf( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    int32_t window_width = 1280, window_height = 720;

#if defined (HYDRA_OPENGL)
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags );

    gl_context = SDL_GL_CreateContext( window );

    SDL_GL_SetSwapInterval( 1 ); // Enable vsync

    // Initialize OpenGL loader
    bool err = glewInit() != GLEW_OK;
    if ( err )
    {
        fprintf( stderr, "Failed to initialize OpenGL loader!\n" );
        return;
    }

    // Init the gfx_device device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    gfx_device.init( device_creation );

    SDL_GL_GetDrawableSize( window, &window_width, &window_height );
    gfx_device.resize( window_width, window_height );

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
#elif defined (HYDRA_VULKAN)

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
#endif

    hydra_Imgui_Init( gfx_device );

    app_init();
}

void Application::main_loop() {

    init();

    ImGuiIO& io = ImGui::GetIO();
    ImVec4 clear_color = ImVec4( 0.45f, 0.05f, 0.00f, 1.00f );

    bool done = false;
    while ( !done )
    {
        SDL_Event event;
        while ( SDL_PollEvent( &event ) )
        {
            ImGui_ImplSDL2_ProcessEvent( &event );
            if ( event.type == SDL_QUIT ) {
                done = true;
            }
            else if ( event.window.windowID == SDL_GetWindowID( window ) ) {
                if ( event.type == SDL_WINDOWEVENT ) {
                    switch ( event.window.event ) {
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                        case SDL_WINDOWEVENT_RESIZED:
                        {
                            uint16_t new_width = event.window.data1 % 2 == 0 ? event.window.data1 : event.window.data1 - 1;
                            uint16_t new_height = event.window.data2 % 2 == 0 ? event.window.data2 : event.window.data2 - 1;

                            gfx_device.resize( new_width, new_height );
                            app_resize( new_width, new_height );

                            break;
                        }

                        case SDL_WINDOWEVENT_CLOSE:
                        {
                            done = true;
                            break;
                        }
                    }
                }

            }
        }

        // Start the Dear ImGui frame
        hydra_Imgui_NewFrame();
        ImGui_ImplSDL2_NewFrame( window );
        ImGui::NewFrame();

        hydra::graphics::CommandBuffer* commands = gfx_device.get_command_buffer( hydra::graphics::QueueType::Graphics, 1024 * 10, false );

        app_render( commands );

        // Rendering
        ImGui::Render();

#if defined (HYDRA_OPENGL)
        SDL_GL_MakeCurrent( window, gl_context );
#endif // HYDRA_OPENGL
        hydra_imgui_collect_draw_data( ImGui::GetDrawData(), gfx_device, *commands );

        gfx_device.queue_command_buffer( commands );
        gfx_device.present();

        commands->reset();

#if defined (HYDRA_OPENGL)
        SDL_GL_SwapWindow( window );
#endif // HYDRA_OPENGL
    }

    terminate();
}

void Application::terminate() {

    app_terminate();
        
    hydra_Imgui_Shutdown( gfx_device );
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    gfx_device.terminate();

    SDL_GL_DeleteContext( gl_context );
    SDL_DestroyWindow( window );
    SDL_Quit();

    hydra::print_format("Exiting application\n\n");
    stb_leakcheck_dumpmem();
}

} // namespace hydra