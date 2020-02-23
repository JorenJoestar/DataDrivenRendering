#include "hydra_imgui.h"
#include "hydra_lib.h"

#include "ShaderCodeGenerator.h"
#include "Lexer.h"

// Hydra Graphics Data
static hydra::graphics::TextureHandle g_font_texture;
static hydra::graphics::PipelineHandle g_imgui_pipeline;
static hydra::graphics::BufferHandle g_vb, g_ib;
static hydra::graphics::BufferHandle g_ui_cb;
static hydra::graphics::ResourceListLayoutHandle g_resource_layout;
static size_t g_vb_size = 665536, g_ib_size = 665536;

// Map to retrieve resource lists based on texture handle.
struct TextureToResourceListMap {
    hydra::graphics::ResourceHandle             key;
    hydra::graphics::ResourceHandle             value;
};

TextureToResourceListMap*                       g_texture_to_resource_list = nullptr;


// Functions
bool hydra_Imgui_Init( hydra::graphics::Device& graphics_device ) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Hydra_ImGui";

    using namespace hydra::graphics;

    // Load font texture atlas //////////////////////////////////////////////////
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    TextureCreation texture_creation = { pixels, width, height, 1, 1, 0, TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D };
    g_font_texture = graphics_device.create_texture( texture_creation );

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)&g_font_texture;

    // Compile shader
    hfx::compile_hfx( "..\\data\\source\\ImGui.hfx", "..\\data\\bin\\", "ImGui.bhfx" );

    // Create shader
    hfx::ShaderEffectFile shader_effect_file;
    hfx::init_shader_effect_file( shader_effect_file, "..\\data\\bin\\ImGui.bhfx" );

    hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file.memory, 0 );
    uint32_t shader_count = pass_header->num_shader_chunks;

    PipelineCreation pipeline_creation = {};

    hfx::get_pipeline( pass_header, pipeline_creation);

    uint8_t num_bindings = 0;
    const hydra::graphics::ResourceListLayoutCreation::Binding* bindings = hfx::get_pass_layout_bindings( pass_header, 0, num_bindings );
    ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };
    g_resource_layout = graphics_device.create_resource_list_layout( resource_layout_creation );

    pipeline_creation.resource_list_layout[0] = g_resource_layout;
    pipeline_creation.num_active_layouts = 1;

    g_imgui_pipeline = graphics_device.create_pipeline( pipeline_creation );

    PipelineDescription pipeline_desc;
    graphics_device.query_pipeline( g_imgui_pipeline, pipeline_desc );

    // Create constant buffer
    BufferCreation cb_creation = { BufferType::Constant, ResourceUsageType::Dynamic, 64, nullptr, "CB_ImGui" };
    g_ui_cb = graphics_device.create_buffer( cb_creation );

    // Create resource list
    ResourceListCreation::Resource rl_resources[] = { g_ui_cb.handle, g_font_texture.handle };
    ResourceListCreation rl_creation = { pipeline_creation.resource_list_layout[0], rl_resources, 2 };
    hydra::graphics::ResourceListHandle ui_resource_list = graphics_device.create_resource_list( rl_creation );

    // Add resource list to the map
    hash_map_set_default( g_texture_to_resource_list, k_invalid_handle );
    hash_map_put( g_texture_to_resource_list, g_font_texture.handle, ui_resource_list.handle );

    // Create vertex and index buffers //////////////////////////////////////////
    BufferCreation vb_creation = { BufferType::Vertex, ResourceUsageType::Dynamic, g_vb_size, nullptr, "VB_ImGui" };
    g_vb = graphics_device.create_buffer( vb_creation );

    BufferCreation ib_creation = { BufferType::Index, ResourceUsageType::Dynamic, g_ib_size, nullptr, "IB_ImGui" };
    g_ib = graphics_device.create_buffer( ib_creation );

    hydra::hy_free( shader_effect_file.memory );

    return true;
}

void hydra_Imgui_Shutdown( hydra::graphics::Device& graphics_device ) {

    for ( size_t i = 0; i < hash_map_length( g_texture_to_resource_list); ++i ) {
        hydra::graphics::ResourceHandle handle = g_texture_to_resource_list[i].value;
        graphics_device.destroy_resource_list( { handle } );
    }

    graphics_device.destroy_buffer( g_vb );
    graphics_device.destroy_buffer( g_ib );
    graphics_device.destroy_buffer( g_ui_cb );
    graphics_device.destroy_resource_list_layout( g_resource_layout );

    graphics_device.destroy_pipeline( g_imgui_pipeline );
    graphics_device.destroy_texture( g_font_texture );
}

void hydra_Imgui_NewFrame() {

}

//
// Create draw commands from ImGui draw data.
//
void hydra_imgui_collect_draw_data( ImDrawData* draw_data, hydra::graphics::Device& gfx_device, hydra::graphics::CommandBuffer& commands )
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if ( fb_width <= 0 || fb_height <= 0 )
        return;

    bool clip_origin_lower_left = true;
