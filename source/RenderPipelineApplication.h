#pragma once

#include "hydra/hydra_lib.h"
#include "hydra/hydra_application.h"
#include "hydra/hydra_graphics.h"
#include "hydra/hydra_rendering.h"

struct RenderPipelineTextureCreation {

    hydra::graphics::TextureCreation texture_creation;
    char                            name[32];
    char                            path[512];

}; // struct RenderPipelineTextureCreation

struct RenderStageCreation {

    char                            name[32];
    char                            material_name[32];
    char                            render_view_name[32];

    RenderPipelineTextureCreation*  inputs[32];
    RenderPipelineTextureCreation*  outputs[8];
    RenderPipelineTextureCreation*  output_depth;

    uint32_t                        input_count;
    uint32_t                        output_count;

    float                           clear_color[4];
    float                           clear_depth_value;
    uint8_t                         clear_stencil_value;
    uint8_t                         clear_rt;
    uint8_t                         clear_depth;
    uint8_t                         clear_stencil;

    uint8_t                         stage_type;             // RenderStage::Type enum
    uint8_t                         material_pass_index;

    hydra::graphics::ShaderResourcesLookup overriding_lookups;    // Lookups to override the material ones used in the texture.

}; // struct RenderStageCreation

struct RenderPipelineCreation {

    struct TextureMap {

        char*                       key;
        RenderPipelineTextureCreation* value;

    }; // struct PipelineMap

    void                            init();
    void                            terminate();

    bool                            is_valid();

    hydra::StringBuffer             string_buffer;

    RenderStageCreation*            render_stages;
    TextureMap*                     name_to_textures;

    char                            name[32];

}; // struct RenderPipelineCreation

struct RenderPipelineManager {

    void                            init( hydra::graphics::Device& device, hydra::StringBuffer& temp_string_buffer );
    void                            terminate();

    void                            set_pipeline( hydra::graphics::Device& device, const char* name, hydra::StringBuffer& temp_string_buffer, hydra::graphics::ShaderResourcesDatabase& initial_db );
    hydra::graphics::RenderPipeline* create_pipeline( hydra::graphics::Device& device, const RenderPipelineCreation& creation, hydra::StringBuffer& temp_string_buffer, hydra::graphics::ShaderResourcesDatabase& initial_db );

    hydra::graphics::PipelineMap*   name_to_render_pipeline     = nullptr;
    RenderPipelineCreation*         render_pipeline_creations   = nullptr;
    hydra::graphics::RenderViewMap* name_to_render_view         = nullptr;

    hydra::graphics::RenderPipeline* current_render_pipeline    = nullptr;
}; // struct RenderPipelineManager


struct CameraInput {

    float                           target_yaw;
    float                           target_pitch;

    float                           mouse_sensitivity;
    float                           movement_delta;
    uint32_t                        ignore_dragging_frames;

    vec3s                           target_movement;

    bool                            mouse_dragging;

    void                            init();
    void                            reset();

    void                            update( ImGuiIO& input, const hydra::graphics::Camera& camera, uint16_t window_center_x, uint16_t window_center_y );

}; // struct CameraInput


struct CameraMovementUpdate {

    float                           rotation_speed;
    float                           movement_speed;

    void                            init( float rotation_speed, float movement_speed );

    void                            update( hydra::graphics::Camera& camera, CameraInput& camera_input, float delta_time );
}; // struct CameraMovementUpdate

struct RenderPipelineApplication : public hydra::Application {

    enum FileType {
        Material_HMT = 0,
        ShaderEffect_HFX,
        Binary_HFX,
        Binary,
        Count
    };

    void                            app_init()                                              override;
    void                            app_terminate()                                         override;
    void                            app_render( hydra::graphics::CommandBuffer* commands )  override;
    void                            app_resize( uint16_t width, uint16_t height )           override;

    hydra::StringBuffer             temporary_string_buffer;

    RenderPipelineManager           render_pipeline_manager;

    hydra::graphics::SceneRenderer  scene_renderer;
    hydra::graphics::LineRenderer   line_renderer;
    hydra::graphics::LightingManager lighting_manager;
    hydra::graphics::RenderView     main_render_view;

    CameraInput                     camera_input;
    CameraMovementUpdate            camera_movement_update;

    bool                            show_grid;
    int8_t                          reload_shaders;

}; // struct RenderPipelineApplication
