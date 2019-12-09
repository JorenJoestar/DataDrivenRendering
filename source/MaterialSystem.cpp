#include "MaterialSystem.h"

#include "imgui/TextEditor.h"
#include "imgui/imgui_memory_editor.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "stb_leakcheck.h"


//
// Utility method
//
static char* remove_extension_from_filename( const char* filename, StringBuffer& temp_string_buffer ) {
    int64_t last_extension_index = strrchr( filename, '.' ) - filename;
    char* name = (char*)filename;
    if ( last_extension_index > 0 ) {
        name = temp_string_buffer.append_use_substring( filename, 0, (uint32_t)last_extension_index );
    }

    return name;
}


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
        shadertoy_render_pipeline.init();

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
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( shadertoy_render_pipeline.name_to_stage, final_stage_name, final_stage );


        // 4. Update resources - actually link stage resources
        shadertoy_render_pipeline.update_resources( gfx_device );

        char pipeline_name[32] = "ShaderToy";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );
    }

    // Create custom compute pipeline
    {
        graphics::RenderPipeline* render_pipeline = ( graphics::RenderPipeline* )malloc( sizeof( graphics::RenderPipeline ) );
        graphics::RenderPipeline& compute_post_render_pipeline = *render_pipeline;
        compute_post_render_pipeline.init();

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
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( compute_post_render_pipeline.name_to_stage, final_stage_name, final_stage );

        compute_post_render_pipeline.update_resources( gfx_device );

        char pipeline_name[32] = "computeTest";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );
    }

    {
        graphics::RenderPipeline* render_pipeline = ( graphics::RenderPipeline* )malloc( sizeof( graphics::RenderPipeline ) );
        render_pipeline->init();

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
        
        final_stage->init();
        char final_stage_name[32] = "final";
        string_hash_put( render_pipeline->name_to_stage, final_stage_name, final_stage );

        render_pipeline->update_resources( gfx_device );
        char pipeline_name[32] = "swapchain";
        string_hash_put( name_to_render_pipeline, pipeline_name, render_pipeline );

        current_render_pipeline = render_pipeline;
    }

    editor_material.material = nullptr;

    load_material( "SimpleFullscreen.hmt" );
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
        current_render_pipeline->render( commands );
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
                hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file, p );
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

    Resource* material_resource = resource_manager.load_resource( ResourceType::Material, filename, parsing_string_buffer, gfx_device, name_to_render_pipeline );
    if ( !material_resource ) {
        return;
    }

    editor_material.material_resource = material_resource;
    editor_material.material = (hydra::graphics::Material*)material_resource->asset;

    material_filename.clear();
    material_filename.append( filename );

    hydra::graphics::RenderPipeline* render_pipeline = string_hash_get( name_to_render_pipeline, editor_material.material->effect->pipeline_name );
    current_render_pipeline = render_pipeline;
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

// ResourceManager //////////////////////////////////////////////////////////////

void ResourceManager::init() {
    string_hash_init_dynamic( name_to_resources );

    static TextureFactory texture_factory;
    static ShaderFactory shader_factory;
    static MaterialFactory material_factory;

    resource_factories[ResourceType::Texture] = &texture_factory;
    resource_factories[ResourceType::ShaderEffect] = &shader_factory;
    resource_factories[ResourceType::Material] = &material_factory;

    for ( size_t i = 0; i < ResourceType::Count; ++i ) {
        resource_factories[i]->init();
    }
}

void ResourceManager::terminate( hydra::graphics::Device& gfx_device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_resources ); ++i ) {
        unload_resource( &name_to_resources[i].value, gfx_device );
    }

    for ( size_t i = 0; i < ResourceType::Count; ++i ) {
        resource_factories[i]->terminate();
    }
}

static const char* guid_to_filename( const char* path, ResourceType::Enum type, StringBuffer& temp_string_buffer ) {
    const char* name = remove_extension_from_filename( path, temp_string_buffer );

    switch ( type ) {
        case ResourceType::ShaderEffect:
            return temp_string_buffer.append_use( "%s.sbhr", name );
            break;

        case ResourceType::Texture:
            return temp_string_buffer.append_use( "%s.tbhr", name );
            break;

        case ResourceType::Material:
            return temp_string_buffer.append_use( "%s.mbhr", name );
            break;
    }

    return path;
}

static const char* guid_to_filename( Resource::ResourceReference& reference, StringBuffer& temp_string_buffer ) {
    return guid_to_filename( reference.path, (ResourceType::Enum)reference.type, temp_string_buffer );
}

