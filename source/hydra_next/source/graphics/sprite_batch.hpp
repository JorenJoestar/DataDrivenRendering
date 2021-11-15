#pragma once

#include "kernel/array.hpp"

#include "graphics/gpu_resources.hpp"
#include "cglm/struct/vec4.h"

namespace hydra {
namespace gfx {

struct Buffer;
struct Camera;
struct CommandBuffer;
struct Renderer;
struct SpriteGPUData;
struct MaterialPass;


//
//
struct SpriteGPUData {

    vec4s                           position;

    vec2s                           uv_size;
    vec2s                           uv_offset;

    vec2s                           size;
    f32                             screen_space_flag;      // True if position is screen space.
    f32                             lighting_flag;          // True if lighting is on.

    u32                             albedo_id;              // Global albedo id

}; // struct SpriteGPUData

//
//
struct DrawBatch {

    hydra::gfx::PipelineHandle          pipeline;
    hydra::gfx::ResourceListHandle      resource_list;
    u32                                 offset;
    u32                                 count;
}; // struct DrawBatch

//
//
struct SpriteBatch {

    void                            init( hydra::gfx::Renderer& renderer, hydra::Allocator* allocator );
    void                            shutdown( hydra::gfx::Renderer& renderer );

    void                            begin( hydra::gfx::Renderer& renderer, hydra::gfx::Camera& camera );
    void                            end( hydra::gfx::Renderer& renderer );

    void                            add( SpriteGPUData& data );
    void                            set( hydra::gfx::PipelineHandle pipeline, hydra::gfx::ResourceListHandle resource_list );
    void                            set( hydra::gfx::MaterialPass& pass );

    void                            draw( hydra::gfx::CommandBuffer* commands, u64& sort_key );

    Array<DrawBatch>                draw_batches;

    hydra::gfx::Buffer*             sprite_cb;
    hydra::gfx::Buffer*             sprite_instance_vb;

    SpriteGPUData*                  gpu_data        = nullptr;
    u32                             num_sprites     = 0;
    u32                             previous_offset = 0;

    hydra::gfx::PipelineHandle      current_pipeline;
    hydra::gfx::ResourceListHandle  current_resource_list;

    //RenderingService*               rendering_service;

}; // struct SpriteBatch

} // namespace gfx
} // namespace hydra
