#include "pixel_art_filtering_app.h"

#include "stb_image.h"

#include "cglm/struct/vec2.h"
#include "imgui/imgui.h"

#include "hydra/hydra_input.h"
#include "hydra/hydra_imgui.h"

// PixelArtFilteringApp /////////////////////////////////////////////////////////

struct CRTConstants {
    vec4s   SourceSize;
    vec4s   OriginalSize;
    vec4s   OutputSize;

    u32     FrameCount = 0;
    float   MASK       = 1.f;
    float   MASK_INTENSITY = .5f;
    float   SCANLINE_THINNESS = .5f;

    float   SCAN_BLUR = 2.5f;
    float   CURVATURE = 0.0f;
    float   TRINITRON_CURVE = 0.f;
    float   CORNER = 0.f;

    float   CRT_GAMMA = 2.4f;
    f32     pad[ 3 ];
};

CRTConstants s_crt_consts;

void PixelArtFilteringApp::app_init() {

    camera.init_orthographic( 0.1f, 100.f, 1280.f, 720.f, 1.f );
    camera_input.init( false );
    camera.position.z = 70.f;

    animations.init();

    hydra::imgui_log_init();
}

void PixelArtFilteringApp::app_terminate() {

    animations.shutdown();

    hydra::imgui_log_shutdown();
}

vec4s get_shader_texture_size( const vec2s& sizes ) {
    return { sizes.x, sizes.y, 1 / sizes.x, 1 / sizes.y };
}

vec4s get_shader_texture_size( u32 width, u32 height ) {
    return { width * 1.f, height * 1.f, 1.f / width, 1.f / height };
}

vec4s get_shader_texture_size2( u32 width, u32 height ) {
    return { 1.f / width, 1.f / height, width * 1.f, height * 1.f};
}

void PixelArtFilteringApp::app_update( hydra::ApplicationUpdate& update ) {

    // Update
    camera.update();

    sprite_feature.update( *update.renderer, update.delta_time );

    CRTConstants* crt = ( CRTConstants* )renderer->map_buffer( crt_cb, 0, sizeof(CRTConstants) );
    if ( crt ) {
        memcpy( crt, &s_crt_consts, sizeof( CRTConstants ) );
        crt->OutputSize = get_shader_texture_size( renderer->width * 1.f, renderer->height * 1.f );
        crt->OriginalSize = get_shader_texture_size( forward_rt->desc.width, forward_rt->desc.height );
        crt->SourceSize = crt->OriginalSize;

        renderer->unmap_buffer( crt_cb );
    }

    // Render
    u64 sort_key = 0;
    renderer->draw( forward_stage, sort_key, update.gpu_commands );
    renderer->draw_material( swapchain, sort_key, update.gpu_commands, apply_material, enable_crt ? 1 : 0 );

    if ( input->is_key_just_pressed( hydra::KEY_R ) ) {
        app_reload();
        hydra::print_format( "Reloaded!\n" );
    }

    if ( ImGui::Begin( "Post" ) ) {
        ImGui::Checkbox( "Enable CRT", &enable_crt );

        static i32 mask_type = 2;
        ImGui::SliderInt( "Mask", &mask_type, 0, 3 );
        s_crt_consts.MASK = mask_type;
        ImGui::SliderFloat( "Mask intensity", &s_crt_consts.MASK_INTENSITY, 0.f, 1.f );
        ImGui::SliderFloat( "Scanline thinness", &s_crt_consts.SCANLINE_THINNESS, 0.f, 1.f );
        ImGui::SliderFloat( "Scan blur", &s_crt_consts.SCAN_BLUR, 1.f, 3.f );
        ImGui::SliderFloat( "Curvature", &s_crt_consts.CURVATURE, 0.f, .25f );
        ImGui::SliderFloat( "Trinitron curve", &s_crt_consts.TRINITRON_CURVE, 0.f, 1.f );
        ImGui::SliderFloat( "Corner", &s_crt_consts.CORNER, 0.f, 11.f );
        ImGui::SliderFloat( "CRT gamma", &s_crt_consts.CRT_GAMMA, 0.f, 51.f );
        ImGui::Separator();
        
    }
    ImGui::End();

    hydra::imgui_log_draw();
}