Resource* ResourceManager::load_resource( ResourceType::Enum type, const char* filename, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::PipelineMap* name_to_render_pipeline ) {

    Resource* resource = string_hash_get( name_to_resources, filename );

    if ( resource ) {
        return resource;
    }

    compile_resource( type, filename, temp_string_buffer );

    // Try to load file. If not present, compile.
    const char* resource_full_filename = temp_string_buffer.append_use( "..\\data\\%s", guid_to_filename(filename, type, temp_string_buffer) );
    char* file_memory = hydra::read_file_into_memory( resource_full_filename, nullptr );
    //if ( !file_memory ) {
    //    resource_factories[type]->compile_resource( type, filename );
    //}

    resource = (Resource*)hydra::hy_malloc( sizeof( Resource ) );
    resource->header = (Resource::Header*)file_memory;
    resource->data = file_memory + sizeof( Resource::Header ) + ((resource->header->num_external_references + resource->header->num_internal_references) * sizeof( Resource::ResourceReference ));
    string_hash_init_arena( resource->name_to_external_resources );

    resource->external_references = (Resource::ResourceReference*)(file_memory + sizeof( Resource::Header ));

    Resource::ResourceReference* external_references = resource->external_references;
    for ( size_t i = 0; i < resource->header->num_external_references; ++i ) {
        Resource* external_resource = load_resource( (ResourceType::Enum)external_references->type, external_references->path, temp_string_buffer, gfx_device, name_to_render_pipeline );

        string_hash_put( resource->name_to_external_resources, external_references->path, external_resource );
        external_references++;
    }

    ResourceFactory::LoadContext load_context = { resource->data, resource->header->data_size, resource, gfx_device, name_to_render_pipeline };
    resource->asset = resource_factories[type]->load( load_context );

    string_hash_put( name_to_resources, filename, resource );

    return resource;
}

void ResourceManager::save_resource( Resource& resource ) {
}

void ResourceManager::unload_resource( Resource** resource, hydra::graphics::Device& gfx_device ) {

    resource_factories[(*resource)->header->type]->unload((*resource)->asset, gfx_device );

    hydra::hy_free( (*resource)->header );
    hydra::hy_free( *resource );
}

Resource* ResourceManager::compile_resource( ResourceType::Enum type, const char* filename, StringBuffer& temp_string_buffer ) {

    size_t file_size;
    const char* source_full_filename = temp_string_buffer.append_use( "..\\data\\%s", filename );
    char* source_file_memory = hydra::read_file_into_memory( source_full_filename, &file_size );
    if ( !source_file_memory ) {
        return nullptr;
    }

    // Linked resource data
    Resource::ResourceReference references[32];
    Resource::Header resource_header;

    ResourceFactory::CompileContext compile_context = { source_file_memory, filename, temp_string_buffer, references, &resource_header, file_size };

    resource_factories[type]->compile_resource( compile_context );

    hydra::hy_free( source_file_memory );

    return nullptr;
}


// Resource Factories ///////////////////////////////////////////////////////////

void TextureFactory::init() {
    textures_pool.init( 4096, sizeof( hydra::graphics::Texture ) );
}

void TextureFactory::terminate() {
    textures_pool.terminate();
}

// TextureFactory ////////////////////////
void TextureFactory::compile_resource( CompileContext& context ) {

    char* output_filename = remove_extension_from_filename( context.source_filename, context.temp_string_buffer );

    FILE* output_file = nullptr;
    context.out_header->type = ResourceType::Texture;
    context.out_header->num_external_references = 0;
    context.out_header->num_internal_references = 0;
    context.out_header->data_size = context.file_size;

    fopen_s( &output_file, context.temp_string_buffer.append_use( "..\\data\\%s.tbhr", output_filename ), "wb" );
    // Write Header
    fwrite( context.out_header, sizeof( Resource::Header ), 1, output_file );
    // Write Data
    fwrite( context.source_file_memory, context.file_size, 1, output_file );
    fclose( output_file );
}

void* TextureFactory::load( LoadContext& context ) {

    using namespace hydra::graphics;

    int image_width, image_height, channels_in_file;
    unsigned char* image_data = stbi_load_from_memory( (const stbi_uc*)context.data, context.size, &image_width, &image_height, &channels_in_file, 4 );

    TextureCreation texture_creation = { image_data, image_width, image_height, 1, 1, 0, TextureFormat::R8G8B8A8_UNORM, TextureType::Texture2D };
    uint32_t pool_id = textures_pool.obtain_resource();
    Texture* texture = (Texture*)textures_pool.access_resource( pool_id );
    texture->handle = context.device.create_texture( texture_creation );
    texture->pool_id = pool_id;
    texture->filename = nullptr;

    return texture;
}

