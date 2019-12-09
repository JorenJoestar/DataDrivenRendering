#include "CustomShaderLanguage.h"


#include <SDL.h>
#if defined (HYDRA_VULKAN)
#include <SDL_vulkan.h>
#endif // HYDRA_OPENGL

static void generate_hdf_classes( Lexer& lexer, hdf::Parser& parser, hdf::CodeGenerator& code_generator, DataBuffer* data_buffer );
static void generate_shader_permutation( const char* filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer );
static void compile_hfx( const char* filename, const char* out_filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer );

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
void CustomShaderLanguageApplication::init() {

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        printf( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

#if defined (HYDRA_OPENGL)
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );

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

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
#endif // HYDRA_OPENGL

    // Init the gfx_device device
    hydra::graphics::DeviceCreation device_creation = {};
    device_creation.window = window;
    gfx_device.init( device_creation );

    // Data Buffer used by the lexer
    DataBuffer data_buffer;
    init_data_buffer( &data_buffer, 256, 1000 );

    ////////
    // 1. HDF (Hydra Data Format) parsing and code generation
    // From a HDF file generate a header file.
    // Article: https://jorenjoestar.github.io/post/writing_a_simple_code_generator/
    generate_hdf_classes( lexer, parser, code_generator, &data_buffer );

    ////////
    // 2. HFX (Hydra Effects)
    // From a HFX file generate single GLSL files to be used by the gfx_device library.
    // Article: https://jorenjoestar.github.io/post/writing_shader_effect_language_1/
    generate_shader_permutation( "..\\data\\SimpleFullscreen.hfx", lexer, effect_parser, hfx_code_generator, &data_buffer );

    ////////
    // 3. HFX full usage
    // Article: https://jorenjoestar.github.io/post/writing_shader_effect_language_2/

    // 3.1 Init gfx_device using shader permutation files coming from HFX.
    //manual_init_graphics();

    // 3.2 Compile HFX file to binary and use it to init gfx_device.
    hfx::compile_shader_effect_file( &hfx_code_generator, "..\\data\\", "SimpleFullscreen.bhfx" );

    // 3.3 Generate shader resources code and use it to draw.
    load_shader_effect();

    // 3.4 Generate constant buffer header file and use it.
    hfx::generate_shader_resource_header( &hfx_code_generator, "..\\source\\" );

    // 4. HFX ImGui Shader
    generate_shader_permutation( "..\\data\\ImGui.hfx", lexer, effect_parser, hfx_code_generator, &data_buffer );
    compile_hfx( "..\\data\\ImGui.hfx", "ImGui.bhfx", lexer, effect_parser, hfx_code_generator, &data_buffer );


    hydra_Imgui_Init( gfx_device );
}

void CustomShaderLanguageApplication::terminate() {

    // Cleanup
    hydra_Imgui_Shutdown( gfx_device );
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

#if defined HYDRA_OPENGL
    SDL_GL_DeleteContext( gl_context );
#endif // HYDRA_OPENGL

    SDL_DestroyWindow( window );
    SDL_Quit();

}

