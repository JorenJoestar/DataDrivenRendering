#pragma once

//
//  Hydra Resources - v0.01
//
//  Simple Resource Manager for Hydra.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2020/02/10, 21.16
//
//
// Revision history //////////////////////
//      0.01 (2020/02/10): + Initial version. Moved resource managers from MaterialSystem application.

#include "hydra/hydra_lib.h"
#include "hydra/hydra_rendering.h"

namespace hydra {

struct ResourceMap;
struct ResourceManager;

//
//
struct ResourceType {

    enum Enum {

        Texture = 0,
        ShaderEffect,
        Material,
        //Scene,

        Count
    }; // enum Enum
}; // struct ResourceType


//
//
struct ResourceID {
    uint8_t                         type;
    char                            path[255];
}; // struct ResourceReference

struct ResourceHeader {

    char                            header[7];
    ResourceID                      id;

    size_t                          source_hash;
    size_t                          data_size;
    uint16_t                        num_external_references;
    uint16_t                        num_internal_references;

}; // struct ResourceHeader

//
//
struct Resource {

    
    ResourceHeader*                 header;
    char*                           data;
    void*                           asset;

    ResourceID*                     external_references;
    // External
    ResourceMap*                    name_to_external_resources;

}; // struct Resource


//
//
struct ResourceMap {
    char*                           key;
    Resource*                       value;
}; // struct ResourceMap


struct ResourceFactory {

    struct CompileContext {

        char*                       source_file_memory;
        const char*                 compiled_filename;

        StringBuffer&               temp_string_buffer;

        ResourceID*                 out_references;
        ResourceHeader*             out_header;

        ResourceManager*            resource_manager;

    }; // struct CompileContext

    struct LoadContext {

        Resource*                   resource;

        hydra::graphics::Device&    device;
        hydra::graphics::RenderPipeline* render_pipeline;

    }; // struct LoadContext

    virtual void                    init() {}
    virtual void                    terminate() {}

    virtual void                    compile_resource( CompileContext& context ) = 0;
    virtual void*                   load( LoadContext& context ) = 0;

    virtual void                    unload( void* resource_data, hydra::graphics::Device& device ) = 0;

    virtual void                    reload( Resource* old_resource, Resource* new_resource, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {}

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

    void                            reload( Resource* old_resource, Resource* new_resource, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) override;
};

struct MaterialFactory : public ResourceFactory {

    hydra::graphics::ResourcePool   materials_pool;

    void                            init() override;
    void                            terminate() override;

    void                            compile_resource( CompileContext& context ) override;
    void*                           load( LoadContext& context ) override;
    void                            unload( void* resource_data, hydra::graphics::Device& device ) override;

    void                            reload( Resource* old_resource, Resource* new_resource, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) override;
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

    Resource*                       compile_resource( ResourceType::Enum type, const char* filename );
    Resource*                       load_resource( ResourceType::Enum type, const char* filename, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline );
    void                            init_resource( Resource** resource, char* memory, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline );

    void                            reload_resources( ResourceType::Enum type, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline );
    void                            reload_resource( Resource* resource, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline );

    void                            save_resource( Resource& resource );
    void                            unload_resource( Resource** resource, hydra::graphics::Device& gfx_device );

    const char*                     get_resource_source_folder() { return resource_source_folder.data; }
    const char*                     get_resource_binary_folder() { return resource_binary_folder.data; }

    ResourceMap*                    name_to_resources;

    ResourceFactory*                resource_factories[ResourceType::Count];

    hydra::StringBuffer             resource_source_folder;
    hydra::StringBuffer             resource_binary_folder;

    hydra::StringBuffer             temporary_string_buffer;    // Used to concatenate names and such.

}; // struct ResourceManager

} // namespace hydra