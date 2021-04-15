#pragma once

#include "hydra/hydra_application.h"
#include "hydra/hydra_rendering.h"
#include "hydra/hydra_animation.h"

//
//
struct Sprite {

    vec2s                       size;

    hydra::graphics::Material*  material;
    hydra::graphics::Texture*   albedo;
    hydra::graphics::Texture*   normals;

}; // struct Sprite


//
//
struct SpriteGPUData {

    vec4s                   position;

    vec2s                   uv_size;
    vec2s                   uv_offset;

    vec2s                   size;
    float                   padding[ 2 ];

}; // struct SpriteGPUData



//
//
struct DrawBatch {

    hydra::graphics::Material* material;
    u32                         material_pass_index;
    u32                         offset;
    u32                         count;

}; // struct SpriteBatch

//
//
struct SpriteFeature : public hydra::graphics::RenderFeature {

    void                            load_resources( hydra::graphics::Renderer& renderer, bool init, bool reload ) override;
    void                            unload_resources( hydra::graphics::Renderer& renderer, bool shutdown, bool reload ) override;

    void                            update( hydra::graphics::Renderer& renderer, f32 delta_time ) override;
    void                            render( hydra::graphics::Renderer& renderer, u64& sort_key, hydra::graphics::CommandBuffer* gpu_commands ) override;

    void                            sprite_init( hydra::graphics::Renderer& renderer, Sprite* s, cstring albedo, cstring normals, const vec2s size );
    void                            sprite_shutdown( hydra::graphics::Renderer& renderer, Sprite* s );

    void                            sprite_add_instance( Sprite* s, SpriteGPUData* gpu_data, hydra::AnimationState* animation, const vec3s& position );
    void                            sprite_render( hydra::graphics::CommandBuffer* gpu_commands, u64 sort_key, Sprite* s, u32 index );

    hydra::graphics::Shader*        shader;
    hydra::graphics::Buffer*        constants;
    hydra::graphics::Buffer*        sprite_instance_buffer;

    hydra::graphics::RenderStage**  forward;
    hydra::graphics::Camera*        camera;
    hydra::AnimationSystem*         animations;

    hydra::AnimationHandle          nightmare_galloping_animation;

    hydra::AnimationState           nightmare_animation;
    Sprite                          nightmare_sprite;

    Sprite                          background_sprite;

    f32                             time            = 0.f;
    f32                             sprite_scale    = 1.f;
    f32                             alpha_threshold = 0.5f;
    i32                             filter_type     = 2;
    f32                             filter_width    = 1.5f;
    bool                            enable_premultiplied = true;

    // Sprite animation
    bool                            pause_sprite_animation = true;
    f32                             sprite_animation_min_size = 0.1f;
    f32                             sprite_animation_max_size = 40.f;
    f32                             sprite_animation_time   = 0.f;

    // Camera animation
    bool                            animate_camera  = true;
    f32                             camera_animation_min_zoom = 3.f;
    f32                             camera_animation_max_zoom = 4.f;
    f32                             camera_animation_speed  = 0.2f;
    f32                             camera_animation_time   = 0.f;

    f32                             camera_max_translation_x = 32.f;
    f32                             camera_max_translation_y = 16.f;
    f32                             camera_translation_speed = 0.05f;

}; // struct SpriteFeature

//
//
struct PixelArtFilteringApp : public hydra::Application {

    void                            app_init() override;
    void                            app_terminate() override;

    void                            app_update( hydra::ApplicationUpdate& update ) override;
    void                            app_resize( uint32_t width, uint32_t height ) override;

    void                            app_load_resources( hydra::ApplicationReload& load ) override;
    void                            app_unload_resources( hydra::ApplicationReload& unload ) override;

    hydra::graphics::Camera         camera;

    hydra::graphics::Shader*        apply_shader;
    hydra::graphics::Material*      apply_material;

    hydra::graphics::RenderStage*   forward_stage;
    hydra::graphics::RenderStage*   swapchain;

    hydra::graphics::Texture*       forward_rt;
    hydra::graphics::Texture*       forward_depth;
    hydra::graphics::Buffer*        crt_cb;

    SpriteFeature                   sprite_feature;
    f32                             sprite_rt_scale = 1.f;
    bool                            enable_crt = false;

    hydra::AnimationSystem          animations;

}; // struct PixelArtApplication
