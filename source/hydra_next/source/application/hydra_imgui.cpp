// Hydra ImGUI - v0.13

#include "hydra_imgui.hpp"

#include "kernel/hash_map.hpp"
#include "kernel/memory.hpp"

#include "graphics/gpu_device.hpp"
#include "graphics/command_buffer.hpp"
#include "graphics/renderer.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

#include <stdio.h>

namespace hydra {

// Hydra Graphics Data
static hydra::gfx::TextureHandle g_font_texture;
static hydra::gfx::PipelineHandle g_imgui_pipeline;
static hydra::gfx::BufferHandle g_vb, g_ib;
static hydra::gfx::BufferHandle g_ui_cb;
static hydra::gfx::ResourceLayoutHandle g_resource_layout;
static hydra::gfx::ResourceListHandle g_ui_resource_list;  // Font resource list

static uint32_t g_vb_size = 665536, g_ib_size = 665536;

// Map to retrieve resource lists based on texture handle.
struct TextureToResourceListMap {
    hydra::gfx::ResourceHandle key;
    hydra::gfx::ResourceHandle value;
};

FlatHashMap<hydra::gfx::ResourceHandle, hydra::gfx::ResourceHandle> g_texture_to_resource_list;

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


static const char* g_vertex_shader_code_vulkan = {
    "#version 450\n"
    "layout( location = 0 ) in vec2 Position;\n"
    "layout( location = 1 ) in vec2 UV;\n"
    "layout( location = 2 ) in uvec4 Color;\n"
    "layout( location = 0 ) out vec2 Frag_UV;\n"
    "layout( location = 1 ) out vec4 Frag_Color;\n"
    "layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color / 255.0f;\n"
    "    gl_Position = ProjMtx * vec4( Position.xy,0,1 );\n"
    "}\n"
};

static const char*                  g_fragment_shader_code = {
    "#version 450\n"
    "#extension GL_EXT_nonuniform_qualifier : enable\n"
    "layout (location = 0) in vec2 Frag_UV;\n"
    "layout (location = 1) in vec4 Frag_Color;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "layout (binding = 1) uniform sampler2D Texture;\n"
    //"#extension GL_EXT_nonuniform_qualifier : enable\n"
    //"layout (binding = 10) uniform sampler2D textures[];\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n"
};

static const char*                  g_fragment_shader_code_bindless = {
    "#version 450\n"
    "#extension GL_EXT_nonuniform_qualifier : enable\n"
    "layout (location = 0) in vec2 Frag_UV;\n"
    "layout (location = 1) in vec4 Frag_Color;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "#extension GL_EXT_nonuniform_qualifier : enable\n"
    "layout (binding = 10) uniform sampler2D textures[];\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(textures[2], Frag_UV.st);\n"
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

static ImGuiService s_imgui_service;

ImGuiService* ImGuiService::instance() {
    return &s_imgui_service;
}

//
//
void ImGuiService::init( void* configuration ) {

    gfx = (hydra::gfx::Renderer*)configuration;
    hydra::gfx::Device* gpu = gfx->gpu;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Hydra_ImGui";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    using namespace hydra::gfx;

    // Load font texture atlas //////////////////////////////////////////////////
    unsigned char* pixels;
    int width, height;
    // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be
    // compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id,
    // consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    TextureCreation texture_creation = { pixels, (u16)width, (u16)height, 1, 1, 0, TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D };
    g_font_texture = gpu->create_texture( texture_creation );

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

    hfx::shader_effect_pass_get_pipeline( pass_header, pipeline_creation );
    pipeline_creation.render_pass = gpu->get_swapchain_pass();

    uint8_t num_bindings = 0;
    const hydra::gfx::ResourceListLayoutCreation::Binding* bindings = hfx::shader_effect_pass_get_layout_bindings( pass_header, 0, num_bindings );
    ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };
    g_resource_layout = gpu->create_resource_list_layout( resource_layout_creation );
#else

    // Manual code. Used to remove dependency from that.
    ShaderStateCreation shader_creation{};

#if defined(HYDRA_OPENGL)

    shader_creation.set_name( "ImGui" ).add_stage( g_vertex_shader_code, (uint32_t)strlen( g_vertex_shader_code ), ShaderStage::Vertex )
        .add_stage( g_fragment_shader_code, (uint32_t)strlen( g_fragment_shader_code ), ShaderStage::Fragment );

#elif defined(HYDRA_VULKAN)

    if ( gpu->bindless_supported ) {
        shader_creation.set_name( "ImGui" ).add_stage( g_vertex_shader_code_vulkan, ( uint32_t )strlen( g_vertex_shader_code_vulkan ), ShaderStage::Vertex )
            .add_stage( g_fragment_shader_code_bindless, ( uint32_t )strlen( g_fragment_shader_code_bindless ), ShaderStage::Fragment );
    }
    else {
        shader_creation.set_name( "ImGui" ).add_stage( g_vertex_shader_code_vulkan, ( uint32_t )strlen( g_vertex_shader_code_vulkan ), ShaderStage::Vertex )
            .add_stage( g_fragment_shader_code, ( uint32_t )strlen( g_fragment_shader_code ), ShaderStage::Fragment );
    }
    
#endif // HYDRA_OPENGL


    PipelineCreation pipeline_creation = {};
    pipeline_creation.name = "Pipeline_ImGui";
    pipeline_creation.shaders = shader_creation;

    pipeline_creation.blend_state.add_blend_state().set_color( hydra::gfx::Blend::SrcAlpha, hydra::gfx::Blend::InvSrcAlpha, hydra::gfx::BlendOperation::Add );

    pipeline_creation.vertex_input.add_vertex_attribute( { 0, 0, 0, VertexComponentFormat::Float2 } )
        .add_vertex_attribute( { 1, 0, 8, VertexComponentFormat::Float2 } )
        .add_vertex_attribute( { 2, 0, 16, VertexComponentFormat::UByte4N } );

    pipeline_creation.vertex_input.add_vertex_stream( { 0, 20, VertexInputRate::PerVertex } );
    pipeline_creation.render_pass = gpu->get_swapchain_output();

    ResourceLayoutCreation resource_layout_creation{};
    // bindless
    if ( gpu->bindless_supported ) {
        resource_layout_creation.add_binding( { ResourceType::Constants, 0, 1, "LocalConstants" } ).add_binding( { ResourceType::Texture, 10, 1, "Texture" } ).set_name( "RLL_ImGui" );
    }
    else {
        resource_layout_creation.add_binding( { ResourceType::Constants, 0, 1, "LocalConstants" } ).add_binding( { ResourceType::Texture, 1, 1, "Texture" } ).set_name( "RLL_ImGui" );
    }
    
    g_resource_layout = gpu->create_resource_layout( resource_layout_creation );
#endif // HYDRA_IMGUI_HFX

    pipeline_creation.add_resource_layout( g_resource_layout );

    g_imgui_pipeline = gpu->create_pipeline( pipeline_creation );

    // Create constant buffer
    BufferCreation cb_creation = { BufferType::Constant_mask, ResourceUsageType::Dynamic, 64, nullptr, "CB_ImGui" };
    g_ui_cb = gpu->create_buffer( cb_creation );

    // Create resource list
    ResourceListCreation rl_creation{};
    rl_creation.set_layout( pipeline_creation.resource_layout[0] ).buffer( g_ui_cb, 0 ).texture( g_font_texture, 1 ).set_name( "RL_ImGui" );
    g_ui_resource_list = gpu->create_resource_list( rl_creation );

    // Add resource list to the map
    // Old Map
    g_texture_to_resource_list.init( &MemoryService::instance()->system_allocator, 4 );
    g_texture_to_resource_list.insert( g_font_texture.index, g_ui_resource_list.index );

    // Create vertex and index buffers //////////////////////////////////////////
    BufferCreation vb_creation = { BufferType::Vertex_mask, ResourceUsageType::Dynamic, 0, nullptr, "VB_ImGui" };
    g_vb = gpu->create_buffer( vb_creation );

    BufferCreation ib_creation = { BufferType::Index_mask, ResourceUsageType::Dynamic, g_ib_size, nullptr, "IB_ImGui" };
    g_ib = gpu->create_buffer( ib_creation );
}

void ImGuiService::shutdown() {
    hydra::gfx::Device* gpu = gfx->gpu;

    FlatHashMapIterator it = g_texture_to_resource_list.iterator_begin();
    while ( it.is_valid() ) {
        hydra::gfx::ResourceHandle handle = g_texture_to_resource_list.get( it );
        gpu->destroy_resource_list( { handle } );

        g_texture_to_resource_list.iterator_advance( it );
    }

    g_texture_to_resource_list.shutdown();

    gpu->destroy_buffer( g_vb );
    gpu->destroy_buffer( g_ib );
    gpu->destroy_buffer( g_ui_cb );
    gpu->destroy_resource_layout( g_resource_layout );

    gpu->destroy_pipeline( g_imgui_pipeline );
    gpu->destroy_texture( g_font_texture );
}

void ImGuiService::new_frame( void* window_handle ) {
    // SDL is always present (?!)
    ImGui_ImplSDL2_NewFrame( (SDL_Window*)window_handle );
    ImGui::NewFrame();
}

void ImGuiService::render( hydra::gfx::Renderer* renderer, hydra::gfx::CommandBuffer& commands ) {

    hydra::gfx::Device& gpu = *renderer->gpu;
    ImGui::Render();
    
    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)( draw_data->DisplaySize.x * draw_data->FramebufferScale.x );
    int fb_height = (int)( draw_data->DisplaySize.y * draw_data->FramebufferScale.y );
    if ( fb_width <= 0 || fb_height <= 0 )
        return;

#if defined(HYDRA_VULKAN)
    bool clip_origin_lower_left = false;
#else
    bool clip_origin_lower_left = true;
#endif // HYDRA_VULKAN

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

