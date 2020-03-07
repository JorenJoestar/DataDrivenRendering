#version 450



		#define FRAGMENT

		layout (std140, binding=7) uniform LocalConstants {

			float					scale;
			float					modulo;
			float					pad_tail[2];

		} local_constants;


		#pragma include "Platform.h"

        #if defined VERTEX
        out vec4 vTexCoord;

        void main() {

            vTexCoord.xy = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
            vTexCoord.zw = vTexCoord.xy;

            gl_Position = vec4(vTexCoord.xy * 2.0f + -1.0f, 0.0f, 1.0f);
        }
        #endif // VERTEX

        #if defined FRAGMENT

        in vec4 vTexCoord;

        out vec4 outColor;

        layout(binding=0) uniform sampler2D input_texture;

        void main() {
            vec3 color = texture2D(input_texture, vTexCoord.xy).xyz;
            outColor = vec4(1, 1, 0, 1);
            outColor = vec4(color, 1);
        }
        #endif // FRAGMENT
    