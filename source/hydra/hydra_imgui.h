#pragma once

//
// Hydra ImGUI - v0.01
//
// ImGUI wrapper using Hydra Graphics.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/16, 19.23
//

#include "imgui.h"
#include "hydra_graphics.h"

bool                                hydra_Imgui_Init( hydra::graphics::Device& graphics_device );
void                                hydra_Imgui_Shutdown( hydra::graphics::Device& graphics_device );
void                                hydra_Imgui_NewFrame();

void                                hydra_imgui_collect_draw_data( ImDrawData* draw_data, hydra::graphics::Device& gfx_device, hydra::graphics::CommandBuffer& commands );