    using namespace hydra::gfx;

    // Upload data
    ImDrawVert* vtx_dst = NULL;
    ImDrawIdx* idx_dst = NULL;

    MapBufferParameters map_parameters_vb = { g_vb, 0, (u32)vertex_size };
    vtx_dst = (ImDrawVert*)gpu.map_buffer( map_parameters_vb );

    if ( vtx_dst ) {
        for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy( vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
            vtx_dst += cmd_list->VtxBuffer.Size;
        }

        gpu.unmap_buffer( map_parameters_vb );
    }

    MapBufferParameters map_parameters_ib = { g_ib, 0, (u32)index_size };
    idx_dst = (ImDrawIdx*)gpu.map_buffer( map_parameters_ib );

    if ( idx_dst ) {
        for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy( idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        gpu.unmap_buffer( map_parameters_ib );
    }

    // TODO_KS: Add the sorting.
    commands.push_marker( "ImGUI" );

    // todo: key
    uint64_t sort_key = 0;// hydra::gfx::SortKey::get_key( 254 );
    commands.bind_pass( sort_key++, gpu.get_swapchain_pass() );
    commands.bind_pipeline( sort_key++, g_imgui_pipeline );
    commands.bind_vertex_buffer( sort_key++, g_vb, 0, 0 );
    commands.bind_index_buffer( sort_key++, g_ib );

    const Viewport viewport = { 0, 0, (u16)fb_width, (u16)fb_height, 0.0f, 1.0f };
    commands.set_viewport( sort_key++, &viewport );

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
    };