void PixelArtFilteringApp::app_resize( uint32_t width, uint32_t height ) {
    using namespace hydra::graphics;

    renderer->resize( forward_stage );

    renderer->reload_resource_list( apply_material, 0 );
    renderer->reload_resource_list( apply_material, 1 );

    camera.set_viewport_size( forward_rt->desc.width, forward_rt->desc.height );
}

void PixelArtFilteringApp::app_load_resources( hydra::ApplicationReload& load ) {
    using namespace hydra::graphics;

    Renderer* r = load.renderer;

    BufferCreation bc;
    crt_cb = r->create_buffer( bc.set( BufferType::Constant, ResourceUsageType::Dynamic, sizeof( CRTConstants ) ).set_name( "crt_cb" ) );

    // Create texture
    u16 rt_w = hydra::roundu16( r->width * sprite_rt_scale );
    u16 rt_h = hydra::roundu16( r->height * sprite_rt_scale );
    TextureCreation tc;
    forward_rt = r->create_texture( tc.set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, TextureCreationFlags::RenderTarget_mask )
                                    .set_size( rt_w, rt_h, 1 ).set_name( "forward_rt" ) );

    forward_depth = r->create_texture( tc.set_format_type( TextureFormat::D24_UNORM_S8_UINT, TextureType::Texture2D ).set_name( "forward_depth" ) );

    // Create render stages
    RenderStageCreation rsc;
    rsc.reset().add_render_texture( forward_rt ).set_depth_stencil_texture(forward_depth).set_scaling( sprite_rt_scale, sprite_rt_scale, 1 ).set_type( RenderPassType::Standard ).set_name( "forward_stage" ).clear.set_color( {.5,.5,.5,1} ).set_depth(1);
    forward_stage = r->create_stage( rsc );
    array_push( forward_stage->features, &sprite_feature );

    swapchain = r->create_stage( rsc.reset().set_type( RenderPassType::Swapchain ).set_name( "swapchain" ) );

    // Load render feature
    sprite_feature.forward = &forward_stage;
    sprite_feature.camera = &camera;
    sprite_feature.animations = &animations;
    sprite_feature.load_resources( *load.renderer, load.init, load.reload );

    // Load shader and material
    hfx::ShaderEffectFile hfx;
    hfx::hfx_compile( "..\\data\\articles\\PixelArtFiltering\\pixel_art_post.hfx", "..\\data\\bin\\pixel_art_post.bin", hfx::CompileOptions_Vulkan | hfx::CompileOptions_Embedded, &hfx );

    ShaderCreation sc;
    RenderPassOutput rpo[] = { r->gpu->get_swapchain_output(), r->gpu->get_swapchain_output() };
    apply_shader = r->create_shader( sc.reset().set_shader_binary( &hfx ).set_outputs( rpo, ArrayLength(rpo) ) );

    ResourceListCreation rl[ 2 ];
    rl[ 0 ].reset().texture( forward_rt->handle, 0 );
    rl[ 1 ].reset().texture( forward_rt->handle, 1 ).buffer( crt_cb->handle, 0 );

    MaterialCreation mc;
    apply_material = r->create_material( mc.reset().set_shader( apply_shader ).set_resource_lists( rl, ArrayLength( rl ) ) );

}

void PixelArtFilteringApp::app_unload_resources( hydra::ApplicationReload& unload ) {
    using namespace hydra::graphics;

    sprite_feature.unload_resources( *unload.renderer, unload.shutdown, unload.reload );

    Renderer* r = unload.renderer;
    r->destroy_buffer( crt_cb );
    r->destroy_material( apply_material );
    r->destroy_shader( apply_shader );
    r->destroy_stage( forward_stage );
    r->destroy_stage( swapchain );
    r->destroy_texture( forward_rt );
}

// SpriteFeature ////////////////////////////////////////////////////////////////

