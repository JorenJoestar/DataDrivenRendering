#include "MaterialSystem.h"

#include "imgui/TextEditor.h"
#include "imgui/imgui_memory_editor.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "stb_leakcheck.h"



// MaterialSystemApplication ////////////////////////////////////////////////////

// Callbacks
static void material_system_open_file_callback( void* user_data, uint8_t button, const char* filename ) {
    ((MaterialSystemApplication*)user_data)->edit_file( filename );
}

static void material_system_popup_callback( void* user_data, const char* filename ) {
    ((MaterialSystemApplication*)user_data)->file_action_popup_render( filename );
}

void MaterialSystemApplication::app_init() {

    using namespace hydra;

    render_stage_pool.init( 128, sizeof( graphics::RenderStage ) );

    text_editor = new TextEditor();
    memory_editor = new MemoryEditor();

    resource_manager.init();

    parsing_string_buffer.init( 10000 );
    ui_string_buffer.init( 100000 );

    material_filename.init( 512 );
    opened_file_path.init( 512 );

    file_browser.init();
    file_browser.open_folder( "..\\data\\" );
    file_browser.set_single_click_callback( material_system_open_file_callback, this );
    file_browser.set_popup_showing_callback( material_system_popup_callback, this );

    choose_file_browser.init();
    choose_file_browser.open_folder( "..\\data\\" );

    string_hash_init_dynamic( name_to_render_pipeline );

    // 1) Local resources vs Global resources (defined in the DB)
    // 2) Material creation. Lookup
    // 3) Resource set creation from the layout.
    //      How to connect a resource to its layout position ?
    //      Reflection/like system

    // Example of 
    //final ShaderLookup bindings = new ShaderBindings();
    //bindings.bindingToResourceMap.put( "albedoMap", tileData.assetName );

    //bindings.bindingToSpecialization.put( "FrameConstants", Frame );
    //bindings.bindingToSpecialization.put( "ViewConstants", View );
    // specialization Pass!!!

    // Create shadertoy pipeline
    {
        // 1. Init pipeline internal structures
        graphics::RenderPipeline* render_pipeline = ( graphics::RenderPipeline* )malloc( sizeof( graphics::RenderPipeline ) );
        graphics::RenderPipeline& shadertoy_render_pipeline = *render_pipeline;
        shadertoy_render_pipeline.init( nullptr );

        // 2. Populate resources
        char texture_name[32] = "pass0_output_texture";
        graphics::TextureCreation rt = {};
        rt.width = gfx_device.swapchain_width;
        rt.height = gfx_device.swapchain_height;
        rt.render_target = 1;
        rt.format = graphics::TextureFormat::R8G8B8A8_UNORM;
        rt.name = texture_name;

        graphics::TextureHandle handle = gfx_device.create_texture( rt );
        string_hash_put( shadertoy_render_pipeline.name_to_texture, texture_name, handle );
        shadertoy_render_pipeline.resource_database.register_texture( texture_name, handle );

        char in_name[32] = "input_texture";
        shadertoy_render_pipeline.resource_lookup.add_binding_to_resource( in_name, texture_name );


        graphics::BufferCreation checker_constants_creation = {};
        checker_constants_creation.type = graphics::BufferType::Constant;
        checker_constants_creation.name = "ShaderToyConstants";
        checker_constants_creation.usage = graphics::ResourceUsageType::Dynamic;
        checker_constants_creation.size = 16;
        checker_constants_creation.initial_data = nullptr;

        shadertoy_buffer = gfx_device.create_buffer( checker_constants_creation );
        shadertoy_render_pipeline.resource_lookup.add_binding_to_resource( (char*)checker_constants_creation.name, (char*)checker_constants_creation.name );
        shadertoy_render_pipeline.resource_database.register_buffer( (char*)checker_constants_creation.name, shadertoy_buffer );

        // 3. Add stages
        // TODO: for now just one pass.
        uint32_t render_stage_pool_id = render_stage_pool.obtain_resource();
        graphics::RenderStage* pass0_stage = new(( graphics::RenderStage* )render_stage_pool.access_resource( render_stage_pool_id ))graphics::RenderStage();
        pass0_stage->type = graphics::RenderStage::Post;
        pass0_stage->pool_id = render_stage_pool_id;
        pass0_stage->num_input_textures = 0;
        pass0_stage->num_output_textures = 1;
        pass0_stage->output_textures = new graphics::TextureHandle[1];
        pass0_stage->output_textures[0] = string_hash_get( shadertoy_render_pipeline.name_to_texture, texture_name );
        pass0_stage->resize_output = 1;
        char stage_name[32] = "pass0";
        pass0_stage->init();
        string_hash_put( shadertoy_render_pipeline.name_to_stage, stage_name, pass0_stage );

        render_stage_pool_id = render_stage_pool.obtain_resource();
        graphics::RenderStage* final_stage = new( ( graphics::RenderStage* )render_stage_pool.access_resource( render_stage_pool_id ) )graphics::RenderStage();
        final_stage->pool_id = render_stage_pool_id;
        final_stage->type = graphics::RenderStage::Swapchain;
        final_stage->num_input_textures = 1;
        final_stage->input_textures = new graphics::TextureHandle[1];
        final_stage->input_textures[0] = string_hash_get( shadertoy_render_pipeline.name_to_texture, texture_name );
        final_stage->num_output_textures = 0;
        final_stage->resize_output = 1;
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( shadertoy_render_pipeline.name_to_stage, final_stage_name, final_stage );


        // 4. Update resources - actually link stage resources
        shadertoy_render_pipeline.load_resources( gfx_device );

        char pipeline_name[32] = "ShaderToy";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );
    }

    // Create custom compute pipeline
    {
        graphics::RenderPipeline* render_pipeline = ( graphics::RenderPipeline* )malloc( sizeof( graphics::RenderPipeline ) );
        graphics::RenderPipeline& compute_post_render_pipeline = *render_pipeline;
        compute_post_render_pipeline.init( nullptr );

        char texture_name[32] = "compute_output_texture";
        graphics::TextureCreation rt = {};
        rt.width = gfx_device.swapchain_width;
        rt.height = gfx_device.swapchain_height;
        rt.render_target = 1;
        rt.format = graphics::TextureFormat::R8G8B8A8_UNORM;
        rt.name = texture_name;

        graphics::TextureHandle render_target = gfx_device.create_texture( rt );
        string_hash_put( compute_post_render_pipeline.name_to_texture, texture_name, render_target );
        compute_post_render_pipeline.resource_database.register_texture( texture_name, render_target );

        char out_name[32] = "destination_texture";
        char in_name[32] = "input_texture";
        compute_post_render_pipeline.resource_lookup.add_binding_to_resource( out_name, texture_name );
        compute_post_render_pipeline.resource_lookup.add_binding_to_resource( in_name, texture_name );

        // Create stages
        uint32_t render_stage_pool_id = render_stage_pool.obtain_resource();
        graphics::RenderStage* pass0_stage = new( ( graphics::RenderStage* )render_stage_pool.access_resource( render_stage_pool_id ) )graphics::RenderStage();
        pass0_stage->type = graphics::RenderStage::PostCompute;
        pass0_stage->pool_id = render_stage_pool_id;
        pass0_stage->num_input_textures = 0;
        pass0_stage->num_output_textures = 1;
        pass0_stage->resize_output = 1;
        pass0_stage->output_textures = new graphics::TextureHandle[1];
        pass0_stage->output_textures[0] = string_hash_get( compute_post_render_pipeline.name_to_texture, texture_name );
        char stage_name[32] = "compute0";
        pass0_stage->init();
        string_hash_put( compute_post_render_pipeline.name_to_stage, stage_name, pass0_stage );

        render_stage_pool_id = render_stage_pool.obtain_resource();
        graphics::RenderStage* final_stage = new( ( graphics::RenderStage* )render_stage_pool.access_resource( render_stage_pool_id ) )graphics::RenderStage();
        final_stage->pool_id = render_stage_pool_id;
        final_stage->type = graphics::RenderStage::Swapchain;
        final_stage->num_input_textures = 1;
        final_stage->input_textures = new graphics::TextureHandle[1];
        final_stage->input_textures[0] = string_hash_get( compute_post_render_pipeline.name_to_texture, texture_name );
        final_stage->num_output_textures = 0;
        final_stage->resize_output = 1;
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( compute_post_render_pipeline.name_to_stage, final_stage_name, final_stage );

        compute_post_render_pipeline.load_resources( gfx_device );

        char pipeline_name[32] = "computeTest";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );
    }

    {
        graphics::RenderPipeline* render_pipeline = ( graphics::RenderPipeline* )malloc( sizeof( graphics::RenderPipeline ) );
        render_pipeline->init( nullptr );

        uint32_t render_stage_pool_id = render_stage_pool.obtain_resource();
        graphics::RenderStage* final_stage = new( ( graphics::RenderStage* )render_stage_pool.access_resource( render_stage_pool_id ) )graphics::RenderStage();
        final_stage->pool_id = render_stage_pool_id;
        final_stage->type = graphics::RenderStage::Swapchain;
        final_stage->num_input_textures = 0;
        final_stage->num_output_textures = 0;
        final_stage->clear_rt = 1;
        final_stage->clear_color[0] = 0.45f;
        final_stage->clear_color[1] = 0.05f;
        final_stage->clear_color[2] = 0.00f;
        final_stage->clear_color[3] = 1.0f;
        final_stage->resize_output = 1;
        
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( render_pipeline->name_to_stage, final_stage_name, final_stage );

        render_pipeline->load_resources( gfx_device );
        char pipeline_name[32] = "swapchain";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );

        current_render_pipeline = render_pipeline;
    }

    editor_material.material = nullptr;

    load_material( "StarNest.hmt" );
}

