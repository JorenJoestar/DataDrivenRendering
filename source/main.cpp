#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "hydra_imgui.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL.h>
#include <GL/glew.h>

#include "hydra_lib.h"
#include "hydra_graphics.h"

#include "CodeGenerator.h"
#include "ShaderCodeGenerator.h"

// HDF generated classes
#include "SimpleData.h"

// HFX generated classes
#include "SimpleFullscreen.h"

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


struct Application {

    void                            init();
    void                            terminate();

    void                            main_loop();

    void                            manual_init_graphics();
    void                            load_shader_effect();

    SDL_Window*                     window = nullptr;
    SDL_GLContext                   gl_context;

    hydra::graphics::Device         gfx_device;

    // hydra::gfx_device::ShaderEffect    shader_effect;

    hydra::graphics::TextureHandle  render_target;
    hydra::graphics::BufferHandle   checker_constants;

    SimpleFullscreen::LocalConstantsBuffer local_constant_buffer;

    hydra::graphics::CommandBuffer* commands;
}; // struct Application

/////////////////////////////////////////////////////////////////////////////////

static void ReflectUI( const hdf::Parser& parser ) {
    ImGui::Begin( "Enums" );

    char name_buffer[256], type_name_buffer[256];

    for ( uint32_t i = 0; i < parser.types_count; ++i ) {
        const hdf::Type& type = parser.types[i];
        if ( type.type == hdf::Type::Types_Enum && type.names.size() ) {
            
            copy( type.name, name_buffer, 256 );

            if ( ImGui::TreeNode( name_buffer ) ) {
                for ( uint32_t v = 0; v < type.names.size(); ++v ) {
                    const StringRef& enum_value = type.names[v];

                    copy( enum_value, name_buffer, 256 );
                    ImGui::Text( "%s", name_buffer );
                }

                ImGui::TreePop();
            }
        }
    }

    ImGui::End();

    ImGui::Begin( "Structs" );
    for ( uint32_t i = 0; i < parser.types_count; ++i ) {
        const hdf::Type& type = parser.types[i];
        if ( type.type == hdf::Type::Types_Struct && type.names.size() ) {

            copy( type.name, name_buffer, 256 );

            if ( ImGui::TreeNode( name_buffer ) ) {
                for ( uint32_t v = 0; v < type.names.size(); ++v ) {
                    const StringRef& member_name = type.names[v];
                    const hdf::Type* member_type = type.types[v];

                    copy( member_name, name_buffer, 256 );
                    copy( member_type->name, type_name_buffer, 256 );

                    ImGui::Text( "%s %s", type_name_buffer, name_buffer );
                }

                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

/////////////////////////////////////////////////////////////////////////////////
void Application::init() {

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        printf( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );
    
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags );
    
    gl_context = SDL_GL_CreateContext( window );

    SDL_GL_SetSwapInterval( 1 ); // Enable vsync

    // Initialize OpenGL loader
    bool err = glewInit() != GLEW_OK;
    if ( err )
    {
        fprintf( stderr, "Failed to initialize OpenGL loader!\n" );
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );

    // Init the gfx_device device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    gfx_device.init( device_creation );

    hydra_Imgui_Init( gfx_device );
}

void Application::terminate() {

    // Cleanup
    hydra_Imgui_Shutdown( gfx_device );
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext( gl_context );
    SDL_DestroyWindow( window );
    SDL_Quit();

}

void Application::manual_init_graphics() {
    
    using namespace hydra;

    ////////
    // Create the needed resources
    // - Compute shader
    Buffer compute_file_buffer;
    ReadFileIntoMemory( "..\\data\\ComputeTest.comp", "r", compute_file_buffer );
    graphics::ShaderCreation first_compute = {};
    if ( compute_file_buffer.size() ) {

        graphics::ShaderCreation::Stage compute_stage = { graphics::ShaderStage::Compute, (const char*)compute_file_buffer.data() };
        first_compute.stages = &compute_stage;
        first_compute.stages_count = 1;
        first_compute.name = "First Compute";
    }

    // - Fullscreen color shader
    Buffer color_vert_buffer, color_frag_buffer;
    ReadFileIntoMemory( "..\\data\\ToScreen.vert", "r", color_vert_buffer );
    ReadFileIntoMemory( "..\\data\\ToScreen.frag", "r", color_frag_buffer );

    graphics::ShaderCreation first_shader = {};
    if ( color_vert_buffer.size() && color_frag_buffer.size() ) {
        graphics::ShaderCreation::Stage stages[] = { { graphics::ShaderStage::Vertex, (const char*)color_vert_buffer.data() },
                                                     { graphics::ShaderStage::Fragment, (const char*)color_frag_buffer.data() } };
        first_shader.stages = stages;
        first_shader.stages_count = 2;
        first_shader.name = "First Fullscreen";
    }

    // - Destination texture
    graphics::TextureCreation first_rt = {};
    first_rt.width = 512;
    first_rt.height = 512;
    first_rt.render_target = 1;
    first_rt.format = graphics::TextureFormat::R8G8B8A8_UNORM;
    render_target = gfx_device.create_texture( first_rt );

    // - Checker GPU constants
    graphics::BufferCreation checker_constants_creation = {};
    checker_constants_creation.type = graphics::BufferType::Constant;
    checker_constants_creation.name = "CheckerConstants";
    checker_constants_creation.usage = graphics::ResourceUsageType::Dynamic;
    checker_constants_creation.size = sizeof( float ) * 4;
    float constants[] = { 32.0f, 2.0f, 0.0f, 0.0f };
    checker_constants_creation.initial_data = constants;
    checker_constants = gfx_device.create_buffer( checker_constants_creation );

    ////////
    // Resource layout
    // - Compute

    const graphics::ResourceListLayoutCreation::Binding compute_bindings[] = { { graphics::ResourceType::TextureRW, 0, 1, "destination_texture" }, { graphics::ResourceType::Constants, 0, 1, "LocalConstants" } };
    graphics::ResourceListLayoutCreation resource_layout_creation = { compute_bindings, 2 };

    graphics::ResourceListLayoutHandle compute_resource_layout = gfx_device.create_resource_list_layout( resource_layout_creation );

    // - Graphics
    const graphics::ResourceListLayoutCreation::Binding gfx_bindings[] = { { graphics::ResourceType::Texture, 0, 1, "input_texture" } };
    graphics::ResourceListLayoutCreation gfx_layout_creation = { gfx_bindings, 1 };

    graphics::ResourceListLayoutHandle gfx_resource_layout = gfx_device.create_resource_list_layout( gfx_layout_creation );

    // Resource sets
    // - Compute
    const graphics::ResourceListCreation::Resource compute_resources_handles[] = { render_target.handle, checker_constants.handle };
    graphics::ResourceListCreation compute_resources_creation = { compute_resource_layout, compute_resources_handles, 2 };
    graphics::ResourceListHandle compute_resources = gfx_device.create_resource_list( compute_resources_creation );

    // - Graphics
    const graphics::ResourceListCreation::Resource gfx_resources_handles[] = { render_target.handle };
    graphics::ResourceListCreation gfx_resources_creation = { gfx_resource_layout, gfx_resources_handles, 1 };

    graphics::ResourceListHandle gfx_resources = gfx_device.create_resource_list( gfx_resources_creation );

    // Pipelines

    // - Compute pipeline
    graphics::PipelineCreation compute_pipeline = {};
    compute_pipeline.shaders = &first_compute;
    compute_pipeline.resource_layout = compute_resource_layout;
    graphics::PipelineHandle first_compute_pipeline = gfx_device.create_pipeline( compute_pipeline );

    // - Graphics pipeline
    graphics::PipelineCreation graphics_pipeline = {};
    graphics_pipeline.shaders = &first_shader;
    graphics_pipeline.resource_layout = gfx_resource_layout;
    graphics::PipelineHandle first_graphics_pipeline = gfx_device.create_pipeline( graphics_pipeline );

    // Command buffer
    commands = gfx_device.get_command_buffer( graphics::QueueType::Graphics, 1024 );

    commands->bind_pipeline( first_compute_pipeline );
    commands->bind_resource_list( compute_resources );
    commands->dispatch( first_rt.width / 32, first_rt.height / 32, 1 );

    commands->bind_pipeline( first_graphics_pipeline );
    commands->bind_resource_list( gfx_resources );
    commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
    commands->draw( graphics::TopologyType::Triangle, 0, 3 );
}

/////////////////////////////////////////////////////////////////////////////////

static void compile_shader_effect_pass( hydra::graphics::Device& device, char* hfx_file_memory, uint16_t pass_index, hydra::graphics::ShaderCreation& out_shader, hydra::graphics::ResourceListLayoutHandle& out_resource_set_layout ) {
    using namespace hydra;

    // Create shader
    char* pass = hfx::getPassMemory( hfx_file_memory, pass_index );
    hfx::ShaderEffectFile::PassHeader* pass_header = (hfx::ShaderEffectFile::PassHeader*)pass;
    uint32_t shader_count = pass_header->num_shader_chunks;
    graphics::ShaderCreation::Stage* stages = new graphics::ShaderCreation::Stage[shader_count];

    for ( uint16_t i = 0; i < shader_count; i++ ) {
        hfx::getShaderCreation( shader_count, pass, i, &stages[i] );
    }

    out_shader.stages = stages;
    out_shader.stages_count = shader_count;
    out_shader.name = pass_header->name;

    // Create Resource Set Layout
    const hydra::graphics::ResourceListLayoutCreation::Binding* bindings = (const hydra::graphics::ResourceListLayoutCreation::Binding*)(pass + pass_header->resource_table_offset);
    hydra::graphics::ResourceListLayoutCreation resource_layout_creation = { bindings, pass_header->num_resources };

    out_resource_set_layout = device.create_resource_list_layout( resource_layout_creation );
}

void Application::load_shader_effect() {

    using namespace hydra;

    char* hfx_file_memory = ReadEntireFileIntoMemory( "..\\data\\SimpleFullscreen.bhfx", nullptr );
    hfx::ShaderEffectFile* hfx_file = (hfx::ShaderEffectFile*)hfx_file_memory;

    graphics::ResourceListLayoutHandle compute_resource_layout, gfx_resource_layout;

    graphics::ShaderCreation compute_shader = {};
    graphics::ShaderCreation fullscreen_shader = {};

    // Generate both shader states AND resource set layout!
    compile_shader_effect_pass( gfx_device, hfx_file_memory, 0, compute_shader, compute_resource_layout );
    compile_shader_effect_pass( gfx_device, hfx_file_memory, 1, fullscreen_shader, gfx_resource_layout );

    // - Destination texture
    graphics::TextureCreation first_rt = {};
    first_rt.width = 512;
    first_rt.height = 512;
    first_rt.render_target = 1;
    first_rt.format = graphics::TextureFormat::R8G8B8A8_UNORM;
    render_target = gfx_device.create_texture( first_rt );

    // Local constants
    local_constant_buffer.create( gfx_device );

    // Resource sets
    // - Compute
    const graphics::ResourceListCreation::Resource compute_resources_handles[] = { local_constant_buffer.buffer.handle, render_target.handle };
    graphics::ResourceListCreation compute_resources_creation = { compute_resource_layout, compute_resources_handles, 2 };
    graphics::ResourceListHandle compute_resources = gfx_device.create_resource_list( compute_resources_creation );

    // - Graphics
    const graphics::ResourceListCreation::Resource gfx_resources_handles[] = { render_target.handle };
    graphics::ResourceListCreation gfx_resources_creation = { gfx_resource_layout, gfx_resources_handles, 2 };

    graphics::ResourceListHandle gfx_resources = gfx_device.create_resource_list( gfx_resources_creation );

    // Pipelines

    // - Compute pipeline
    graphics::PipelineCreation compute_pipeline = {};
    compute_pipeline.compute = true;
    compute_pipeline.shaders = &compute_shader;
    compute_pipeline.resource_layout = compute_resource_layout;
    graphics::PipelineHandle first_compute_pipeline = gfx_device.create_pipeline( compute_pipeline );

    // - Graphics pipeline
    graphics::PipelineCreation graphics_pipeline = {};
    graphics_pipeline.shaders = &fullscreen_shader;
    graphics_pipeline.resource_layout = gfx_resource_layout;
    graphics::PipelineHandle first_graphics_pipeline = gfx_device.create_pipeline( graphics_pipeline );

    // Command buffer
    commands = gfx_device.get_command_buffer( graphics::QueueType::Graphics, 1024 );

    commands->bind_pipeline( first_compute_pipeline );
    commands->bind_resource_list( compute_resources );
    commands->dispatch( first_rt.width / 32, first_rt.height / 32, 1 );

    commands->bind_pipeline( first_graphics_pipeline );
    commands->bind_resource_list( gfx_resources );
    commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
    commands->draw( graphics::TopologyType::Triangle, 0, 3 );
}

static void generate_hdf_classes( Lexer& lexer, hdf::Parser& parser, hdf::CodeGenerator& code_generator, DataBuffer* data_buffer ) {
    
    char* text = ReadEntireFileIntoMemory( "..\\data\\SimpleData.hdf", nullptr );
    initLexer( &lexer, (char*)text, data_buffer );

    hdf::initParser( &parser, &lexer, 1024 );
    hdf::generateAST( &parser );

    hdf::initCodeGenerator( &code_generator, &parser, 6000 );
    code_generator.generate_imgui = true;
    hdf::generateCode( &code_generator, "..\\source\\SimpleData.h" );
}

static void generate_shader_permutation( const char* filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer ) {

    char* text = ReadEntireFileIntoMemory( filename, nullptr );
    initLexer( &lexer, (char*)text, data_buffer );

    hfx::initParser( &effect_parser, &lexer );
    hfx::generateAST( &effect_parser );

    hfx::initCodeGenerator( &hfx_code_generator, &effect_parser, 8000, 8 );
    hfx::generateShaderPermutations( &hfx_code_generator, "..\\data\\" );
}

//
// Compile HFX to output filename from scratch
//
static void compile_hfx( const char* filename, const char* out_filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer ) {
    char* text = ReadEntireFileIntoMemory( filename, nullptr );
    initLexer( &lexer, (char*)text, data_buffer );

    hfx::initParser( &effect_parser, &lexer );
    hfx::generateAST( &effect_parser );

    hfx::initCodeGenerator( &hfx_code_generator, &effect_parser, 8000, 8 );
    hfx::compileShaderEffectFile( &hfx_code_generator, "..\\data\\", out_filename );
}

void Application::main_loop() {

    // Init the application
    init();

    ImGuiIO& io = ImGui::GetIO();

    // Lexer used by all code generators.
    Lexer lexer;
    // Data Buffer used by the lexer
    DataBuffer data_buffer;
    initDataBuffer( &data_buffer, 256, 1000 );

    ////////
    // 1. HDF (Hydra Data Format) parsing and code generation
    // From a HDF file generate a header file.
    // Article: https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

    hdf::Parser parser;
    hdf::CodeGenerator code_generator;

    generate_hdf_classes( lexer, parser, code_generator, &data_buffer );
    
    // Declare generated class from HDF
    RenderTarget rt = {};
    RenderPass rp = {};

    ////////
    // 2. HFX (Hydra Effects)
    // From a HFX file generate single GLSL files to be used by the gfx_device library.
    // Article: https://jorenjoestar.github.io/post/writing_shader_effect_language_1/

    hfx::Parser effect_parser;
    hfx::CodeGenerator hfx_code_generator;

    generate_shader_permutation( "..\\data\\SimpleFullscreen.hfx", lexer, effect_parser, hfx_code_generator, &data_buffer );

    ////////
    // 3. HFX full usage
    // Article: https://jorenjoestar.github.io/post/writing_shader_effect_language_2/

    // 3.1 Init gfx_device using shader permutation files coming from HFX.
    //manual_init_graphics();

    // 3.2 Compile HFX file to binary and use it to init gfx_device.
    hfx::compileShaderEffectFile( &hfx_code_generator, "..\\data\\", "SimpleFullscreen.bhfx" );

    // 3.3 Generate shader resources code and use it to draw.
    load_shader_effect();

    // 3.4 Generate constant buffer header file and use it.
    hfx::generateShaderResourceHeader( &hfx_code_generator, "..\\source\\" );

    // 4. HFX ImGui Shader
    generate_shader_permutation( "..\\data\\ImGui.hfx", lexer, effect_parser, hfx_code_generator, &data_buffer );
    compile_hfx( "..\\data\\ImGui.hfx", "ImGui.bhfx", lexer, effect_parser, hfx_code_generator, &data_buffer );


    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4( 0.45f, 0.05f, 0.00f, 1.00f );

    hydra::graphics::CommandBuffer* ui_commands = gfx_device.get_command_buffer( hydra::graphics::QueueType::Graphics, 1024 );

    // Main loop
    bool done = false;
    while ( !done )
    {
        SDL_Event event;
        while ( SDL_PollEvent( &event ) )
        {
            ImGui_ImplSDL2_ProcessEvent( &event );
            if ( event.type == SDL_QUIT )
                done = true;
            if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( window ) )
                done = true;
        }

        // Start the Dear ImGui frame
        hydra_Imgui_NewFrame();
        ImGui_ImplSDL2_NewFrame( window );
        ImGui::NewFrame();

        // Use reflection informations to generate UI
        ReflectUI( parser );
        // Use generated code for UI
        rt.reflectUI();
        rp.reflectUI();
        // Constant buffer UI
        local_constant_buffer.updateUI( gfx_device );

        if ( show_demo_window )
            ImGui::ShowDemoWindow( &show_demo_window );

        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent( window, gl_context );

        glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
        glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
        glClear( GL_COLOR_BUFFER_BIT );

        gfx_device.execute_command_buffer( commands );

        hydra_Imgui_RenderDrawData( ImGui::GetDrawData(), gfx_device, *ui_commands );
        gfx_device.execute_command_buffer( ui_commands );

        gfx_device.present();

        SDL_GL_SwapWindow( window );
    }

    // Cleanup
    terminate();
}


int main(int argc, char** argv) {

    Application application;
    application.main_loop();
    
    return 0;
}
