#pragma once

//
// Hydra ImGUI - v0.10
//
// ImGUI wrapper using Hydra Graphics.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/16, 19.23
//
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

struct ImDrawData;

namespace hydra {

namespace graphics {
    struct Device;
    struct CommandBuffer;
} // namespace graphics


bool                                imgui_init( hydra::graphics::Device& gpu );
void                                imgui_shutdown( hydra::graphics::Device& gpu );
void                                imgui_new_frame();
void                                imgui_on_resize( hydra::graphics::Device& gpu, uint32_t width, uint32_t height );

void                                imgui_collect_draw_data( ImDrawData* draw_data, hydra::graphics::Device& gpu_device, hydra::graphics::CommandBuffer& commands );


// File Dialog /////////////////////////////////////////////////////////

bool                                imgui_file_dialog_open(const char* button_name, const char* path, const char* extension);
const char*                         imgui_file_dialog_get_filename();

bool                                imgui_path_dialog_open( const char* button_name, const char* path );
const char*                         imgui_path_dialog_get_path();

// Application Log /////////////////////////////////////////////////////

void                                imgui_log_init();
void                                imgui_log_shutdown();

void                                imgui_log_draw();

} // namespace hydra