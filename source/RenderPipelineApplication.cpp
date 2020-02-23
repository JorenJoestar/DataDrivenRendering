
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_USE_RAPIDJSON
#include "tiny_gltf.h"

#include "RenderPipelineApplication.h"
#include "ShaderCodeGenerator.h"
#include "MaterialSystem.h"

#include "hydra/hydra_resources.h"

#include "imgui_node_editor.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#include <stb_image.h>

#include "SDL_scancode.h"
#include "SDL_mouse.h"

// cglm includes
#include "cglm/types-struct.h"
#include "cglm/struct/vec3.h"
#include "cglm/struct/project.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/box.h"

namespace ed = ax::NodeEditor;

static ed::EditorContext* g_node_editor_context = nullptr;
static hydra::ResourceManager g_resource_manager;
static hydra::graphics::BufferHandle shadertoy_buffer;

// Graph ////////////////////////////////////////////////////////////////////////

struct GraphNode {
    uint16_t                        id;
    uint16_t                        data_offset;
    uint8_t                         type;
};

struct GraphLink {
    uint16_t                        link_id;
    uint16_t                        start_node_id;
    uint16_t                        end_node_id;
    uint8_t                         start_pin_id_offset;
    uint8_t                         end_pin_id_offset;
};

struct Graph {

    void                            init();

    array( GraphNode )              graph_nodes;
    array( GraphLink )              graph_links;
};

void Graph::init() {
    array_free( graph_nodes );
    array_free( graph_links );
}

struct TextureHandleNodeIdMap {
    hydra::graphics::TextureHandle  key;
    uint16_t                        value;
};

static Graph render_graph;
static hash_map( TextureHandleNodeIdMap ) texture_to_node = nullptr;


// Mesh/Models //////////////////////////////////////////////////////////////////

static int32_t find_texture_index( tinygltf::ParameterMap& material_values, const char* parameter_name ) {
    if ( material_values.find( parameter_name ) != material_values.end() ) {
        tinygltf::Parameter& parameter = material_values[parameter_name];

        auto iterator = parameter.json_double_value.find( "index" );
        if ( iterator != parameter.json_double_value.end() ) {

            return (int32_t)iterator->second;
        }
    }

    return -1;
}

static void create_mesh( hydra::graphics::Device& device, tinygltf::Model& model, tinygltf::Mesh& mesh, 
                         hydra::graphics::RenderScene& render_scene, hydra::graphics::Mesh& render_mesh,
                         hydra::ResourceManager& resource_manager, hydra::StringBuffer& string_buffer,
                         hydra::graphics::RenderPipeline* render_pipeline, const mat4s& world_transform ) {

    // Scan through primitives/sub-meshes
    for ( size_t i = 0; i < mesh.primitives.size(); ++i ) {
        tinygltf::Primitive primitive = mesh.primitives[i];

        hydra::graphics::SubMesh sub_mesh = { 0, 0, nullptr, 0 };

        if ( primitive.indices >= 0 ) {
            tinygltf::Accessor& index_buffer_accessor = model.accessors[primitive.indices];
            
            sub_mesh.index_buffer = render_scene.buffers[index_buffer_accessor.bufferView];
            sub_mesh.start_index = index_buffer_accessor.byteOffset / tinygltf::GetComponentSizeInBytes( index_buffer_accessor.componentType );
            sub_mesh.end_index = index_buffer_accessor.count;
        }

        const std::map<std::string, int>& primitive_attributes = primitive.attributes;

        array_set_length( sub_mesh.vertex_buffers, 3 );
        array_set_length( sub_mesh.vertex_buffer_offsets, 3 );
        // TODO: improve
        // Add vertex layout. For now use fixed position,normal vertex layout.
        for ( auto &vertex_attribute : primitive.attributes ) {
            tinygltf::Accessor vertex_attribute_accessor = model.accessors[vertex_attribute.second];

            // TODO: Add multiple vertex buffers
            if ( (vertex_attribute.first.compare( "NORMAL" ) == 0) ) {
                hydra::graphics::BufferHandle vertex_buffer_handle = render_scene.buffers[vertex_attribute_accessor.bufferView];
                sub_mesh.vertex_buffers[1] = vertex_buffer_handle;
                sub_mesh.vertex_buffer_offsets[1] = vertex_attribute_accessor.byteOffset;
            }
            else if ( (vertex_attribute.first.compare( "POSITION" ) == 0) ) {
                hydra::graphics::BufferHandle vertex_buffer_handle = render_scene.buffers[vertex_attribute_accessor.bufferView];
                sub_mesh.vertex_buffers[0] = vertex_buffer_handle;
                sub_mesh.vertex_buffer_offsets[0] = vertex_attribute_accessor.byteOffset;

                // Add Bounding Box
                if ( vertex_attribute_accessor.minValues.size() == 3 && vertex_attribute_accessor.maxValues.size() == 3 ) {
                    sub_mesh.bounding_box.min = { (float)vertex_attribute_accessor.minValues[0], (float)vertex_attribute_accessor.minValues[1], (float)vertex_attribute_accessor.minValues[2] };
                    sub_mesh.bounding_box.max = { (float)vertex_attribute_accessor.maxValues[0], (float)vertex_attribute_accessor.maxValues[1], (float)vertex_attribute_accessor.maxValues[2] };

                    glms_aabb_transform( sub_mesh.bounding_box.box, world_transform, sub_mesh.bounding_box.box );
                }
            }
            else if ( ( vertex_attribute.first.compare( "TEXCOORD_0" ) == 0 ) ) {
                hydra::graphics::BufferHandle vertex_buffer_handle = render_scene.buffers[vertex_attribute_accessor.bufferView];
                sub_mesh.vertex_buffers[2] = vertex_buffer_handle;
                sub_mesh.vertex_buffer_offsets[2] = vertex_attribute_accessor.byteOffset;
            }
        }

        // Search for material
        tinygltf::Material& material = model.materials[primitive.material];

        hydra::Resource* material_resource = resource_manager.load_resource( hydra::ResourceType::Material, string_buffer.append_use("%s.hmt", material.name.c_str()), device, render_pipeline );

        // If not present, create it.
        if ( !material_resource ) {
            // Create material
            rapidjson::Document document;
            document.SetObject();

            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value json_material_name;
            json_material_name.SetString( material.name.c_str(), material.name.size(), allocator );

            document.AddMember( "name", json_material_name, allocator );
            document.AddMember( "effect_path", "PBR.hfx", allocator );

            // Write bindings
            rapidjson::Value bindings( rapidjson::kArrayType );
            rapidjson::Value json_property_bindings( rapidjson::kObjectType );

            rapidjson::Value json_name, json_value;

            //"LocalConstants": "CB_Lines",
            json_name.SetString( "ViewConstants", allocator );
            json_value.SetString( "CB_Lines", allocator );

            json_property_bindings.AddMember( json_name, json_value, allocator );

            //"Transform" : "Transform"
            json_name.SetString( "Transform", allocator );
            json_value.SetString( "Transform", allocator );

            json_property_bindings.AddMember( json_name, json_value, allocator );

            // Add both properties and bindings
            rapidjson::Value properties( rapidjson::kArrayType );
            rapidjson::Value json_property( rapidjson::kObjectType );

            // Search for textures
            // 1. albedo
            int32_t texture_index = find_texture_index( material.values, "baseColorTexture" );
            if ( texture_index >= 0 ) {
                tinygltf::Texture& tex = model.textures[texture_index];
                tinygltf::Image& image = model.images[tex.source];
                
                json_name.SetString( "albedo", allocator );
                json_value.SetString( image.uri.c_str(), image.uri.size(), allocator );
    
                json_property.AddMember( json_name, json_value, allocator );
                json_property_bindings.AddMember( json_name, json_name, allocator );
            }

            // 2. normals
            texture_index = material.normalTexture.index;

            // 3. metallic/roughness
            texture_index = find_texture_index( material.values, "metallicRoughnessTexture" );
            if ( texture_index >= 0 ) {

            }

            // 4. occlusion

            // 5. emissive

            bindings.PushBack( json_property_bindings, allocator );
            document.AddMember( "bindings", bindings, allocator );

            // Write properties
            // TODO: add PBR properties here.
            
            properties.PushBack( json_property, allocator );
            document.AddMember( "properties", properties, allocator );

            // Write document
            rapidjson::StringBuffer strbuf;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( strbuf );
            document.Accept( writer );

            // Write the material file
            hydra::FileHandle file;
            const char* filename = string_buffer.append_use("..\\data\\source\\%s.hmt", material.name.c_str() );
            hydra::open_file( filename, "w", &file );
            if ( file ) {
                fwrite( strbuf.GetString(), strbuf.GetSize(), 1, file );
                hydra::close_file( file );
            }

            // load newly created material
            material_resource = resource_manager.load_resource( hydra::ResourceType::Material, string_buffer.append_use( "%s.hmt", material.name.c_str() ), device, render_pipeline );
        }

        sub_mesh.material = ( hydra::graphics::Material* )material_resource->asset;
        sub_mesh.material->load_resources( render_pipeline->resource_database, device );

        array_push( render_mesh.sub_meshes, sub_mesh );
    }
}

