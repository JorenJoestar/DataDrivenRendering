#pragma once

#include "hydra/hydra_lib.h"
#include "hydra/hydra_application.h"
#include "hydra/hydra_graphics.h"

#include "ShaderCodeGenerator.h"

class TextEditor;
struct MemoryEditor;
struct Resource;


// Material system //////////////////////////////////////////////////////////////

namespace hydra {
namespace graphics {

struct RenderPipeline;

//
//
struct ShaderResourcesDatabase {

    struct BufferStringMap {
        char*                       key;
        BufferHandle                value;
    }; // struct BufferStringMap

    struct TextureStringMap {
        char*                       key;
        TextureHandle               value;
    }; // struct TextureStringMap

    BufferStringMap*                name_to_buffer = nullptr;
    TextureStringMap*               name_to_texture = nullptr;

    void                            init();
    void                            terminate();

    void                            register_buffer( char* name, BufferHandle buffer );
    void                            register_texture( char* name, TextureHandle texture );

    BufferHandle                    find_buffer( char* name );
    TextureHandle                   find_texture( char* name );

}; // struct ShaderResourcesDatabase

//
//
struct ShaderResourcesLookup {

    enum Specialization {
        Frame, Pass, View, Shader
    }; // enum Specialization

    struct NameMap {
        char*                       key;
        char*                       value;
    }; // struct NameMap

    struct SpecializationMap {
        char*                       key;
        Specialization              value;
    }; // struct SpecializationMap

    NameMap*                        binding_to_resource     = nullptr;
    SpecializationMap*              binding_to_specialization = nullptr;

    void                            init();
    void                            terminate();

    void                            add_binding_to_resource( char* binding, char* resource );
    void                            add_binding_to_specialization( char* binding, Specialization specialization );

    char*                           find_resource( char* binding );
    Specialization                  find_specialization( char* binding );

    void                            specialize( char* pass, char* view, ShaderResourcesLookup& final_lookup );

}; // struct ShaderResourcesLookup

//
//
struct Texture {
    
    TextureHandle                   handle;
    const char*                     filename;
    uint32_t                        pool_id;

}; // struct Texture

//
//
struct ShaderEffect {

    //
    //
    struct PropertiesMap {

        char*                       key;
        hfx::ShaderEffectFile::MaterialProperty* value;

    }; // struct PropertiesMap

    struct Pass {
        PipelineCreation            pipeline_creation;
        char                        name[32];
        PipelineHandle              pipeline_handle;
        uint32_t                    pool_id;
    }; // struct Pass

    Pass*                           passes;

    uint16_t                        num_passes              = 0;
    uint16_t                        num_properties          = 0;
    uint32_t                        local_constants_size    = 0;

    char*                           local_constants_default_data = nullptr;
    char*                           properties_data         = nullptr;

    PropertiesMap*                  name_to_property        = nullptr;

    char                            name[32];
    char                            pipeline_name[32];
    uint32_t                        pool_id;

}; // struct ShaderEffect

//
//
struct ShaderInstance {

    PipelineHandle                  pipeline;
    ResourceListHandle              resource_lists[hydra::graphics::k_max_resource_layouts];

    uint32_t                        num_resource_lists;
}; // struct ShaderInstance

// Instances

static const char* s_local_constants_name = "LocalConstants";

struct MaterialFile {

    struct Property {
        char                        name[64];
        char                        data[192];
    };

    struct Binding {
        char                        name[64];
        char                        value[64];
    };

    struct Header {
        uint8_t                     num_properties;
        uint8_t                     num_bindings;
        uint8_t                     num_textures;
        char                        name[64];
        char                        hfx_filename[192];
    };

    Header*                         header;
    Property*                       property_array;
    Binding*                        binding_array;

}; // struct MaterialFile

//
//
struct Material {

    ShaderInstance*                 shader_instances        = nullptr;
    uint32_t                        num_instances           = 0;

    ShaderResourcesLookup           lookups;                            // Per-pass resource lookup. Same count as shader instances.
    ShaderEffect*                   effect                  = nullptr;

    BufferHandle                    local_constants_buffer;
    char*                           local_constants_data    = nullptr;

    const char*                     name                    = nullptr;
    StringBuffer                    loaded_string_buffer;               // TODO: replace with global string repository!
    
    uint32_t                        num_textures            = 0;
    uint32_t                        pool_id                 = 0;

    Texture**                       textures                = nullptr;

}; // struct Material

// RenderPipeline ///////////////////////////////////////////////////////////////

//
//
struct RenderStage {

    enum Type {
        Geometry, Post, PostCompute, Swapchain, Count
    };

    TextureHandle*                  input_textures          = nullptr;
    TextureHandle*                  output_textures         = nullptr;

    TextureHandle                   depth_texture;

    float                           scale_x                 = 1.0f;
    float                           scale_y                 = 1.0f;
    uint16_t                        current_width           = 1;
    uint16_t                        current_height          = 1;
    uint8_t                         num_input_textures      = 0;
    uint8_t                         num_output_textures     = 0;

    bool                            resize_output           = true;

    RenderPassHandle                render_pass;

    // TODO: pre-render graph code
    Material*                       material                = nullptr;

    float                           clear_color[4];
    uint8_t                         clear_rt                = false;
    uint8_t                         pass_index              = 0;

    Type                            type                    = Count;
    uint32_t                        pool_id                 = 0xffffffff;

    // Interface
    virtual void                    init();
    virtual void                    terminate();

