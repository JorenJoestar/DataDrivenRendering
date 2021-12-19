#pragma once

#include "graphics/renderer.hpp"

namespace hydra {
namespace gfx {

struct Camera;

//
//
struct DebugRenderer {

    void                            init( hydra::gfx::Renderer* renderer );
    void                            shutdown( hydra::gfx::Renderer* renderer );

    void                            reload( hydra::gfx::Renderer* renderer, hydra::ResourceManager* resource_manager );

    void                            render( hydra::gfx::Renderer& renderer, hydra::gfx::CommandBuffer* commands, hydra::gfx::Camera& camera );

    void                            line( const vec3s& from, const vec3s& to, hydra::Color color );
    void                            line( const vec3s& from, const vec3s& to, hydra::Color color0, hydra::Color color1 );
    
    void                            box( const vec3s& min, const vec3s max, hydra::Color color );

    hydra::gfx::Material*           material;

    hydra::gfx::Buffer*             lines_cb;
    hydra::gfx::Buffer*             lines_vb;
    hydra::gfx::Buffer*             lines_vb_2d;

    u32                             current_line;
    u32                             current_line_2d;

}; // struct DebugRenderer

} // namespace gfx
} // namespace hydra