void MaterialSystemApplication::app_terminate() {

    // Delete render pipelines
    for ( size_t i = 0; i < string_hash_length(name_to_render_pipeline); i++ ) {
        hydra::graphics::RenderPipeline* render_pipeline = name_to_render_pipeline[i].value;
        render_pipeline->terminate( gfx_device );
        free( render_pipeline );
    }

    render_stage_pool.terminate();

    resource_manager.terminate( gfx_device );

    ui_string_buffer.terminate();
    parsing_string_buffer.terminate();

    material_filename.terminate();
    opened_file_path.terminate();

    file_browser.terminate();
    choose_file_browser.terminate();

    delete text_editor;
    delete memory_editor;
}

void MaterialSystemApplication::app_render( hydra::graphics::CommandBuffer* commands ) {
    using namespace hydra;

    if ( current_render_pipeline ) {
        current_render_pipeline->render( gfx_device, commands );
    }

    if ( show_demo_window )
        ImGui::ShowDemoWindow( &show_demo_window );

    ui_string_buffer.clear();

    file_browser.draw_window("Main File Browser");

    uint32_t changed_texture_index = 0xffffffff;

    ImGui::Begin( "Material" );

    if ( editor_material.material ) {
        ImGui::Text( "%s", editor_material.material->name );
        ImGui::Text( "%s", material_filename.data );
        ImGui::Separator();

        if ( ImGui::Button( "Load" ) ) {
            // Cache material filename
            char material_filename_cache[512];
            strcpy( material_filename_cache, material_filename.data );
            load_material( material_filename_cache );
        }
        ImGui::SameLine();
        if ( ImGui::Button( "Save" ) ) {
            char material_filename_cache[512];
            strcpy( material_filename_cache, material_filename.data );
            save_material( material_filename_cache );
        }

        bool property_changed = false;

        uint32_t current_texture = 0;
        for ( uint32_t p = 0; p < editor_material.material->effect->num_properties; ++p ) {
            hfx::ShaderEffectFile::MaterialProperty* property = hfx::get_property( editor_material.material->effect->properties_data, p );

            switch ( property->type ) {
                case hfx::Property::Float:
                {
                    property_changed = property_changed || ImGui::InputScalar( property->name, ImGuiDataType_Float, editor_material.material->local_constants_data + property->offset );
                    break;
                }

                case hfx::Property::Texture2D:
                {
                    ImGui::Text( property->name );
                    ImGui::SameLine();
                    ImGui::Text( editor_material.material->textures[current_texture]->filename);
                    ImGui::SameLine();
                    if ( ImGui::Button( "Open File" ) ) {
                        ImGui::OpenPopup( "Choose File" );
                    }
                    if ( ImGui::BeginPopupModal( "Choose File" ) ) {
                        if ( ImGui::Button( "Choose", ImVec2( 120, 0 ) ) ) {
                            changed_texture_index = current_texture;
                            ImGui::CloseCurrentPopup();
                        }
                        choose_file_browser.open_folder( "..\\data\\", ".png" );
                        choose_file_browser.draw_contents();

                        ImGui::EndPopup();
                    }

                    ++current_texture;
                    
                    break;
                }

                default:
                {
                    ImGui::Text( property->name );
                    break;
                }
            }
        }

        // If we changed a property, update the constant buffer.
        if ( property_changed ) {

            hydra::graphics::MapBufferParameters map_parameters = { editor_material.material->local_constants_buffer, 0, 0 };

            void* buffer_data = gfx_device.map_buffer( map_parameters );
            if ( buffer_data ) {
                memcpy( buffer_data, editor_material.material->local_constants_data, editor_material.material->effect->local_constants_size );
                gfx_device.unmap_buffer( map_parameters );
            }
        }
    }

    ImGui::End();


    // Text editor
    {
        ImGui::Begin( "Text Editor" );
        ImGui::Text( opened_file_path.current_size ? opened_file_path.data : "No File Opened" );
        ImGui::Separator();
        bool save_pressed = ImGui::Button( "Save File" );
        ImGui::SameLine();
        bool load_pressed = ImGui::Button( "Load File" );
        ImGui::Separator();

        if ( opened_file_type == Binary_HFX || opened_file_type == Binary ) {
            memory_editor->DrawContents( file_text, file_size );
        }
        else if ( text_editor->GetText().length() > 0 ) {
            // Track if at least once there were changes to the file.
            // IsTextChanged is reset every time the text_editor is rendered.
            file_save_changes = file_save_changes || text_editor->IsTextChanged();
            if ( file_save_changes && save_pressed ) {
                // Save file
                {
                    ScopedFile updated_file( file_browser.current_filename.data, "wb" );
                    fwrite( text_editor->GetText().c_str(), text_editor->GetText().length(), 1, updated_file._file );
                }

                file_save_changes = false;
            }

            if ( load_pressed ) {
                edit_file( file_browser.current_filename.data );
            }

            text_editor->Render( file_browser.current_filename.data );

        }

        ImGui::End();
    }

    // BHFX inpsector
    ImGui::Begin( "BHFX Inspector" );
    if ( opened_file_type == Binary_HFX ) {

        ImGui::Text( shader_effect_file.header->name );
        ImGui::Text( "Num passes %u", shader_effect_file.header->num_passes );
        
        for ( uint16_t p = 0; p < shader_effect_file.header->num_passes; p++ ) {
            if ( ImGui::TreeNode( (void*)(intptr_t)p, "Pass %d", p ) ) {
                hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file.memory, p );
                ImGui::Text( "Pass %s", pass_header->name );
                ImGui::Text( "Resources");
                for ( uint16_t l = 0; l < pass_header->num_resource_layouts; l++ ) {

                    ImGui::Text( "Resource Layout %d", l );

                    uint8_t num_bindings = 0;
                    const hydra::graphics::ResourceListLayoutCreation::Binding* bindings = get_pass_layout_bindings( pass_header, l, num_bindings );

                    for ( size_t i = 0; i < num_bindings; i++ ) {
                        const hydra::graphics::ResourceListLayoutCreation::Binding& binding = bindings[i];
                        ImGui::Text( binding.name );
                        ImGui::SameLine();
                        ImGui::Text( hydra::graphics::ResourceType::ToString( binding.type ) );
                    }

                    ImGui::Separator();
                }

                ImGui::Separator();
                ImGui::Text( "Shaders" );
                uint32_t shader_count = pass_header->num_shader_chunks;

                for ( uint16_t i = 0; i < shader_count; i++ ) {
                    hydra::graphics::ShaderCreation::Stage stage;
                    hfx::get_shader_creation( pass_header, i, &stage);

                    ImGui::Text( stage.code );
                }

                ImGui::TreePop();
            }
        }
    }
    ImGui::End();

    ImGui::Begin( "Render Pipeline" );
    if ( current_render_pipeline ) {
        ImGui::Text( "Stages" );

        for ( size_t i = 0; i < string_hash_length(current_render_pipeline->name_to_stage); i++ ) {
            const hydra::graphics::RenderPipeline::StageMap& render_stage_struct = current_render_pipeline->name_to_stage[i];
            hydra::graphics::RenderStage* render_stage = render_stage_struct.value;
            
            ImGui::Text( render_stage_struct.key );
            for ( size_t rt = 0; rt < render_stage->num_output_textures; rt++ ) {
                ImGui::Image( (ImTextureID)(&render_stage->output_textures[rt]), ImVec2(128,128) );
            }
        }
    }
    ImGui::End();

    // Update Shadertoy constant buffer
    hydra::graphics::MapBufferParameters map_parameters = { shadertoy_buffer, 0, 0 };

    float* buffer_data = (float*)gfx_device.map_buffer( map_parameters );
    if ( buffer_data ) {
        buffer_data[0] = gfx_device.swapchain_width;
        buffer_data[1] = gfx_device.swapchain_height;
        static float time = 0.0f;
        time += 0.016f;

        buffer_data[2] = time;

        gfx_device.unmap_buffer( map_parameters );
    }

    // Update texture
    // TODO:
    //if ( changed_texture_index != 0xffffffff && choose_file_browser.current_filename.current_size ) {
    //    const char* texture_path = editor_material.material->loaded_string_buffer.append_use( choose_file_browser.current_filename.data );
    //    int image_width, image_height, channels_in_file;
    //    unsigned char* my_image_data = stbi_load( texture_path, &image_width, &image_height, &channels_in_file, 4 );
    //    hydra::graphics::TextureCreation texture_creation = { my_image_data, image_width, image_height, 1, 1, 0, hydra::graphics::TextureFormat::R8G8B8A8_UNORM, hydra::graphics::TextureType::Texture2D };
    //    hydra::graphics::TextureHandle texture_handle = gfx_device.create_texture( texture_creation );

    //    if ( texture_handle.handle != hydra::graphics::k_invalid_handle ) {
    //        current_render_pipeline->resource_database.register_texture( (char*)"albedo", texture_handle );

    //        editor_material.material->textures[changed_texture_index] = texture_handle;
    //        editor_material.material->texture_filenames[changed_texture_index] = texture_path;

    //        //Resource* texture_resource = string_hash_get( editor_material.material_resource->name_to_external_resources, texture_path );
    //    }
    //}
}

