#include "window.hpp"

#include "kernel/log.hpp"

#include <SDL.h>

#if defined (HYDRA_VULKAN)
    #include <SDL_vulkan.h>
#endif // HYDRA_VULKAN

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

static SDL_Window* window = nullptr;

namespace hydra {

static f32 sdl_get_monitor_refresh() {
    SDL_DisplayMode current;
    int should_be_zero = SDL_GetCurrentDisplayMode( 0, &current );
    hy_assert( !should_be_zero );
    return 1.0f / current.refresh_rate;
}

void Window::init( void* configuration_ ) {
    hprint( "WindowService init\n" );

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        hprint( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );

    WindowConfiguration& configuration = *( WindowConfiguration* )configuration_;

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

#if defined (HYDRA_VULKAN)

    SDL_WindowFlags window_flags = ( SDL_WindowFlags )( SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI );

    window = SDL_CreateWindow( configuration.name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, configuration.width, configuration.height, window_flags );

    hprint( "Window created successfully\n" );

    int window_width, window_height;
    SDL_Vulkan_GetDrawableSize( window, &window_width, &window_height );

    width = (u32)window_width;
    height = (u32)window_height;

    // Assing this so it can be accessed from outside.
    platform_handle = window;

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForVulkan( window );
#endif // HYDRA_VULKAN

    // Callbacks
    os_messages_callbacks.init( configuration.allocator, 4 );
    os_messages_callbacks_data.init( configuration.allocator, 4 );

    display_refresh = sdl_get_monitor_refresh();
}

void Window::shutdown() {

    os_messages_callbacks_data.shutdown();
    os_messages_callbacks.shutdown();

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow( window );
    SDL_Quit();

    hprint( "WindowService shutdown\n" );
}

void Window::handle_os_messages() {

    SDL_Event event;
    while ( SDL_PollEvent( &event ) ) {

        ImGui_ImplSDL2_ProcessEvent( &event );

        switch ( event.type ) {
            case SDL_QUIT:
            {
                requested_exit = true;
                goto propagate_event;
                break;
            }

            // Handle subevent
            case SDL_WINDOWEVENT:
            {
                switch ( event.window.event ) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        // Resize only if even numbers are used?
                        //u32 new_width = ( u32 )( event.window.data1 % 2 == 0 ? event.window.data1 : event.window.data1 - 1 );
                        //u32 new_height = ( u32 )( event.window.data2 % 2 == 0 ? event.window.data2 : event.window.data2 - 1 );

                        u32 new_width = ( u32 )( event.window.data1 );
                        u32 new_height = ( u32 )( event.window.data2 );

                        // Update only if needed.
                        if ( new_width != width || new_height != height ) {
                            resized = true;
                            width = new_width;
                            height = new_height;

                            hprint( "Resizing to %u, %u\n", width, height );
                        }

                        break;
                    }

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    {
                        hprint( "Focus Gained\n" );
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                    {
                        hprint( "Focus Lost\n" );
                        break;
                    }
                    case SDL_WINDOWEVENT_MAXIMIZED:
                    {
                        hprint( "Maximized\n" );
                        minimized = false;
                        break;
                    }
                    case SDL_WINDOWEVENT_MINIMIZED:
                    {
                        hprint( "Minimized\n" );
                        minimized = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_RESTORED:
                    {
                        hprint( "Restored\n" );
                        minimized = false;
                        break;
                    }
                    case SDL_WINDOWEVENT_TAKE_FOCUS:
                    {
                        hprint( "Take Focus\n" );
                        break;
                    }
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        hprint( "Exposed\n" );
                        break;
                    }

                    case SDL_WINDOWEVENT_CLOSE:
                    {
                        requested_exit = true;
                        hprint( "Window close event received.\n" );
                        break;
                    }
                    default:
                    {
                        display_refresh = sdl_get_monitor_refresh();
                        break;
                    }
                }
                goto propagate_event;
                break;
            }
        }
    // Maverick: 
    propagate_event:
        // Callbacks
        for ( u32 i = 0; i < os_messages_callbacks.size; ++i ) {
            OsMessagesCallback callback = os_messages_callbacks[ i ];
            callback( &event, os_messages_callbacks_data[i] );
        }
    }
}

void Window::set_fullscreen( bool value ) {
    if ( value )
        SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP );
    else {
        SDL_SetWindowFullscreen( window, 0 );
    }
}

void Window::register_os_messages_callback( OsMessagesCallback callback, void* user_data ) {

    os_messages_callbacks.push( callback );
    os_messages_callbacks_data.push( user_data );
}

void Window::unregister_os_messages_callback( OsMessagesCallback callback ) {
    hy_assertm( os_messages_callbacks.size < 8, "This array is too big for a linear search. Consider using something different!" );
    
    for ( u32 i = 0; i < os_messages_callbacks.size; ++i ) {
        if ( os_messages_callbacks[ i ] == callback ) {
            os_messages_callbacks.delete_swap( i );
            os_messages_callbacks_data.delete_swap( i );
        }
    }
}

void Window::center_mouse( bool dragging ) {
    if ( dragging ) {
        SDL_WarpMouseInWindow( window, width / 2, height / 2 );
        SDL_SetWindowGrab( window, SDL_TRUE );
    } else {
        SDL_SetWindowGrab( window, SDL_FALSE );
    }
}

} // namespace hydra