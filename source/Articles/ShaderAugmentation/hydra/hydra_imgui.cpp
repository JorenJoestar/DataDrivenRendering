// Hydra ImGUI - v0.04

#include "hydra_imgui.h"
#include "hydra_lib.h"
#include "hydra_graphics.h"

#include "imgui.h"

namespace hydra {

// Hydra Graphics Data
static hydra::graphics::TextureHandle g_font_texture;
static hydra::graphics::PipelineHandle g_imgui_pipeline;
static hydra::graphics::BufferHandle g_vb, g_ib;
static hydra::graphics::BufferHandle g_ui_cb;
static hydra::graphics::ResourceListLayoutHandle g_resource_layout;

static uint32_t g_vb_size = 665536, g_ib_size = 665536;

// Map to retrieve resource lists based on texture handle.
struct TextureToResourceListMap {
    hydra::graphics::ResourceHandle key;
    hydra::graphics::ResourceHandle value;
};

TextureToResourceListMap*           g_texture_to_resource_list = nullptr;

#if defined (HYDRA_IMGUI_HFX)
static const char*                  s_source_filename = "..\\data\\source\\ImGui.hfx";
static const char*                  s_destination_filename = "..\\data\\bin\\ImGui.bhfx";
static const char*                  s_compiler_filename = "C:\\Coding\\github\\HydraShaderFX\\Bin\\HydraShaderFX_Debug.exe";

#else

static const char*                  g_vertex_shader_code = {
    "#version 450\n"
    "layout( location = 0 ) in vec2 Position;\n"
    "layout( location = 1 ) in vec2 UV;\n"
    "layout( location = 2 ) in vec4 Color;\n"
    "layout( location = 0 ) out vec2 Frag_UV;\n"
    "layout( location = 1 ) out vec4 Frag_Color;\n"
    "layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4( Position.xy,0,1 );\n"
    "}\n"
};

static const char*                  g_fragment_shader_code = {
    "#version 450\n"
    "layout (location = 0) in vec2 Frag_UV;\n"
    "layout (location = 1) in vec4 Frag_Color;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "layout (binding = 1) uniform sampler2D Texture;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n"
};

// SPV compiled with glslangvalidator version // 7.11.3170
static const uint32_t              s_vertex_spv[] = {
    0x07230203,0x00010000,0x00080007,0x0000002b,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x0000000f,
    0x00000011,0x00000018,0x00000022,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00040005,0x00000009,0x67617246,0x0056555f,0x00030005,0x0000000b,
    0x00005655,0x00050005,0x0000000f,0x67617246,0x6c6f435f,0x0000726f,0x00040005,0x00000011,
    0x6f6c6f43,0x00000072,0x00060005,0x00000016,0x505f6c67,0x65567265,0x78657472,0x00000000,
    0x00060006,0x00000016,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x00000016,
    0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x00000016,0x00000002,
    0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00070006,0x00000016,0x00000003,0x435f6c67,
    0x446c6c75,0x61747369,0x0065636e,0x00030005,0x00000018,0x00000000,0x00060005,0x0000001c,
    0x61636f4c,0x6e6f436c,0x6e617473,0x00007374,0x00050006,0x0000001c,0x00000000,0x6a6f7250,
    0x0078744d,0x00030005,0x0000001e,0x00000000,0x00050005,0x00000022,0x69736f50,0x6e6f6974,
    0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000b,0x0000001e,
    0x00000001,0x00040047,0x0000000f,0x0000001e,0x00000001,0x00040047,0x00000011,0x0000001e,
    0x00000002,0x00050048,0x00000016,0x00000000,0x0000000b,0x00000000,0x00050048,0x00000016,
    0x00000001,0x0000000b,0x00000001,0x00050048,0x00000016,0x00000002,0x0000000b,0x00000003,
    0x00050048,0x00000016,0x00000003,0x0000000b,0x00000004,0x00030047,0x00000016,0x00000002,
    0x00040048,0x0000001c,0x00000000,0x00000005,0x00050048,0x0000001c,0x00000000,0x00000023,
    0x00000000,0x00050048,0x0000001c,0x00000000,0x00000007,0x00000010,0x00030047,0x0000001c,
    0x00000002,0x00040047,0x0000001e,0x00000022,0x00000000,0x00040047,0x0000001e,0x00000021,
    0x00000000,0x00040047,0x00000022,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
    0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000002,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,
    0x00000003,0x00040020,0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,
    0x00000001,0x00040017,0x0000000d,0x00000006,0x00000004,0x00040020,0x0000000e,0x00000003,
    0x0000000d,0x0004003b,0x0000000e,0x0000000f,0x00000003,0x00040020,0x00000010,0x00000001,
    0x0000000d,0x0004003b,0x00000010,0x00000011,0x00000001,0x00040015,0x00000013,0x00000020,
    0x00000000,0x0004002b,0x00000013,0x00000014,0x00000001,0x0004001c,0x00000015,0x00000006,
    0x00000014,0x0006001e,0x00000016,0x0000000d,0x00000006,0x00000015,0x00000015,0x00040020,
    0x00000017,0x00000003,0x00000016,0x0004003b,0x00000017,0x00000018,0x00000003,0x00040015,
    0x00000019,0x00000020,0x00000001,0x0004002b,0x00000019,0x0000001a,0x00000000,0x00040018,
    0x0000001b,0x0000000d,0x00000004,0x0003001e,0x0000001c,0x0000001b,0x00040020,0x0000001d,
    0x00000002,0x0000001c,0x0004003b,0x0000001d,0x0000001e,0x00000002,0x00040020,0x0000001f,
    0x00000002,0x0000001b,0x0004003b,0x0000000a,0x00000022,0x00000001,0x0004002b,0x00000006,
    0x00000024,0x00000000,0x0004002b,0x00000006,0x00000025,0x3f800000,0x00050036,0x00000002,
    0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000c,
    0x0000000b,0x0003003e,0x00000009,0x0000000c,0x0004003d,0x0000000d,0x00000012,0x00000011,
    0x0003003e,0x0000000f,0x00000012,0x00050041,0x0000001f,0x00000020,0x0000001e,0x0000001a,
    0x0004003d,0x0000001b,0x00000021,0x00000020,0x0004003d,0x00000007,0x00000023,0x00000022,
    0x00050051,0x00000006,0x00000026,0x00000023,0x00000000,0x00050051,0x00000006,0x00000027,
    0x00000023,0x00000001,0x00070050,0x0000000d,0x00000028,0x00000026,0x00000027,0x00000024,
    0x00000025,0x00050091,0x0000000d,0x00000029,0x00000021,0x00000028,0x00050041,0x0000000e,
    0x0000002a,0x00000018,0x0000001a,0x0003003e,0x0000002a,0x00000029,0x000100fd,0x00010038
};

static const uint32_t               s_fragment_spv[] = {
    0x07230203,0x00010000,0x00080007,0x00000018,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0008000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00000014,
    0x00030010,0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00050005,0x00000009,0x5f74754f,0x6f6c6f43,0x00000072,0x00050005,
    0x0000000b,0x67617246,0x6c6f435f,0x0000726f,0x00040005,0x00000010,0x74786554,0x00657275,
    0x00040005,0x00000014,0x67617246,0x0056555f,0x00040047,0x00000009,0x0000001e,0x00000000,
    0x00040047,0x0000000b,0x0000001e,0x00000001,0x00040047,0x00000010,0x00000022,0x00000000,
    0x00040047,0x00000010,0x00000021,0x00000001,0x00040047,0x00000014,0x0000001e,0x00000000,
    0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
    0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,
    0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,0x0000000a,0x00000001,0x00000007,
    0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00090019,0x0000000d,0x00000006,0x00000001,
    0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000e,0x0000000d,
    0x00040020,0x0000000f,0x00000000,0x0000000e,0x0004003b,0x0000000f,0x00000010,0x00000000,
    0x00040017,0x00000012,0x00000006,0x00000002,0x00040020,0x00000013,0x00000001,0x00000012,
    0x0004003b,0x00000013,0x00000014,0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000c,0x0000000b,0x0004003d,
    0x0000000e,0x00000011,0x00000010,0x0004003d,0x00000012,0x00000015,0x00000014,0x00050057,
    0x00000007,0x00000016,0x00000011,0x00000015,0x00050085,0x00000007,0x00000017,0x0000000c,
    0x00000016,0x0003003e,0x00000009,0x00000017,0x000100fd,0x00010038
};

#endif // HYDRA_IMGUI_HFX

//
//
bool imgui_init( hydra::graphics::Device& graphics_device ) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Hydra_ImGui";