void MaterialSystemApplication::app_resize( uint16_t width, uint16_t height ) {
    if ( current_render_pipeline ) {
        current_render_pipeline->resize( width, height, gfx_device );
    }
}

static MaterialSystemApplication::FileType file_type_from_name( const char* filename ) {
    if ( strstr( filename, ".hfx" ) ) {
        return MaterialSystemApplication::ShaderEffect_HFX;
    } else if ( strstr( filename, ".bhfx" ) ) {
        return MaterialSystemApplication::Binary_HFX;
    } else if ( strstr( filename, ".hmt" ) ) {
        return MaterialSystemApplication::Material_HMT;
    } else if ( strstr( filename, "bhr" ) ) {
        return MaterialSystemApplication::Binary;
    }

    return MaterialSystemApplication::Count;
}

void MaterialSystemApplication::edit_file( const char* filepath ) {

    if ( file_text ) {
        hydra::hy_free( file_text );
    }
    
    file_text = hydra::read_file_into_memory( filepath, &file_size );

    opened_file_path.clear();
    opened_file_path.append( filepath );

    opened_file_type = file_type_from_name( filepath );

    switch ( opened_file_type ) {
        case ShaderEffect_HFX:
        case Material_HMT:
        {
            if ( file_text ) {
                text_editor->SetText( file_text );
            }
            break;
        }
        case Binary_HFX:
        {
            text_editor->SetText( "" );
            hfx::init_shader_effect_file( shader_effect_file, filepath );

            break;
        }
        case Binary:
        {
            text_editor->SetText( "" );

            break;
        }
    }
}