static void create_meshes_from_node( hydra::graphics::Device& device, tinygltf::Model& model, tinygltf::Node& node,
                                     hydra::graphics::RenderScene& render_scene, hydra::graphics::RenderNode& render_node,
                                     hydra::ResourceManager& resource_manager, hydra::StringBuffer& string_buffer, hydra::graphics::RenderPipeline* render_pipeline ) {

    mat4s world_transform = glms_mat4_identity();

    // Concatenate SRT transforms
    if ( node.scale.size() == 3 ) {
        mat4s matrix = glms_scale_make( { (float)node.scale[0], (float)node.scale[1], (float)node.scale[2] } );
        world_transform = glms_mat4_mul( world_transform, matrix );
    }

    if ( node.rotation.size() == 4 ) {
        mat4s matrix = glms_quat_mat4( { (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3] } );
        world_transform = glms_mat4_mul( world_transform, matrix );
    }

    if ( node.translation.size() == 3 ) {
        mat4s matrix = glms_translate_make( { (float)node.translation[0], (float)node.translation[1], (float)node.translation[2] } );
        world_transform = glms_mat4_mul( world_transform, matrix );
    }

    if ( node.matrix.size() == 16 ) {
        hydra::print_format( "Matrix transform still not implemented!\n" );
        //local_transform = HMM_MultiplyMat4( local_transform, {} );
    }

    // If parent is present, concatenate matrices
    if ( render_node.parent_id != -1 ) {
        world_transform = glms_mat4_mul( render_scene.node_transforms[render_node.parent_id], world_transform );
    }

    // Add local mesh of the node
    if ( node.mesh >= 0 ) {
        render_node.mesh = new hydra::graphics::Mesh();
        create_mesh( device, model, model.meshes[node.mesh], render_scene, *render_node.mesh, resource_manager, string_buffer, render_pipeline, world_transform );
    }

    // Calculate node id.
    render_node.node_id = array_length_u( render_scene.nodes );

    // Add render node and local transform to the scene.
    array_push( render_scene.nodes, render_node );
    array_push( render_scene.node_transforms, world_transform );

    // Add children nodes to the render scene.
    for ( size_t i = 0; i < node.children.size(); i++ ) {

        hydra::graphics::RenderNode children_node = { nullptr, array_length_u( render_scene.nodes ), render_node.node_id };
        create_meshes_from_node( device, model, model.nodes[node.children[i]], render_scene, children_node, resource_manager, string_buffer, render_pipeline );
    }
}

static bool load_model( hydra::graphics::Device& device, tinygltf::Model& model, const char* filename, 
                        hydra::graphics::RenderScene& render_scene, hydra::ResourceManager& resource_manager,
                        hydra::StringBuffer& string_buffer, hydra::graphics::RenderPipeline* render_pipeline ) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool res = loader.LoadASCIIFromFile( &model, &err, &warn, filename );
    if ( !warn.empty() ) {
        hydra::print_format( "GLTF WARNING: %s\n", warn.c_str() );
    }

    if ( !err.empty() ) {
        hydra::print_format( "GLTF ERROR: %s\n", err.c_str() );
    }

    if ( !res )
        hydra::print_format( "Failed to load glTF: %s\n", filename );
    else
        hydra::print_format( "Loaded glTF: %s\n", filename );

    if ( res ) {
        // Create shared graphics resources
        hydra::graphics::BufferCreation buffer_creation;

        array_init( render_scene.buffers );
        array_init( render_scene.nodes );
        array_init( render_scene.node_transforms );

        // Create all buffers (vertex + index)
        for ( size_t i = 0; i < model.bufferViews.size(); ++i ) {
            const tinygltf::BufferView& buffer_view = model.bufferViews[i];
            const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

            switch ( buffer_view.target ) {
                case GL_ARRAY_BUFFER:
                {
                    buffer_creation.type = hydra::graphics::BufferType::Vertex;
                    break;
                }

                case GL_ELEMENT_ARRAY_BUFFER:
                {
                    buffer_creation.type = hydra::graphics::BufferType::Index;
                    break;
                }

                default:
                {
                    hydra::print_format( "Unsupported type %u\n", buffer_view.target );
                    break;
                }
            }

            buffer_creation.initial_data = (void*)(&buffer.data.at( 0 ) + buffer_view.byteOffset);
            buffer_creation.size = buffer_view.byteLength;
            buffer_creation.usage = hydra::graphics::ResourceUsageType::Immutable;
            
            hydra::graphics::BufferHandle buffer_handle = device.create_buffer( buffer_creation );
            
            array_push( render_scene.buffers, buffer_handle );
        }

        // Create meshes for each render node
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];
        for ( size_t i = 0; i < scene.nodes.size(); ++i ) {
            tinygltf::Node& node = model.nodes[scene.nodes[i]];

            hydra::graphics::RenderNode render_node = { nullptr, -1, -1 };
            create_meshes_from_node( device, model, node, render_scene, render_node, resource_manager, string_buffer, render_pipeline );
        }

        // Create shared transformation matrix buffer.
        {
            hydra::graphics::BufferCreation buffer_creation;
            buffer_creation.type = hydra::graphics::BufferType::Constant;
            buffer_creation.initial_data = (void*)( &render_scene.node_transforms[0] );
            buffer_creation.size = array_length_u( render_scene.node_transforms ) * sizeof(mat4s);
            buffer_creation.usage = hydra::graphics::ResourceUsageType::Immutable;

            render_scene.node_transforms_buffer = device.create_buffer( buffer_creation );
        }
    }

    return res;
}