    MapBufferParameters cb_map = { g_ui_cb, 0, 0 };
    float* cb_data = (float*)gpu.map_buffer( cb_map );
    if ( cb_data ) {
        memcpy( cb_data, &ortho_projection[0][0], 64 );
        gpu.unmap_buffer( cb_map );
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    //
    int counts = draw_data->CmdListsCount;

    TextureHandle last_texture = g_font_texture;
    // todo:map
    ResourceListHandle last_resource_list = { g_texture_to_resource_list.get( last_texture.index ) };

    commands.bind_resource_list( sort_key++, &last_resource_list, 1, nullptr, 0 );

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
                clip_rect.x = ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x;
                clip_rect.y = ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y;
                clip_rect.z = ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x;
                clip_rect.w = ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y;

                if ( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
                {
                    // Apply scissor/clipping rectangle
                    if ( clip_origin_lower_left ) {
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)( fb_height - clip_rect.w ), (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
                        commands.set_scissor( sort_key++, &scissor_rect );
                    }
                    else {
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)clip_rect.y, (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
                        commands.set_scissor( sort_key++, &scissor_rect );
                    }

                    // Retrieve 
                    TextureHandle new_texture = *(TextureHandle*)( pcmd->TextureId );
                    if ( new_texture.index != last_texture.index && new_texture.index != k_invalid_texture.index ) {
                        last_texture = new_texture;
                        FlatHashMapIterator it = g_texture_to_resource_list.find( last_texture.index );

                        // TODO: invalidate handles and update resource list when needed ?
                        // Found this problem when reusing the handle from a previous 
                        // If not present
                        if ( it.is_invalid() ) {
                            // Create new resource list
                            ResourceListCreation rl_creation{};

                            rl_creation.set_layout( g_resource_layout ).buffer( g_ui_cb, 0 ).texture( last_texture, 1 ).set_name( "RL_Dynamic_ImGUI" );
                            last_resource_list = gpu.create_resource_list( rl_creation );

                            g_texture_to_resource_list.insert( new_texture.index, last_resource_list.index );
                        }
                        else {
                            last_resource_list.index = g_texture_to_resource_list.get( it );
                        }
                        commands.bind_resource_list( sort_key++, &last_resource_list, 1, nullptr, 0 );
                    }

                    commands.draw_indexed( sort_key++, hydra::gfx::TopologyType::Triangle, pcmd->ElemCount, 1, index_buffer_offset + pcmd->IdxOffset, vtx_buffer_offset + pcmd->VtxOffset, 0 );
                }
            }
            
        }
        index_buffer_offset += cmd_list->IdxBuffer.Size;
        vtx_buffer_offset += cmd_list->VtxBuffer.Size;
    }

    commands.pop_marker();
}