    using namespace hydra::graphics;

    // Load font texture atlas //////////////////////////////////////////////////
    unsigned char* pixels;
    int width, height;
    // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be
    // compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id,
    // consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    TextureCreation texture_creation = { pixels, width, height, 1, 1, 0, TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D };
    g_font_texture = graphics_device.create_texture( texture_creation );

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)&g_font_texture;

#if defined (HYDRA_IMGUI_HFX)
    // Implementation using HFX files.

#if defined(HYDRA_OPENGL)
    uint32_t compiling_flags = hfx::CompileOptions_OpenGL | hfx::CompileOptions_Embedded;
    hfx::hfx_compile( source_filename, destination_filename, compiling_flags );
#elif defined(HYDRA_VULKAN)
    char arguments_buffer[256];
    sprintf( argument_buffer, "HydraShaderFX_Debug.exe %s -V -b -o %s", source_filename, destination_filename );
    //const char* arguments = "HydraShaderFX_Debug.exe ..\\data\\source\\ImGui.hfx -V -b -o ..\\data\\bin\\ImGui.bhfx";
    hydra::execute_process( "..\\bin", compiler_filename, argument_buffer );
#endif // HYDRA_OPENGL

    // Create shader
    hfx::ShaderEffectFile shader_effect_file;
    hfx::shader_effect_init( shader_effect_file, destination_filename );

    hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( shader_effect_file.memory, 0 );
    uint32_t shader_count = pass_header->num_shader_chunks;

    PipelineCreation pipeline_creation = {};

    hfx::shader_effect_pass_get_pipeline( pass_header, pipeline_creation);
    pipeline_creation.render_pass = graphics_device.get_swapchain_pass();

    uint8_t num_bindings = 0;
    const hydra::graphics::ResourceListLayoutCreation::Binding* bindings = hfx::shader_effect_pass_get_layout_bindings( pass_header, 0, num_bindings );
    ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };
    g_resource_layout = graphics_device.create_resource_list_layout( resource_layout_creation );
