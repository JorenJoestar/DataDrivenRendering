#pragma once

#include "application/application.hpp"

#include "graphics/hydra_shaderfx.h"
#include "graphics/debug_renderer.hpp"
#include "graphics/camera.hpp"

#include "kernel/array.hpp"

// Forward declarations
namespace hydra {
    struct ApplicationConfiguration;
    struct Window;
    struct InputService;
    struct MemoryService;
    struct LogService;
    struct ImGuiService;

namespace gfx {
    struct Device;
    struct Renderer;
    struct Texture;
} // namespace gfx
} // namespace hydra

// GameApplication ///////////////////////////////////////////////////////

namespace hydra {

    struct GameApplication : public Application {

        void                        create( const hydra::ApplicationConfiguration& configuration ) override;
        void                        destroy() override;
        bool                        main_loop() override;

        void                        fixed_update( f32 delta ) override;
        void                        variable_update( f32 delta ) override;

        void                        frame_begin() override;
        void                        frame_end() override;

        void                        render( f32 interpolation ) override;

        // Game application only methods
        void                        handle_begin_frame();

        void                        on_resize( u32 new_width, u32 new_height );

        f64                         accumulator     = 0.0;
        f64                         current_time    = 0.0;
        f32                         step            = 1.0f / 60.0f;
        f32                         delta_time      = 0.0f;
        i64                         begin_frame_tick = 0;

        hydra::Window*              window          = nullptr;

        hydra::InputService*        input           = nullptr;
        hydra::gfx::Renderer*       renderer        = nullptr;
        hydra::ImGuiService*        imgui           = nullptr;

    }; // struct GameApp

} // namespace hydra