#version 430



		#define FRAGMENT

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
    