// RenderPipelineApplication ////////////////////////////////////////////////////

void RenderPipelineApplication::app_init() {

    temporary_string_buffer.init( 1024 * 1024 );
    g_resource_manager.init();

    show_grid = true;

    ed::Config config;
    //config.SettingsFile = "Simple.json";
    g_node_editor_context = ed::CreateEditor( &config );

    main_render_view.visible_render_scenes = nullptr;
    main_render_view.camera.init( true, 0.1f, 1000.f );

    render_graph.init();

    camera_input.init();
    camera_movement_update.init( 20.0f, 20.0f );

    hydra::graphics::ShaderResourcesDatabase initial_db;
    initial_db.init();

    // Init lighting manager
    lighting_manager.init( initial_db, gfx_device );

    // Init line renderer
    line_renderer.init( initial_db, gfx_device );

    // Init render pipeline manager
    render_pipeline_manager.init( this->gfx_device, this->temporary_string_buffer );
    string_hash_put( render_pipeline_manager.name_to_render_view, "main", &main_render_view );


    // Choose pipeline
    render_pipeline_manager.set_pipeline( gfx_device, "PBR_Deferred", temporary_string_buffer, initial_db );

    // Initialize graph
    if ( render_pipeline_manager.current_render_pipeline ) {

        uint16_t unique_node_id = 1;

        // Create 1 node for each texture
        hydra::graphics::RenderPipeline::TextureMap* name_to_texture = render_pipeline_manager.current_render_pipeline->name_to_texture;
        for ( size_t t = 0; t < string_hash_length( name_to_texture ); t++ ) {

            const hydra::graphics::RenderPipeline::TextureMap& texture_map_entry = name_to_texture[t];

            hash_map_put( texture_to_node, texture_map_entry.value, unique_node_id );

            GraphNode stage_node{ unique_node_id, (uint16_t)t, 1 };
            array_push( render_graph.graph_nodes, stage_node );

            // A Texture needs 3 ids: node, input pin and output pin
            unique_node_id += 3;
        }

        // Create 1 node for the stage
        hydra::graphics::RenderPipeline::StageMap* name_to_stage = render_pipeline_manager.current_render_pipeline->name_to_stage;
        for ( size_t s = 0; s < string_hash_length( name_to_stage ); s++ ) {

            const hydra::graphics::RenderPipeline::StageMap& stage_map_entry = render_pipeline_manager.current_render_pipeline->name_to_stage[s];
            const hydra::graphics::RenderStage* stage = stage_map_entry.value;

            GraphNode stage_node{ unique_node_id, (uint16_t)s, 0};
            array_push( render_graph.graph_nodes, stage_node );

            // A Stage needs 1 id for the node + input + output pins
            unique_node_id += stage->num_input_textures + stage->num_output_textures + 1;

            // Add links between nodes
            GraphLink link;

            for ( size_t i = 0; i < stage->num_input_textures; ++i ) {
                link.link_id = unique_node_id++;
                // Stage node is the END node of the input texture node.
                // This link will go from the Texture Node to the Stage Node.
                link.end_node_id = stage_node.id;
                link.end_pin_id_offset = i + 1;
                link.start_node_id = hash_map_get( texture_to_node, stage->input_textures[i].handle );
                link.start_pin_id_offset = 2;

                array_push( render_graph.graph_links, link );
            }

            for ( size_t i = 0; i < stage->num_output_textures; ++i ) {
                link.link_id = unique_node_id++;
                // Stage node is the START node of the output texture node.
                // This link will go from the Stage Node to the Texture Node.
                link.start_node_id = stage_node.id;
                link.start_pin_id_offset = stage->num_input_textures + 1 + i;
                link.end_node_id = hash_map_get( texture_to_node, stage->output_textures[i].handle );
                link.end_pin_id_offset = 1;

                array_push( render_graph.graph_links, link );
            }
        }

        hydra::graphics::RenderStage* debug_rendering_stage = string_hash_get( render_pipeline_manager.current_render_pipeline->name_to_stage, "DebugRendering" );
        if ( debug_rendering_stage ) {
            debug_rendering_stage->register_render_manager( &line_renderer );
        }

        debug_rendering_stage = string_hash_get( render_pipeline_manager.current_render_pipeline->name_to_stage, "DeferredLights" );
        if ( debug_rendering_stage ) {
            debug_rendering_stage->register_render_manager( &lighting_manager );
        }
    }

    // Load line material
    char* material_filename = temporary_string_buffer.append_use( "%s.hmt", "Lines" );
    hydra::Resource* material_resource = g_resource_manager.load_resource( hydra::ResourceType::Material, material_filename, gfx_device, render_pipeline_manager.current_render_pipeline );
    line_renderer.line_material = (hydra::graphics::Material*)material_resource->asset;

    // Load scene
    tinygltf::Model loaded_model;

    hydra::graphics::RenderScene render_scene;
    //load_model( gfx_device, loaded_model, "../data/GLTF/Lantern/Lantern.gltf", render_scene, g_resource_manager, temporary_string_buffer, render_pipeline_manager.current_render_pipeline );
    //load_model( gfx_device, loaded_model, "../data/GLTF/Box/Box.gltf", render_scene, g_resource_manager, temporary_string_buffer, render_pipeline_manager.current_render_pipeline );
    load_model( gfx_device, loaded_model, "../data/source/GLTF/DamagedHelmet/DamagedHelmet.gltf", render_scene, g_resource_manager, temporary_string_buffer, render_pipeline_manager.current_render_pipeline );
    render_scene.render_manager = &scene_renderer;

    render_pipeline_manager.current_render_pipeline->resource_database.register_buffer( (char*)"Transform", render_scene.node_transforms_buffer );

    line_renderer.line_material->load_resources( render_pipeline_manager.current_render_pipeline->resource_database, gfx_device );


    hydra::graphics::RenderStage* rendering_stage = string_hash_get( render_pipeline_manager.current_render_pipeline->name_to_stage, "GBufferOpaque" );
    render_scene.stage_mask.value = rendering_stage ? rendering_stage->geometry_stage_mask : 0;

    scene_renderer.material = line_renderer.line_material;

    array_push( main_render_view.visible_render_scenes, render_scene );

    reload_shaders = -1;
}

