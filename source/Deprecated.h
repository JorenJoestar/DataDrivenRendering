#pragma once

/*







/////////////////////////////////////////////////////////////////////////////////
//
// Constant buffer code prototype.
// Generated code will look like this.
//
struct LocalConstantsUI {
    float                           scale = 32.0f;
    float                           modulo = 2.0f;

    void reflectMembers() {
        ImGui::InputScalar( "scale", ImGuiDataType_Float, &scale );
        ImGui::InputScalar( "modulo", ImGuiDataType_Float, &modulo );
    }

    void reflectUI() {
        ImGui::Begin( "LocalConstants" );
        reflectMembers();
        ImGui::End();
    }
};

struct LocalConstants {

    float                           scale = 32.0f;
    float                           modulo = 2.0f;

    float                           pad[2];
};

struct LocalConstantsBuffer {

    hydra::graphics::BufferHandle   buffer;
    LocalConstants                  constants;
    LocalConstantsUI                constantsUI;

    void create( hydra::graphics::Device& device ) {

        using namespace hydra;

        graphics::BufferCreation constants_creation = { graphics::BufferType::Constant, graphics::ResourceUsageType::Dynamic, sizeof( LocalConstants ), &constants, "LocalConstants" };
        buffer = device.create_buffer( constants_creation );
    }

    void destroy( hydra::graphics::Device& device ) {

        device.destroy_buffer( buffer );
    }

    void updateUI( hydra::graphics::Device& device ) {
        // Draw UI
        constantsUI.reflectUI();

        // TODO:
        // Ideally there should be a way to tell if a variable has changed and update only in that case.

        // Map buffer to GPU and upload parameters from the UI
        hydra::graphics::MapBufferParameters map_parameters = { buffer.handle, 0, 0 };
        LocalConstants* buffer_data = (LocalConstants*)device.map_buffer( map_parameters );
        if ( buffer_data ) {
            buffer_data->scale = constantsUI.scale;
            buffer_data->modulo = constantsUI.modulo;
            device.unmap_buffer( map_parameters );
        }
    }
};

// FrameGraph bottom-down approach.
// Code is unclear and horrible.
// Starting from my already existing implementation would be a better idea.

hydra_graphics.h

// RenderGraph //////////////////////////////////////////////////////////////////

//
// Struct holding input and output binding for a render pass.
//
struct RenderPassBindings {

    ShaderBinding                   inputs[16];
    ShaderBinding                   outputs[16];

    uint32_t                        num_inputs          = 0;
    uint32_t                        num_outputs         = 0;

    const char*                     get_resource_name( const char* binding_name ) const;

}; // struct RenderPassBindings

//
//
struct ComputePassCreation {

    const ShaderCreation*           shader_creation     = nullptr;
    RenderPassBindings*             bindings            = nullptr;

    ResourceListLayoutHandle        resource_list_layout_handle;

    uint32_t                        dispatch_size_x     = 1;
    uint32_t                        dispatch_size_y     = 1;
    uint32_t                        dispatch_size_z     = 1;

}; // struct ComputePassCreation

//
//
struct FullscreenPassCreation {
    const ShaderCreation*           shader_creation     = nullptr;
    RenderPassBindings*             bindings            = nullptr;

    ResourceListLayoutHandle        resource_list_layout_handle;
    uint8_t                         clear_color_flag    = false;
};

//
//
struct RenderGraph {


    TextureHandle                   add_texture( const TextureCreation& creation, Device& device );

    void                            add_compute_pass( const ComputePassCreation& creation, Device& device );
    void                            add_fullscreen_pass( const FullscreenPassCreation& creation, Device& device );

    // Register resources methods
    void                            register_buffer( BufferHandle buffer, Device& device );

    // Main interface
    void                            render( CommandBuffer* commands, Device& device );

    void                            reload( Device& device );

    RenderManager*                  manager             = nullptr;
#if defined (HYDRA_OPENGL)


    // Retrieve resources
    BufferGL*                       get_buffer( const char* name );
    TextureGL*                      get_texture( const char* name );

    RenderPassGL**                  render_passes       = nullptr;
    TextureGL**                     textures            = nullptr;
    BufferGL**                      buffers             = nullptr;

    uint16_t                        num_textures        = 0;
    uint16_t                        max_textures        = 0;
    uint16_t                        num_render_passes   = 0;
    uint16_t                        max_render_passes   = 0;

    uint16_t                        num_buffers         = 0;
    uint16_t                        max_buffers         = 0;
#endif

}; // struct RenderGraph


// hydra_graphics.cpp ///////////////////////////////////////////////////////////

// RenderGraph //////////////////////////////////////////////////////////////////

const char* RenderPassBindings::get_resource_name( const char* binding_name ) const {

    for ( uint32_t i = 0; i < num_inputs; ++i ) {
        if ( strcmp( inputs[i].binding_name, binding_name ) == 0 ) {
            return inputs[i].resource_name;
            break;
        }
    }

    for ( uint32_t i = 0; i < num_outputs; ++i ) {
        if ( strcmp( outputs[i].binding_name, binding_name ) == 0 ) {
            return outputs[i].resource_name;
            break;
        }
    }
    return nullptr;
}


TextureHandle RenderGraph::add_texture( const TextureCreation& creation, Device& device ) {

    if ( !this->textures ) {
        max_textures = 8;
        num_textures = 0;
        this->textures = new TextureGL*[max_textures];
    }

    if ( num_textures == max_textures - 1 ) {
        max_textures *= 2;

        TextureGL** new_textures = new TextureGL*[max_textures];
        memcpy( new_textures, textures, sizeof( TextureGL* ) * num_textures);

        delete[] textures;

        textures = new_textures;
    }

    TextureHandle new_texture = device.create_texture( creation );
    TextureGL* native_texture = device.access_texture( new_texture );

    textures[num_textures++] = native_texture;

    return new_texture;
}

void RenderGraph::add_fullscreen_pass( const FullscreenPassCreation& creation, Device& device ) {
    if ( !this->render_passes ) {
        max_render_passes = 8;
        num_render_passes = 0;

        this->render_passes = new RenderPassGL*[max_render_passes];
    }

    RenderPassCreation render_pass_creation = {};
    // Render pass is direct to the swapchain if there are no outputs.
    render_pass_creation.is_swapchain = creation.bindings->num_outputs == 0;

    RenderPassHandle render_pass_handle = device.create_render_pass( render_pass_creation );
    RenderPassGL* render_pass = device.access_render_pass( render_pass_handle );

    // Create fullscreen pipeline
    PipelineCreation pipeline_creation = {};
    pipeline_creation.shaders = creation.shader_creation;
    pipeline_creation.resource_layout = creation.resource_list_layout_handle;
    pipeline_creation.compute = false;
    PipelineHandle pipeline_handle = device.create_pipeline( pipeline_creation );

    // Fill Render Pass struct
    render_pass->pipeline = device.access_pipeline( pipeline_handle );
    render_pass->bindings = *creation.bindings;
    render_pass->fullscreen_vertex_buffer = device.get_fullscreen_vertex_buffer();
    render_pass->clear_color = creation.clear_color_flag;
    render_pass->fullscreen = true;

    render_passes[num_render_passes++] = render_pass;
}

void RenderGraph::add_compute_pass( const ComputePassCreation& creation, Device& device ) {

    if ( !this->render_passes ) {
        max_render_passes = 8;
        num_render_passes = 0;

        this->render_passes = new RenderPassGL*[max_render_passes];
    }

    // Retrieve texture
    TextureGL* output_texture = get_texture( creation.bindings->outputs[0].resource_name );

    // Create pass
    RenderPassCreation render_pass_creation = {};
    render_pass_creation.is_compute_post = 1;
    render_pass_creation.output = output_texture->handle;

    RenderPassHandle render_pass_handle = device.create_render_pass( render_pass_creation );
    RenderPassGL* render_pass = device.access_render_pass( render_pass_handle );

    // Create compute pipeline
    PipelineCreation pipeline_creation = {};
    pipeline_creation.shaders = creation.shader_creation;
    pipeline_creation.resource_layout = creation.resource_list_layout_handle;
    pipeline_creation.compute = true;

    PipelineHandle pipeline_handle = device.create_pipeline( pipeline_creation );
    render_pass->pipeline = device.access_pipeline( pipeline_handle );

    // Setup dispatch size
    render_pass->dispatch_x = output_texture->width / creation.dispatch_size_x;
    render_pass->dispatch_y = output_texture->height / creation.dispatch_size_y;
    render_pass->dispatch_z = output_texture->depth / creation.dispatch_size_z;

    render_pass->bindings = *creation.bindings;

    render_passes[num_render_passes++] = render_pass;
}

void RenderGraph::register_buffer( BufferHandle handle, Device& device ) {

    if ( !this->buffers ) {
        max_buffers = 8;
        num_buffers = 0;

        buffers = new BufferGL*[max_buffers];
    }

    BufferGL* buffer = device.access_buffer( handle );
    buffers[num_buffers++] = buffer;
}

void RenderGraph::render( CommandBuffer* commands, Device& device ) {

    for ( uint32_t i = 0; i < num_render_passes; ++i ) {
        RenderPassGL* pass = render_passes[i];

        // Graphics pipelines: either fullscreen or standard
        if ( pass->pipeline->graphics_pipeline ) {

            if ( pass->fullscreen ) {
                commands->begin_submit( 1 );

                Viewport viewport = {};
                viewport.rect.width = pass->is_swapchain ? device.swapchain_width : 1;
                viewport.rect.height = pass->is_swapchain ? device.swapchain_height : 1;
                commands->set_viewport( viewport );
                if ( pass->clear_color ) {
                    commands->clear( 0, 0, 0, 0 );
                }

                commands->bind_pipeline( pass->pipeline->handle );
                commands->bind_resource_list( pass->resource_list_handle );
                commands->bind_vertex_buffer( pass->fullscreen_vertex_buffer );
                commands->draw( hydra::graphics::TopologyType::Triangle, 0, 3 );
                commands->end_submit();

                if ( manager ) {
                    manager->render( commands, device );
                }
            }
            else {

                // Begin

                // Render
                if ( manager ) {
                    manager->render( commands, device );
                }

                // End
            }
        }
        else {
            commands->begin_submit( 0 );
            commands->bind_pipeline( pass->pipeline->handle );
            commands->bind_resource_list( pass->resource_list_handle );
            commands->dispatch( pass->dispatch_x, pass->dispatch_y, pass->dispatch_z );
            commands->end_submit();
        }
    }
}

BufferGL* RenderGraph::get_buffer( const char* name ) {
    for ( uint32_t c = 0; c < num_buffers; ++c ) {
        BufferGL* buffer = buffers[c];

        if ( strcmp( buffer->name, name ) == 0 ) {
            return buffer;
            break;
        }
    }
}

TextureGL* RenderGraph::get_texture( const char* name ) {
    for ( uint32_t c = 0; c < num_textures; ++c ) {
        TextureGL* texture = textures[c];

        if ( strcmp( texture->name, name ) == 0 ) {
            return texture;
            break;
        }
    }
}

static ResourceListHandle create_resource_list( Device& device, const ResourceListLayoutGL& resource_layout, RenderGraph& graph, RenderPassBindings& bindings ) {
    // Search resources
    ResourceListCreation::Resource* resource_handles = new ResourceListCreation::Resource[resource_layout.num_bindings];

    for ( size_t i = 0; i < resource_layout.num_bindings; ++i ) {

        const ResourceBindingGL& binding = resource_layout.bindings[i];

        // Search resource name for this binding
        bool found = false;
        const char* resource_name = bindings.get_resource_name( binding.name );

        switch ( binding.type ) {
            case ResourceType::Constants:
            {
                BufferGL* buffer = graph.get_buffer( binding.name );
                if ( buffer )
                    resource_handles[i].handle = buffer->handle.handle;

                break;
            }

            case ResourceType::Texture:
            case ResourceType::TextureRW:
            {
                TextureGL* texture = graph.get_texture( resource_name );
                if ( texture )
                    resource_handles[i].handle = texture->handle.handle;

                break;
            }
        }
    }

    ResourceListCreation resource_list_creation = { resource_layout.handle, resource_handles, resource_layout.num_bindings };
    // Create resource list
    ResourceListHandle resource_list_handle = device.create_resource_list( resource_list_creation );

    delete[] resource_handles;

    return resource_list_handle;
}

void RenderGraph::reload( Device& device ) {

    for ( uint32_t i = 0; i < num_render_passes; ++i ) {
        RenderPassGL* pass = render_passes[i];

        pass->resource_list_handle = create_resource_list( device, *pass->pipeline->resource_list_layout, *this, pass->bindings );
    }

    if ( manager ) {
        manager->reload( device );
    }
}

// Example usage:
// main.cpp ///////////////////////////////////////////////////////////////////// 
//
//
//
static hydra::graphics::RenderGraph render_graph;

struct UIRenderManager : public hydra::graphics::RenderManager {


    void                    render( hydra::graphics::CommandBuffer* commands, hydra::graphics::Device& device ) override {
        hydra_imgui_collect_draw_data( ImGui::GetDrawData(), device, *commands );
    }

    void                    reload( hydra::graphics::Device& device ) override {

    }

}; // struct UIRenderManager

static void create_render_graph( hydra::graphics::Device& gfx_device, hydra::graphics::BufferHandle constants ) {
    // TODO: load this from file.
    using namespace hydra::graphics;

    // Create resources ///////////////////////////////////////
    hydra::graphics::TextureCreation first_rt = {};
    first_rt.width = 512;
    first_rt.height = 512;
    first_rt.render_target = 1;
    first_rt.format = hydra::graphics::TextureFormat::R8G8B8A8_UNORM;
    first_rt.name = "CheckerTexture";

    TextureHandle texture = render_graph.add_texture( first_rt, gfx_device );

    render_graph.register_buffer( constants, gfx_device );

    // Add compute pass //////////////////////////////////////
    // Load compute shader post-process
    char* hfx_file_memory = read_file_into_memory( "..\\data\\SimpleFullscreen.bhfx", nullptr );
    ResourceListLayoutHandle compute_resource_layout;
    ShaderCreation compute_shader = {};
    compile_shader_effect_pass( gfx_device, hfx_file_memory, 0, compute_shader, compute_resource_layout );

    RenderPassBindings bindings;
    bindings.outputs[0].binding_name = "destination_texture";
    bindings.outputs[0].resource_name = first_rt.name;
    bindings.num_outputs = 1;

    ComputePassCreation compute_pass_creation = { &compute_shader, &bindings, compute_resource_layout, 32, 32, 1 };
    render_graph.add_compute_pass( compute_pass_creation, gfx_device );

    // Add swapchain pass ////////////////////////////////////
    // Reuse binding struct
    bindings.num_outputs = 0;
    bindings.inputs[0].binding_name = "input_texture";
    bindings.inputs[0].resource_name = first_rt.name;
    bindings.num_inputs = 1;

    ResourceListLayoutHandle fullscreen_resource_layout;
    ShaderCreation fullscreen_shader = {};
    compile_shader_effect_pass( gfx_device, hfx_file_memory, 1, fullscreen_shader, fullscreen_resource_layout );

    FullscreenPassCreation fullscreen_pass_creation = { &fullscreen_shader, &bindings, fullscreen_resource_layout, 1 };
    render_graph.add_fullscreen_pass( fullscreen_pass_creation, gfx_device );

    // Create manager for UI
    UIRenderManager* ui_render_manager = new UIRenderManager();
    render_graph.manager = ui_render_manager;

    // Init graph
    render_graph.reload( gfx_device );

}
*/

*/