void TextureFactory::unload( void* resource_data, hydra::graphics::Device& device ) {
    using namespace hydra::graphics;
    Texture* texture = (Texture*)resource_data;

    device.destroy_texture( texture->handle );
    textures_pool.release_resource( texture->pool_id );
}

// ShaderFactory /////////////////////////

void ShaderFactory::init() {
    shaders_pool.init( 1000, sizeof( hydra::graphics::ShaderEffect ) );
}

void ShaderFactory::terminate() {
    shaders_pool.terminate();
}

void ShaderFactory::compile_resource( CompileContext& context ) {

    FILE* output_file = nullptr;
    context.out_header->type = ResourceType::ShaderEffect;
    context.out_header->num_external_references = 0;
    context.out_header->num_internal_references = 0;
    context.out_header->data_size = context.file_size;

    char* output_filename = remove_extension_from_filename( context.source_filename, context.temp_string_buffer );

    // Compile to bhfx
    const char* bhfx_full_filename = context.temp_string_buffer.append_use( "%s.bhfx", output_filename );
    const char* hfx_full_filename = context.temp_string_buffer.append_use( "..\\data\\%s", context.source_filename );
    hfx::compile_hfx( hfx_full_filename, "..\\data\\", bhfx_full_filename );
    char* bhfx_memory = hydra::read_file_into_memory( context.temp_string_buffer.append_use( "..\\data\\%s.bhfx", output_filename ), &context.file_size );

    fopen_s( &output_file, context.temp_string_buffer.append_use( "..\\data\\%s.sbhr", output_filename ), "wb" );
    
    // Write Header
    fwrite( context.out_header, sizeof( Resource::Header ), 1, output_file );
    // Write Data
    fwrite( bhfx_memory, context.file_size, 1, output_file );
    fclose( output_file );

    hydra::hy_free( bhfx_memory );
}

void* ShaderFactory::load( LoadContext& context ) {

    using namespace hydra::graphics;

    hfx::ShaderEffectFile shader_effect_file;
    hfx::init_shader_effect_file( shader_effect_file, context.data );

    // 1. Create shader effect
    uint32_t effect_pool_id = shaders_pool.obtain_resource();
    ShaderEffect* effect = new(shaders_pool.access_resource(effect_pool_id)) ShaderEffect();
    effect->pool_id = effect_pool_id;
    memcpy( effect->name, shader_effect_file.header->name, 32 );
    memcpy( effect->pipeline_name, shader_effect_file.header->pipeline_name, 32 );
    effect->num_passes = shader_effect_file.header->num_passes;
    effect->passes = new((ShaderEffect::Pass*)malloc( sizeof( ShaderEffect::Pass ) * shader_effect_file.header->num_passes ))ShaderEffect::Pass[shader_effect_file.header->num_passes];
    effect->local_constants_size = shader_effect_file.local_constants_size;
    effect->local_constants_default_data = shader_effect_file.local_constants_default_data;
    effect->num_properties = shader_effect_file.num_properties;
    effect->properties_data = shader_effect_file.properties_data;

    bool invalid_effect = false;

    // 2. Create pipelines
    for ( uint16_t p = 0; p < shader_effect_file.header->num_passes; p++ ) {
        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file, p );

        uint32_t shader_count = pass_header->num_shader_chunks;

        memcpy( effect->passes[p].name, pass_header->stage_name, 32 );

        PipelineCreation& pipeline_creation = effect->passes[p].pipeline_creation;
        ShaderCreation& creation = pipeline_creation.shaders;
        bool compute = false;
        for ( uint16_t i = 0; i < shader_count; i++ ) {
            hfx::get_shader_creation( pass_header, i, &creation.stages[i] );

            if ( creation.stages[i].type == ShaderStage::Compute )
                compute = true;
        }

        creation.name = pass_header->name;
        creation.stages_count = shader_count;

        effect->passes[p].pipeline_creation.compute = compute;

        // 2.1 Create Resource Set Layouts
        for ( uint16_t l = 0; l < pass_header->num_resource_layouts; l++ ) {

            uint8_t num_bindings = 0;
            const ResourceListLayoutCreation::Binding* bindings = get_pass_layout_bindings( pass_header, l, num_bindings );
            ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };

            pipeline_creation.resource_list_layout[l] = context.device.create_resource_list_layout( resource_layout_creation );

        }

        pipeline_creation.num_active_layouts = pass_header->num_resource_layouts;

        // Create pipeline
        effect->passes[p].pipeline_handle = context.device.create_pipeline( pipeline_creation );
        if ( effect->passes[p].pipeline_handle.handle == k_invalid_handle ) {
            invalid_effect = true;
            break;
        }
    }

    if ( !invalid_effect ) {
        // 3. Cache properties for materials. Used mainly to put the property in the local constants.
        string_hash_init_arena( effect->name_to_property );

        for ( uint32_t p = 0; p < effect->num_properties; ++p ) {
            hfx::ShaderEffectFile::MaterialProperty* property = hfx::get_property( effect->properties_data, p );

            string_hash_put( effect->name_to_property, property->name, property );
        }
    }
    else {
        // 4. Cleanup of resources
        // TODO:

        //delete[]( effect->pipelines );
        //delete[]( effect->pipeline_handles );

        return nullptr;
    }

    return effect;
}

