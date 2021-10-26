#include "graphics/debug_renderer.hpp"
#include "graphics/camera.hpp"
#include "graphics/command_buffer.hpp"

namespace hydra {
namespace gfx {


static const u32            k_max_lines = 100000;

static LineVertex           s_line_buffer[ k_max_lines ];
static LineVertex           s_line_buffer_2d[ k_max_lines ];


struct LinesGPULocalConstants {
    mat4s                   view_projection;
    mat4s                   projection;
    vec4s                   resolution;

    f32                     line_width;
    f32                     pad[ 3 ];
};


// DebugRenderer ////////////////////////////////////////////////////////////////
void DebugRenderer::init( hydra::gfx::Renderer& renderer, hydra::gfx::Shader* shader ) {
    using namespace hydra::gfx;

    // Constants
    lines_cb = renderer.create_buffer( BufferType::Constant_mask, ResourceUsageType::Dynamic, sizeof(LinesGPULocalConstants), nullptr, "lines_cb" );

    // Instance buffers
    lines_vb = renderer.create_buffer( BufferType::Vertex_mask, ResourceUsageType::Dynamic, sizeof( LineVertex ) * k_max_lines, nullptr, "lines_vb" );

    // Shader and material
    shader_effect = shader;

    ResourceListCreation rlc[ 1 ];
    rlc[ 0 ].reset().buffer( lines_cb->handle, 0 );
    material = renderer.create_material( shader_effect, rlc, ArrayLength(rlc) );

    current_line = 0;
}

void DebugRenderer::shutdown( hydra::gfx::Renderer& renderer ) {

    renderer.destroy_material( material );
    renderer.destroy_buffer( lines_vb );
    renderer.destroy_buffer( lines_cb );
}


void DebugRenderer::render( hydra::gfx::Renderer& renderer, hydra::gfx::CommandBuffer* gpu_commands, hydra::gfx::Camera& camera ) {
    using namespace hydra::gfx;

    if ( current_line ) {

        LinesGPULocalConstants* cb_data = ( LinesGPULocalConstants* )renderer.map_buffer( lines_cb );
        if ( cb_data ) {
            cb_data->view_projection = camera.view_projection;
            cb_data->projection = camera.projection;
            cb_data->resolution = { renderer.width * 1.0f, renderer.height * 1.0f, 1.0f / renderer.width, 1.0f / renderer.height };
            cb_data->line_width = 1.f;

            renderer.unmap_buffer( lines_cb );
        }

        const uint32_t mapping_size = sizeof( LineVertex ) * current_line;
        LineVertex* vtx_dst = ( LineVertex* )renderer.map_buffer( lines_vb, 0, mapping_size );

        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer[ 0 ], mapping_size );

            renderer.unmap_buffer( lines_vb );
        }
        u64 sort_key = 0;
        gpu_commands->bind_pipeline( sort_key++, material->pipelines[0] );
        gpu_commands->bind_vertex_buffer( sort_key++, lines_vb->handle, 0, 0 );
        gpu_commands->bind_resource_list( sort_key++, &material->resource_lists[0], 1, nullptr, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, num_vertices, 0, current_line / 2 );
        current_line = 0;
    }
}

void DebugRenderer::line( const vec3s& from, const vec3s& to, hydra::gfx::ColorUint color0, hydra::gfx::ColorUint color1 ) {

    if ( current_line >= k_max_lines )
        return;

    s_line_buffer[ current_line++ ].set( from, color0 );
    s_line_buffer[ current_line++ ].set( to, color1 );
}
void DebugRenderer::box( const vec3s& min, const vec3s max, hydra::gfx::ColorUint color ) {

    const float x0 = min.x;
    const float y0 = min.y;
    const float z0 = min.z;
    const float x1 = max.x;
    const float y1 = max.y;
    const float z1 = max.z;

    line( { x0, y0, z0 }, { x0, y1, z0 }, color, color );
    line( { x0, y1, z0 }, { x1, y1, z0 }, color, color );
    line( { x1, y1, z0 }, { x1, y0, z0 }, color, color );
    line( { x1, y0, z0 }, { x0, y0, z0 }, color, color );
    line( { x0, y0, z0 }, { x0, y0, z1 }, color, color );
    line( { x0, y1, z0 }, { x0, y1, z1 }, color, color );
    line( { x1, y1, z0 }, { x1, y1, z1 }, color, color );
    line( { x1, y0, z0 }, { x1, y0, z1 }, color, color );
    line( { x0, y0, z1 }, { x0, y1, z1 }, color, color );
    line( { x0, y1, z1 }, { x1, y1, z1 }, color, color );
    line( { x1, y1, z1 }, { x1, y0, z1 }, color, color );
    line( { x1, y0, z1 }, { x0, y0, z1 }, color, color );
}

} // namespace gfx
} // namespace hydra