void RenderPipelineApplication::app_terminate() {

}

void RenderPipelineApplication::app_render( hydra::graphics::CommandBuffer* commands ) {

    if ( reload_shaders > 0 ) {
        --reload_shaders;

        return;
    }

    if ( reload_shaders == 0 ) {
        g_resource_manager.reload_resources( hydra::ResourceType::Material, gfx_device, render_pipeline_manager.current_render_pipeline );
        reload_shaders = -1;
    }

    auto& io = ImGui::GetIO();

    hydra::graphics::Camera& camera = main_render_view.camera;

    float window_center_x = gfx_device.swapchain_width / 2.0f;
    float window_center_y = gfx_device.swapchain_height / 2.0f;

    camera_input.update( io, camera, window_center_x, window_center_y );
    camera_movement_update.update( camera, camera_input, io.DeltaTime );

    if ( camera_input.mouse_dragging ) {
        SDL_WarpMouseInWindow( window, window_center_x, window_center_y );
        SDL_SetWindowGrab( window, SDL_TRUE );
    }
    else {
        SDL_SetWindowGrab( window, SDL_FALSE );
    }
    
    camera.update( gfx_device );

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

    // Draw grid
    if ( show_grid ) {
        const int32_t num_cells = 16;
        for ( int32_t i = -num_cells; i <= num_cells; ++i ) {

            float x = i * 1.0f;
            float z = num_cells * 1.0f;
            line_renderer.line( { x, 0, -z }, { x, 0, z }, hydra::graphics::ColorUint::white, hydra::graphics::ColorUint::white );
            line_renderer.line( { -z, 0, x }, { z, 0, x }, hydra::graphics::ColorUint::white, hydra::graphics::ColorUint::white );
        }
    }

    // Draw view orientation axis.
    // Calculate a decentered world position for the starting point.
    vec3s world_position = glms_vec3_add( camera.position, glms_vec3_scale( camera.direction, -1.5f ) );
    world_position = glms_vec3_add( world_position, glms_vec3_scale( camera.right, -1.333f ) );
    world_position = glms_vec3_add( world_position, glms_vec3_scale( camera.up, -0.8f ) );
    
    // Draw 3 lines in world space in the camera view.
    static float axis_length = 0.1f;
    line_renderer.line( world_position, glms_vec3_add( world_position, { axis_length,0,0 } ), hydra::graphics::ColorUint::red, hydra::graphics::ColorUint::red );
    line_renderer.line( world_position, glms_vec3_add( world_position, { 0,axis_length,0 } ), hydra::graphics::ColorUint::green, hydra::graphics::ColorUint::green );
    line_renderer.line( world_position, glms_vec3_add( world_position, { 0,0,-axis_length } ), hydra::graphics::ColorUint::blue, hydra::graphics::ColorUint::blue );

    static hydra::graphics::SubMesh* submesh_picked = nullptr;

    if ( submesh_picked ) {
        line_renderer.box( submesh_picked->bounding_box, hydra::graphics::ColorUint::red );
    }

    // Picking
    if ( ImGui::IsMouseClicked( 0 ) ) {
        ImVec2 click_position = ImGui::GetIO().MouseClickedPos[0];
        vec3s screen_space_position{ click_position.x, click_position.y, 0 };
        vec4s viewport{ 0, 0, gfx_device.swapchain_width, gfx_device.swapchain_height };
        vec3s world_position = glms_unproject( screen_space_position, camera.view_projection, viewport );

        hydra::graphics::Ray ray{ world_position, camera.direction };

        for ( size_t i = 0; i < array_length( main_render_view.visible_render_scenes ); ++i ) {
            hydra::graphics::RenderScene& render_scene = main_render_view.visible_render_scenes[i];

            // For each node
            for ( size_t n = 0; n < array_length( render_scene.nodes ); ++n ) {
                hydra::graphics::RenderNode& render_node = render_scene.nodes[n];

                if ( !render_node.mesh ) {
                    continue;
                }

                // For each submesh
                for ( size_t s = 0; s < array_length( render_node.mesh->sub_meshes ); ++s ) {
                    hydra::graphics::SubMesh& sub_mesh = render_node.mesh->sub_meshes[s];

                    float t;
                    if ( hydra::graphics::ray_box_intersection( sub_mesh.bounding_box, ray, t ) ) {
                        submesh_picked = &sub_mesh;
                    }
                }
            }
        }
    }

    // Render the pipeline
    if ( render_pipeline_manager.current_render_pipeline ) {
        render_pipeline_manager.current_render_pipeline->render( gfx_device, commands );
    }

    // Taken from NodeEditor simple example:
    ImGui::Text( "FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f );

    ImGui::Separator();

    ed::SetCurrentEditor( g_node_editor_context );
    ed::Begin( "Render Pipeline Editor", ImVec2( 0.0, 0.0f ) );

    // Draw all nodes (both stages and textures)
    for ( size_t i = 0; i < array_length(render_graph.graph_nodes); ++i ) {
        const GraphNode& node = render_graph.graph_nodes[i];

        // Stage node
        if ( node.type == 0 ) {
            hydra::graphics::RenderPipeline::StageMap& stage_map_entry = render_pipeline_manager.current_render_pipeline->name_to_stage[node.data_offset];
            hydra::graphics::RenderStage* stage = stage_map_entry.value;
            ed::BeginNode( node.id );

            ImGui::Text( stage_map_entry.key );
            ImGui::Text("");

            // Write pins on the same line.
            // Node + 1 because first pin id follows the node id.
            const uint16_t input_pin_id = node.id + 1;
            const uint16_t output_pin_id = input_pin_id + stage->num_input_textures;
            const size_t max_textures = std::max( stage->num_input_textures, stage->num_output_textures );
            
            for ( size_t i = 0; i < max_textures; ++i ) {

                ImGui::BeginGroup();

                if ( i < stage->num_input_textures ) {
                    ed::BeginPin( input_pin_id + i, ed::PinKind::Input );
                    ImGui::Text( "-> In" );
                    ed::EndPin();
                }
                else {
                    ImGui::SameLine(50);
                }

                if ( i < stage->num_output_textures ) {
                    ed::BeginPin( output_pin_id + i, ed::PinKind::Output );
                    ImGui::Text( "Out ->" );
                    ed::EndPin();
                }

                ImGui::EndGroup();
            }

            ed::EndNode();
        }
        else {
            // Texture node
            const hydra::graphics::RenderPipeline::TextureMap& texture_map_entry = render_pipeline_manager.current_render_pipeline->name_to_texture[node.data_offset];

            ed::BeginNode( node.id );
            ed::BeginPin( node.id + 1, ed::PinKind::Input );
            ImGui::Text( "-> In" );
            ed::EndPin();
            ImGui::SameLine();
            ImGui::Text( texture_map_entry.key );
            ImGui::SameLine();
            ed::BeginPin( node.id + 2, ed::PinKind::Output );
            ImGui::Text( "-> Out" );
            ed::EndPin();

            hydra::graphics::TextureDescription texture_description;
            gfx_device.query_texture( texture_map_entry.value, texture_description );

            ImGui::Text( "Size %u,%u", texture_description.width, texture_description.height );
            ImGui::Text( "Format %s", hydra::graphics::TextureFormat::s_value_names[texture_description.format] );

            ImGui::Image( (ImTextureID)( &texture_map_entry.value ), ImVec2( 128, 128 ), ImVec2( 0, 1 ), ImVec2( 1, 0) );
            
            ed::EndNode();
        }
    }

    for ( size_t i = 0; i < array_length( render_graph.graph_links ); ++i ) {
        const GraphLink& link = render_graph.graph_links[i];

        ed::Link( link.link_id, link.start_node_id + link.start_pin_id_offset, link.end_node_id + link.end_pin_id_offset );
    }

    ed::End();
    ed::SetCurrentEditor( nullptr );

    // Camera
    if ( ImGui::Begin( "Camera" ) ) {
        ImGui::SameLine();
        if ( ImGui::Button( "Reset" ) ) {
            camera.position = glms_vec3_zero();
            camera.yaw = 0;
            camera.pitch = 0;

            camera_input.reset();
        }

        ImGui::Text( "Position %f, %f, %f", camera.position.x, camera.position.y, camera.position.z );
        ImGui::Text( "Direction %f, %f, %f", camera.direction.x, camera.direction.y, camera.direction.z );
        ImGui::Text( "Up %f, %f, %f", camera.up.x, camera.up.y, camera.up.z );
        ImGui::Text( "Right %f, %f, %f", camera.right.x, camera.right.y, camera.right.z );
        ImGui::Text( "Mouse %f, %f", ImGui::GetMousePos().x, ImGui::GetMousePos().y );
        ImGui::Text( "Mouse Drag %f, %f", gfx_device.swapchain_width / 2.0f - io.MousePos.x, ImGui::GetMouseDragDelta().y );

        if ( ImGui::Button( "Orthographic" ) ) {
            camera.perspective = false;
            camera.update_projection = true;
        }
        if ( ImGui::Button( "Perspective" ) ) {
            camera.perspective = true;
            camera.update_projection = true;
        }
    }
    ImGui::End();

    if ( ImGui::Begin( "Application" ) ) {
        ImGui::DragFloat3( "Point Light Position", &lighting_manager.point_light_position.x, 0.1f, -30.0f, 30.0f );
        ImGui::DragFloat( "Point Light Intensity", &lighting_manager.point_light_intensity, 0.1f, 0, 100 );

        ImGui::DragFloat3( "Directional Light", &lighting_manager.directional_light.x, 0.1f, -1, 1 );

        ImGui::Checkbox( "Use Point Light", &lighting_manager.use_point_light );

        ImGui::Checkbox( "Show Grid", &show_grid );

        if ( ImGui::Button( "Reload Shaders" ) ) {
            reload_shaders = 3;
        }
    }

    ImGui::End();
}

void RenderPipelineApplication::app_resize( uint16_t width, uint16_t height ) {

    main_render_view.camera.update_projection = true;
    
    if ( render_pipeline_manager.current_render_pipeline ) {
        render_pipeline_manager.current_render_pipeline->resize( width, height, gfx_device );
    }
}


// RenderPipelineCreation ///////////////////////////////////////////////////////

void RenderPipelineCreation::init() {
    
    render_stages = nullptr;
    string_hash_init_arena( name_to_textures );

    string_buffer.init( 1000 );
}

void RenderPipelineCreation::terminate() {

}

bool RenderPipelineCreation::is_valid() {
    return true;
}

// CameraInput //////////////////////////////////////////////////////////////////
void CameraInput::init() {

    reset();
}

void CameraInput::reset() {

    target_yaw = 0.0f;
    target_pitch = 0.0f;

    target_movement = glms_vec3_zero();
    
    mouse_dragging = false;
    ignore_dragging_frames = 3;
    mouse_sensitivity = 0.005f;
    movement_delta = 0.03f;
}

void CameraInput::update( ImGuiIO& input, const hydra::graphics::Camera& camera, uint16_t window_center_x, uint16_t window_center_y ) {
    // Ignore first dragging frames for mouse movement waiting the cursor to be placed at the center of the screen.

    if ( ImGui::IsMouseDragging( 1 ) && !ImGui::IsAnyItemHovered() ) {

        if ( ignore_dragging_frames == 0 ) {
            target_yaw -= (input.MousePos.x - window_center_x) * mouse_sensitivity;
            target_pitch += (input.MousePos.y - window_center_y) * mouse_sensitivity;
        }
        else {
            --ignore_dragging_frames;
        }
        mouse_dragging = true;

    }
    else {
        mouse_dragging = false;

        ignore_dragging_frames = 3;
    }

    vec3s camera_movement{ 0, 0, 0 };
    float camera_movement_delta = movement_delta;

    if ( ImGui::IsKeyDown( SDL_SCANCODE_RSHIFT ) ) {
        camera_movement_delta *= 10.0f;
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_RCTRL ) ) {
        camera_movement_delta *= 0.1f;
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_LEFT ) ) {
        camera_movement = glms_vec3_scale( camera.right, -camera_movement_delta );
    }
    else if ( ImGui::IsKeyDown( SDL_SCANCODE_RIGHT ) ) {
        camera_movement = glms_vec3_scale( camera.right, camera_movement_delta );
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_PAGEDOWN ) ) {
        camera_movement = glms_vec3_scale( camera.up, -camera_movement_delta );
    }
    else if ( ImGui::IsKeyDown( SDL_SCANCODE_PAGEUP ) ) {
        camera_movement = glms_vec3_scale( camera.up, camera_movement_delta );
    }

    if ( ImGui::IsKeyDown( SDL_SCANCODE_UP ) ) {
        camera_movement = glms_vec3_scale( camera.direction, -camera_movement_delta );
    }
    else if ( ImGui::IsKeyDown( SDL_SCANCODE_DOWN ) ) {
        camera_movement = glms_vec3_scale( camera.direction, camera_movement_delta );
    }

    target_movement = glms_vec3_add( target_movement, camera_movement );
}