void ShaderFactory::unload( void* resource_data, hydra::graphics::Device& device ) {
    using namespace hydra::graphics;

    ShaderEffect* effect = (ShaderEffect*)resource_data;

    string_hash_free( effect->name_to_property );

    for ( size_t i = 0; i < effect->num_passes; ++i ) {

        ShaderEffect::Pass& pass = effect->passes[i];
        for ( size_t l = 0; l < pass.pipeline_creation.num_active_layouts; ++l ) {
            device.destroy_resource_list_layout( pass.pipeline_creation.resource_list_layout[l] );
        }

        device.destroy_pipeline( pass.pipeline_handle );
    }

    shaders_pool.release_resource( effect->pool_id );

    hydra::hy_free( effect->passes );
}


// MaterialFactory ///////////////////////

void MaterialFactory::init() {

    materials_pool.init( 512, sizeof( hydra::graphics::Material ));
}

void MaterialFactory::terminate() {
    
    materials_pool.terminate();

}

void MaterialFactory::compile_resource( CompileContext& context ) {
    FILE* output_file = nullptr;
    context.out_header->type = ResourceType::Material;
    context.out_header->num_external_references = 0;
    context.out_header->num_internal_references = 0;
    context.out_header->data_size = context.file_size;

    char* output_filename = remove_extension_from_filename( context.source_filename, context.temp_string_buffer );

    rapidjson::Document document;
    if ( document.Parse<0>( context.source_file_memory ).HasParseError() ) {
        return;
    }

    hydra::graphics::MaterialFile::Header material_file_header;

    Resource::ResourceReference* references = context.out_references;
    Resource::Header& resource_header = *context.out_header;

    // Add HFX dependency
    const char* hfx_filename = document["effect_path"].GetString();
    memcpy( &references[resource_header.num_external_references].path, hfx_filename, 255 );
    references[resource_header.num_external_references++].type = (uint8_t)ResourceType::ShaderEffect;

    const char* material_name = document["name"].GetString();
    strcpy( material_file_header.name, material_name );
    strcpy( material_file_header.hfx_filename, hfx_filename );

    material_file_header.num_properties = 0;
    material_file_header.num_bindings = 0;
    material_file_header.num_textures = 0;

    const rapidjson::Value& properties = document["properties"];
    const rapidjson::Value& property_container = properties[0];
    for ( rapidjson::Value::ConstMemberIterator itr = property_container.MemberBegin(); itr != property_container.MemberEnd(); ++itr ) {

        if ( itr->value.IsString() ) {
            const char* res_filename = itr->value.GetString();

            memcpy( &references[resource_header.num_external_references].path, res_filename, 255 );
            references[resource_header.num_external_references++].type = (uint8_t)ResourceType::Texture;

            ++material_file_header.num_textures;
        }

        ++material_file_header.num_properties;
    }

    // Just count bindings
    const rapidjson::Value& bindings = document["bindings"];
    for ( rapidjson::SizeType i = 0; i < bindings.Size(); i++ ) {
        const rapidjson::Value& binding = bindings[i];

        for ( rapidjson::Value::ConstMemberIterator itr = binding.MemberBegin(); itr != binding.MemberEnd(); ++itr ) {
            ++material_file_header.num_bindings;
        }
    }

    fopen_s( &output_file, context.temp_string_buffer.append_use( "..\\data\\%s.mbhr", output_filename ), "wb" );
    fwrite( &resource_header, sizeof( Resource::Header ), 1, output_file );
    fwrite( &references[0], sizeof( Resource::ResourceReference ), resource_header.num_external_references, output_file );
            
    // Write material data

    fwrite( &material_file_header, sizeof( hydra::graphics::MaterialFile::Header ), 1, output_file );

    // Write properties
    hydra::graphics::MaterialFile::Property material_property;

    for ( rapidjson::Value::ConstMemberIterator itr = property_container.MemberBegin(); itr != property_container.MemberEnd(); ++itr ) {

        strcpy( material_property.name, itr->name.GetString() );

        // Use data to write value
        if ( itr->value.IsString() ) {
            strcpy( material_property.data, itr->value.GetString() );
        }
        else if ( itr->value.IsFloat() ) {
            float value = itr->value.GetFloat();
            memcpy( material_property.data, &value, sizeof( float ) );
        }
        else {
            hydra::print_format( "ERROR!" );
        }
        // Output to file
        fwrite( &material_property, sizeof( hydra::graphics::MaterialFile::Property ), 1, output_file );
    }

    // Write bindings
    hydra::graphics::MaterialFile::Binding material_binding;
    for ( rapidjson::SizeType i = 0; i < bindings.Size(); i++ ) {
        const rapidjson::Value& binding = bindings[i];

        for ( rapidjson::Value::ConstMemberIterator itr = binding.MemberBegin(); itr != binding.MemberEnd(); ++itr ) {

            strcpy( material_binding.name, itr->name.GetString() );
            strcpy( material_binding.value, itr->value.GetString() );
                    
            // Output to file
            fwrite( &material_binding, sizeof( hydra::graphics::MaterialFile::Binding ), 1, output_file );
        }
    }
    fclose( output_file );
}

