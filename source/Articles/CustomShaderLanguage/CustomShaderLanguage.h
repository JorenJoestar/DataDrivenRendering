#pragma once

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "hydra/hydra_imgui.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "hydra/hydra_lib.h"
#include "hydra/hydra_graphics.h"


#define HFX_PARSING
#include "ShaderCodeGenerator.h"

#include "CodeGenerator.h"

// HDF generated classes
#include "SimpleData.h"

// HFX generated classes
#include "SimpleFullscreen.h"


struct CustomShaderLanguageApplication {

    void                            init();
    void                            terminate();

    void                            main_loop();

    void                            manual_init_graphics();
    void                            load_shader_effect( const char* filename );

    struct SDL_Window*              window = nullptr;

#if defined HYDRA_OPENGL
    void*                           gl_context;
#endif // HYDRA_OPENGL

    hydra::graphics::Device         gfx_device;

    // hydra::gfx_device::ShaderEffect    shader_effect;

    hydra::graphics::TextureHandle  render_target;
    hydra::graphics::BufferHandle   checker_constants;

    SimpleFullscreen::LocalConstantsBuffer local_constant_buffer;

    hydra::graphics::CommandBuffer* commands;

    // Language specific fields
    Lexer                           lexer;

    hdf::Parser                     parser;
    hdf::CodeGenerator              code_generator;

    hfx::Parser                     effect_parser;
    hfx::CodeGenerator              hfx_code_generator;


}; // struct WritingLanguageApplication
