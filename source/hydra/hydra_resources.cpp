//
//  Hydra Resources - v0.01
//

#include "hydra/hydra_resources.h"

#include "ShaderCodeGenerator.h"
#include <stb_image.h>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"


namespace hydra {

    
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

    resource_binary_folder.init( 64 );
    resource_source_folder.init( 64 );
    temporary_string_buffer.init( 1024 * 100 );

    resource_binary_folder.append( "..\\data\\bin\\" );
    resource_source_folder.append( "..\\data\\source\\" );
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
            return temp_string_buffer.append_use( "%s.bhfx", name );
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

static const char* guid_to_filename( ResourceID& reference, StringBuffer& temp_string_buffer ) {
    return guid_to_filename( reference.path, (ResourceType::Enum)reference.type, temp_string_buffer );
}

static const size_t                 k_resource_random_seed = 0x7bba666dea69a46;

void ResourceManager::init_resource( Resource** resource, char* memory, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {

    (*resource)->header = (ResourceHeader*)memory;
    (*resource)->data = memory + sizeof( ResourceHeader ) + ( ( (*resource)->header->num_external_references + (*resource)->header->num_internal_references ) * sizeof( ResourceID ) );
    string_hash_init_arena( (*resource)->name_to_external_resources );

    (*resource)->external_references = (ResourceID*)( memory + sizeof( ResourceHeader ) );

    ResourceID* external_references = (*resource)->external_references;
    for ( size_t i = 0; i < (*resource)->header->num_external_references; ++i ) {
        Resource* external_resource = load_resource( ( ResourceType::Enum )external_references->type, external_references->path, gfx_device, render_pipeline );

        string_hash_put( (*resource)->name_to_external_resources, external_references->path, external_resource );
        external_references++;
    }
}

Resource* ResourceManager::compile_resource( ResourceType::Enum type, const char* filename ) {

    // Reset temporary string buffer
    temporary_string_buffer.clear();

    // Try to open the source file
    // TODO: optimize - don't need to read all the file here. It can be done only if the file will be compiled.
    size_t file_size;
    const char* source_full_filename = temporary_string_buffer.append_use( "%s%s", resource_source_folder.data, filename );
    char* source_file_memory = hydra::read_file_into_memory( source_full_filename, &file_size );
    if ( !source_file_memory ) {
        hydra::print_format( "Missing source file %s - requested by %s\n", source_full_filename, filename );
        return nullptr;
    }

    // TODO: fix hash working.
    size_t source_file_hash = 0;

    //// Search the hash file. If not found, create it.
    //// Hash file is used to avoid recompiling the binary resource.
    //const char* hash_full_filename = temporary_string_buffer.append_use( "..\\data\\%s.hash", filename );
    //char* hash_memory = hydra::read_file_into_memory( hash_full_filename, nullptr );
    //if ( !hash_memory ) {

    //    // Generate hash
    //    set_rand_seed( k_resource_random_seed );
    //    source_file_hash = hash_bytes( source_file_memory, file_size, k_resource_random_seed );

    //    // Write hash to file
    //    FILE* hash_file = nullptr;
    //    fopen_s( &hash_file, hash_full_filename, "wb" );
    //    if ( hash_file ) {
    //        fwrite( &source_file_hash, sizeof( size_t ), 1, hash_file );
    //        fclose( hash_file );
    //    }
    //}
    //else {
    //    // Read hash from file
    //    FILE* hash_file = nullptr;
    //    fopen_s( &hash_file, hash_full_filename, "rb" );
    //    if ( hash_file ) {
    //        fread( &source_file_hash, sizeof( size_t ), 1, hash_file );
    //        fclose( hash_file );
    //    }
    //}

    Resource* resource = (Resource*)hydra::hy_malloc( sizeof( Resource ) );

    bool compile_resource = false;
    // Try to open the binary resource.
    // If not present or the saved source hash is different than compile it.
    const char* compiled_resource_filename = temporary_string_buffer.append_use( "%s%s", resource_binary_folder.data, guid_to_filename( filename, type, temporary_string_buffer ) );

    //FILE* compiled_file = nullptr;
    //fopen_s( &compiled_file, compiled_resource_filename, "rb" );

    //if ( compiled_file ) {
    //    // Read the hash
    //    fread( &resource_header, sizeof( ResourceHeader ), 1, compiled_file );
    //    fclose( compiled_file );
    //    compile_resource = resource_header.source_hash != source_file_hash;
    //}
    //else {
    //    compile_resource = true;
    //}

    // Compile resource
    //if ( compile_resource ) 
    {
        // Init resource header    
        ResourceHeader resource_header;
        strcpy( resource_header.id.path, filename );
        resource_header.id.type = type;
        resource_header.num_external_references = 0;
        resource_header.num_internal_references = 0;
        resource_header.data_size = file_size;
        resource_header.source_hash = source_file_hash;

        ResourceID references[32];
        ResourceFactory::CompileContext compile_context = { source_file_memory, compiled_resource_filename, temporary_string_buffer, references, &resource_header, this };

        resource_factories[type]->compile_resource( compile_context );
    }

    hydra::hy_free( source_file_memory );

    // Reset temporary string buffer
    temporary_string_buffer.clear();

    return resource;
}

Resource* ResourceManager::load_resource( ResourceType::Enum type, const char* filename, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {

    // Reset temporary string buffer
    temporary_string_buffer.clear();

    Resource* resource = string_hash_get( name_to_resources, filename );

    if ( resource ) {
        return resource;
    }

    resource = compile_resource( type, filename );

    // Try to load file. If not present, compile.
    const char* resource_full_filename = temporary_string_buffer.append_use( "%s%s", resource_binary_folder.data, guid_to_filename(filename, type, temporary_string_buffer ) );
    char* file_memory = hydra::read_file_into_memory( resource_full_filename, nullptr );
    if ( !file_memory ) {
        hydra::print_format( "Missing resource file %s\n", resource_full_filename );
        return nullptr;
    }

    init_resource( &resource, file_memory, gfx_device, render_pipeline );

    ResourceFactory::LoadContext load_context = { resource, gfx_device, render_pipeline };
    resource->asset = resource_factories[type]->load( load_context );

    string_hash_put( name_to_resources, filename, resource );

    // Reset temporary string buffer
    temporary_string_buffer.clear();

    return resource;
}

void ResourceManager::reload_resource( Resource* resource, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {

    // Reset temporary string buffer
    temporary_string_buffer.clear();

    // Reload dependencies
    ResourceID* external_references = resource->external_references;
    for ( size_t i = 0; i < resource->header->num_external_references; ++i ) {
        Resource* external_resource = string_hash_get( name_to_resources, external_references->path );
        if ( external_resource ) {
            reload_resource( external_resource, gfx_device, render_pipeline );
        }
        
        external_references++;
    }

    ResourceType::Enum type = (ResourceType::Enum)resource->header->id.type;
    const char* filename = resource->header->id.path;


    Resource* new_resource = compile_resource( type, filename );

    // Try to load file. If not present, compile.
    const char* resource_full_filename = temporary_string_buffer.append_use( "%s%s", resource_binary_folder.data, guid_to_filename( filename, type, temporary_string_buffer ) );
    char* file_memory = hydra::read_file_into_memory( resource_full_filename, nullptr );
    if ( !file_memory ) {
        hydra::print_format( "Missing resource file %s\n", resource_full_filename );
        return;
    }

    init_resource( &new_resource, file_memory, gfx_device, render_pipeline );

    // Reload resource
    resource_factories[resource->header->id.type]->reload( resource, new_resource, temporary_string_buffer, gfx_device, render_pipeline );

    // Reset temporary string buffer
    temporary_string_buffer.clear();
}

void ResourceManager::reload_resources( ResourceType::Enum type, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {

    for ( size_t i = 0; i < string_hash_length( name_to_resources ); i++ ) {

        ResourceManager::ResourceMap& map_entry = name_to_resources[i];
        // Reload resources by type
        if ( map_entry.value->header->id.type == type ) {
            reload_resource( map_entry.value, gfx_device, render_pipeline );
        }
    }
}

void ResourceManager::save_resource( Resource& resource ) {
}

void ResourceManager::unload_resource( Resource** resource, hydra::graphics::Device& gfx_device ) {

    resource_factories[(*resource)->header->id.type]->unload((*resource)->asset, gfx_device );

    hydra::hy_free( (*resource)->header );
    hydra::hy_free( *resource );
}

// Resource Factories ///////////////////////////////////////////////////////////

// TextureFactory ///////////////////////////////////////////////////////////////
void TextureFactory::init() {
    textures_pool.init( 4096, sizeof( hydra::graphics::Texture ) );
}

void TextureFactory::terminate() {
    textures_pool.terminate();
}


void TextureFactory::compile_resource( CompileContext& context ) {

    // Open binary file
    FILE* output_file = nullptr;
    fopen_s( &output_file, context.compiled_filename, "wb" );
    // Write Header
    fwrite( context.out_header, sizeof( ResourceHeader ), 1, output_file );
    // Write Data
    fwrite( context.source_file_memory, context.out_header->data_size, 1, output_file );
    fclose( output_file );
}

void* TextureFactory::load( LoadContext& context ) {

    using namespace hydra::graphics;

    int image_width, image_height, channels_in_file;
    unsigned char* image_data = stbi_load_from_memory( (const stbi_uc*)context.resource->data, context.resource->header->data_size, &image_width, &image_height, &channels_in_file, 4 );

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

// ShaderFactory ////////////////////////////////////////////////////////////////

void ShaderFactory::init() {
    shaders_pool.init( 1000, sizeof( hydra::graphics::ShaderEffect ) );
}

void ShaderFactory::terminate() {
    shaders_pool.terminate();
}

void ShaderFactory::compile_resource( CompileContext& context ) {

    char* output_filename = remove_extension_from_filename( context.out_header->id.path, context.temp_string_buffer );

    // Compile to bhfx
    const char* bhfx_filename = context.temp_string_buffer.append_use( "%s.bhfx", output_filename );
    const char* hfx_full_filename = context.temp_string_buffer.append_use( "%s%s", context.resource_manager->get_resource_source_folder(), context.out_header->id.path );
    hfx::compile_hfx( hfx_full_filename, context.resource_manager->get_resource_binary_folder(), bhfx_filename );

    // Read the newly generated bhfx file
    char* bhfx_memory = hydra::read_file_into_memory( context.compiled_filename, &context.out_header->data_size );

    FILE* output_file = nullptr;
    fopen_s( &output_file, context.compiled_filename, "wb" );
    
    // Write Header
    fwrite( context.out_header, sizeof( ResourceHeader ), 1, output_file );
    // Write Data
    fwrite( bhfx_memory, context.out_header->data_size, 1, output_file );
    fclose( output_file );

    hydra::hy_free( bhfx_memory );
}

void* ShaderFactory::load( LoadContext& context ) {

    using namespace hydra::graphics;

    hfx::ShaderEffectFile shader_effect_file;
    hfx::init_shader_effect_file( shader_effect_file, context.resource->data );

    // 1. Create shader effect
    uint32_t effect_pool_id = shaders_pool.obtain_resource();
    ShaderEffect* effect = new(shaders_pool.access_resource(effect_pool_id)) ShaderEffect();
    effect->pool_id = effect_pool_id;
    effect->init( shader_effect_file );
    
    bool invalid_effect = false;

    // 2. Create pipelines
    for ( uint16_t p = 0; p < effect->num_passes; p++ ) {
        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file.memory, p );

        ShaderEffectPass& shader_pass = effect->passes[p];
        memcpy( shader_pass.name, pass_header->stage_name, 32 );

        PipelineCreation& pipeline_creation = shader_pass.pipeline_creation;
        memset( &pipeline_creation, 0, sizeof( PipelineCreation ) );

        hfx::get_pipeline( pass_header, pipeline_creation );

        // 2.1 Create Resource Set Layouts
        for ( uint16_t l = 0; l < pass_header->num_resource_layouts; l++ ) {

            uint8_t num_bindings = 0;
            const ResourceListLayoutCreation::Binding* bindings = get_pass_layout_bindings( pass_header, l, num_bindings );
            ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };

            pipeline_creation.resource_list_layout[l] = context.device.create_resource_list_layout( resource_layout_creation );
        }

        pipeline_creation.num_active_layouts = pass_header->num_resource_layouts;

        // Create pipeline
        shader_pass.pipeline_handle = context.device.create_pipeline( pipeline_creation );
        if ( shader_pass.pipeline_handle.handle == k_invalid_handle ) {
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

        ShaderEffectPass& pass = effect->passes[i];
        for ( size_t l = 0; l < pass.pipeline_creation.num_active_layouts; ++l ) {
            device.destroy_resource_list_layout( pass.pipeline_creation.resource_list_layout[l] );
        }

        device.destroy_pipeline( pass.pipeline_handle );
    }

    shaders_pool.release_resource( effect->pool_id );
}

void ShaderFactory::reload( Resource* old_resource, Resource* new_resource, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {

    using namespace hydra::graphics;
    ShaderEffect* effect = (ShaderEffect*)old_resource->asset;

    // Compile hfx file

    // Open binary hfx file
    hfx::ShaderEffectFile shader_effect_file;
    hfx::init_shader_effect_file( shader_effect_file, new_resource->data );

    effect->init( shader_effect_file );
    
    for ( uint16_t p = 0; p < effect->num_passes; p++ ) {
        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( new_resource->data, p );

        ShaderEffectPass& shader_pass = effect->passes[p];
        memcpy( shader_pass.name, pass_header->stage_name, 32 );

        PipelineCreation& pipeline_creation = shader_pass.pipeline_creation;
        memset( &pipeline_creation, 0, sizeof( PipelineCreation ) );

        hfx::get_pipeline( pass_header, pipeline_creation );

        // 2.1 Create Resource Set Layouts
        for ( uint16_t l = 0; l < pass_header->num_resource_layouts; l++ ) {

            uint8_t num_bindings = 0;
            const ResourceListLayoutCreation::Binding* bindings = get_pass_layout_bindings( pass_header, l, num_bindings );
            ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };

            pipeline_creation.resource_list_layout[l] = gfx_device.create_resource_list_layout( resource_layout_creation );
        }

        pipeline_creation.num_active_layouts = pass_header->num_resource_layouts;

        // Create pipeline
        shader_pass.pipeline_handle = gfx_device.create_pipeline( pipeline_creation );
        //if ( shader_pass.pipeline_handle.handle == k_invalid_handle ) {
        //    //invalid_effect = true;
        //    break;
        //}
    }
}


// MaterialFactory //////////////////////////////////////////////////////////////

void MaterialFactory::init() {

    materials_pool.init( 512, sizeof( hydra::graphics::Material ));
}

void MaterialFactory::terminate() {
    
    materials_pool.terminate();

}

void MaterialFactory::compile_resource( CompileContext& context ) {
    FILE* output_file = nullptr;
    
    rapidjson::Document document;
    if ( document.Parse<0>( context.source_file_memory ).HasParseError() ) {
        hydra::print_format( "JSON error parsing file %s\n", context.out_header->id.path );
        return;
    }

    hydra::graphics::MaterialFile::Header material_file_header;

    ResourceID* references = context.out_references;
    ResourceHeader& resource_header = *context.out_header;

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
    if (properties.GetArray().Size() > 0) {
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
    }

    // Count bindings
    const rapidjson::Value& bindings = document["bindings"];
    material_file_header.num_bindings = bindings.Size();

    // Count sampler bindings (associations between a resource and a sampler)
    uint8_t sampler_bindings = 0;

    for ( rapidjson::SizeType i = 0; i < bindings.Size(); i++ ) {
        if ( bindings[i].HasMember( "sampler" ) ) {
            ++sampler_bindings;
        }
    }

    material_file_header.num_sampler_bindings = sampler_bindings;

    fopen_s( &output_file, context.compiled_filename, "wb" );
    fwrite( &resource_header, sizeof( ResourceHeader ), 1, output_file );
    fwrite( &references[0], sizeof( ResourceID ), resource_header.num_external_references, output_file );
            
    // Write material data

    fwrite( &material_file_header, sizeof( hydra::graphics::MaterialFile::Header ), 1, output_file );

    // Write properties
    hydra::graphics::MaterialFile::Property material_property;
    if ( properties.GetArray().Size() > 0 ) {
        const rapidjson::Value& property_container = properties[0];
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
    }

    // Write bindings
    hydra::graphics::MaterialFile::Binding material_binding;
    for ( rapidjson::SizeType i = 0; i < bindings.Size(); i++ ) {
        const rapidjson::Value& binding = bindings[i];

        if ( binding.HasMember( "name" ) && binding.HasMember( "resource_name" ) ) {

            strcpy( material_binding.name, binding["name"].GetString() );
            strcpy( material_binding.value, binding["resource_name"].GetString() );

            // Output to file
            fwrite( &material_binding, sizeof( hydra::graphics::MaterialFile::Binding ), 1, output_file );
        }
    }
    // Write sampler bindings
    for ( rapidjson::SizeType i = 0; i < bindings.Size(); i++ ) {
        const rapidjson::Value& binding = bindings[i];

        if ( binding.HasMember( "name" ) && binding.HasMember( "sampler" ) ) {
            strcpy( material_binding.name, binding["name"].GetString() );
            strcpy( material_binding.value, binding["sampler"].GetString() );

            // Output to file
            fwrite( &material_binding, sizeof( hydra::graphics::MaterialFile::Binding ), 1, output_file );
        }
    }

    fclose( output_file );
}

void* MaterialFactory::load( LoadContext& context ) {
    
    using namespace hydra::graphics;

    // 1. Read header
    MaterialFile material_file;
    material_file.header = (MaterialFile::Header*)context.resource->data;
    material_file.property_array = (MaterialFile::Property*)(context.resource->data + sizeof( MaterialFile::Header ));
    material_file.binding_array = (MaterialFile::Binding*)(context.resource->data + sizeof( MaterialFile::Header ) + sizeof( MaterialFile::Property ) * material_file.header->num_properties);
    material_file.sampler_binding_array = ( MaterialFile::Binding* )((char*)material_file.binding_array + sizeof(MaterialFile::Binding) * material_file.header->num_bindings );

    // 2. Read shader effect
    Resource* shader_effect_resource = string_hash_get( context.resource->name_to_external_resources, material_file.header->hfx_filename );
    ShaderEffect* shader_effect = shader_effect_resource ? (ShaderEffect*)shader_effect_resource->asset : nullptr;
    if ( !shader_effect ) {
        hydra::print_format( "Error loading shader effect %s\n", material_file.header->hfx_filename );
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

        hfx::ShaderEffectFile::MaterialProperty* material_property = (hfx::ShaderEffectFile::MaterialProperty*)string_hash_get( material->effect->name_to_property, property.name );
        if ( !material_property ) {
            hydra::print_format( "ERROR: Material % s - Cannot find property %s.\n", material->name, property.name );
            continue;
        }

        switch ( material_property->type ) {
            case hfx::Property::Texture2D:
            {
                const char* texture_path = material->loaded_string_buffer.append_use( property.data );
                Resource* texture_resource = string_hash_get( context.resource->name_to_external_resources, texture_path );

                if ( texture_resource ) {
                    Texture* texture = (Texture*)texture_resource->asset;
                    texture->filename = texture_path;

                    context.render_pipeline->resource_database.register_texture( property.name, texture->handle );
                    material->textures[current_texture++] = texture;
                }
                else {
                    hydra::print_format( "ERROR: Material %s - Cannot find texture resource %s for property %s", material->name, texture_path, property.name );
                    material->textures[current_texture++] = nullptr;
                }

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

    // Add sampler bindings
    for ( size_t b = 0; b < material_file.header->num_sampler_bindings; ++b ) {
        MaterialFile::Binding& binding = material_file.sampler_binding_array[b];

        char* name = material->loaded_string_buffer.append_use( binding.name );
        char* value = material->loaded_string_buffer.append_use( binding.value );
        material->lookups.add_binding_to_sampler( name, value );
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

    // Create constant buffer only if needed
    if ( material->effect->local_constants_size ) {
        BufferCreation constants_creation = {};
        constants_creation.type = BufferType::Constant;
        // Prepend material name to ensure uniqueness of it.
        constants_creation.name = material->loaded_string_buffer.append_use( "%s_%s", material->name, s_local_constants_name );
        constants_creation.usage = ResourceUsageType::Dynamic;
        constants_creation.size = shader_effect->local_constants_size;
        constants_creation.initial_data = material->local_constants_data;

        material->local_constants_buffer = context.device.create_buffer( constants_creation );
        context.render_pipeline->resource_database.register_buffer( (char*)constants_creation.name, material->local_constants_buffer );

        // Add binding with the correct name
        material->lookups.add_binding_to_resource( (char*)s_local_constants_name, (char*)constants_creation.name );
    }

    // 5. Bind material to pipeline
    //for ( uint8_t p = 0; p < shader_effect->num_passes; ++p ) {
    //    char* stage_name = shader_effect->passes[p].name;
    //    hydra::graphics::RenderStage* stage = string_hash_get( context.render_pipeline->name_to_stage, stage_name );

    //    if ( stage ) {
    //        stage->material = material;
    //        stage->pass_index = (uint8_t)p;
    //    }
    //}

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

void MaterialFactory::reload( Resource* old_resource, Resource* new_resource, StringBuffer& temp_string_buffer, hydra::graphics::Device& gfx_device, hydra::graphics::RenderPipeline* render_pipeline ) {
    
    using namespace hydra::graphics;
    Material* material = (Material*)old_resource->asset;

    material->load_resources( render_pipeline->resource_database, gfx_device );
}

} // namespace hydra