static void update_material_resources( hydra::graphics::Material* material, hydra::graphics::ShaderResourcesDatabase& database, hydra::graphics::Device& device ) {

    using namespace hydra::graphics;

    // 1. Create resource list
    // Get all resource handles from the database.
    ResourceListCreation::Resource resources_handles[k_max_resources_per_list];

    // For each pass
    for ( uint32_t i = 0; i < material->effect->num_passes; i++ ) {
        PipelineCreation& pipeline = material->effect->passes[i].pipeline_creation;

        for ( uint32_t l = 0; l < pipeline.num_active_layouts; ++l ) {
            // Get resource layout description
            ResourceListLayoutDescription layout;
            device.query_resource_list_layout( pipeline.resource_list_layout[l], layout );

            // For each resource
            for ( uint32_t r = 0; r < layout.num_active_bindings; r++ ) {
                const ResourceBinding& binding = layout.bindings[r];

                // Find resource name
                // Copy string_buffer 
                char* resource_name = material->lookups.find_resource( (char*)binding.name );

                switch ( binding.type ) {
                    case hydra::graphics::ResourceType::Constants:
                    case hydra::graphics::ResourceType::Buffer:
                    {
                        BufferHandle handle = resource_name ? database.find_buffer( resource_name ) : device.get_dummy_constant_buffer();
                        resources_handles[r].handle = handle.handle;

                        break;
                    }

                    case hydra::graphics::ResourceType::Texture:
                    case hydra::graphics::ResourceType::TextureRW:
                    {
                        TextureHandle handle = resource_name ? database.find_texture( resource_name ) : device.get_dummy_texture();
                        resources_handles[r].handle = handle.handle;

                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }

            ResourceListCreation creation = { pipeline.resource_list_layout[l], resources_handles, layout.num_active_bindings };
            material->shader_instances[i].resource_lists[l] = device.create_resource_list( creation );
        }
        material->shader_instances[i].num_resource_lists = pipeline.num_active_layouts;
        material->shader_instances[i].pipeline = material->effect->passes[i].pipeline_handle;
    }
}

void* MaterialFactory::load( LoadContext& context ) {
    
    using namespace hydra::graphics;

    // 1. Read header
    MaterialFile material_file;
    material_file.header = (MaterialFile::Header*)context.data;
    material_file.property_array = (MaterialFile::Property*)(context.data + sizeof( MaterialFile::Header ));
    material_file.binding_array = (MaterialFile::Binding*)(context.data + sizeof( MaterialFile::Header ) + sizeof( MaterialFile::Property ) * material_file.header->num_properties);

    // 2. Read shader effect
    Resource* shader_effect_resource = string_hash_get( context.resource->name_to_external_resources, material_file.header->hfx_filename );
    ShaderEffect* shader_effect = shader_effect_resource ? (ShaderEffect*)shader_effect_resource->asset : nullptr;
    if ( !shader_effect ) {
        return nullptr;
    }

    // 3. Search pipeline
    RenderPipeline* render_pipeline = string_hash_get( context.name_to_render_pipeline, shader_effect->pipeline_name );
    if ( !render_pipeline ) {
        return nullptr;
    }

    // 4. Load material
    char* material_name = material_file.header->name;
    uint32_t pool_id = materials_pool.obtain_resource();
    Material* material = new (materials_pool.access_resource(pool_id))Material();
    material->loaded_string_buffer.init( 1024 );
    material->pool_id = pool_id;

    // TODO: for now just have one lookup shared.
    material->lookups.init();
    // TODO: properly specialize.
    // For each pass
    //for ( uint32_t i = 0; i < effect->num_pipelines; i++ ) {
    //    PipelineCreation& pipeline = effect->pipelines[i];
    //    //final ShaderBindings specializedBindings = bindings.specialize( shaderTechnique.passName, shaderTechnique.viewName );
    //    //shaderBindings.add( specializedBindings );
    //}

    material->effect = shader_effect;
    material->num_instances = shader_effect->num_passes;
    material->shader_instances = new ShaderInstance[shader_effect->num_passes];
    material->name = material->loaded_string_buffer.append_use( material_name );
    material->num_textures = material_file.header->num_textures;

    // Init memory for local constants
    material->local_constants_data = (char*)hydra::hy_malloc( shader_effect->local_constants_size );
    // Copy default values to init to sane valuess
    memcpy( material->local_constants_data, material->effect->local_constants_default_data, material->effect->local_constants_size );

    material->textures = (Texture**)hydra::hy_malloc( sizeof( Texture* ) * material->num_textures );

    // Add properties
    uint32_t current_texture = 0;
    for ( size_t p = 0; p < material_file.header->num_properties; ++p ) {
        MaterialFile::Property& property = material_file.property_array[p];

        hfx::ShaderEffectFile::MaterialProperty* material_property = string_hash_get( material->effect->name_to_property, property.name );

        switch ( material_property->type ) {
            case hfx::Property::Texture2D:
            {
                const char* texture_path = material->loaded_string_buffer.append_use( property.data );
                Resource* texture_resource = string_hash_get( context.resource->name_to_external_resources, texture_path );
                Texture* texture = (Texture*)texture_resource->asset;
                texture->filename = texture_path;

                render_pipeline->resource_database.register_texture( property.name, texture->handle );

                material->textures[current_texture] = texture;

                ++current_texture;

                break;
            }

            case hfx::Property::Float:
            {
                memcpy( material->local_constants_data + material_property->offset, property.data, sizeof( float ) );
                break;
            }
        }
    }

     // Add bindings
    for ( size_t b = 0; b < material_file.header->num_bindings; ++b ) {
        MaterialFile::Binding& binding = material_file.binding_array[b];

        char* name = material->loaded_string_buffer.append_use( binding.name );
        char* value = material->loaded_string_buffer.append_use( binding.value );
        material->lookups.add_binding_to_resource( name, value );
    }

//#define MATERIAL_DEBUG_BINDINGS
#if defined MATERIAL_DEBUG_BINDINGS
    const ShaderResourcesLookup& resource_lookup = *material->lookups;
    uint32_t num_map_entries = string_hash_length_u( resource_lookup.binding_to_resource );

    for ( uint32_t p = 0; p < num_map_entries; ++p ) {
        const ShaderResourcesLookup::NameMap& name_map = resource_lookup.binding_to_resource[p];
        print_format( "Binding %s, %s\n", name_map.key, name_map.value );
    }
#endif //MATERIAL_DEBUG_BINDINGS

    BufferCreation checker_constants_creation = {};
    checker_constants_creation.type = BufferType::Constant;
    checker_constants_creation.name = s_local_constants_name;
    checker_constants_creation.usage = ResourceUsageType::Dynamic;
    checker_constants_creation.size = shader_effect->local_constants_size;
    checker_constants_creation.initial_data = material->local_constants_data;

    material->local_constants_buffer = context.device.create_buffer( checker_constants_creation );
    render_pipeline->resource_database.register_buffer( (char*)s_local_constants_name, material->local_constants_buffer );

    // Bind material resources
    update_material_resources( material, render_pipeline->resource_database, context.device );

    // 5. Bind material to pipeline
    for ( uint8_t p = 0; p < shader_effect->num_passes; ++p ) {
        char* stage_name = shader_effect->passes[p].name;
        hydra::graphics::RenderStage* stage = string_hash_get( render_pipeline->name_to_stage, stage_name );

        if ( stage ) {
            stage->material = material;
            stage->pass_index = (uint8_t)p;
        }
    }

    return material;
}

void MaterialFactory::unload( void* resource_data, hydra::graphics::Device& device ) {
    using namespace hydra::graphics;

    Material* material = (Material*)resource_data;

    for ( size_t i = 0; i < material->num_instances; ++i ) {
        
        for ( size_t l = 0; l < material->shader_instances[i].num_resource_lists; ++l ) {
            device.destroy_resource_list( material->shader_instances[i].resource_lists[l] );
        }
    }

    material->loaded_string_buffer.terminate();

    device.destroy_buffer( material->local_constants_buffer );

    materials_pool.release_resource( material->pool_id );

    hydra::hy_free(material->local_constants_data);
    hydra::hy_free( material->textures );
}


namespace hydra {
namespace graphics {

//
//void MaterialManager::unload_material( Material& material, Device& device ) {
//
//    // 1. Destroy material graphics resources
//    for ( size_t i = 0; i < material.num_instances; ++i ) {
//        ShaderInstance& shader_instance = material.shader_instances[i];
//
//        for ( size_t r = 0; r < shader_instance.num_resource_lists; ++r ) {
//            device.destroy_resource_list( shader_instance.resource_lists[r] );
//        }
//    }
//
//    // 2. Remove material from map
//    string_hash_delete( name_to_materials, material.name );
//    
//    // 3. Unload material
//    material.loaded_string_buffer.terminate();
//    free( material.local_constants_data);
//
//    material.lookups.terminate();
//    free( material.textures );
//    free( material.texture_filenames );
//}

// ShaderResourcesDatabase //////////////////////////////////////////////////////
void ShaderResourcesDatabase::init() {
    string_hash_init_arena( name_to_buffer );
    string_hash_init_arena( name_to_texture );
}

void ShaderResourcesDatabase::terminate() {
    string_hash_free( name_to_buffer );
    string_hash_free( name_to_texture );
}

void ShaderResourcesDatabase::register_buffer( char* name, BufferHandle buffer ) {
    string_hash_put( name_to_buffer, name, buffer );
}

void ShaderResourcesDatabase::register_texture( char* name, TextureHandle texture ) {
    string_hash_put( name_to_texture, name, texture );
}

BufferHandle ShaderResourcesDatabase::find_buffer( char* name ) {

    return string_hash_get( name_to_buffer, name );
}

TextureHandle ShaderResourcesDatabase::find_texture( char* name ) {

    return string_hash_get( name_to_texture, name );
}

// ShaderResourcesLookup ////////////////////////////////////////////////////////

void ShaderResourcesLookup::init() {
    string_hash_init_arena( binding_to_resource );
    string_hash_init_arena( binding_to_specialization );
}

void ShaderResourcesLookup::terminate() {
    string_hash_free( binding_to_resource );
    string_hash_free( binding_to_specialization );
}

void ShaderResourcesLookup::add_binding_to_resource( char* binding, char* resource ) {
    string_hash_put( binding_to_resource, binding, resource );
}

void ShaderResourcesLookup::add_binding_to_specialization( char* binding, Specialization specialization ) {
    string_hash_put( binding_to_specialization, binding, specialization );
}
char* ShaderResourcesLookup::find_resource( char* binding ) {
    return string_hash_get( binding_to_resource, binding );
}

ShaderResourcesLookup::Specialization ShaderResourcesLookup::find_specialization( char* binding ) {
    return string_hash_get( binding_to_specialization, binding );
}

void ShaderResourcesLookup::specialize( char* pass, char* view, ShaderResourcesLookup& final_lookup ) {

    // TODO
    final_lookup.init();
    // Copy hash maps ?
}

// RenderPipeline ///////////////////////////////////////////////////////////////

void RenderPipeline::init() {

    string_hash_init_arena( name_to_stage );
    string_hash_init_arena( name_to_texture );

    resource_database.init();
    resource_lookup.init();
}

void RenderPipeline::terminate( Device& device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->terminate();
    }

    for ( size_t i = 0; i < string_hash_length(name_to_texture); i++ ) {
        TextureHandle texture = name_to_texture[i].value;
        device.destroy_texture( texture );
    }
}

void RenderPipeline::update() {
}

void RenderPipeline::render( CommandBuffer* commands ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->begin( commands );
        stage->render( commands );
        stage->end( commands );
    }
}

void RenderPipeline::update_resources( Device& device ) {

    for ( size_t i = 0; i < string_hash_length(name_to_stage); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->update_resources( resource_database, device );
    }
}

void RenderPipeline::resize( uint16_t width, uint16_t height, Device& device ) {

    for ( size_t i = 0; i < string_hash_length( name_to_stage ); i++ ) {

        RenderStage* stage = name_to_stage[i].value;
        stage->resize( width, height, device );
    }
}

// RenderStage //////////////////////////////////////////////////////////////////

void RenderStage::init() {

    render_pass.handle = k_invalid_handle;
    depth_texture.handle = k_invalid_handle;
}

void RenderStage::terminate() {
    // Destroy render pass
}

void RenderStage::begin( CommandBuffer* commands ) {
    // Render Pass Begin
    commands->begin_submit( 0 );
    commands->begin_pass( render_pass );
    if ( clear_rt ) {
        commands->clear( clear_color[0], clear_color[1], clear_color[2], clear_color[3] );
    }
    commands->set_viewport( { 0, 0, (float)current_width, (float)current_height, 0.0f, 1.0f } );
    commands->end_submit();
    // Set render stage states (depth, alpha, ...)
}

void RenderStage::render( CommandBuffer* commands ) {
    // For each manager, render

    // TODO: for now use the material and the pass specified
    if ( material ) {
        ShaderInstance& shader_instance = material->shader_instances[pass_index];
        switch ( type ) {
            case Geometry:
            {
                break;
            }

            case Post:
            {
                commands->begin_submit( pass_index );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists );
                //commands->bind_vertex_buffer( device.get_fullscreen_vertex_buffer() );
                commands->draw( graphics::TopologyType::Triangle, 0, 3 );
                commands->end_submit();
                break;
            }

            case PostCompute:
            {
                commands->begin_submit( 0 );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists );
                commands->dispatch( (uint8_t)ceilf(current_width / 32.0f), (uint8_t)ceilf( current_height / 32.0f), 1 );
                commands->end_submit();   
                break;
            }

            case Swapchain:
            {
                commands->begin_submit( pass_index );
                commands->bind_pipeline( shader_instance.pipeline );
                commands->bind_resource_list( &shader_instance.resource_lists[0], shader_instance.num_resource_lists );
                //commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
                commands->draw( graphics::TopologyType::Triangle, 0, 3 );
                commands->end_submit();
                break;
            }
        }        
    }
}

