#pragma once

#include "graphics/renderer.hpp"

namespace hydra {
namespace gfx {

struct Camera;

//
//
struct LineVertex {
    vec3s                           position;
    hydra::gfx::ColorUint           color;

    void                            set( vec3s position_, hydra::gfx::ColorUint color_ ) { position = position_; color = color_; }
    void                            set( vec2s position_, hydra::gfx::ColorUint color_ ) { position = { position_.x, position_.y, 0 }; color = color_; }
};


struct DebugRenderer {

    void                            init( hydra::gfx::Renderer& renderer, hydra::gfx::Shader* shader );
    void                            shutdown( hydra::gfx::Renderer& renderer );

    void                            render( hydra::gfx::Renderer& renderer, hydra::gfx::CommandBuffer* commands, hydra::gfx::Camera& camera );

    void                            line( const vec3s& from, const vec3s& to, hydra::gfx::ColorUint color0, hydra::gfx::ColorUint color1 );
    void                            box( const vec3s& min, const vec3s max, hydra::gfx::ColorUint color );

    hydra::gfx::Shader*             shader_effect;
    hydra::gfx::Material*           material;

    hydra::gfx::Buffer*             lines_cb;
    hydra::gfx::Buffer*             lines_vb;

    u32                             current_line;

}; // struct DebugRenderer

} // namespace gfx
} // namespace hydra