// CameraMovementUpdate /////////////////////////////////////////////////////////

void CameraMovementUpdate::init( float rotation_speed, float movement_speed ) {
    this->rotation_speed = rotation_speed;
    this->movement_speed = movement_speed;
}

void CameraMovementUpdate::update( hydra::graphics::Camera& camera, CameraInput& camera_input, float delta_time ) {
    // Update camera rotation
    const float tween_speed = rotation_speed * delta_time;
    camera.yaw += (camera_input.target_yaw - camera.yaw) * tween_speed;
    camera.pitch += (camera_input.target_pitch - camera.pitch) * tween_speed;

    // Update camera position
    const float tween_position_speed = movement_speed * delta_time;
    vec3s delta_movement { camera_input.target_movement };
    delta_movement = glms_vec3_scale( delta_movement, tween_position_speed );

    camera.position = glms_vec3_add( delta_movement, camera.position );

    // Remove delta movement from target movement
    camera_input.target_movement = glms_vec3_sub( camera_input.target_movement, delta_movement );
}


// RenderPipelineManager ////////////////////////////////////////////////////////

void RenderPipelineManager::init( hydra::graphics::Device& device, hydra::StringBuffer& temp_string_buffer ) {
    
    render_pipeline_creations = nullptr;

     // Load pipelines
    const char* resource_full_filename = temp_string_buffer.append_use( "..\\data\\source\\RenderPipelines.json" );
    char* file_memory = hydra::read_file_into_memory( resource_full_filename, nullptr );

    if ( file_memory ) {

        using namespace rapidjson;
        Document document;

        if ( !document.Parse<0>( file_memory ).HasParseError() ) {
            const Value& pipelines = document["RenderPipelines"];

            if ( pipelines.IsArray() ) {
                const Value::ConstArray& pipeline_array = pipelines.GetArray();

                for ( uint32_t p = 0; p < pipeline_array.Size(); ++p ) {

                    // 1. Parse all the render pipeline data
                    const auto& render_pipeline_definition = pipeline_array[p];

                    RenderPipelineCreation render_pipeline_creation;
                    render_pipeline_creation.init();

                    // Retrieve pipeline name and if not present just create one.
                    char* pipeline_name = nullptr;

                    if ( render_pipeline_definition.HasMember( "name" ) ) {
                        const Value& name = render_pipeline_definition["name"];

                        pipeline_name = temp_string_buffer.append_use( name.GetString() );
                    }
                    else {
                        pipeline_name = temp_string_buffer.append_use( "unnamed_%u", p );
                    }
                    
                    strcpy_s( render_pipeline_creation.name, 32, pipeline_name );

                    // Parse textures from files
                    if ( render_pipeline_definition.HasMember( "Textures" ) ) {
                        const Value::ConstArray& textures = render_pipeline_definition["Textures"].GetArray();

                        for ( uint32_t t = 0; t < textures.Size(); ++t ) {
                            const auto& texture_definition = textures[t];

                            RenderPipelineTextureCreation* texture_creation = new RenderPipelineTextureCreation();
                            texture_creation->path[0] = 0;

                            const Value& name = texture_definition["name"];
                            const Value& path = texture_definition["path"];

                            strcpy_s( texture_creation->name, 32, name.GetString() );
                            strcpy_s( texture_creation->path, 512, path.GetString() );
                            texture_creation->texture_creation.render_target = 0;

                            string_hash_put( render_pipeline_creation.name_to_textures, texture_creation->name, texture_creation );
                        }
                    }
                    
                    // Parse render targets
                    if ( render_pipeline_definition.HasMember( "RenderTargets" ) ) {
                        const Value::ConstArray& textures = render_pipeline_definition["RenderTargets"].GetArray();

                        for ( uint32_t t = 0; t < textures.Size(); ++t ) {
                            const auto& texture_definition = textures[t];

                            RenderPipelineTextureCreation* texture_creation = new RenderPipelineTextureCreation();
                            texture_creation->path[0] = 0;

                            const Value& name = texture_definition["name"];
                            strcpy_s( texture_creation->name, 32, name.GetString() );

                            texture_creation->texture_creation.render_target = 1;

                            const char* texture_format_name = texture_definition["format"].GetString();
                            for ( size_t f = 0; f < hydra::graphics::TextureFormat::Count; ++f ) {
                                if ( strcmp(hydra::graphics::TextureFormat::s_value_names[f], texture_format_name ) == 0 ) {

                                    texture_creation->texture_creation.format = (hydra::graphics::TextureFormat::Enum)f;
                                    break;
                                }
                            }

                            string_hash_put( render_pipeline_creation.name_to_textures, texture_creation->name, texture_creation );
                        }
                    }

                    // TODO: Parse render states

                    // Parse stages
                    if ( render_pipeline_definition.HasMember( "RenderStages" ) ) {
                        const Value::ConstArray& render_stages = render_pipeline_definition["RenderStages"].GetArray();

                        for ( uint32_t t = 0; t < render_stages.Size(); ++t ) {
                            const auto& render_stage = render_stages[t];

                            RenderStageCreation render_stage_creation;
                            const Value& name = render_stage["name"];
                            strcpy_s( render_stage_creation.name, 32, name.GetString() );

                            // Material name and index - used for 'post process' effects.
                            if ( render_stage.HasMember( "material_name" ) ) {
                                strcpy_s( render_stage_creation.material_name, 32, render_stage["material_name"].GetString() );
                            }
                            else {
                                render_stage_creation.material_name[0] = 0;
                            }

                            if ( render_stage.HasMember( "material_pass_index" ) ) {
                                render_stage_creation.material_pass_index = (uint8_t)render_stage["material_pass_index"].GetInt();
                            }
                            else {
                                render_stage_creation.material_pass_index = 0;
                            }

                            const Value& type = render_stage["type"];
                            const char* type_string = type.GetString();
                            if ( strcmp(type_string, "Geometry" ) == 0 ) {
                                render_stage_creation.stage_type = (uint8_t)hydra::graphics::RenderStage::Geometry;
                            }
                            else if ( strcmp( type_string, "Post" ) == 0 ) {
                                render_stage_creation.stage_type = (uint8_t)hydra::graphics::RenderStage::Post;
                            }
                            else if ( strcmp( type_string, "PostCompute" ) == 0 ) {
                                render_stage_creation.stage_type = (uint8_t)hydra::graphics::RenderStage::PostCompute;
                            }
                            else {
                                render_stage_creation.stage_type = (uint8_t)hydra::graphics::RenderStage::Swapchain;
                            }

                            if ( render_stage.HasMember( "render_view" ) ) {
                                strcpy_s( render_stage_creation.render_view_name, 32, render_stage["render_view"].GetString() );
                            }
                            else {
                                render_stage_creation.render_view_name[0] = 0;
                            }

                            render_stage_creation.overriding_lookups.init();

                            // Get inputs
                            const Value::ConstArray& input_textures = render_stage["inputs"].GetArray();
                            for ( uint32_t i = 0; i < input_textures.Size(); ++i ) {
                                const auto& input_texture = input_textures[i];
                                
                                const Value& input_texture_name = input_texture["name"];
                                const char* texture_cstring = render_pipeline_creation.string_buffer.append_use( input_texture_name.GetString());
                                render_stage_creation.inputs[i] = string_hash_get( render_pipeline_creation.name_to_textures, texture_cstring);

                                // Add to the lookups
                                const char* binding_cstring = render_pipeline_creation.string_buffer.append_use( input_texture["binding"].GetString() );
                                render_stage_creation.overriding_lookups.add_binding_to_resource((char*)binding_cstring, (char*)texture_cstring);

                                // TODO: Add sampling
                            }

                            render_stage_creation.input_count = (uint32_t)input_textures.Size();

                            // Get outputs
                            const Value& output = render_stage["outputs"];
                            // Standard output used for all non-compute stages
                            if ( output.HasMember("rts") ) {
                                const Value::ConstArray& output_rts = output["rts"].GetArray();
                                for ( uint32_t i = 0; i < output_rts.Size(); ++i ) {
                                    const char* texture_cstring = output_rts[i].GetString();
                                    render_stage_creation.outputs[i] = string_hash_get( render_pipeline_creation.name_to_textures, texture_cstring );
                                }
                                render_stage_creation.output_count = (uint32_t)output_rts.Size();
                            }
                            else if ( output.HasMember( "images" ) ) {
                                const Value::ConstArray& output_rts = output["images"].GetArray();
                                for ( uint32_t i = 0; i < output_rts.Size(); ++i ) {

                                    const auto& output_image = output_rts[i];

                                    const Value& output_texture_name = output_image["name"];
                                    const char* texture_cstring = render_pipeline_creation.string_buffer.append_use( output_texture_name.GetString() );
                                    render_stage_creation.outputs[i] = string_hash_get( render_pipeline_creation.name_to_textures, texture_cstring );

                                    // Add to the lookups
                                    const char* binding_cstring = render_pipeline_creation.string_buffer.append_use( output_image["binding"].GetString() );
                                    render_stage_creation.overriding_lookups.add_binding_to_resource( (char*)binding_cstring, (char*)texture_cstring );
                                }

                                render_stage_creation.output_count = (uint32_t)output_rts.Size();
                            }
                            

                            if ( output.HasMember( "depth" ) ) {
                                const char* texture_cstring = output["depth"].GetString();
                                render_stage_creation.output_depth = string_hash_get( render_pipeline_creation.name_to_textures, texture_cstring );
                            }
                            else {
                                render_stage_creation.output_depth = nullptr;
                            }

                            if ( output.HasMember( "clear_color" ) ) {
                                render_stage_creation.clear_rt = true;
                                render_stage_creation.clear_color[0] = 0.0f;
                                render_stage_creation.clear_color[1] = 0.0f;
                                render_stage_creation.clear_color[2] = 0.0f;
                                render_stage_creation.clear_color[3] = 0.0f;
                            }
                            else {
                                render_stage_creation.clear_rt = false;
                            }

                            if ( output.HasMember( "clear_depth" ) ) {
                                render_stage_creation.clear_depth = true;
                                render_stage_creation.clear_depth_value = output["clear_depth"].GetFloat();
                            }
                            else {
                                render_stage_creation.clear_depth = false;
                            }

                            if ( output.HasMember( "clear_stencil" ) ) {
                                render_stage_creation.clear_stencil = true;
                                render_stage_creation.clear_stencil_value = (uint8_t)output["clear_stencil"].GetUint();
                            }
                            else {
                                render_stage_creation.clear_stencil = false;
                            }

                            array_push( render_pipeline_creation.render_stages, render_stage_creation );
                        }
                    }

                    // Check if render pipeline is valid and add it. It will be created only when used for the first time.
                    if ( render_pipeline_creation.is_valid() ) {
                        array_push( render_pipeline_creations, render_pipeline_creation );
                    }
                }
            }
        }

        //free( file_memory );
    }

    current_render_pipeline = nullptr;
}

