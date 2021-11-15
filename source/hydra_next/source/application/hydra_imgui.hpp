#pragma once

//
// Hydra ImGUI - v0.15
//
// ImGUI wrapper using Hydra Graphics.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/16, 19.23
//
//      0.15 (2021/11/10): + Finished fully functional bindless support, using instance id to retrieve texture in the bindless texture.
//      0.14 (2021/11/10): + Added custom ImGui styles.
//      0.13 (2021/10/28): + Added initial support for bindless rendering.
//      0.12 (2021/10/27): + Added support for Vertex and Index offsets in ImGui rendering.
//      0.11 (2021/10/21): + Changed ImGuiService to use Renderer instead of GpuDevice.
//      0.10 (2020/01/21): + Added resize method. Internally destroys resource lists provoking a rebind the frame after.
//      0.09 (2020/12/28): + Fixed scissor clipping.
//      0.08 (2020/12/20): + Added AppLog widget copied from ImGui demo.
//      0.07 (2020/09/16): + Updates for Vulkan gfx device. + Using glsl programs for Vulkan as well. + Fixed rgba/rgba32 color conversion. + Fixed scissor to be upper for Vulkan.
//      0.06 (2020/09/15): + Update resource list and layouts to new hydra_graphics interface.
//      0.05 (2020/03/25): + Added Path Dialog.
//      0.04 (2020/03/20): + Added File Dialog. + Moved everything to be under hydra namespace.
//      0.03 (2020/03/12): + Embedded SpirV shaders.
//      0.02 (2020/03/10): + Embedded GLSL shaders and handwritten code to remove dependency on compiler. Code cleanup.
//      0.01 (2019/09/16): + Initial implementation.

#include <stdint.h>

#include "kernel/service.hpp"

struct ImDrawData;

namespace hydra {

    namespace gfx {
        struct Device;
        struct CommandBuffer;
        struct TextureHandle;
        struct Renderer;
    } // namespace graphics

    //
    //
    enum ImGuiStyles {
        Default = 0,
        GreenBlue,
        DarkRed,
        DarkGold
    }; // enum ImGuiStyles

    //
    //
    struct ImGuiService : public Service {

        hy_declare_service( ImGuiService );

        void                            init( void* configuration ) override;
        void                            shutdown() override;

        void                            new_frame( void* window_handle );
        void                            render( hydra::gfx::Renderer* renderer, hydra::gfx::CommandBuffer& commands );

        // Removes the Texture from the Cache and destroy the associated Resource List.
        void                            remove_cached_texture( hydra::gfx::TextureHandle& texture );

        //void                          imgui_on_resize( hydra::gfx::Device& gpu, uint32_t width, uint32_t height );

        void                            set_style( ImGuiStyles style );

        hydra::gfx::Renderer*           gfx;

        static constexpr cstring        k_name = "hydra_imgui_service";

    }; // ImGuiService

    // File Dialog /////////////////////////////////////////////////////////

    /*bool                                imgui_file_dialog_open( const char* button_name, const char* path, const char* extension );
    const char*                         imgui_file_dialog_get_filename();

    bool                                imgui_path_dialog_open( const char* button_name, const char* path );
    const char*                         imgui_path_dialog_get_path();*/

    // Application Log /////////////////////////////////////////////////////

    void                                imgui_log_init();
    void                                imgui_log_shutdown();

    void                                imgui_log_draw();

    // FPS graph ///////////////////////////////////////////////////
    void                                imgui_fps_init();
    void                                imgui_fps_shutdown();
    void                                imgui_fps_add( f32 dt );
    void                                imgui_fps_draw();

} // namespace hydra