#version 450

// Common defines

#define UniformBuffer(slot) 	layout (std140, binding=slot) uniform
#define StructuredBuffer(slot) 	layout (std430, binding=slot) buffer

#if defined VULKAN

#define gl_VertexID 			gl_VertexIndex
#define gl_InstanceID			gl_InstanceIndex

#define texture2D				texture

#elif defined OPENGL

#endif // VULKAN
