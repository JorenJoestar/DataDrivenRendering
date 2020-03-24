#include <stdio.h>

#include "optick/optick.h"
#include "enkits/TaskScheduler.h"

#include "imgui.h"

#include "hydra/hydra_application.h"
#include "hydra/hydra_graphics.h"
#include "hydra/hydra_lib.h"
#include "hydra/hydra_imgui.h"
#include "hydra/hydra_shaderfx.h"

#include <windows.h>

#include <math.h> // fmodf

// ShaderAugmentationApplication ////////////////////////////////////////////////////
struct ShaderAugmentationApplication : public hydra::Application {

    void                            app_update() override;
};

void show_compiler_ui() {

    static char input_filename[MAX_PATH];
    static char output_filename[MAX_PATH];

    if ( ImGui::Begin( "Compiler" ) ) {

        if ( hydra::imgui_file_dialog_open( "Choose HFX file", "..\\data\\", ".hfx" ) ) {
            strcpy( input_filename, hydra::imgui_file_dialog_get_filename() );
        }
        ImGui::InputText( "Input File:", input_filename, MAX_PATH );

        if ( hydra::imgui_file_dialog_open( "Choose Binary HFX file", "..\\data\\", ".bhfx" ) ) {
            strcpy( output_filename, hydra::imgui_file_dialog_get_filename() );
        }
        ImGui::InputText( "Output File:", output_filename, MAX_PATH );

        // Show options
        ImGui::Separator();
        ImGui::Text( "Options:" );
        static int32_t opengl = 0;
        if ( ImGui::Combo( "API", &opengl, "OpenGL\0Vulkan\0" ) ) {

        }
        
        if ( ImGui::Button( "Compile" ) ) {
            uint32_t options = hfx::CompileOptions_OpenGL | hfx::CompileOptions_Embedded;
            hfx::hfx_compile( input_filename, output_filename, options );
        }

        static hfx::ShaderEffectFile hfx_file;
        static char* text = nullptr;

        if ( ImGui::Button( "Inspect" ) ) {

            if ( text ) {
                hydra::hy_free( text );
            }

            text = hydra::file_read_into_memory( output_filename, nullptr, false );
            if ( text ) {
                shader_effect_init( hfx_file, text );
            }
        }

        if ( text ) {
            hfx::hfx_inspect_imgui( hfx_file );
        }
    }
    ImGui::End();
}

//
//
void ShaderAugmentationApplication::app_update() {
    uint64_t key = 0;
    gfx_commands->clear( key, 0, 0, 0, 1 );

    {
        OPTICK_CATEGORY( "ImGui", Optick::Category::UI );
        show_compiler_ui();
    }
}

int main( int argc, char** argv ) {

    ShaderAugmentationApplication application {};
    application.main_loop( { nullptr, 1280, 800, hydra::ApplicationRootTask_SDL, "Shader Augmentation" } );

    return 0;
}