#else

    // Manual code. Used to remove dependency from that.
    ShaderCreation shader_creation{};
    shader_creation.name = "ImGui";
    shader_creation.stages_count = 2;
    shader_creation.stages[0] = { g_vertex_shader_code, (uint32_t)strlen( g_vertex_shader_code ), ShaderStage::Vertex };
    shader_creation.stages[1] = { g_fragment_shader_code, (uint32_t)strlen( g_fragment_shader_code), ShaderStage::Fragment };

    PipelineCreation pipeline_creation = {};

    pipeline_creation.shaders = shader_creation;

    pipeline_creation.blend_state.active_states = 1;
    pipeline_creation.blend_state.blend_states[0] = {};
    pipeline_creation.blend_state.blend_states[0].color_operation = hydra::graphics::BlendOperation::Add;
    pipeline_creation.blend_state.blend_states[0].source_color = hydra::graphics::Blend::SrcAlpha;
    pipeline_creation.blend_state.blend_states[0].destination_color = hydra::graphics::Blend::InvSrcAlpha;

    VertexAttribute vertex_attributes[] = { { 0, 0, 0, VertexComponentFormat::Float2 },
                                            { 1, 0, 8, VertexComponentFormat::Float2 },
                                            { 2, 0, 16, VertexComponentFormat::UByte4N } };
    pipeline_creation.vertex_input.vertex_attributes = vertex_attributes;
    pipeline_creation.vertex_input.num_vertex_attributes = ArrayLength( vertex_attributes );

    VertexStream vertex_streams[] = { {0, 20, VertexInputRate::PerVertex } };
    pipeline_creation.vertex_input.vertex_streams = vertex_streams;
    pipeline_creation.vertex_input.num_vertex_streams = ArrayLength( vertex_streams );

    pipeline_creation.render_pass = graphics_device.get_swapchain_pass();

    ResourceListLayoutCreation::Binding bindings[2] = { {ResourceType::Constants, 0, 1, "LocalConstants"}, { ResourceType::Texture, 1, 1, "Texture" } };
    ResourceListLayoutCreation resource_layout_creation = { bindings, ArrayLength(bindings) };
    g_resource_layout = graphics_device.create_resource_list_layout( resource_layout_creation );
#endif // HYDRA_IMGUI_HFX

#if defined (HYDRA_VULKAN)
    assert( false && "not implemented!" );
#endif // HYDRA_VULKAN
    
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

    return true;
}