void imgui_on_resize( hydra::gfx::Device& gpu, u32 width, u32 height ) {
    using namespace hydra::gfx;

    // Rebind textures to resource lists
    // todo:map
    /*const u32 num_textures = hash_map_size( g_texture_to_resource_list );
    for ( u32 i = 0; i < num_textures; ++i ) {
        const TextureToResourceListMap& entry = g_texture_to_resource_list[ i ];
        ResourceListUpdate u;
        ResourceListHandle rl = { entry.value };
        u.resource_list = rl;
        gpu.update_resource_list_instant( u );
    }*/
}

void imgui_remove_cached_texture( hydra::gfx::Device& gpu, hydra::gfx::TextureHandle& texture ) {
    FlatHashMapIterator it = g_texture_to_resource_list.find( texture.index );
    if ( it.is_valid() ) {
        
        // Destroy resource list
        hydra::gfx::ResourceListHandle resource_list{ g_texture_to_resource_list.get(it) };
        gpu.destroy_resource_list( resource_list );

        // Remove from cache
        g_texture_to_resource_list.remove( texture.index );
    }
    
}
//
// Create draw commands from ImGui draw data.
//
void imgui_collect_draw_data( ImDrawData* draw_data, hydra::gfx::Device& gpu_device, hydra::gfx::CommandBuffer& commands )
{
    
}

