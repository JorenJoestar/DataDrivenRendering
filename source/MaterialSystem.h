#pragma once

#include "hydra/hydra_lib.h"
#include "hydra/hydra_application.h"
#include "hydra/hydra_graphics.h"
#include "hydra/hydra_rendering.h"
#include "hydra/hydra_resources.h"

#include "ShaderCodeGenerator.h"

class TextEditor;
struct MemoryEditor;


// FileBrowser //////////////////////////////////////////////////////////////////

struct FileBrowser {

    using                           FileSingleClicked   = void(*)(void* user_data, uint8_t button, const char* filename );
    using                           FileDoubleClicked   = void(*)(void* user_data, uint8_t button, const char* filename );
    using                           PopupShowing        = void(*)(void* user_data, const char* filename );

    void                            init();
    void                            terminate();

    void                            open_folder( const char* folder );
    void                            open_folder( const char* folder, const char* extension );

    void                            draw_window( const char* name );
    void                            draw_contents();

    void                            set_single_click_callback( FileSingleClicked callback, void* user_data );
    void                            set_double_click_callback( FileDoubleClicked callback, void* user_data );
    void                            set_popup_showing_callback( PopupShowing callback, void* user_data );


    hydra::StringArray              files;
    hydra::StringArray              directories;

    StringBuffer                    current_working_directory;
    StringBuffer                    current_filename;
    const char*                     last_selected_filename                  = nullptr;

    FileSingleClicked               file_single_clicked_callback            = nullptr;
    void*                           file_single_clicked_callback_user_data  = nullptr;

    FileDoubleClicked               file_double_clicked_callback            = nullptr;
    void*                           file_double_clicked_callback_user_data  = nullptr;

    PopupShowing                    popup_showing_callback                  = nullptr;
    void*                           popup_showing_callback_user_data        = nullptr;

}; // struct FileBrowser

// MaterialSystemApplication ////////////////////////////////////////////////////

struct EditorMaterial {

    hydra::Resource*                material_resource;
    hydra::graphics::Material*      material;

    hfx::ShaderEffectFile*          shader_effect_file;
    
};

struct MaterialSystemApplication : public hydra::Application {

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

    // Actions
    void                            edit_file( const char* filepath );
    void                            file_action_popup_render( const char* filename );
    void                            create_material( const char* filename );
    void                            load_material( const char* filename );
    void                            save_material( const char* filename );
    void                            compile_hfx( const char* filename );

    bool                            show_demo_window        = false;

    hydra::graphics::PipelineMap*   name_to_render_pipeline = nullptr;

    hydra::ResourceManager          resource_manager;

    StringBuffer                    ui_string_buffer;
    StringBuffer                    parsing_string_buffer;

    hydra::StringArray              render_pipeline_string_array;

    hydra::graphics::BufferHandle   shadertoy_buffer;

    EditorMaterial                  editor_material;
    

    // Visual widgets used for editing.
    TextEditor*                     text_editor             = nullptr;
    MemoryEditor*                   memory_editor           = nullptr;

    FileBrowser                     file_browser;
    FileBrowser                     choose_file_browser;

    StringBuffer                    material_filename;
    StringBuffer                    opened_file_path;

    FileType                        opened_file_type;
    char*                           file_text               = nullptr;
    size_t                          file_size               = 0;
    bool                            file_save_changes       = false;

    hfx::ShaderEffectFile           shader_effect_file;
    hydra::graphics::RenderPipeline* current_render_pipeline = nullptr;

    hydra::graphics::ResourcePool   render_stage_pool;

}; // struct MaterialSystemApplication