//
//
struct SpriteConstants {
    mat4s                   view_projection_matrix;

    float                   time            = 0.f;
    float                   sprite_scale    = 1.f;
    float                   alpha_threshold = 0.99f;
    u32                     filter_type     = 2;

    vec4s                   screen_size;

    float                   filter_width;
    f32                     camera_scale;
    f32                     texels_per_pixel;
    u32                     enable_premultiplied;

}; // struct SpriteConstants


static hydra::graphics::Texture* load_texture_from_file( hydra::graphics::Renderer& renderer, cstring path ) {
    using namespace hydra::graphics;

    int comp, width, height;
    uint8_t* image_data = stbi_load( path, &width, &height, &comp, 4 );
    TextureCreation tc;
    hydra::graphics::Texture* texture = renderer.create_texture( tc.set_data( image_data ).set_format_type( TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D ).set_flags( 1, 0 )
                                                                 .set_size( ( u16 )width, ( u16 )height, 1 ) );
    stbi_image_free( image_data );

    return texture;
}

void SpriteFeature::sprite_init( hydra::graphics::Renderer& renderer, Sprite* s, cstring albedo, cstring normals, const vec2s size ) {
    using namespace hydra::graphics;

    s->albedo = load_texture_from_file( renderer, albedo );
    s->normals = load_texture_from_file( renderer, normals );

    s->size = size;

    ResourceListCreation rl[ 2 ];
    rl[ 0 ].reset().buffer( constants->handle, 0 ).texture( s->albedo->handle, 1 ).texture( s->normals->handle, 2 );
    rl[ 1 ].reset().buffer( constants->handle, 0 ).texture( s->albedo->handle, 1 ).texture( s->normals->handle, 2 );

    MaterialCreation mc;
    s->material = renderer.create_material( mc.reset().set_shader( shader ).set_resource_lists( rl, ArrayLength( rl ) ) );

}

void SpriteFeature::sprite_shutdown( hydra::graphics::Renderer& renderer, Sprite* s ) {

    renderer.destroy_texture( s->albedo );
    renderer.destroy_texture( s->normals );
    renderer.destroy_material( s->material );
}

void SpriteFeature::sprite_add_instance( Sprite* s, SpriteGPUData* gpu_data, hydra::AnimationState* sprite_animation, const vec3s& position ) {
    SpriteGPUData& sprite0 = *gpu_data;
    sprite0.position = { position.x, position.y, position.z, 0 };
    sprite0.size = s->size;
    sprite0.uv_offset = sprite_animation ? sprite_animation->uv0 : vec2s{0, 0};
    sprite0.uv_size = sprite_animation ? glms_vec2_sub( sprite_animation->uv1, sprite_animation->uv0 ) : vec2s{1, 1};
}

void SpriteFeature::sprite_render( hydra::graphics::CommandBuffer* gpu_commands, u64 sort_key, Sprite* s, u32 index ) {
    using namespace hydra::graphics;

    gpu_commands->bind_pipeline( sort_key++, s->material->pipelines[ enable_premultiplied ? 1 : 0 ] );
    gpu_commands->bind_resource_list( sort_key++, &s->material->resource_lists[ enable_premultiplied ? 1 : 0 ], 1, 0, 0 );
    gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 6, index, 1 );
}

