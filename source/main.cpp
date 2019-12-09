
#include "MaterialSystem.h"
#include "CustomShaderLanguage.h"

int main(int argc, char** argv) {

    // 1. Application showing some usages of HFX and HDF.
    //WritingLanguageApplication application;
    //application.main_loop();

    // 2. Application showing material system examples.
    MaterialSystemApplication material_application;
    material_application.main_loop();

    return 0;
}