void CustomShaderLanguageApplication::manual_init_graphics() {

    using namespace hydra;

    graphics::PipelineCreation compute_pipeline = {};
    graphics::PipelineCreation graphics_pipeline = {};

    ////////
    // Create the needed resources
    // - Compute shader
    Buffer compute_file_buffer = nullptr;
    read_file( "..\\data\\SimpleFullscreen_ComputeTest.comp", "r", compute_file_buffer );
    if ( array_length( compute_file_buffer ) ) {

        compute_pipeline.shaders.stages[0] = { graphics::ShaderStage::Compute, (const char*)compute_file_buffer };
        compute_pipeline.shaders.stages_count = 1;
        compute_pipeline.shaders.name = "First Compute";
    }

    // - Fullscreen color shader
    Buffer color_vert_buffer = nullptr, color_frag_buffer = nullptr;
    read_file( "..\\data\\SimpleFullscreen_ToScreen.vert", "r", color_vert_buffer );
    read_file( "..\\data\\SimpleFullscreen_ToScreen.frag", "r", color_frag_buffer );

    if ( array_length( color_vert_buffer ) && array_length( color_frag_buffer ) ) {
        graphics_pipeline.shaders.stages[0] = { graphics::ShaderStage::Vertex, (const char*)color_vert_buffer };
        graphics_pipeline.shaders.stages[1] = { graphics::ShaderStage::Fragment, (const char*)color_frag_buffer };
        graphics_pipeline.shaders.stages_count = 2;
        graphics_pipeline.shaders.name = "First Fullscreen";
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
    compute_pipeline.resource_list_layout[0] = compute_resource_layout;
    compute_pipeline.num_active_layouts = 1;
    graphics::PipelineHandle first_compute_pipeline = gfx_device.create_pipeline( compute_pipeline );

    // - Graphics pipeline
    graphics_pipeline.resource_list_layout[0] = gfx_resource_layout;
    graphics_pipeline.num_active_layouts = 1;
    graphics::PipelineHandle first_graphics_pipeline = gfx_device.create_pipeline( graphics_pipeline );

    // Command buffer
    if ( commands ) {
        gfx_device.destroy_command_buffer( commands );
    }
    commands = gfx_device.create_command_buffer( graphics::QueueType::Graphics, 1024, true );

    commands->begin_submit( 0 );
    commands->bind_pipeline( first_compute_pipeline );
    commands->bind_resource_list( &compute_resources, 1 );
    commands->dispatch( first_rt.width / 32u, first_rt.height / 32u, 1 );
    commands->end_submit();

    commands->begin_submit( 1 );
    commands->bind_pipeline( first_graphics_pipeline );
    commands->bind_resource_list( &gfx_resources, 1 );
    commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
    commands->draw( graphics::TopologyType::Triangle, 0, 3 );
    commands->end_submit();
}

/////////////////////////////////////////////////////////////////////////////////

static void compile_shader_effect_pass( hydra::graphics::Device& device, hfx::ShaderEffectFile& shader_effect_file, uint16_t pass_index, hydra::graphics::ShaderCreation& out_shader, hydra::graphics::ResourceListLayoutHandle& out_resource_set_layout ) {
    using namespace hydra;


    hfx::ShaderEffectFile::PassHeader* pass_header = hfx::get_pass( shader_effect_file, pass_index );
    uint32_t shader_count = pass_header->num_shader_chunks;

    for ( uint16_t i = 0; i < shader_count; i++ ) {
        hfx::get_shader_creation( pass_header, i, &out_shader.stages[i] );
    }

    out_shader.stages_count = shader_count;
    out_shader.name = pass_header->name;

    // Create Resource Set Layout
    uint8_t num_bindings = 0;
    const hydra::graphics::ResourceListLayoutCreation::Binding* bindings = hfx::get_pass_layout_bindings( pass_header, 0, num_bindings );
    hydra::graphics::ResourceListLayoutCreation resource_layout_creation = { bindings, num_bindings };

    out_resource_set_layout = device.create_resource_list_layout( resource_layout_creation );
}

void CustomShaderLanguageApplication::load_shader_effect() {

    using namespace hydra;

    // Create shader
    hfx::ShaderEffectFile shader_effect_file;
    hfx::init_shader_effect_file( shader_effect_file, "..\\data\\SimpleFullscreen.bhfx" );

    graphics::ResourceListLayoutHandle compute_resource_layout, gfx_resource_layout;

    graphics::PipelineCreation compute_pipeline = {};
    graphics::PipelineCreation graphics_pipeline = {};

    // Generate both shader states AND resource set layout!
    compile_shader_effect_pass( gfx_device, shader_effect_file, 0, compute_pipeline.shaders, compute_resource_layout );
    compile_shader_effect_pass( gfx_device, shader_effect_file, 1, graphics_pipeline.shaders, gfx_resource_layout );

    // - Destination texture
    graphics::TextureCreation first_rt = {};
    first_rt.width = 512;
    first_rt.height = 512;
    first_rt.render_target = 1;
    first_rt.format = graphics::TextureFormat::R8G8B8A8_UNORM;
    first_rt.name = "CheckerTexture";
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
    compute_pipeline.compute = true;
    compute_pipeline.resource_list_layout[0] = compute_resource_layout;
    compute_pipeline.num_active_layouts = 1;
    graphics::PipelineHandle first_compute_pipeline = gfx_device.create_pipeline( compute_pipeline );

    // - Graphics pipeline
    
    graphics_pipeline.resource_list_layout[0] = gfx_resource_layout;
    graphics_pipeline.num_active_layouts = 1;
    graphics::PipelineHandle first_graphics_pipeline = gfx_device.create_pipeline( graphics_pipeline );

    // Command buffer
    if ( commands ) {
        gfx_device.destroy_command_buffer( commands );
    }
    commands = gfx_device.create_command_buffer( graphics::QueueType::Graphics, 1024, true );

    commands->begin_submit( 0 );
    commands->bind_pipeline( first_compute_pipeline );
    commands->bind_resource_list( &compute_resources, 1 );
    commands->dispatch( first_rt.width / 32u, first_rt.height / 32u, 1 );
    commands->end_submit();

    commands->begin_submit( 1 );
    commands->bind_pipeline( first_graphics_pipeline );
    commands->bind_resource_list( &gfx_resources, 1 );
    commands->bind_vertex_buffer( gfx_device.get_fullscreen_vertex_buffer() );
    commands->draw( graphics::TopologyType::Triangle, 0, 3 );
    commands->end_submit();
}

static void generate_hdf_classes( Lexer& lexer, hdf::Parser& parser, hdf::CodeGenerator& code_generator, DataBuffer* data_buffer ) {

    char* text = hydra::read_file_into_memory( "..\\data\\SimpleData.hdf", nullptr );
    init_lexer( &lexer, (char*)text, data_buffer );

    hdf::init_parser( &parser, &lexer, 1024 );
    hdf::generate_ast( &parser );

    hdf::init_code_generator( &code_generator, &parser, 6000 );
    code_generator.generate_imgui = true;
    hdf::generate_code( &code_generator, "..\\source\\SimpleData.h" );
}

static void generate_shader_permutation( const char* filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer ) {

    char* text = hydra::read_file_into_memory( filename, nullptr );
    init_lexer( &lexer, (char*)text, data_buffer );

    hfx::init_parser( &effect_parser, &lexer );
    hfx::generate_ast( &effect_parser );

    hfx::init_code_generator( &hfx_code_generator, &effect_parser, 8000, 8 );
    hfx::generate_shader_permutations( &hfx_code_generator, "..\\data\\" );
}

//
// Compile HFX to output filename from scratch
//
static void compile_hfx( const char* filename, const char* out_filename, Lexer& lexer, hfx::Parser& effect_parser, hfx::CodeGenerator& hfx_code_generator, DataBuffer* data_buffer ) {
    char* text = hydra::read_file_into_memory( filename, nullptr );
    init_lexer( &lexer, (char*)text, data_buffer );

    hfx::init_parser( &effect_parser, &lexer );
    hfx::generate_ast( &effect_parser );

    hfx::init_code_generator( &hfx_code_generator, &effect_parser, 8000, 8 );
    hfx::compile_shader_effect_file( &hfx_code_generator, "..\\data\\", out_filename );
}


void CustomShaderLanguageApplication::main_loop() {

    // Init the WritingLanguageApplication
    init();

    ImGuiIO& io = ImGui::GetIO();

    // Initial resize.
    gfx_device.resize( (uint16_t)io.DisplaySize.x, (uint16_t)io.DisplaySize.y );


    // Declare generated class from HDF
    RenderTarget rt = {};
    RenderPass rp = {};


    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4( 0.45f, 0.05f, 0.00f, 1.00f );

    hydra::graphics::CommandBuffer* ui_commands = gfx_device.create_command_buffer( hydra::graphics::QueueType::Graphics, 1024, false );

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

        ui_commands->reset();

#if defined HYDRA_OPENGL
        glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
        glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
        glClear( GL_COLOR_BUFFER_BIT );

        SDL_GL_MakeCurrent( window, gl_context );
#endif
        hydra_imgui_collect_draw_data( ImGui::GetDrawData(), gfx_device, *ui_commands );

        gfx_device.queue_command_buffer( commands );
        gfx_device.queue_command_buffer( ui_commands );

        gfx_device.present();

#if defined HYDRA_OPENGL
        SDL_GL_SwapWindow( window );
#endif
    }

    gfx_device.destroy_command_buffer( ui_commands );

    // Cleanup
    terminate();
}

