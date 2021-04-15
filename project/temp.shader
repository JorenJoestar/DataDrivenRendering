#version 450
layout (location = 0) in vec2 Frag_UV;
layout (location = 1) in vec4 Frag_Color;
layout (location = 0) out vec4 Out_Color;
layout (binding = 1) uniform sampler2D Texture;
void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