void MaterialSystemApplication::file_action_popup_render( const char* filename ) {

    ImGui::Separator();

    if ( ImGui::MenuItem( "Edit" ) ) {
        edit_file( filename );
    }

    if ( strstr( filename, ".hfx" ) ) {
        if ( ImGui::MenuItem( "Create material" ) ) {
            create_material( filename );
        }

        if ( ImGui::MenuItem( "Compile" ) ) {
            compile_hfx( filename );
        }
    }
    else if ( strstr( filename, ".bhfx" ) ) {
        if ( ImGui::MenuItem( "Inspect" ) ) {
            //inspect_bhfx( filename, full_filepath );
        }
    }
    else if ( strstr( filename, ".hmt" ) ) {
        if ( ImGui::MenuItem( "Load material" ) ) {
            load_material( filename );
        }
    }
}

void MaterialSystemApplication::load_material( const char* filename ) {

    hydra::graphics::RenderPipeline* render_pipeline = nullptr;

    if ( strcmp(filename, "SimpleFullscreen.hmt") == 0 ) {
        render_pipeline = string_hash_get( name_to_render_pipeline, "swapchain" );
    }
    else if ( strcmp( filename, "StarNest.hmt" ) == 0 ) {
        render_pipeline = string_hash_get( name_to_render_pipeline, "ShaderToy" );
    }

    if ( !render_pipeline ) {
        hydra::print_format( "Cannot find pipeline Default. Cannot load material %s.\n", filename );
        return;
    }

    current_render_pipeline = render_pipeline;

    hydra::Resource* material_resource = resource_manager.load_resource( hydra::ResourceType::Material, filename, gfx_device, render_pipeline );
    if ( !material_resource ) {
        return;
    }

    editor_material.material_resource = material_resource;
    editor_material.material = (hydra::graphics::Material*)material_resource->asset;

    material_filename.clear();
    material_filename.append( filename );


    if ( editor_material.material && current_render_pipeline ) {
        editor_material.material->load_resources( current_render_pipeline->resource_database, gfx_device );
    }
}

