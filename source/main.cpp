#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL.h>
#include <GL/glew.h>

#include "CodeGenerator.h"

// HDF generated classes
#include "SimpleData.h"

// Flatbuffers generated classes
#include "RenderDefinitions_generated.h"

#define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )

// Utility method
static char* ReadEntireFileIntoMemory( const char* fileName, size_t* size ) {
    char *text = 0;

    FILE *file = fopen( fileName, "rb" );

    if ( file ) {

        fseek( file, 0, SEEK_END );
        size_t filesize = ftell( file );
        fseek( file, 0, SEEK_SET );

        text = (char *)malloc( filesize + 1 );
        size_t result = fread( text, filesize, 1, file );
        text[filesize] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return(text);
}

static void ReflectUI( const Parser& parser ) {
    ImGui::Begin( "Enums" );

    char name_buffer[256], type_name_buffer[256];

    for ( uint32_t i = 0; i < parser.types_count; ++i ) {
        const ast::Type& type = parser.types[i];
        if ( type.type == ast::Type::Types_Enum && type.names.size() ) {
            
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
        const ast::Type& type = parser.types[i];
        if ( type.type == ast::Type::Types_Struct && type.names.size() ) {

            copy( type.name, name_buffer, 256 );

            if ( ImGui::TreeNode( name_buffer ) ) {
                for ( uint32_t v = 0; v < type.names.size(); ++v ) {
                    const StringRef& member_name = type.names[v];
                    const ast::Type* member_type = type.types[v];

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


int main(int argc, char** argv) {

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        printf( "SDL Init error: %s\n", SDL_GetError() );
        return -1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );
    SDL_WindowFlags window_flags = (SDL_WindowFlags)( SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI );
    SDL_Window* window = SDL_CreateWindow( "Data Driven Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags );
    SDL_GLContext gl_context = SDL_GL_CreateContext( window );
    SDL_GL_SetSwapInterval( 1 ); // Enable vsync

    // Initialize OpenGL loader
    bool err = glewInit() != GLEW_OK;
    if ( err )
    {
        fprintf( stderr, "Failed to initialize OpenGL loader!\n" );
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL( window, gl_context );
    ImGui_ImplOpenGL3_Init( glsl_version );

    // Code generation part
    Lexer lexer;
    char* text = ReadEntireFileIntoMemory( "..\\data\\SimpleData.hdf", nullptr );
    initLexer( &lexer, (char*)text );

    Parser parser;
    initParser( &parser, &lexer, 1024 );
    generateAST( &parser );

    CodeGenerator code_generator;
    initCodeGenerator( &code_generator, &parser, 6000 );
    code_generator.generate_imgui = true;
    generateCode( &code_generator, "..\\source\\SimpleData.h" );

    // Generated class
    RenderTarget rt = {};
    RenderPass rp = {};

    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4( 0.45f, 0.05f, 0.00f, 1.00f );

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
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame( window );
        ImGui::NewFrame();

        // Use reflection informations to generate UI
        ReflectUI( parser );
        // Use generated code for UI
        rt.reflectUI();
        rp.reflectUI();

        if ( show_demo_window )
            ImGui::ShowDemoWindow( &show_demo_window );

        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent( window, gl_context );
        glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
        glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
        glClear( GL_COLOR_BUFFER_BIT );
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
        SDL_GL_SwapWindow( window );
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext( gl_context );
    SDL_DestroyWindow( window );
    SDL_Quit();

    return 0;
}