void RenderStage::end( CommandBuffer* commands ) {
    // TODO: Post render (for always submitting managers)
    // Render Pass End
    commands->begin_submit( 0 );
    commands->end_pass();
    commands->end_submit();
}

void RenderStage::update_resources( ShaderResourcesDatabase& db, Device& device ) {

    // Create render pass
    if ( render_pass.handle == k_invalid_handle ) {
        RenderPassCreation creation = {};
        switch ( type ) {
            case Geometry:
            {
                break;
            }

            case Post:
            {
                creation.is_compute_post = 0;
                creation.is_swapchain = 0;
                break;
            }

            case PostCompute:
            {
                creation.is_swapchain = 0;
                break;
            }

            case Swapchain:
            {
                creation.is_compute_post = 0;
                creation.is_swapchain = 1;
                break;
            }
        }

        creation.num_render_targets = num_output_textures;
        creation.output_textures = output_textures;
        creation.depth_stencil_texture = depth_texture;

        render_pass = device.create_render_pass( creation );
    }

    if ( resize_output ) {
        current_width = device.swapchain_width * scale_x;
        current_height = device.swapchain_height * scale_y;
    }
}

void RenderStage::resize( uint16_t width, uint16_t height, Device& device ) {

    if ( !resize_output ) {
        return;
    }

    uint16_t new_width = width * scale_x;
    uint16_t new_height = height * scale_y;

    if ( new_width != current_width || new_height != current_height ) {
        current_width = new_width;
        current_height = new_height;

        device.resize_output_textures( render_pass, new_width, new_height );
    }
}


} // namespace graphics
} // namespace hydra
