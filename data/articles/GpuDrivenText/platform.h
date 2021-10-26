
#if defined (FULLSCREEN_TRI) && defined(VERTEX)

layout (location = 0) out vec2 vTexCoord;

void main() {

    vTexCoord.xy = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(vTexCoord.xy * 2.0f - 1.0f, 0.0f, 1.0f);
    gl_Position.y = -gl_Position.y;
}

#endif // FULLSCREEN_TRI

vec3 world_position_from_depth( vec2 uv, float raw_depth, mat4 inverse_view_projection ) {

    vec4 H = vec4( uv.x * 2 - 1, uv.y * -2 + 1, raw_depth * 2 - 1, 1 );
    vec4 D = inverse_view_projection * H;

    return D.xyz / D.w;
}