void MaterialSystemApplication::create_material( const char* filename ) {

    //bool effect_compiled;
    //hydra::graphics::ShaderEffect* effect = material_manager.load_shader_effect( filename, parsing_string_buffer, effect_compiled, gfx_device );
    //if ( !effect ) {
    //    return;
    //}

    //// Create json document
    //rapidjson::Document document;
    //document.SetObject();

    //const char* name = filename;

    //rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
    //rapidjson::Value material_name;
    //material_name.SetString( name, strlen( name ) );
    //document.AddMember( "name", material_name, allocator );

    //rapidjson::Value effect_filename;
    //effect_filename.SetString( filename, strlen( filename ) );
    //document.AddMember( "effect_path", effect_filename, allocator );

    //// Write properties
    //rapidjson::Value properties( rapidjson::kArrayType );

    //// Get properties and use their offset to retrieve default values in the resource defaults area.
    //for ( uint32_t p = 0; p < effect->num_properties; ++p ) {
    //    hfx::ShaderEffectFile::MaterialProperty* property = hfx::get_property( effect->properties_data, p );

    //    switch ( property->type ) {
    //        case hfx::Property::Float:
    //        {
    //            float value = *(float*)(effect->local_constants_default_data + property->offset);
    //            rapidjson::Value json_property( rapidjson::kObjectType );
    //            rapidjson::Value json_name, json_value;
    //            json_name.SetString( property->name, strlen( property->name ) );
    //            json_value.SetFloat( value );

    //            json_property.AddMember( json_name, json_value, allocator );

    //            properties.PushBack( json_property, allocator );

    //            break;
    //        }
    //    }
    //}

    //document.AddMember( "properties", properties, allocator );

    //rapidjson::Value bindings( rapidjson::kArrayType );
    //// Initially just add local constants binding.
    //rapidjson::Value json_property( rapidjson::kObjectType );
    //rapidjson::Value json_name, json_value;
    //json_name.SetString( "LocalConstants", strlen( "LocalConstants" ) );
    //json_value.SetString( "LocalConstants", strlen( "LocalConstants" ) );

    //json_property.AddMember( json_name, json_value, allocator );

    ////string_buffer.append( "\t%s = %f\n", property->name, value );
    //bindings.PushBack( json_property, allocator );
    //document.AddMember( "bindings", bindings, allocator );

    //// Write document
    //rapidjson::StringBuffer strbuf;
    //rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( strbuf );
    //document.Accept( writer );

    //const char* hfx_folder = "..\\data\\";

    //const char* output_material_filename = parsing_string_buffer.append_use( "%s%s.hmt", hfx_folder, name );

    //// Write the material file
    //hydra::FileHandle file;
    //hydra::open_file( output_material_filename, "w", &file );
    //if ( file ) {
    //    fwrite( strbuf.GetString(), strbuf.GetSize(), 1, file );
    //    hydra::close_file( file );
    //}
}