//
//
void imgui_shutdown( hydra::graphics::Device& graphics_device ) {

    for ( size_t i = 0; i < hash_map_size( g_texture_to_resource_list); ++i ) {
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

//
//
void imgui_new_frame() {

}

//
// Create draw commands from ImGui draw data.
//
void imgui_collect_draw_data( ImDrawData* draw_data, hydra::graphics::Device& gfx_device, hydra::graphics::CommandBuffer& commands )
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if ( fb_width <= 0 || fb_height <= 0 )
        return;

    // TODO:
#if defined (HYDRA_VULKAN)
    return;
#endif

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

    // TODO_CB
    //commands.begin_submit( 2 );

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

    // TODO_KS: Add the sorting.
    uint64_t key = 1000;
    commands.bind_pipeline( key++, g_imgui_pipeline );
    commands.bind_vertex_buffer( key++, g_vb, 0, 0 );
    commands.bind_index_buffer( key++, g_ib );

    const Viewport viewport = { 0, 0, fb_width, fb_height, 0.0f, 1.0f };
    commands.set_viewport( key++, viewport );

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

    commands.bind_resource_list( key++, &last_resource_list, 1, nullptr, 0 );

    uint32_t vtx_buffer_offset = 0, index_buffer_offset = 0;
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
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)(fb_height - clip_rect.w), (uint16_t)(clip_rect.z - clip_rect.x), (uint16_t)(clip_rect.w - clip_rect.y) };
                        commands.set_scissor( key++, scissor_rect );
                    }
                    else {
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)clip_rect.y, (uint16_t)clip_rect.z, (uint16_t)clip_rect.w }; // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
                        commands.set_scissor( key++, scissor_rect );
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
                        commands.bind_resource_list( key++, &last_resource_list, 1, nullptr, 0 );
                    }

                    //const uint32_t start_index_offset = index_buffer_offset;// *sizeof( ImDrawIdx );
                    //const uint32_t end_index_offset = start_index_offset + pcmd->ElemCount * sizeof( ImDrawIdx );
                    commands.drawIndexed( key++, hydra::graphics::TopologyType::Triangle, pcmd->ElemCount, 1, index_buffer_offset, vtx_buffer_offset, 0 );
                }
            }
            index_buffer_offset += pcmd->ElemCount;
        }

        vtx_buffer_offset += cmd_list->VtxBuffer.Size;
    }

   // commands.end_submit();
}

// File Dialog //////////////////////////////////////////////////////////////////

//
//
struct FileDialogOpenMap {

    char*                           key;
    bool                            value;

}; // struct FileDialogOpenMap

static string_hash( FileDialogOpenMap ) file_dialog_open_map = nullptr;

static hydra::Directory             directory;
static char                         filename[MAX_PATH];
static char                         last_path[MAX_PATH];
static char                         last_extension[16];
static bool                         scan_folder             = true;
static bool                         init                    = false;

static hydra::StringArray           files;
static hydra::StringArray           directories;

bool imgui_file_dialog_open( const char* button_name, const char* path, const char* extension ) {

    bool opened = string_hash_get( file_dialog_open_map, button_name );
    if ( ImGui::Button( button_name ) ) {
        opened = true;
    }

    bool selected = false;

    if ( opened && ImGui::Begin( "hydra_imgui_file_dialog", &opened, ImGuiWindowFlags_AlwaysAutoResize ) ) {

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 20 ) );
        ImGui::Text( directory.path );
        ImGui::PopStyleVar();

        ImGui::Separator();

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 4 ) );

        hydra::MemoryAllocator* allocator = hydra::memory_get_system_allocator();

        if ( !init ) {
            init = true;

            files.init( 10000, allocator );
            directories.init( 10000, allocator );

            string_hash_init_dynamic( file_dialog_open_map );

            filename[0] = 0;
            last_path[0] = 0;
            last_extension[0] = 0;
        }

        if ( strcmp( path, last_path ) != 0 ) {
            strcpy( last_path, path );

            hydra::file_open_directory( path, &directory );

            scan_folder = true;
        }

        if ( strcmp( extension, last_extension ) != 0 ) {
            strcpy( last_extension, extension );

            scan_folder = true;
        }

        // Search files
        if ( scan_folder ) {
            scan_folder = false;

            hydra::file_find_files_in_path( extension, directory.path, files, directories );
        }

        for ( size_t d = 0; d < directories.get_string_count(); ++d ) {

            const char* directory_name = directories.get_string( d );
            if ( ImGui::Selectable( directory_name, selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

                if ( strcmp( directory_name, ".." ) == 0 ) {
                    hydra::file_parent_directory( &directory );
                } else {
                    hydra::file_sub_directory( &directory, directory_name );
                }

                scan_folder = true;
            }
        }

        for ( size_t f = 0; f < files.get_string_count(); ++f ) {
            const char* file_name = files.get_string( f );
            if ( ImGui::Selectable( file_name, selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

                strcpy( filename, directory.path );
                filename[strlen( filename ) - 1] = 0;
                strcat( filename, file_name );

                selected = true;
                opened = false;
            }
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    // Update opened map
    string_hash_put( file_dialog_open_map, button_name, opened );

    return selected;
}

const char * imgui_file_dialog_get_filename() {
    return filename;
}

} // namespace hydra