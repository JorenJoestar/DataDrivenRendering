# Data Driven Rendering

Testing data driven rendering code using ImGUI and SDL.<br>
This is a Visual Studio 2017 + Windows 10 only tested code.<br>
The interesting part of the code is self contained and should be easily portable.<br>

Refer to articles at https://jorenjoestar.github.io/ for more informations, and in the following sections to know more.

This code is not bound only to a platform or API, it is just a demonstration and an experiment to craft a language and generate code.

## Build
In order to build the included solution, do the following:

1. Download SDL 2.0.9, Glew 2.1.0, Flatbuffers 1.11.0
2. Unzip them in a common folder
3. Add the environment variable LIB_PATH pointing to that folder
4. Copy the x64 dll of SDL and GLEW into the Bin folder.
5. Compile and run!


## Hydra Definition Format

HDF is a custom format similar to Flatbuffers (but with less feature) and used to start the journey on code generation.<br>
The code is all in CodeGenerator.h and Lexer.h.

The code is based on this article:

https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

It parses a HDF file format and generates a C++ header.

## Hydra Effects

HFX is another format to write shaders, similar to Unity ShaderLab.

It is relative to the following articles:

* https://jorenjoestar.github.io/post/writing_shader_effect_language_1/
* https://jorenjoestar.github.io/post/writing_shader_effect_language_2/

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