// File Dialog //////////////////////////////////////////////////////////////////
/*

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

bool imgui_path_dialog_open( const char* button_name, const char* path ) {
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

        // Search files
        if ( scan_folder ) {
            scan_folder = false;

            hydra::file_find_files_in_path( ".", directory.path, files, directories );
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

        if ( ImGui::Button( "Choose Current Folder" ) ) {
            strcpy( last_path, directory.path );
            // Remove the asterisk
            last_path[strlen( last_path ) - 1] = 0;

            selected = true;
            opened = false;
        }
        ImGui::SameLine();
        if ( ImGui::Button( "Cancel" ) ) {
            opened = false;
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    // Update opened map
    string_hash_put( file_dialog_open_map, button_name, opened );

    return selected;
}

const char* imgui_path_dialog_get_path() {
    return last_path;
}

////////////////////////////////////////////////////////////
*/
// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct ExampleAppLog {
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
    bool                AutoScroll;     // Keep scrolling if already at the bottom

    ExampleAppLog() {
        AutoScroll = true;
        Clear();
    }

    void    Clear() {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back( 0 );
    }

    void    AddLog( const char* fmt, ... ) IM_FMTARGS( 2 ) {
        int old_size = Buf.size();
        va_list args;
        va_start( args, fmt );
        Buf.appendfv( fmt, args );
        va_end( args );
        for ( int new_size = Buf.size(); old_size < new_size; old_size++ )
            if ( Buf[ old_size ] == '\n' )
                LineOffsets.push_back( old_size + 1 );
    }

    void    Draw( const char* title, bool* p_open = NULL ) {
        if ( !ImGui::Begin( title, p_open ) ) {
            ImGui::End();
            return;
        }

        // Options menu
        if ( ImGui::BeginPopup( "Options" ) ) {
            ImGui::Checkbox( "Auto-scroll", &AutoScroll );
            ImGui::EndPopup();
        }

        // Main window
        if ( ImGui::Button( "Options" ) )
            ImGui::OpenPopup( "Options" );
        ImGui::SameLine();
        bool clear = ImGui::Button( "Clear" );
        ImGui::SameLine();
        bool copy = ImGui::Button( "Copy" );
        ImGui::SameLine();
        Filter.Draw( "Filter", -100.0f );

        ImGui::Separator();
        ImGui::BeginChild( "scrolling", ImVec2( 0, 0 ), false, ImGuiWindowFlags_HorizontalScrollbar );

        if ( clear )
            Clear();
        if ( copy )
            ImGui::LogToClipboard();

        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if ( Filter.IsActive() ) {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have a random access on the result on our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of search/filter.
            // especially if the filtering function is not trivial (e.g. reg-exp).
            for ( int line_no = 0; line_no < LineOffsets.Size; line_no++ ) {
                const char* line_start = buf + LineOffsets[ line_no ];
                const char* line_end = ( line_no + 1 < LineOffsets.Size ) ? ( buf + LineOffsets[ line_no + 1 ] - 1 ) : buf_end;
                if ( Filter.PassFilter( line_start, line_end ) )
                    ImGui::TextUnformatted( line_start, line_end );
            }
        } else {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward to skip non-visible lines.
            // Here we instead demonstrate using the clipper to only process lines that are within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
            // Using ImGuiListClipper requires A) random access into your data, and B) items all being the  same height,
            // both of which we can handle since we an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display anymore, which is why we don't use the clipper.
            // Storing or skimming through the search result would make it possible (and would be recommended if you want to search through tens of thousands of entries)
            ImGuiListClipper clipper;
            clipper.Begin( LineOffsets.Size );
            while ( clipper.Step() ) {
                for ( int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++ ) {
                    const char* line_start = buf + LineOffsets[ line_no ];
                    const char* line_end = ( line_no + 1 < LineOffsets.Size ) ? ( buf + LineOffsets[ line_no + 1 ] - 1 ) : buf_end;
                    ImGui::TextUnformatted( line_start, line_end );
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if ( AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
            ImGui::SetScrollHereY( 1.0f );

        ImGui::EndChild();
        ImGui::End();
    }
}; // struct ExampleAppLog

static ExampleAppLog        s_imgui_log;
static bool                 s_imgui_log_open = true;

static void imgui_print( const char* text ) {
    s_imgui_log.AddLog( "%s", text );
}

void imgui_log_init() {

    LogService::instance()->set_callback( &imgui_print );
}

void imgui_log_shutdown() {

    LogService::instance()->set_callback( nullptr );
}

void imgui_log_draw() {
    s_imgui_log.Draw( "Log", &s_imgui_log_open );
}


// Plot with ringbuffer

// https://github.com/leiradel/ImGuiAl
template<typename T, size_t L>
class Sparkline {
public:
    Sparkline() {
        setLimits( 0, 1 );
        clear();
    }

    void setLimits( T const min, T const max ) {
        _min = static_cast< float >( min );
        _max = static_cast< float >( max );
    }

    void add( T const value ) {
        _offset = ( _offset + 1 ) % L;
        _values[ _offset ] = value;
    }

    void clear() {
        memset( _values, 0, L * sizeof( T ) );
        _offset = L - 1;
    }

    void draw( char const* const label = "", ImVec2 const size = ImVec2() ) const {
        char overlay[ 32 ];
        print( overlay, sizeof( overlay ), _values[ _offset ] );

        ImGui::PlotLines( label, getValue, const_cast< Sparkline* >( this ), L, 0, overlay, _min, _max, size );
    }

protected:
    float _min, _max;
    T _values[ L ];
    size_t _offset;

    static float getValue( void* const data, int const idx ) {
        Sparkline const* const self = static_cast< Sparkline* >( data );
        size_t const index = ( idx + self->_offset + 1 ) % L;
        return static_cast< float >( self->_values[ index ] );
    }

    static void print( char* const buffer, size_t const bufferLen, int const value ) {
        snprintf( buffer, bufferLen, "%d", value );
    }

    static void print( char* const buffer, size_t const bufferLen, double const value ) {
        snprintf( buffer, bufferLen, "%f", value );
    }
};

static Sparkline<f32, 100> s_fps_line;

void imgui_fps_init() {
    s_fps_line.clear();
    s_fps_line.setLimits( 0.0f, 33.f );
}

void imgui_fps_shutdown() {
}

void imgui_fps_add( f32 dt ) {
    s_fps_line.add( dt );
}

void imgui_fps_draw() {
    s_fps_line.draw( "FPS", { 0,100 } );
}
} // namespace hydra