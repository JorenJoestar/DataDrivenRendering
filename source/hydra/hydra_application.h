#pragma once

//
//  Hydra Application v0.01 /////////////////////////////////////////////////////
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/09/24, 21.50
//
//
// Revision history //////////////////////
//
//      0.01 (2019/09/24) initial implementation.
//
// Documentation /////////////////////////
//
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "imgui.h"    
#include "imgui_impl_sdl.h"

#include "hydra_graphics.h"
#include "hydra_imgui.h"

namespace hydra {

    struct Application {

        void                            init();
        void                            terminate();

        void                            main_loop();

        virtual void                    app_init() = 0;
        virtual void                    app_terminate() = 0;
        virtual void                    app_render( hydra::graphics::CommandBuffer* commands ) = 0;
        virtual void                    app_resize( uint16_t width, uint16_t height ) = 0;

        struct SDL_Window*              window = nullptr;
        void*                           gl_context;

        hydra::graphics::Device         gfx_device;

    }; // struct Application

} // namespace hydra