    virtual void                    begin( CommandBuffer* commands );
    virtual void                    render( CommandBuffer* commands );
    virtual void                    end( CommandBuffer* commands );

    virtual void                    update_resources( ShaderResourcesDatabase& db, Device& device );
    virtual void                    resize( uint16_t width, uint16_t height, Device& device );

}; // struct RenderStage

struct RenderPipeline {

    struct StageMap {
        char*                       key;
        RenderStage*                value;
    };

    struct TextureMap {
        char*                       key;
        TextureHandle               value;
    };

    void                            init();
    void                            terminate( Device& device );

    void                            update();
    void                            render( CommandBuffer* commands );

    void                            update_resources( Device& device );
    void                            resize( uint16_t width, uint16_t height, Device& device );

    StageMap*                       name_to_stage           = nullptr;
    TextureMap*                     name_to_texture         = nullptr;

    ShaderResourcesDatabase         resource_database;
    ShaderResourcesLookup           resource_lookup;
};

//
//
struct PipelineMap {
    char* key;
    hydra::graphics::RenderPipeline* value;
};

} // namespace graphics
} // namespace hydra


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

// ResourceManager //////////////////////////////////////////////////////////////

//
//
struct ResourceType {
    enum Enum {
        Texture = 0,
        ShaderEffect,
        Material,
        Count
    }; // enum Enum
}; // struct ResourceType

//
//
struct Resource {

    struct ResourceReference {
        uint8_t                     type;
        char                        path[255];
    }; // struct ResourceReference

    struct Header {

        char                        header[7];
        uint8_t                     type;   // ResourceType::enum

        size_t                      data_size;
        uint16_t                    num_external_references;
        uint16_t                    num_internal_references;
        
    }; // struct Header

    struct ResourceMap {
        char*                       key;
        Resource*                   value;
    };

    Header*                         header;
    char*                           data;
    void*                           asset;

    Resource::ResourceReference*    external_references;
    // External
    ResourceMap*                    name_to_external_resources;
    // Interal

}; // struct Resource


struct ResourceFactory {

    struct CompileContext {

        char*                       source_file_memory;
        const char*                 source_filename;
        
        StringBuffer&               temp_string_buffer;
        
        Resource::ResourceReference* out_references;
        Resource::Header*           out_header;

        size_t                      file_size;

    }; // struct CompileContext

    struct LoadContext {

        char*                       data;
        uint32_t                    size;

        Resource*                   resource;

        hydra::graphics::Device&    device;
        hydra::graphics::PipelineMap* name_to_render_pipeline;

    }; // struct LoadContext

    virtual void                    init() {}
    virtual void                    terminate() {}

    virtual void                    compile_resource( CompileContext& context ) = 0;
    virtual void*                   load( LoadContext& context ) = 0;
    virtual void                    unload( void* resource_data, hydra::graphics::Device& device ) = 0;

    virtual void                    reload( char* data, uint32_t size, void* old_resource_data ) {}

}; //struct ResourceFactory

struct TextureFactory : public ResourceFactory {

    hydra::graphics::ResourcePool   textures_pool;

    void                            init() override;
    void                            terminate() override;

    void                            compile_resource( CompileContext& context ) override;
    void*                           load( LoadContext& context ) override;
    void                            unload( void* resource_data, hydra::graphics::Device& device ) override;
};

struct ShaderFactory : public ResourceFactory {

    hydra::graphics::ResourcePool   shaders_pool;

    void                            init() override;
    void                            terminate() override;

    void                            compile_resource( CompileContext& context ) override;
    void*                           load( LoadContext& context ) override;
    void                            unload( void* resource_data, hydra::graphics::Device& device ) override;
};

struct MaterialFactory : public ResourceFactory {

    hydra::graphics::ResourcePool   materials_pool;

    void                            init() override;
    void                            terminate() override;

    void                            compile_resource( CompileContext& context ) override;
    void*                           load( LoadContext& context ) override;
    void                            unload( void* resource_data, hydra::graphics::Device& device ) override;
};

//
//
struct ResourceManager {

    struct ResourceMap {
        char*                       key;
        Resource*                   value;
    };

    void                            init();
    void                            terminate( hydra::graphics::Device& gfx_device );

    Resource*                       compile_resource( ResourceType::Enum type, const char* filename, StringBuffer& temp_string_buffer );
    Resource*                       load_resource( ResourceType::Enum type, const char* filename, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::PipelineMap* name_to_render_pipeline );

    void                            save_resource( Resource& resource );
    void                            unload_resource( Resource** resource, hydra::graphics::Device& gfx_device );

    ResourceMap*                    name_to_resources;

    ResourceFactory*                resource_factories[ResourceType::Count];

}; // struct ResourceManager

// MaterialSystemApplication ////////////////////////////////////////////////////

struct EditorMaterial {

    Resource*                       material_resource;
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

    void                            app_init()              override;
    void                            app_terminate()         override;
    void                            app_render( hydra::graphics::CommandBuffer* commands ) override;
    void                            app_resize( uint16_t width, uint16_t height ) override;

    // Actions
    void                            edit_file( const char* filepath );
    void                            file_action_popup_render( const char* filename );
    void                            create_material( const char* filename );
    void                            load_material( const char* filename );
    void                            save_material( const char* filename );
    void                            compile_hfx( const char* filename );

    bool                            show_demo_window        = false;

    hydra::graphics::PipelineMap*   name_to_render_pipeline = nullptr;

    ResourceManager                 resource_manager;

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


