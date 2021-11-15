#include "sprite_batch.hpp"

#include "cglm/struct/mat4.h"

#include "graphics/renderer.hpp"
#include "graphics/command_buffer.hpp"
#include "graphics/camera.hpp"

namespace hydra {
namespace gfx {



struct SpriteConstants {
    mat4s                   view_projection_matrix;
    mat4s                   projection_matrix_2d;
}; // struct SpriteConstants


// SpriteBatch ////////////////////////////////////////////////////////////
static const u32 k_max_sprites = 3000;

void SpriteBatch::init( hydra::gfx::Renderer& renderer, Allocator* allocator ) {

    draw_batches.init( allocator, 8 );

    using namespace hydra::gfx;

    BufferCreation vbc = { BufferType::Vertex_mask, ResourceUsageType::Dynamic, sizeof( SpriteGPUData ) * k_max_sprites, nullptr, "sprites_batch_vb" };
    sprite_instance_vb = renderer.create_buffer( vbc );

    vbc.set_name( "sprite_batch_cb" ).set( BufferType::Constant_mask, ResourceUsageType::Dynamic, sizeof( SpriteConstants ) );
    sprite_cb = renderer.create_buffer( vbc );

    current_pipeline.index = k_invalid_pipeline.index;
    current_resource_list.index = k_invalid_list.index;
}

void SpriteBatch::shutdown( hydra::gfx::Renderer& renderer ) {
    renderer.destroy_buffer( sprite_cb );
    renderer.destroy_buffer( sprite_instance_vb );

    draw_batches.shutdown();
}

void SpriteBatch::begin( hydra::gfx::Renderer& renderer, hydra::gfx::Camera& camera ) {

    using namespace hydra::gfx;

    SpriteConstants* cb_data = ( SpriteConstants* )renderer.map_buffer( sprite_cb );
    // Copy constants
    if ( cb_data ) {
        // Calculate view projection matrix
        memcpy( cb_data->view_projection_matrix.raw, &camera.view_projection.m00, 64 );
        // Calculate 2D projection matrix
        f32 L = 0;
        f32 R = camera.viewport_width * camera.zoom;
        f32 T = 0;
        f32 B = camera.viewport_height * camera.zoom;
        const f32 ortho_projection[ 4 ][ 4 ] =
        {
            { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
        };
        memcpy( cb_data->projection_matrix_2d.raw, &ortho_projection[ 0 ][ 0 ], 64 );

        renderer.unmap_buffer( sprite_cb );
    }

    // Map sprite instance data
    num_sprites = 0;
    gpu_data = ( SpriteGPUData* )renderer.map_buffer( sprite_instance_vb );
}

void SpriteBatch::end( hydra::gfx::Renderer& renderer ) {
    using namespace hydra::gfx;
    /*if ( num_sprites ) {
        draw();
    }*/

    set( k_invalid_pipeline, k_invalid_list );

    renderer.unmap_buffer( sprite_instance_vb );
    gpu_data = nullptr;
}

void SpriteBatch::add( SpriteGPUData& data ) {

    if ( num_sprites == k_max_sprites ) {
        hprint( "WARNING: sprite batch capacity finished. Increase it! Max sprites %u\n", k_max_sprites );
        return;
    }
    gpu_data[ num_sprites++ ] = data;
}

void SpriteBatch::set( hydra::gfx::PipelineHandle pipeline, hydra::gfx::ResourceListHandle resource_list ) {
    using namespace hydra::gfx;

    if ( current_pipeline.index != k_invalid_pipeline.index && current_resource_list.index != k_invalid_list.index ) {
        // Add a batch
        DrawBatch batch { current_pipeline, current_resource_list, previous_offset, num_sprites - previous_offset };
        draw_batches.push( batch );
    }

    previous_offset = num_sprites;

    current_pipeline = pipeline;
    current_resource_list = resource_list;
}

void SpriteBatch::set( hydra::gfx::MaterialPass& pass ) {
    set( pass.pipeline, pass.resource_list );
}

void SpriteBatch::draw( hydra::gfx::CommandBuffer* commands, u64& sort_key ) {
    using namespace hydra::gfx;

    const u32 batches_count = draw_batches.size;
    for ( u32 i = 0; i < batches_count; ++i ) {

        DrawBatch& batch = draw_batches[ i ];
        if ( batch.count ) {
            commands->bind_vertex_buffer( sort_key++, sprite_instance_vb->handle, 0, 0 );
            commands->bind_pipeline( sort_key++, batch.pipeline );
            commands->bind_resource_list( sort_key++, &batch.resource_list, 1, 0, 0 );
            commands->draw( sort_key++, TopologyType::Triangle, 0, 6, batch.offset, batch.count);
        }
    }

    draw_batches.set_size( 0 );
}

} // namespace gfx
} // namespace hydra