void RenderPipelineManager::terminate() {

}

void RenderPipelineManager::set_pipeline( hydra::graphics::Device& device, const char* name, hydra::StringBuffer& temp_string_buffer,
                                          hydra::graphics::ShaderResourcesDatabase& initial_db ) {
    // Search for render pipeline creation
    for ( uint32_t p = 0; p < array_length( render_pipeline_creations ); ++p ) {
        const RenderPipelineCreation& creation = render_pipeline_creations[p];

        if ( strcmp(creation.name, name) == 0 ) {

            if ( current_render_pipeline ) {
                current_render_pipeline->terminate( device );
            }

            // Actually create pipeline
            current_render_pipeline = create_pipeline( device, creation, temp_string_buffer, initial_db );

            break;
        }
    }

    if ( current_render_pipeline ) {
        current_render_pipeline->load_resources( device );
    }
}

hydra::graphics::RenderPipeline* RenderPipelineManager::create_pipeline( hydra::graphics::Device& device, const RenderPipelineCreation& creation,
                                                                         hydra::StringBuffer& temp_string_buffer, hydra::graphics::ShaderResourcesDatabase& initial_db ) {

    using namespace hydra;

    graphics::RenderPipeline* render_pipeline = (graphics::RenderPipeline*)malloc( sizeof( graphics::RenderPipeline ) );
    render_pipeline->init( &initial_db );

    string_hash_put( name_to_render_pipeline, creation.name, render_pipeline );

    // Create textures and render targets
    for ( uint32_t t = 0; t < string_hash_length( creation.name_to_textures ); ++t ) {

        const RenderPipelineTextureCreation& texture_creation = *(creation.name_to_textures[t].value);
        // If texture is coming from file, load it
        if ( strlen(texture_creation.path) != 0 ) {
            int image_width, image_height, channels_in_file;
            unsigned char* my_image_data = stbi_load( texture_creation.path, &image_width, &image_height, &channels_in_file, 4 );

            hydra::graphics::TextureCreation graphics_texture_creation = { my_image_data, image_width, image_height, 1, 1, 0, hydra::graphics::TextureFormat::R8G8B8A8_UNORM, hydra::graphics::TextureType::Texture2D };
            graphics_texture_creation.name = texture_creation.name;
            graphics::TextureHandle handle = device.create_texture( graphics_texture_creation );
            
            string_hash_put( render_pipeline->name_to_texture, texture_creation.name, handle );
            render_pipeline->resource_database.register_texture( (char*)texture_creation.name, handle );
        }        
    }

    // Mask used to send geometries to the right stage.
    uint64_t geometry_stage_mask = 1;

    // Create render stages
    for ( uint32_t s = 0; s < array_length( creation.render_stages ); ++s ) {

        const RenderStageCreation& render_stage_creation = creation.render_stages[s];

        graphics::RenderStage* stage = new graphics::RenderStage();
        stage->type = (graphics::RenderStage::Type)render_stage_creation.stage_type;
        stage->pool_id = -1;
        stage->num_input_textures = render_stage_creation.input_count;
        stage->num_output_textures = render_stage_creation.output_count;
        stage->input_textures = render_stage_creation.input_count ? new graphics::TextureHandle[render_stage_creation.input_count] : nullptr;
        stage->output_textures = render_stage_creation.output_count ? new graphics::TextureHandle[render_stage_creation.output_count] : nullptr;
        
        // Get Input textures
        for ( size_t i = 0; i < render_stage_creation.input_count; i++ ) {
            const RenderPipelineTextureCreation& rt_creation = *render_stage_creation.inputs[i];
            stage->input_textures[i] = string_hash_get( render_pipeline->name_to_texture, rt_creation.name );
        }

        // Create Output textures.
        for ( size_t i = 0; i < render_stage_creation.output_count; i++ ) {
            // Create Render Target
            const RenderPipelineTextureCreation& rt_creation = *render_stage_creation.outputs[i];
            // Search render target first
            graphics::TextureHandle handle = string_hash_get( render_pipeline->name_to_texture, rt_creation.name );
            if ( handle.handle == 0 ) {
                graphics::TextureCreation rt = {};
                rt.width = device.swapchain_width;
                rt.height = device.swapchain_height;
                rt.depth = rt_creation.texture_creation.depth;
                rt.render_target = 1;
                rt.format = rt_creation.texture_creation.format;
                rt.name = rt_creation.name;
                rt.type = rt_creation.texture_creation.type;

                handle = device.create_texture( rt );

                string_hash_put( render_pipeline->name_to_texture, rt_creation.name, handle );
                render_pipeline->resource_database.register_texture( (char*)rt_creation.name, handle );
            }

            stage->output_textures[i] = handle;
        }

        bool depth_stencil = false;
        bool only_depth = false;
        bool only_stencil = false;

        // Create depth/stencil texture
        if ( render_stage_creation.output_depth ) {
            const RenderPipelineTextureCreation& rt_creation = *render_stage_creation.output_depth;
            // Search render target first
            graphics::TextureHandle handle = string_hash_get( render_pipeline->name_to_texture, rt_creation.name );
            if ( handle.handle == 0 ) {
                graphics::TextureCreation rt = {};
                rt.width = device.swapchain_width;
                rt.height = device.swapchain_height;
                rt.depth = rt_creation.texture_creation.depth;
                rt.render_target = 1;
                rt.format = rt_creation.texture_creation.format;
                rt.name = rt_creation.name;
                rt.type = rt_creation.texture_creation.type;

                handle = device.create_texture( rt );
                string_hash_put( render_pipeline->name_to_texture, rt_creation.name, handle );
                render_pipeline->resource_database.register_texture( (char*)rt_creation.name, handle );
            }
            stage->depth_texture = handle;

            depth_stencil = graphics::TextureFormat::is_depth_stencil( rt_creation.texture_creation.format );
            only_depth = graphics::TextureFormat::is_depth_only( rt_creation.texture_creation.format );
            only_stencil = graphics::TextureFormat::is_stencil_only( rt_creation.texture_creation.format);

            if ( only_depth ) {
                stage->clear_depth = 1;
                stage->clear_stencil = 0;
            }
            else if ( only_stencil ) {
                stage->clear_depth = 0;
                stage->clear_stencil = 1;
            }
            else {
                stage->clear_depth = 1;
                stage->clear_stencil = 1;
            }
        }

        // Handle clear of targets
        stage->clear_rt = stage->clear_depth = stage->clear_stencil = false;

        if ( render_stage_creation.clear_rt ) {
            stage->clear_rt = true;
            memcpy( &stage->clear_color[0], &render_stage_creation.clear_color[0], sizeof(float) * 4 );
        }

        if ( render_stage_creation.clear_depth && (depth_stencil || only_depth) ) {
            stage->clear_depth = true;
            stage->clear_depth_value = render_stage_creation.clear_depth_value;
        }

        if ( render_stage_creation.clear_stencil && (depth_stencil || only_stencil)) {
            stage->clear_stencil = true;
            stage->clear_stencil_value = render_stage_creation.clear_stencil_value;
        }

        // Handle geometry mask
        if ( stage->type == hydra::graphics::RenderStage::Geometry ) {
            stage->geometry_stage_mask = geometry_stage_mask;

            geometry_stage_mask <<= 1;
        }

        // Retrieve Shader Instance from the material, used mostly by postprocess passes.
        if ( strlen( render_stage_creation.material_name ) ) {
            char* material_filename = temp_string_buffer.append_use( "%s.hmt", render_stage_creation.material_name );
            hydra::Resource* material_resource = g_resource_manager.load_resource( ResourceType::Material, material_filename, device, render_pipeline );
            stage->material = (hydra::graphics::Material*)material_resource->asset;
            stage->pass_index = render_stage_creation.material_pass_index;

            // Override specialization
            for ( size_t i = 0; i < string_hash_length(render_stage_creation.overriding_lookups.binding_to_resource); ++i ) {
                const graphics::ShaderResourcesLookup::NameMap& binding_entry = render_stage_creation.overriding_lookups.binding_to_resource[i];

                stage->material->lookups.add_binding_to_resource( binding_entry.key, binding_entry.value );
            }
        }

        stage->resize_output = 1;

        // Retrieve render view
        stage->render_view = string_hash_get( name_to_render_view, render_stage_creation.render_view_name );

        stage->init();
        string_hash_put( render_pipeline->name_to_stage, render_stage_creation.name, stage );
    }

    // Create ShaderToy Constants
    graphics::BufferCreation checker_constants_creation = {};
    checker_constants_creation.type = graphics::BufferType::Constant;
    checker_constants_creation.name = "ShaderToyConstants";
    checker_constants_creation.usage = graphics::ResourceUsageType::Dynamic;
    checker_constants_creation.size = 16;
    checker_constants_creation.initial_data = nullptr;

    shadertoy_buffer = device.create_buffer( checker_constants_creation );
    render_pipeline->resource_lookup.add_binding_to_resource( (char*)checker_constants_creation.name, (char*)checker_constants_creation.name );
    render_pipeline->resource_database.register_buffer( (char*)checker_constants_creation.name, shadertoy_buffer );

    return render_pipeline;
}
