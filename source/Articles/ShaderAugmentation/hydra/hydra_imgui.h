#pragma once

//
// Hydra ImGUI - v0.04
//
// ImGUI wrapper using Hydra Graphics.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/16, 19.23
//
//      0.04 (2020/03/20): + Added File Dialog. + Moved everything to be under hydra namespace.
//      0.03 (2020/03/12): + Embedded SpirV shaders.
//      0.02 (2020/03/10): + Embedded GLSL shaders and handwritten code to remove dependency on compiler. Code cleanup.
//      0.01 (2019/09/16): + Initial implementation.

struct ImDrawData;

namespace hydra {

namespace graphics {
    struct Device;
    struct CommandBuffer;
} // namespace graphics


bool                                imgui_init( hydra::graphics::Device& graphics_device );
void                                imgui_shutdown( hydra::graphics::Device& graphics_device );
void                                imgui_new_frame();

void                                imgui_collect_draw_data( ImDrawData* draw_data, hydra::graphics::Device& gfx_device, hydra::graphics::CommandBuffer& commands );

//
// File Dialog
//

bool                                imgui_file_dialog_open(const char* button_name, const char* path, const char* extension);
const char*                         imgui_file_dialog_get_filename();

} // namespace hydra