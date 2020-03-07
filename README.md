# Data Driven Rendering

Testing data driven rendering code using ImGUI and SDL.<br>
This is a Visual Studio 2017 + Windows 10 only tested code.<br>
The interesting part of the code is self contained and should be easily portable.<br>

Refer to articles at https://jorenjoestar.github.io/ for more informations, and in the following sections to know more.

This code is not bound only to a platform or API, it is just a demonstration and an experiment to craft a language and generate code.

## Build
In order to build the included solution, do the following:

1. Download SDL 2.0.9, Glew 2.1.0
2. Unzip them in a common folder
3. Add the environment variable LIB_PATH pointing to that folder
4. Copy the x64 dll of SDL and GLEW into the Bin folder.
5. Compile and run!

## Directory Layout

In general the subdivision is the following:

1. All the source code in the *source/* folder.
2. All the Visual Studio projects are in the *project/* folder.
3. All the Shaders, Textures, Models and data are in *data/* folder.

To better separate the article-related code, I put the relevant classes in the different folders.<br>

The first two articles about creating a custom shader language, HFX are in data/articles/CustomShaderLanguage and source/articles/CustomShaderLanguage.
These are the articles:
https://jorenjoestar.github.io/post/writing_shader_effect_language_1/ and https://jorenjoestar.github.io/post/writing_shader_effect_language_2/

The Material System article and Render Pipeline article have the source separated, but data shared:
**source/articles/MaterialSystem** the code for https://jorenjoestar.github.io/post/writing_shader_effect_language_3/.

**source/articles/RenderPipeline** the code for https://jorenjoestar.github.io/post/data_driven_rendering_pipeline/.

With the evolutions of the APIs (hydra_***) I will separate even further the code so that I can change the libraries as I want, probably having a snapshot of the used libraries and the code that works in that state.


## Hydra Definition Format

HDF is a custom format similar to Flatbuffers (but with less feature) and used to start the journey on code generation.<br>
The code is all in CodeGenerator.h and Lexer.h.

The code is based on this article:

https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

It parses a HDF file format and generates a C++ header.

## Hydra Effects

HFX is another format to write shaders, similar to Unity ShaderLab.<br>
It does not want to replace any Shader Language, but rather add support for missing features like:

1. Defining Render States
2. Defining Resource Lists
3. Defining Material Properties
4. Compiling shader parts in separate files
5. Compiling shaders informations in one binary file
6. (Still not done) Compiling permutations


It is relative to the following articles:

* https://jorenjoestar.github.io/post/writing_shader_effect_language_1/
* https://jorenjoestar.github.io/post/writing_shader_effect_language_2/
* https://jorenjoestar.github.io/post/writing_shader_effect_language_3/
* https://jorenjoestar.github.io/post/data_driven_rendering_pipeline/

(and soon more!).

HFX is a simple language to help generating rendering code.<br>
The render API is a simple custom made hydra_graphics(.h/.cpp).

Here is an example of HFX, currently used to draw ImGui:

```
shader ImGui {

    // For the artist
    properties {

    }

    // For the developer
    layout {
        
        list Local {
            cbuffer LocalConstants;
            texture2D Texture;
        }
    }
    
    glsl ToScreen {
        
        #pragma include "Platform.h"

        #if defined VERTEX

        layout (location = 0) in vec2 Position;
        layout (location = 1) in vec2 UV;
        layout (location = 2) in vec4 Color;

        uniform LocalConstants { mat4 ProjMtx; };

        out vec2 Frag_UV;
        out vec4 Frag_Color;

        void main()
        {
            Frag_UV = UV;
            Frag_Color = Color;
            gl_Position = ProjMtx * vec4(Position.xy,0,1);
        }
        out vec4 vTexCoord;

        #endif // VERTEX

        #if defined FRAGMENT
        in vec2 Frag_UV;
        in vec4 Frag_Color;
        
        uniform sampler2D Texture;
        
        layout (location = 0) out vec4 Out_Color;
        
        void main()
        {
            Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
        }
        #endif // FRAGMENT
    }

    pass ToScreen {
        // stage = fullscreen
        resources = Local
        vertex = ToScreen
        fragment = ToScreen
    }
}
```

Please read the articles to know more!

Any comment, feedback, error please contact me.

Thank you!
