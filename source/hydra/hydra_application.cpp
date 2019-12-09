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


#if defined (HYDRA_OPENGL)
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags );

    gl_context = SDL_GL_CreateContext( window );

    SDL_GL_SetSwapInterval( 1 ); // Enable vsync

    // Initialize OpenGL loader
    bool err = glewInit() != GLEW_OK;
    if ( err )
    {
        fprintf( stderr, "Failed to initialize OpenGL loader!\n" );
        return;
    }

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
#elif defined (HYDRA_VULKAN)

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags );

    ImGui_ImplSDL2_InitForVulkan( window );
#endif

    // Init the gfx_device device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    gfx_device.init( device_creation );

    hydra_Imgui_Init( gfx_device );

    int window_width, window_height;
    SDL_GL_GetDrawableSize( window, &window_width, &window_height );
    gfx_device.resize( window_width, window_height );

    app_init();
}

void Application::main_loop() {

    init();

    ImGuiIO& io = ImGui::GetIO();
    ImVec4 clear_color = ImVec4( 0.45f, 0.05f, 0.00f, 1.00f );

    hydra::graphics::CommandBuffer* ui_commands = gfx_device.create_command_buffer( hydra::graphics::QueueType::Graphics, 1024 * 10, false );

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
                            gfx_device.resize( event.window.data1, event.window.data2 );
                            app_resize( event.window.data1, event.window.data2 );

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

        //ui_commands->set_viewport( { 0, 0, io.DisplaySize.x, io.DisplaySize.y, 0.0f, 1.0f } );
        //ui_commands->clear( clear_color.x, clear_color.y, clear_color.z, clear_color.w );

        app_render( ui_commands );
            
        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent( window, gl_context );

        //glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
        //glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
        //glClear( GL_COLOR_BUFFER_BIT );

        hydra_imgui_collect_draw_data( ImGui::GetDrawData(), gfx_device, *ui_commands );

        gfx_device.queue_command_buffer( ui_commands );
        gfx_device.present();

        ui_commands->reset();

        SDL_GL_SwapWindow( window );
    }

    gfx_device.destroy_command_buffer( ui_commands );

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