void MaterialSystemApplication::save_material( const char* filename ) {
//    Resource* resource = resource_manager.load_resource( filename, parsing_string_buffer );
//    if ( !resource ) {
//        return;
//    }
//
//    // Save material data
//    const char* file_memory = hydra::read_file_into_memory( filename, nullptr );
//    rapidjson::Document cached_document;
//    if ( cached_document.Parse<0>( file_memory ).HasParseError() ) {
//        return;
//    }
//
//    hydra::graphics::Material* material = editor_material.material;
//    if ( !material ) {
//        return;
//    }
//
//    // Modify only properties and bindings
//    rapidjson::Document document;
//    document.SetObject();
//
//    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
//
//    document.AddMember( "name", cached_document["name"], allocator );
//    document.AddMember( "effect_path", cached_document["effect_path"], allocator );
//
//    // Write properties
//    rapidjson::Value properties( rapidjson::kArrayType );
//
//    rapidjson::Value json_property( rapidjson::kObjectType );
//    // Get properties and use their offset to retrieve default values in the resource defaults area.
//    uint32_t current_texture = 0;
//
//    for ( uint32_t p = 0; p < material->effect->num_properties; ++p ) {
//        hfx::ShaderEffectFile::MaterialProperty* property = hfx::get_property( material->effect->properties_data, p );
//
//        switch ( property->type ) {
//            case hfx::Property::Float:
//            {
//                float value = *(float*)(material->local_constants_data + property->offset);
//
//                rapidjson::Value json_name, json_value;
//                json_name.SetString( property->name, strlen( property->name ) );
//                json_value.SetFloat( value );
//
//                json_property.AddMember( json_name, json_value, allocator );
//
//                break;
//            }
//
//            case hfx::Property::Texture2D:
//            {
//                rapidjson::Value json_name, json_value;
//                json_name.SetString( property->name, strlen( property->name ) );
//                const char* texture_filepath = material->texture_filenames[current_texture++];
//                json_value.SetString( texture_filepath, strlen( texture_filepath ) );
//
//                json_property.AddMember( json_name, json_value, allocator );
//
//                break;
//            }
//        }
//    }
//    properties.PushBack( json_property, allocator );
//
//    document.AddMember( "properties", properties, allocator );
//
//    // Write bindings
//    rapidjson::Value bindings( rapidjson::kArrayType );
//    rapidjson::Value json_property_bindings( rapidjson::kObjectType );
//
//
//#if defined MATERIAL_DEBUG_BINDINGS
//    const ShaderResourcesLookup& resource_lookup = *material.lookups;
//    uint32_t num_map_entries = string_hash_length_u( resource_lookup.binding_to_resource );
//
//    for ( uint32_t p = 0; p < num_map_entries; ++p ) {
//        const ShaderResourcesLookup::NameMap& name_map = resource_lookup.binding_to_resource[p];
//        print_format( "Binding %s, %s\n", name_map.key, name_map.value );
//    }
//#endif //MATERIAL_DEBUG_BINDINGS 
//
//    for ( size_t b = 0; b < string_hash_length( material->lookups.binding_to_resource ); ++b ) {
//
//        rapidjson::Value json_name, json_value;
//
//        const hydra::graphics::ShaderResourcesLookup::NameMap& binding = material->lookups.binding_to_resource[b];
//        json_name.SetString( binding.key, strlen( binding.key ) );
//        json_value.SetString( binding.value, strlen( binding.value ) );
//
//        json_property_bindings.AddMember( json_name, json_value, allocator );
//    }
//
//    bindings.PushBack( json_property_bindings, allocator );
//    document.AddMember( "bindings", bindings, allocator );
//
//    // Write document
//    rapidjson::StringBuffer strbuf;
//    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( strbuf );
//    document.Accept( writer );
//
//    // Write the material file
//    hydra::FileHandle file;
//    hydra::open_file( filename, "w", &file );
//    if ( file ) {
//        fwrite( strbuf.GetString(), strbuf.GetSize(), 1, file );
//        hydra::close_file( file );
//    }

    // Save resource
}