void SpriteFeature::load_resources( hydra::graphics::Renderer& renderer, bool init, bool reload ) {

    using namespace hydra::graphics;
    
    // Common code
    BufferCreation bc;
    constants = renderer.create_buffer( bc.set( BufferType::Constant, ResourceUsageType::Dynamic, sizeof( SpriteConstants ) ).set_name( "sprite_cb" ) );
    sprite_instance_buffer = renderer.create_buffer( bc.set( BufferType::Vertex, ResourceUsageType::Dynamic, sizeof( SpriteGPUData ) * 100 ).set_name( "sprite_instances" ) );
    
    hfx::ShaderEffectFile hfx;
    hfx::hfx_compile( "..\\data\\articles\\PixelArtFiltering\\pixel_art_filtering.hfx", "..\\data\\bin\\pixel_art_filtering.bin", hfx::CompileOptions_Vulkan | hfx::CompileOptions_Embedded, &hfx );

    RenderPassOutput rpo[ 2 ];
    rpo[ 0 ] = (*forward)->output;
    rpo[ 1 ] = ( *forward )->output;

    ShaderCreation sc;
    shader = renderer.create_shader( sc.reset().set_shader_binary( &hfx ).set_outputs( rpo, ArrayLength( rpo ) ) );

    // Nightmare sprite
    // Downloaded from here:
    // https://ansimuz.itch.io/gothicvania-patreon-collection
    // 
    const vec2s sprite_size = { 576 / 4.f, 96 * 1.f };
    sprite_init( renderer, &nightmare_sprite, "..\\data\\articles\\PixelArtFiltering\\nightmare-galloping_edge.png", "..\\data\\articles\\PixelArtFiltering\\nightmare-galloping_edge.png", sprite_size );

    // Animation
    hydra::AnimationCreation ac;
    nightmare_galloping_animation = animations->create_animation( ac.reset().set_texture_size( { nightmare_sprite.albedo->desc.width * 1.f, nightmare_sprite.albedo->desc.height * 1.f } ).set_origin( { 0, 0 } ).set_size( sprite_size )
                                                                  .set_animation( 4, 4, 12, true ) );

    animations->start_animation( nightmare_animation, nightmare_galloping_animation, true );

    // Background sprite
    sprite_init( renderer, &background_sprite, "..\\data\\articles\\PixelArtFiltering\\night-town-background-town.png", "..\\data\\articles\\PixelArtFiltering\\night-town-background-town.png", { 512, 99 } );
}

void SpriteFeature::unload_resources( hydra::graphics::Renderer& renderer, bool shutdown, bool reload ) {
    using namespace hydra::graphics;

    animations->destroy_animation( nightmare_galloping_animation );

    sprite_shutdown( renderer, &nightmare_sprite );
    sprite_shutdown( renderer, &background_sprite );

    renderer.destroy_buffer( sprite_instance_buffer );
    renderer.destroy_buffer( constants );
    renderer.destroy_shader( shader );
}

static f32 calculate_texels_per_pixel( f32 camera_width, f32 camera_height, f32 camera_zoom, f32 screen_width, f32 screen_height ) {
    f32 texels_per_pixel = 1.f;

    const f32 camera_aspect_ratio = camera_width / camera_height;
    const f32 screen_aspect_ratio = screen_width / screen_height;

    if ( screen_aspect_ratio > camera_aspect_ratio ) {
        texels_per_pixel = camera_height / screen_height;
    }
    else {
        texels_per_pixel = camera_width / screen_width;
    }
    // Zoom is inverted compared to ColeCecil post, so we keep same calculation here but in the shader we multiply.
    return texels_per_pixel / camera_zoom;
}

void SpriteFeature::update( hydra::graphics::Renderer& renderer, f32 delta_time ) {

    SpriteConstants* cb_data = ( SpriteConstants* )renderer.map_buffer( constants, 0, sizeof(SpriteConstants) );
    if ( cb_data ) {

        memcpy( cb_data->view_projection_matrix.raw, &camera->view_projection.m00, 64 );
        cb_data->sprite_scale = sprite_scale;
        cb_data->time = time;
        cb_data->alpha_threshold = alpha_threshold;
        cb_data->filter_type = ( u32 )filter_type;
        cb_data->filter_width = filter_width;
        cb_data->screen_size = { renderer.width * 1.f, renderer.height * 1.f, 1.f / renderer.width, 1.f / renderer.height };
        cb_data->camera_scale = 1.f / camera->zoom;
        cb_data->enable_premultiplied = enable_premultiplied ? 1 : 0;
        cb_data->texels_per_pixel = calculate_texels_per_pixel( camera->viewport_width, camera->viewport_height, camera->zoom, renderer.width * 1.f, renderer.height * 1.f );

        renderer.unmap_buffer( constants );
    }

    animations->update_animation( nightmare_animation, pause_sprite_animation ? 0.f : delta_time );

    // Zoom and/or Translation animation
    if ( animate_camera ) {
        camera_animation_time += delta_time * camera_animation_speed;

        // Apply zoom to camera
        const f32 camera_01 = cosf( camera_animation_time ) * .5f + .5f;
        const f32 camera_zoom = glm_lerp( camera_animation_min_zoom, camera_animation_max_zoom, camera_01 );
        camera->set_zoom( 1.f / camera_zoom );

        // Apply translation to camera
        camera->position.x = cosf( camera_animation_time * camera_translation_speed ) * camera_max_translation_x;
        camera->position.y = cosf( camera_animation_time * camera_translation_speed ) * camera_max_translation_y;
    }
}

