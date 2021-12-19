#include "graphics/debug_renderer.hpp"
#include "graphics/camera.hpp"
#include "graphics/command_buffer.hpp"

namespace hydra {
namespace gfx {

//
//
struct LineVertex {
    vec3s                           position;
    hydra::Color                    color;

    void                            set( vec3s position_, hydra::Color color_ ) { position = position_; color = color_; }
    void                            set( vec2s position_, hydra::Color color_ ) { position = { position_.x, position_.y, 0 }; color = color_; }
}; // struct LineVertex

//
//
struct LineVertex2D {
    vec2s                           position;
    u32                             color;
}; // struct LineVertex2D

static const u32            k_max_lines = 100000;

static LineVertex           s_line_buffer[ k_max_lines ];
static LineVertex2D         s_line_buffer_2d[ k_max_lines ];

struct LinesGPULocalConstants {
    mat4s                   view_projection;
    mat4s                   projection;
    vec4s                   resolution;

    f32                     line_width;
    f32                     pad[ 3 ];
};


// DebugRenderer ////////////////////////////////////////////////////////////////
void DebugRenderer::init( hydra::gfx::Renderer* renderer ) {
    using namespace hydra::gfx;

    // Constants
    lines_cb = renderer->create_buffer( BufferType::Constant_mask, ResourceUsageType::Dynamic, sizeof(LinesGPULocalConstants), nullptr, "line_renderer_cb" );

    // Line segments buffers
    lines_vb = renderer->create_buffer( BufferType::Vertex_mask, ResourceUsageType::Dynamic, sizeof( LineVertex ) * k_max_lines, nullptr, "lines_vb" );
    lines_vb_2d = renderer->create_buffer( BufferType::Vertex_mask, ResourceUsageType::Dynamic, sizeof( LineVertex2D ) * k_max_lines, nullptr, "lines_vb_2d" );

    current_line = current_line_2d = 0;

    this->material = nullptr;
}

void DebugRenderer::shutdown( hydra::gfx::Renderer* renderer ) {

    //renderer.destroy_material( material );
    renderer->destroy_buffer( lines_vb );
    renderer->destroy_buffer( lines_vb_2d );
    renderer->destroy_buffer( lines_cb );
}

void DebugRenderer::reload( hydra::gfx::Renderer* renderer, hydra::ResourceManager* resource_manager ) {

    renderer->destroy_material( material );

    material = resource_manager->load<hydra::gfx::Material>( "line_material" );
}


void DebugRenderer::render( hydra::gfx::Renderer& renderer, hydra::gfx::CommandBuffer* gpu_commands, hydra::gfx::Camera& camera ) {
    using namespace hydra::gfx;

    if ( current_line || current_line_2d ) {

        LinesGPULocalConstants* cb_data = ( LinesGPULocalConstants* )renderer.dynamic_allocate( lines_cb );
        if ( cb_data ) {
            cb_data->view_projection = camera.view_projection;
            camera.get_projection_ortho_2d( cb_data->projection.raw );
            cb_data->resolution = { renderer.width * 1.0f, renderer.height * 1.0f, 1.0f / renderer.width, 1.0f / renderer.height };
            cb_data->line_width = 1.f;
        }

        if ( this->material == nullptr ) {
            hprint( "DebugRenderer does not have assigned a material. Skipping rendering.\n" );
            return;
        }
    }

    u64 sort_key = 0;

    if ( current_line ) {
        const u32 mapping_size = sizeof( LineVertex ) * current_line;
        LineVertex* vtx_dst = ( LineVertex* )renderer.map_buffer( lines_vb, 0, mapping_size );

        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer[ 0 ], mapping_size );

            renderer.unmap_buffer( lines_vb );
        }
        
        MaterialPass& pass = material->passes[ 0 ];
        gpu_commands->bind_pipeline( sort_key++, pass.pipeline );
        gpu_commands->bind_vertex_buffer( sort_key++, lines_vb->handle, 0, 0 );
        gpu_commands->bind_resource_list( sort_key++, &pass.resource_list, 1, nullptr, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, num_vertices, 0, current_line / 2 );
        current_line = 0;
    }

    if ( current_line_2d ) {
        const u32 mapping_size = sizeof( LineVertex2D ) * current_line;
        LineVertex2D* vtx_dst = ( LineVertex2D* )renderer.map_buffer( lines_vb, 0, mapping_size );

        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer_2d[ 0 ], mapping_size );
        }

        MaterialPass& pass = material->passes[ 1 ];
        gpu_commands->bind_pipeline( sort_key++, pass.pipeline );
        gpu_commands->bind_vertex_buffer( sort_key++, lines_vb->handle, 0, 0 );
        gpu_commands->bind_resource_list( sort_key++, &pass.resource_list, 1, nullptr, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, num_vertices, 0, current_line_2d / 2 );
        current_line_2d = 0;
    }
}

void DebugRenderer::line( const vec3s& from, const vec3s& to, hydra::Color color ) {
    line( from, to, color, color );
}

void DebugRenderer::line( const vec3s& from, const vec3s& to, hydra::Color color0, hydra::Color color1 ) {

    if ( current_line >= k_max_lines )
        return;

    s_line_buffer[ current_line++ ].set( from, color0 );
    s_line_buffer[ current_line++ ].set( to, color1 );
}
void DebugRenderer::box( const vec3s& min, const vec3s max, hydra::Color color ) {

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