void MaterialSystemApplication::compile_hfx( const char* filename ) {

}

// FileBrowser //////////////////////////////////////////////////////////////////
void FileBrowser::init() {
    
    hydra::init( files, 1024 * 4 );
    hydra::init( directories, 1024 * 4 );

    current_working_directory.init( 1024 );
    current_filename.init( 1024 );

    current_working_directory.clear();
    current_filename.clear();
}

void FileBrowser::terminate() {

    hydra::terminate( files );
    hydra::terminate( directories );

    current_filename.terminate();
    current_working_directory.terminate();
}

void FileBrowser::draw_window( const char* name ) {
    ImGui::Begin( name );
    draw_contents();
    ImGui::End();
}

void FileBrowser::draw_contents() {

    ImGui::Text( "Current Directory: %s", current_working_directory.data );
    ImGui::Separator();

    // Handle directories first to change directory.
    ImGui::BeginChild( "File Browser Files" );

    bool update_full_filename = false;

    int8_t double_clicked_button = -1;
    int8_t single_clicked_button = -1;

    // Files after, using a callback for double clicking of a file.
    for ( uint32_t i = 0; i < hydra::get_string_count(files); i++ ) {

        bool selected = false;
        const char* filename = hydra::get_string( files, i );

        if ( ImGui::Selectable( filename, &selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

            update_full_filename = last_selected_filename != filename;
            last_selected_filename = filename;

            double_clicked_button = ImGui::IsMouseDoubleClicked( 0 ) ? 0 : -1;
            double_clicked_button = ImGui::IsMouseDoubleClicked( 1 ) ? 1 : double_clicked_button;
            double_clicked_button = ImGui::IsMouseDoubleClicked( 2 ) ? 2 : double_clicked_button;

            single_clicked_button = ImGui::IsMouseReleased( 0 ) ? 0 : -1;
            single_clicked_button = ImGui::IsMouseReleased( 1 ) ? 1 : single_clicked_button;
            single_clicked_button = ImGui::IsMouseReleased( 2 ) ? 2 : single_clicked_button;
        }

        // Cache filename also in right click case, for popup window.
        if ( ImGui::IsItemHovered() && ImGui::IsMouseReleased( 1 ) ) {
            update_full_filename = last_selected_filename != filename;
            last_selected_filename = filename;
        }
    }

    if ( update_full_filename ) {
        // Prepare full filepath
        current_filename.clear();
        current_filename.append( current_working_directory );
        current_filename.current_size -= 1;  // Remove the asterisk added in the current working directory
        current_filename.append( last_selected_filename );
    }

    if ( ImGui::BeginPopupContextWindow() ) {
        ImGui::Text( last_selected_filename );

        if ( popup_showing_callback )
            popup_showing_callback( popup_showing_callback_user_data, last_selected_filename );

        ImGui::EndPopup();
    }

    ImGui::EndChild();

    // Handle input callback
    // Saved click/double click when selecting an item
    // Check double clicks
    if ( file_double_clicked_callback && double_clicked_button > -1 ) {
        file_double_clicked_callback( file_double_clicked_callback_user_data, double_clicked_button, current_filename.data );
    }

    // Check single clicks
    if ( file_single_clicked_callback && single_clicked_button > -1 ) {
        file_single_clicked_callback( file_single_clicked_callback_user_data, single_clicked_button, current_filename.data );
    }

}

void FileBrowser::open_folder( const char* folder ) {

    current_working_directory.clear();

    uint32_t written_size = hydra::get_full_path_name( folder, current_working_directory.data, current_working_directory.buffer_size );
    current_working_directory.current_size += written_size;
    current_working_directory.append( "*" );

    hydra::find_files_in_path( ".", current_working_directory.data, files, directories );
}

void FileBrowser::open_folder( const char* folder, const char* extension ) {
    current_working_directory.clear();

    uint32_t written_size = hydra::get_full_path_name( folder, current_working_directory.data, current_working_directory.buffer_size );
    current_working_directory.current_size += written_size;
    current_working_directory.append( "*" );

    hydra::find_files_in_path( extension, current_working_directory.data, files, directories );
}

void FileBrowser::set_single_click_callback( FileSingleClicked callback, void* user_data ) {
    file_single_clicked_callback = callback;
    file_single_clicked_callback_user_data = user_data;
}

void FileBrowser::set_double_click_callback( FileDoubleClicked callback, void* user_data ) {
    file_double_clicked_callback = callback;
    file_double_clicked_callback_user_data = user_data;
}

void FileBrowser::set_popup_showing_callback( PopupShowing callback, void* user_data ) {
    popup_showing_callback = callback;
    popup_showing_callback_user_data = user_data;
}



int main(int argc, char** argv) {

    MaterialSystemApplication material_application;
    material_application.main_loop();

    return 0;
}