void SpriteFeature::render( hydra::graphics::Renderer& renderer, u64& sort_key, hydra::graphics::CommandBuffer* gpu_commands ) {

    using namespace hydra::graphics;
    SpriteGPUData* gpu_data = ( SpriteGPUData* )renderer.map_buffer( sprite_instance_buffer, 0, sizeof(SpriteGPUData) );

    if ( gpu_data ) {
        
        sprite_add_instance( &background_sprite, gpu_data, nullptr, { 0,0,0 } );
        ++gpu_data;

        sprite_add_instance( &nightmare_sprite, gpu_data, &nightmare_animation, { 0,5,5 } );
        ++gpu_data;

        renderer.unmap_buffer( sprite_instance_buffer );

        gpu_commands->bind_vertex_buffer( sort_key++, sprite_instance_buffer->handle, 0, 0 );

        sprite_render( gpu_commands, sort_key, &background_sprite, 0 );
        sprite_render( gpu_commands, sort_key, &nightmare_sprite, 1 );
    }

    if ( ImGui::Begin( "PixelArtFiltering" ) ) {
          
        if ( ImGui::SliderFloat( "Camera Zoom", &camera->zoom, 0.001f, 40.f ) )
            camera->update_projection = true;
        ImGui::Checkbox( "Camera Animation Enabled", &animate_camera );
        ImGui::SliderFloat( "Camera Zoom Min", &camera_animation_min_zoom, 0.001f, 250.f );
        ImGui::SliderFloat( "Camera Zoom Max", &camera_animation_max_zoom, 0.001f, 250.f );
        ImGui::SliderFloat( "Camera Animation Speed", &camera_animation_speed, 0.001f, 50.f );

        ImGui::SliderFloat( "Camera Translation X", &camera_max_translation_x, 0.001f, 128.f );
        ImGui::SliderFloat( "Camera Translation Y", &camera_max_translation_y, 0.001f, 128.f );
        ImGui::SliderFloat( "Camera Translation Speed", &camera_translation_speed, 0.001f, 50.f );

        ImGui::Separator();
        ImGui::SliderFloat( "Alpha Test Threshold", &alpha_threshold, 0.f, 1.f );
        ImGui::SliderFloat( "Sprite Scale", &sprite_scale, 0.001f, 40.f );
        static const char* filters[] = { "Nearest", "Fat Pixels", "IQ", "Klems", "ColeCecil", "Blocky", "AALinear", "AASmoothStep", "CSantosBH", "AADistance" };
        ImGui::Combo( "Pixel Art Filter", &filter_type, filters, IM_ARRAYSIZE( filters ) );
        ImGui::SliderFloat( "Filter Width", &filter_width, 0.001f, 10.f );
        ImGui::Checkbox( "Pause Sprite Animation", &pause_sprite_animation );
        ImGui::Checkbox( "Enable Premultiplied Alpha", &enable_premultiplied );
    }
    ImGui::End();
}


// MAIN ///////////////////////////////////////////////////////////////////////////////////////////

int main( int argc, char** argv ) {

    PixelArtFilteringApp app;
    app.main_loop( { 1280, 720, "Pixel Art Filtering" } );
    return 0;
}