#if defined(GL_CLIP_ORIGIN) && !defined(__APPLE__)
    GLenum last_clip_origin = 0; glGetIntegerv( GL_CLIP_ORIGIN, (GLint*)&last_clip_origin ); // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
    if ( last_clip_origin == GL_UPPER_LEFT )
        clip_origin_lower_left = false;
#endif
    size_t vertex_size = draw_data->TotalVtxCount * sizeof( ImDrawVert );
    size_t index_size = draw_data->TotalIdxCount * sizeof( ImDrawIdx );

    if ( vertex_size >= g_vb_size || index_size >= g_ib_size ) {
        return;
    }

    if ( vertex_size == 0 && index_size == 0 ) {
        return;
    }

    using namespace hydra::graphics;

    commands.begin_submit( 2 );

    // Upload data
    ImDrawVert* vtx_dst = NULL;
    ImDrawIdx* idx_dst = NULL;

    MapBufferParameters map_parameters_vb = { g_vb, 0, vertex_size };
    vtx_dst = (ImDrawVert*)gfx_device.map_buffer( map_parameters_vb );

    if ( vtx_dst ) {
        for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy( vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
            vtx_dst += cmd_list->VtxBuffer.Size;
        }

        gfx_device.unmap_buffer( map_parameters_vb );
    }

    MapBufferParameters map_parameters_ib = { g_ib, 0, index_size };
    idx_dst = (ImDrawIdx*)gfx_device.map_buffer( map_parameters_ib );

    if ( idx_dst ) {
        for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy( idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        gfx_device.unmap_buffer( map_parameters_ib );
    }

    commands.bind_pipeline( g_imgui_pipeline );
    commands.bind_vertex_buffer( g_vb, 0, 0 );
    commands.bind_index_buffer( g_ib );

    const Viewport viewport = { 0, 0, (float)fb_width, (float)fb_height, 0.0f, 1.0f };
    commands.set_viewport( viewport );

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / (R - L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / (T - B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
    };

    MapBufferParameters cb_map = { g_ui_cb, 0, 0 };
    float* cb_data = (float*)gfx_device.map_buffer( cb_map );
    if ( cb_data ) {
        memcpy( cb_data, &ortho_projection[0][0], 64 );
        gfx_device.unmap_buffer( cb_map );
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    //
    int counts = draw_data->CmdListsCount;

    TextureHandle last_texture = g_font_texture;
    ResourceListHandle last_resource_list = { hash_map_get( g_texture_to_resource_list, last_texture.handle ) };

    commands.bind_resource_list( &last_resource_list, 1, nullptr, 0 );

    size_t vtx_buffer_offset = 0, idx_buffer_offset = 0;
    for ( int n = 0; n < counts; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for ( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if ( pcmd->UserCallback )
            {
                // User callback (registered via ImDrawList::AddCallback)
                pcmd->UserCallback( cmd_list, pcmd );
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if ( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
                {
                    // Apply scissor/clipping rectangle
                    if ( clip_origin_lower_left ) {
                        Rect2D scissor_rect = { (int)clip_rect.x, (int)(fb_height - clip_rect.w), (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y) };
                        commands.set_scissor( scissor_rect );
                    }
                    else {
                        Rect2D scissor_rect = { (int)clip_rect.x, (int)clip_rect.y, (int)clip_rect.z, (int)clip_rect.w }; // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
                        commands.set_scissor( scissor_rect );
                    }

                    // Retrieve 
                    TextureHandle new_texture = *( TextureHandle* )(pcmd->TextureId);
                    if ( new_texture.handle != last_texture.handle ) {
                        last_texture = new_texture;
                        last_resource_list.handle = hash_map_get( g_texture_to_resource_list, last_texture.handle );

                        if ( last_resource_list.handle == k_invalid_handle ) {
                            // Create
                            ResourceListCreation::Resource rl_resources[] = { g_ui_cb.handle, last_texture.handle };
                            ResourceListCreation rl_creation = { g_resource_layout, rl_resources, 2 };
                            last_resource_list = gfx_device.create_resource_list( rl_creation );
                            hash_map_put( g_texture_to_resource_list, new_texture.handle, last_resource_list.handle );
                        }
                        commands.bind_resource_list( &last_resource_list, 1, nullptr, 0 );
                    }

                    const uint32_t start_index_offset = idx_buffer_offset * sizeof( ImDrawIdx );
                    const uint32_t end_index_offset = start_index_offset + pcmd->ElemCount * sizeof( ImDrawIdx );
                    commands.drawIndexed( hydra::graphics::TopologyType::Triangle, pcmd->ElemCount, 1, idx_buffer_offset, vtx_buffer_offset, 0 );
                }
            }
            idx_buffer_offset += pcmd->ElemCount;
        }

        vtx_buffer_offset += cmd_list->VtxBuffer.Size;
    }

    commands.end_submit();
}
