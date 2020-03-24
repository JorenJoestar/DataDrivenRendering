#pragma once

#include <stdint.h>

//
//  Hydra Graphics - v0.051
//  3D API wrapper around Vulkan/Direct3D12/OpenGL.
//  Mostly based on the amazing Sokol library (https://github.com/floooh/sokol), but with a different target (wrapping Vulkan/Direct3D12).
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/05/22, 18.50
//      Last Modified   : 2019/12/17, 18.42
//
//
// Revision history //////////////////////
//
//      0.051 (2020/03/20): + Moved vertex input rate to verte stream.
//      0.050 (2020/03/16): + Fixed usage of malloc/free.
//      0.049 (2020/03/12): + Added sorting of draw keys.
//      0.048 (2020/03/11): + Rewritten command buffer interface. + Added RectInt. + Changed Viewport and Scissor to use RectInt.
//      0.047 (2020/03/04): + Added swapchain present. + Reworked command buffer interface.
//      0.046 (2020/03/03): + 90% graphics pipeline creation. + Added RenderPass handle to Pipeline creation.
//      0.045 (2020/02/25): + Initial Vulkan creation with SDL support. Still missing resources.
//      0.044 (2020/01/14): + Fixed depth/stencil FBO generation.
//      0.043 (2019/12/17): + Removed 'compute' creation flag.
//      0.042 (2019/10/09): + Added initial support for render passes creation. + Added begin/end render pass commands interface.
//
// Documentation /////////////////////////
//
//  To choose the different rendering backend, define one of the following:
//
//  #define HYDRA_OPENGL
//
//  The API is divided in two sections: resource handling and rendering.
//
//  hydra::graphics::Device             -- responsible for resource handling and main rendering operations.
//  hydra::graphics::CommandBuffer      -- commands for rendering, compute and copy/transfer
//
//
// Usage /////////////////////////////////
//
//  1. Define hydra::graphics::Device in your code.
//  2. call device.init();
//
// Defines ///////////////////////////////
//
//  Different defines can be used to use custom code:
//
//  HYDRA_LOG( message, ... )
//  HYDRA_MALLOC( size )
//  HYDRA_FREE( pointer )
//  HYDRA_ASSERT( condition, message, ... )
//
//
// Code Philosophy ////////////////////////
//
//  1. Create healthy defaults for each struct.
//  2. init/terminate initialize or terminate a class/struct.
//  3. Create/Destroy are used for actual creation/destruction of resources.
//


//////////////////////////////////
// Api selection

// Define these either here or in project.
//#define HYDRA_VULKAN
//#define HYDRA_D3D12
//#define HYDRA_OPENGL

//////////////////////////////////
// Window management selection

//#define HYDRA_SDL
//#define HYDRA_GLFW

// Disable some warnings
#ifdef _MSC_VER
#pragma warning (disable: 4505)     // unreference function removed
#pragma warning (disable: 4100)
#endif // _MSC_VER

// API includes
#if defined (HYDRA_OPENGL)
// 
#include <GL/glew.h>

#elif defined (HYDRA_VULKAN)

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

#endif // HYDRA_VULKAN

namespace hydra {
namespace graphics {


// Resources ////////////////////////////////////////////////////////////////////

typedef uint32_t                    ResourceHandle;

struct BufferHandle {
    ResourceHandle                  handle;
}; // struct BufferHandle

struct TextureHandle {
    ResourceHandle                  handle;
}; // struct TextureHandle

struct ShaderHandle {
    ResourceHandle                  handle;
}; // struct ShaderHandle

struct SamplerHandle {
    ResourceHandle                  handle;
}; // struct SamplerHandle

struct ResourceListLayoutHandle {
    ResourceHandle                  handle;
}; // struct ResourceListLayoutHandle

struct ResourceListHandle {
    ResourceHandle                  handle;
}; // struct ResourceListHandle

struct PipelineHandle {
    ResourceHandle                  handle;
}; // struct PipelineHandle

struct RenderPassHandle {
	ResourceHandle                  handle;
}; // struct RenderPassHandle

// Enums ////////////////////////////////////////////////////////////////////////


// !!! WARNING !!!
// THIS CODE IS GENERATED WITH HYDRA DATA FORMAT CODE GENERATOR.

/////////////////////////////////////////////////////////////////////////////////

namespace Blend {
    enum Enum {
        Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor, SrcAlphasat, Src1Color, InvSrc1Color, Src1Alpha, InvSrc1Alpha, Count
    };

    static const char* s_value_names[] = {
        "Zero", "One", "SrcColor", "InvSrcColor", "SrcAlpha", "InvSrcAlpha", "DestAlpha", "InvDestAlpha", "DestColor", "InvDestColor", "SrcAlphaSat", "Src1Color", "InvSrc1Color", "Src1Alpha", "InvSrc1Alpha", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace Blend

namespace BlendOperation {
    enum Enum {
        Add, Subtract, RevSubtract, Min, Max, Count
    };

    enum Mask {
        Add_mask = 1 << 0, Subtract_mask = 1 << 1, RevSubtract_mask = 1 << 2, Min_mask = 1 << 3, Max_mask = 1 << 4, Count_mask = 1 << 5
    };

    static const char* s_value_names[] = {
        "Add", "Subtract", "RevSubtract", "Min", "Max", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace BlendOperation

namespace ColorWriteEnabled {
    enum Enum {
        Red, Green, Blue, Alpha, All, Count
    };

    enum Mask {
        Red_mask = 1 << 0, Green_mask = 1 << 1, Blue_mask = 1 << 2, Alpha_mask = 1 << 3, All_mask = Red_mask | Green_mask | Blue_mask | Alpha_mask
    };

    static const char* s_value_names[] = {
        "Red", "Green", "Blue", "Alpha", "All", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ColorWriteEnabled

namespace ComparisonFunction {
    enum Enum {
        Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always, Count
    };

    enum Mask {
        Never_mask = 1 << 0, Less_mask = 1 << 1, Equal_mask = 1 << 2, LessEqual_mask = 1 << 3, Greater_mask = 1 << 4, NotEqual_mask = 1 << 5, GreaterEqual_mask = 1 << 6, Always_mask = 1 << 7, Count_mask = 1 << 8
    };

    static const char* s_value_names[] = {
        "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ComparisonFunction

namespace CullMode {
    enum Enum {
        None, Front, Back, Count
    };

    enum Mask {
        None_mask = 1 << 0, Front_mask = 1 << 1, Back_mask = 1 << 2, Count_mask = 1 << 3
    };

    static const char* s_value_names[] = {
        "None", "Front", "Back", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace CullMode

namespace DepthWriteMask {
    enum Enum {
        Zero, All, Count
    };

    enum Mask {
        Zero_mask = 1 << 0, All_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "Zero", "All", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace DepthWriteMask

namespace FillMode {
    enum Enum {
        Wireframe, Solid, Point, Count
    };

    enum Mask {
        Wireframe_mask = 1 << 0, Solid_mask = 1 << 1, Point_mask = 1 << 2, Count_mask = 1 << 3
    };

    static const char* s_value_names[] = {
        "Wireframe", "Solid", "Point", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace FillMode

namespace FrontClockwise {
    enum Enum {
        True, False, Count
    };

    enum Mask {
        True_mask = 1 << 0, False_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "True", "False", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace FrontClockwise

namespace StencilOperation {
    enum Enum {
        Keep, Zero, Replace, IncrSat, DecrSat, Invert, Incr, Decr, Count
    };

    enum Mask {
        Keep_mask = 1 << 0, Zero_mask = 1 << 1, Replace_mask = 1 << 2, IncrSat_mask = 1 << 3, DecrSat_mask = 1 << 4, Invert_mask = 1 << 5, Incr_mask = 1 << 6, Decr_mask = 1 << 7, Count_mask = 1 << 8
    };

    static const char* s_value_names[] = {
        "Keep", "Zero", "Replace", "IncrSat", "DecrSat", "Invert", "Incr", "Decr", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace StencilOperation

namespace TextureFormat {
    enum Enum {
        UNKNOWN, R32G32B32A32_TYPELESS, R32G32B32A32_FLOAT, R32G32B32A32_UINT, R32G32B32A32_SINT, R32G32B32_TYPELESS, R32G32B32_FLOAT, R32G32B32_UINT, R32G32B32_SINT, R16G16B16A16_TYPELESS, R16G16B16A16_FLOAT, R16G16B16A16_UNORM, R16G16B16A16_UINT, R16G16B16A16_SNORM, R16G16B16A16_SINT, R32G32_TYPELESS, R32G32_FLOAT, R32G32_UINT, R32G32_SINT, R10G10B10A2_TYPELESS, R10G10B10A2_UNORM, R10G10B10A2_UINT, R11G11B10_FLOAT, R8G8B8A8_TYPELESS, R8G8B8A8_UNORM, R8G8B8A8_UNORM_SRGB, R8G8B8A8_UINT, R8G8B8A8_SNORM, R8G8B8A8_SINT, R16G16_TYPELESS, R16G16_FLOAT, R16G16_UNORM, R16G16_UINT, R16G16_SNORM, R16G16_SINT, R32_TYPELESS, R32_FLOAT, R32_UINT, R32_SINT, R8G8_TYPELESS, R8G8_UNORM, R8G8_UINT, R8G8_SNORM, R8G8_SINT, R16_TYPELESS, R16_FLOAT, R16_UNORM, R16_UINT, R16_SNORM, R16_SINT, R8_TYPELESS, R8_UNORM, R8_UINT, R8_SNORM, R8_SINT, R9G9B9E5_SHAREDEXP, D32_FLOAT_S8X24_UINT, D24_UNORM_S8_UINT, D32_FLOAT, D24_UNORM_X8_UINT, D16_UNORM, S8_UINT, BC1_TYPELESS, BC1_UNORM, BC1_UNORM_SRGB, BC2_TYPELESS, BC2_UNORM, BC2_UNORM_SRGB, BC3_TYPELESS, BC3_UNORM, BC3_UNORM_SRGB, BC4_TYPELESS, BC4_UNORM, BC4_SNORM, BC5_TYPELESS, BC5_UNORM, BC5_SNORM, B5G6R5_UNORM, B5G5R5A1_UNORM, B8G8R8A8_UNORM, B8G8R8X8_UNORM, R10G10B10_XR_BIAS_A2_UNORM, B8G8R8A8_TYPELESS, B8G8R8A8_UNORM_SRGB, B8G8R8X8_TYPELESS, B8G8R8X8_UNORM_SRGB, BC6H_TYPELESS, BC6H_UF16, BC6H_SF16, BC7_TYPELESS, BC7_UNORM, BC7_UNORM_SRGB, FORCE_UINT, Count
    };

    static const char* s_value_names[] = {
        "UNKNOWN", "R32G32B32A32_TYPELESS", "R32G32B32A32_FLOAT", "R32G32B32A32_UINT", "R32G32B32A32_SINT", "R32G32B32_TYPELESS", "R32G32B32_FLOAT", "R32G32B32_UINT", "R32G32B32_SINT", "R16G16B16A16_TYPELESS", "R16G16B16A16_FLOAT", "R16G16B16A16_UNORM", "R16G16B16A16_UINT", "R16G16B16A16_SNORM", "R16G16B16A16_SINT", "R32G32_TYPELESS", "R32G32_FLOAT", "R32G32_UINT", "R32G32_SINT", "R10G10B10A2_TYPELESS", "R10G10B10A2_UNORM", "R10G10B10A2_UINT", "R11G11B10_FLOAT", "R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "R8G8B8A8_UNORM_SRGB", "R8G8B8A8_UINT", "R8G8B8A8_SNORM", "R8G8B8A8_SINT", "R16G16_TYPELESS", "R16G16_FLOAT", "R16G16_UNORM", "R16G16_UINT", "R16G16_SNORM", "R16G16_SINT", "R32_TYPELESS", "R32_FLOAT", "R32_UINT", "R32_SINT", "R8G8_TYPELESS", "R8G8_UNORM", "R8G8_UINT", "R8G8_SNORM", "R8G8_SINT", "R16_TYPELESS", "R16_FLOAT", "R16_UNORM", "R16_UINT", "R16_SNORM", "R16_SINT", "R8_TYPELESS", "R8_UNORM", "R8_UINT", "R8_SNORM", "R8_SINT", "R9G9B9E5_SHAREDEXP", "D32_FLOAT_S8X24_UINT", "D24_UNORM_S8_UINT", "D32_FLOAT", "D24_UNORM_X8_UINT", "D16_UNORM", "S8_UINT", "BC1_TYPELESS", "BC1_UNORM", "BC1_UNORM_SRGB", "BC2_TYPELESS", "BC2_UNORM", "BC2_UNORM_SRGB", "BC3_TYPELESS", "BC3_UNORM", "BC3_UNORM_SRGB", "BC4_TYPELESS", "BC4_UNORM", "BC4_SNORM", "BC5_TYPELESS", "BC5_UNORM", "BC5_SNORM", "B5G6R5_UNORM", "B5G5R5A1_UNORM", "B8G8R8A8_UNORM", "B8G8R8X8_UNORM", "R10G10B10_XR_BIAS_A2_UNORM", "B8G8R8A8_TYPELESS", "B8G8R8A8_UNORM_SRGB", "B8G8R8X8_TYPELESS", "B8G8R8X8_UNORM_SRGB", "BC6H_TYPELESS", "BC6H_UF16", "BC6H_SF16", "BC7_TYPELESS", "BC7_UNORM", "BC7_UNORM_SRGB", "FORCE_UINT", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TextureFormat

namespace TopologyType {
    enum Enum {
        Unknown, Point, Line, Triangle, Patch, Count
    };

    enum Mask {
        Unknown_mask = 1 << 0, Point_mask = 1 << 1, Line_mask = 1 << 2, Triangle_mask = 1 << 3, Patch_mask = 1 << 4, Count_mask = 1 << 5
    };

    static const char* s_value_names[] = {
        "Unknown", "Point", "Line", "Triangle", "Patch", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TopologyType

namespace BufferType {
    enum Enum {
        Vertex, Index, Constant, Indirect, Count
    };

    enum Mask {
        Vertex_mask = 1 << 0, Index_mask = 1 << 1, Constant_mask = 1 << 2, Indirect_mask = 1 << 3, Count_mask = 1 << 4
    };

    static const char* s_value_names[] = {
        "Vertex", "Index", "Constant", "Indirect", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace BufferType

namespace ResourceUsageType {
    enum Enum {
        Immutable, Dynamic, Stream, Count
    };

    enum Mask {
        Immutable_mask = 1 << 0, Dynamic_mask = 1 << 1, Stream_mask = 1 << 2, Count_mask = 1 << 3
    };

    static const char* s_value_names[] = {
        "Immutable", "Dynamic", "Stream", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ResourceUsageType

namespace IndexType {
    enum Enum {
        Uint16, Uint32, Count
    };

    enum Mask {
        Uint16_mask = 1 << 0, Uint32_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "Uint16", "Uint32", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace IndexType

namespace TextureType {
    enum Enum {
        Texture1D, Texture2D, Texture3D, Texture_1D_Array, Texture_2D_Array, Texture_Cube_Array, Count
    };

    enum Mask {
        Texture1D_mask = 1 << 0, Texture2D_mask = 1 << 1, Texture3D_mask = 1 << 2, Texture_1D_Array_mask = 1 << 3, Texture_2D_Array_mask = 1 << 4, Texture_Cube_Array_mask = 1 << 5, Count_mask = 1 << 6
    };

    static const char* s_value_names[] = {
        "Texture1D", "Texture2D", "Texture3D", "Texture_1D_Array", "Texture_2D_Array", "Texture_Cube_Array", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TextureType

namespace ShaderStage {
    enum Enum {
        Vertex, Fragment, Geometry, Compute, Hull, Domain, Count
    };

    enum Mask {
        Vertex_mask = 1 << 0, Fragment_mask = 1 << 1, Geometry_mask = 1 << 2, Compute_mask = 1 << 3, Hull_mask = 1 << 4, Domain_mask = 1 << 5, Count_mask = 1 << 6
    };

    static const char* s_value_names[] = {
        "Vertex", "Fragment", "Geometry", "Compute", "Hull", "Domain", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ShaderStage

namespace TextureFilter {
    enum Enum {
        Nearest, Linear, Count
    };

    enum Mask {
        Nearest_mask = 1 << 0, Linear_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "Nearest", "Linear", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TextureFilter

namespace TextureMipFilter {
    enum Enum {
        Nearest, Linear, Count
    };

    enum Mask {
        Nearest_mask = 1 << 0, Linear_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "Nearest", "Linear", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TextureMipFilter

namespace TextureAddressMode {
    enum Enum {
        Repeat, Mirrored_Repeat, Clamp_Edge, Clamp_Border, Count
    };

    enum Mask {
        Repeat_mask = 1 << 0, Mirrored_Repeat_mask = 1 << 1, Clamp_Edge_mask = 1 << 2, Clamp_Border_mask = 1 << 3, Count_mask = 1 << 4
    };

    static const char* s_value_names[] = {
        "Repeat", "Mirrored_Repeat", "Clamp_Edge", "Clamp_Border", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace TextureAddressMode

namespace VertexComponentFormat {
    enum Enum {
        Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Count
    };

    static const char* s_value_names[] = {
        "Float", "Float2", "Float3", "Float4", "Mat4", "Byte", "Byte4N", "UByte", "UByte4N", "Short2", "Short2N", "Short4", "Short4N", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace VertexComponentFormat

namespace VertexInputRate {
    enum Enum {
        PerVertex, PerInstance, Count
    };

    enum Mask {
        PerVertex_mask = 1 << 0, PerInstance_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "PerVertex", "PerInstance", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace VertexInputRate

namespace LogicOperation {
    enum Enum {
        Clear, Set, Copy, CopyInverted, Noop, Invert, And, Nand, Or, Nor, Xor, Equiv, AndReverse, AndInverted, OrReverse, OrInverted, Count
    };

    enum Mask {
        Clear_mask = 1 << 0, Set_mask = 1 << 1, Copy_mask = 1 << 2, CopyInverted_mask = 1 << 3, Noop_mask = 1 << 4, Invert_mask = 1 << 5, And_mask = 1 << 6, Nand_mask = 1 << 7, Or_mask = 1 << 8, Nor_mask = 1 << 9, Xor_mask = 1 << 10, Equiv_mask = 1 << 11, AndReverse_mask = 1 << 12, AndInverted_mask = 1 << 13, OrReverse_mask = 1 << 14, OrInverted_mask = 1 << 15, Count_mask = 1 << 16
    };

    static const char* s_value_names[] = {
        "Clear", "Set", "Copy", "CopyInverted", "Noop", "Invert", "And", "Nand", "Or", "Nor", "Xor", "Equiv", "AndReverse", "AndInverted", "OrReverse", "OrInverted", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace LogicOperation

namespace QueueType {
    enum Enum {
        Graphics, Compute, CopyTransfer, Count
    };

    enum Mask {
        Graphics_mask = 1 << 0, Compute_mask = 1 << 1, CopyTransfer_mask = 1 << 2, Count_mask = 1 << 3
    };

    static const char* s_value_names[] = {
        "Graphics", "Compute", "CopyTransfer", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace QueueType

namespace CommandType {
    enum Enum {
        BindPipeline, BindResourceTable, BindVertexBuffer, BindIndexBuffer, BindResourceSet, Draw, DrawIndexed, DrawInstanced, DrawIndexedInstanced, Dispatch, CopyResource, SetScissor, SetViewport, Clear, ClearDepth, ClearStencil, BeginPass, EndPass, Count
    };

    static const char* s_value_names[] = {
        "BindPipeline", "BindResourceTable", "BindVertexBuffer", "BindIndexBuffer", "BindResourceSet", "Draw", "DrawIndexed", "DrawInstanced", "DrawIndexedInstanced", "Dispatch", "CopyResource", "SetScissor", "SetViewport", "Clear", "ClearDepth", "ClearStencil", "BeginPass", "EndPass", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace CommandType

namespace ResourceType {
    enum Enum {
        Sampler, Texture, TextureRW, Constants, Buffer, BufferRW, Count
    };

    enum Mask {
        Sampler_mask = 1 << 0, Texture_mask = 1 << 1, TextureRW_mask = 1 << 2, Constants_mask = 1 << 3, Buffer_mask = 1 << 4, BufferRW_mask = 1 << 5, Count_mask = 1 << 6
    };

    static const char* s_value_names[] = {
        "Sampler", "Texture", "TextureRW", "Constants", "Buffer", "BufferRW", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ResourceType

// Manually typed enums
enum DeviceExtensions {
    DeviceExtensions_DebugCallback                      = 1 << 0,
};

// TODO: Error enum?

// Consts ///////////////////////////////////////////////////////////////////////

static const uint8_t                k_max_image_outputs         = 8;        // Maximum number of images/render_targets/fbo attachments usable.
static const uint8_t                k_max_resource_layouts      = 8;        // Maximum number of layouts in the pipeline.
static const uint8_t                k_max_shader_stages         = 5;

static const uint32_t               k_submit_header_sentinel    = 0xfefeb7ba;
static const uint32_t               k_invalid_handle = 0xffffffff;

// Resource creation structs ////////////////////////////////////////////////////

//
//
struct Rect2D {
    float                           x                   = 0.0f;
    float                           y                   = 0.0f;
    float                           width               = 0.0f;
    float                           height              = 0.0f;
}; // struct Rect2D

//
//
struct Rect2DInt {
    int16_t                         x                   = 0;
    int16_t                         y                   = 0;
    uint16_t                        width               = 0;
    uint16_t                        height              = 0;
}; // struct Rect2D

//
//
struct Viewport {
    Rect2DInt                       rect;
    float                           min_depth           = 0.0f;
    float                           max_depth           = 0.0f;
}; // struct Viewport

//
//
struct ViewportState {
    uint32_t                        num_viewports       = 0;
    uint32_t                        num_scissors        = 0;
    Viewport*                       viewport            = nullptr;
    Rect2DInt*                      scissors            = nullptr;
}; // struct ViewportState

//
//
struct StencilOperationState {

    StencilOperation::Enum          fail                = StencilOperation::Keep;
    StencilOperation::Enum          pass                = StencilOperation::Keep;
    StencilOperation::Enum          depth_fail          = StencilOperation::Keep;
    ComparisonFunction::Enum        compare             = ComparisonFunction::Always;
    uint32_t                        compare_mask        = 0xff;
    uint32_t                        write_mask          = 0xff;
    uint32_t                        reference           = 0xff;

}; // struct StencilOperationState

//
//
struct DepthStencilCreation {

    StencilOperationState           front;
    StencilOperationState           back;
    ComparisonFunction::Enum        depth_comparison    = ComparisonFunction::Always;

    uint8_t                         depth_enable        : 1;
    uint8_t                         depth_write_enable  : 1;
    uint8_t                         stencil_enable      : 1;
    uint8_t                         pad                 : 5;

    // Default constructor
    DepthStencilCreation() : depth_enable( 0 ), depth_write_enable( 0 ), stencil_enable( 0 ), depth_comparison( ComparisonFunction::Less ) {
    }

}; // struct DepthStencilCreation

struct BlendState {
    
    Blend::Enum                     source_color        = Blend::One;
    Blend::Enum                     destination_color   = Blend::One;
    BlendOperation::Enum            color_operation     = BlendOperation::Add;

    Blend::Enum                     source_alpha        = Blend::One;
    Blend::Enum                     destination_alpha   = Blend::One;
    BlendOperation::Enum            alpha_operation     = BlendOperation::Add;

    ColorWriteEnabled::Mask         color_write_mask    = ColorWriteEnabled::All_mask;

    uint8_t                         blend_enabled       : 1;
    uint8_t                         separate_blend      : 1;
    uint8_t                         pad                 : 6;


    BlendState() : blend_enabled( 0 ), separate_blend( 0 ) {
    }

}; // struct BlendState

struct BlendStateCreation {

    BlendState                      blend_states[k_max_image_outputs];
    uint32_t                        active_states       = 0;

}; // BlendStateCreation

//
//
struct RasterizationCreation {

    CullMode::Enum                  cull_mode           = CullMode::None;
    FrontClockwise::Enum            front               = FrontClockwise::False;
    FillMode::Enum                  fill                = FillMode::Solid;
}; // struct RasterizationCreation

//
//
struct DeviceCreation {

    void*                           window              = nullptr; // Pointer to API-specific window: SDL_Window, GLFWWindow
    uint16_t                        width               = 1;
    uint16_t                        height              = 1;
    bool                            debug               = false;

}; // struct DeviceCreation

//
//
struct BufferCreation {

    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    uint32_t                        size                = 0;
    void*                           initial_data        = nullptr;
    const char*                     name                = nullptr;

}; // struct BufferCreation

//
//
struct TextureCreation {

    void*                           initial_data        = nullptr;
    uint16_t                        width               = 1;
    uint16_t                        height              = 1;
    uint16_t                        depth               = 1;
    uint8_t                         mipmaps             = 1;
    uint8_t                         render_target       = 0;

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

    const char*                     name                = nullptr;

}; // struct TextureCreation

//
//
struct SamplerCreation {
    
    TextureFilter::Enum             min_filter          = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter          = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter          = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w      = TextureAddressMode::Repeat;

    const char*                     name                = nullptr;

}; // struct SamplerCreation

//
//
struct ShaderCreation {

    struct Stage {

        const char*                 code                = nullptr;
        uint32_t                    code_size           = 0;
        ShaderStage::Enum           type                = ShaderStage::Compute;

    }; // struct Stage

    Stage                           stages[k_max_shader_stages];

    const char*                     name                = nullptr;

    uint32_t                        stages_count        = 0;

}; // struct ShaderCreation

//
// 
struct ResourceListLayoutCreation {
    
    //
    // A single resource binding. It can be relative to one or more resources of the same type.
    //
    struct Binding {

        ResourceType::Enum          type                = ResourceType::Buffer;
        uint16_t                    start               = 0;
        uint16_t                    count               = 0;
        char                        name[32];
    }; // struct Binding

    const Binding*                  bindings            = nullptr;
    uint32_t                        num_bindings        = 0;

}; // struct ResourceListLayoutCreation

//
//
struct ResourceListCreation {

    ResourceListLayoutHandle            layout;

    // List of resources
    struct Resource {

        ResourceHandle              handle;

    }; // struct Resource

    const Resource*                 resources           = nullptr;
    uint32_t                        num_resources       = 0;

}; // struct ResourceListCreation


//
//
struct VertexAttribute {

    uint16_t                        location            = 0;
    uint16_t                        binding             = 0;
    uint32_t                        offset              = 0;
    VertexComponentFormat::Enum     format              = VertexComponentFormat::Count;

}; // struct VertexAttribute

//
//
struct VertexStream {

    uint16_t                        binding             = 0;
    uint16_t                        stride              = 0;
    VertexInputRate::Enum           input_rate          = VertexInputRate::Count;

}; // struct VertexStream

//
//
struct VertexInputCreation {

    uint32_t                        num_vertex_streams	= 0;
    uint32_t                        num_vertex_attributes = 0;

    const VertexStream*             vertex_streams		= nullptr;
    const VertexAttribute*          vertex_attributes	= nullptr;
}; // struct VertexInputCreation

//
//
struct ShaderBinding {

    const char*                     binding_name        = nullptr;
    const char*                     resource_name       = nullptr;

}; // struct ShaderBinding 

//
//
struct RenderPassCreation {

	uint16_t						num_render_targets  = 0;
    uint8_t                         is_swapchain        = false;
    uint8_t                         is_compute_post     = false;                // This is set to determine if the post-process pass is compute or not.

    TextureHandle*   			    output_textures;
    TextureHandle				    depth_stencil_texture;

}; // struct RenderPassCreation

//
//
struct PipelineCreation {

    RasterizationCreation           rasterization;
    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    VertexInputCreation             vertex_input;
    ShaderCreation                  shaders;

    RenderPassHandle                render_pass;
    ResourceListLayoutHandle        resource_list_layout[k_max_resource_layouts];
    const ViewportState*            viewport            = nullptr;

    uint32_t                        num_active_layouts  = 0;

}; // struct PipelineCreation

// API-agnostic structs /////////////////////////////////////////////////////////

//
// Helper methods for texture formats
//
namespace TextureFormat {
                                                                        // D32_FLOAT_S8X24_UINT, D24_UNORM_S8_UINT, D32_FLOAT, D24_UNORM_X8_UINT, D16_UNORM, S8_UINT
    inline bool                     is_depth_stencil( Enum value )      { return value == D32_FLOAT_S8X24_UINT || value == D24_UNORM_S8_UINT; }
    inline bool                     is_depth_only( Enum value )         { return value >= D32_FLOAT && value < S8_UINT; }
    inline bool                     is_stencil_only( Enum value )       { return value == S8_UINT; }

    inline bool                     has_depth( Enum value )             { return value >= D32_FLOAT && value < S8_UINT; }
    inline bool                     has_stencil( Enum value )           { return value == D32_FLOAT_S8X24_UINT || value == D24_UNORM_S8_UINT || value == S8_UINT; }

} // namespace TextureFormat

struct ResourceData {

    void*                           data                = nullptr;

}; // struct ResourceData

//
//
struct ResourceBinding {
    uint16_t                        type                = 0;    // ResourceType
    uint16_t                        start               = 0;
    uint16_t                        count               = 0;
    uint16_t                        set                 = 0;

    const char*                     name                = nullptr;
}; // struct ResourceBinding


// API-agnostic descriptions ////////////////////////////////////////////////////

//
//
struct ShaderStateDescription {

    void*                           native_handle       = nullptr;
    const char*                     name                = nullptr;

}; // struct ShaderStateDescription

//
//
struct BufferDescription {

    void*                           native_handle       = nullptr;

    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    uint32_t                        size                = 0;
    const char*                     name                = nullptr;

}; // struct BufferDescription

//
//
struct TextureDescription {

    void*                           native_handle       = nullptr;

    uint16_t                        width               = 1;
    uint16_t                        height              = 1;
    uint16_t                        depth               = 1;
    uint8_t                         mipmaps             = 1;
    uint8_t                         render_target       = 0;

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

}; // struct Texture

//
//
struct SamplerDescription {

    TextureFilter::Enum             min_filter          = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter          = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter          = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w      = TextureAddressMode::Repeat;

}; // struct Sampler

static const uint32_t               k_max_resources_per_list = 32;

//
//
struct ResourceListLayoutDescription {

    ResourceBinding                 bindings[k_max_resources_per_list];
    uint32_t                        num_active_bindings = 0;

}; // struct ResourceListLayoutDescription

//
//
struct ResourceListDescription {

    ResourceData                    resources[k_max_resources_per_list];
    uint32_t                        num_active_resources = 0;

}; // struct ResourceListDescription

//
//
struct PipelineDescription {

    ShaderHandle                    shader;

}; // struct PipelineDescription


// API-agnostic resource modifications //////////////////////////////////////////

struct MapBufferParameters {
    BufferHandle                    buffer;
    uint32_t                        offset              = 0;
    uint32_t                        size                = 0;

}; // struct MapBufferParameters


// API-gnostic resources ////////////////////////////////////////////////////////

#if defined (HYDRA_OPENGL)

struct ShaderStateGL;
struct TextureGL;
struct BufferGL;
struct PipelineGL;
struct SamplerGL;
struct ResourceListLayoutGL;
struct ResourceListGL;
struct RenderPassGL;

struct DeviceStateGL;

#define ShaderStateAPIGnostic ShaderStateGL
#define TextureAPIGnostic TextureGL
#define BufferAPIGnostic BufferGL
#define PipelineAPIGnostic PipelineGL
#define SamplerAPIGnostic SamplerGL
#define ResourceListLayoutAPIGnostic ResourceListLayoutGL
#define ResourceListAPIGnostic ResourceListGL
#define RenderPassAPIGnostic RenderPassGL

#elif defined (HYDRA_VULKAN)

static const uint32_t               k_max_swapchain_images = 3;

struct ShaderStateVulkan;
struct TextureVulkan;
struct BufferVulkan;
struct PipelineVulkan;
struct SamplerVulkan;
struct ResourceListLayoutVulkan;
struct ResourceListVulkan;
struct RenderPassVulkan;

struct DeviceStateVulkan;

#define ShaderStateAPIGnostic ShaderStateVulkan
#define TextureAPIGnostic TextureVulkan
#define BufferAPIGnostic BufferVulkan
#define PipelineAPIGnostic PipelineVulkan
#define SamplerAPIGnostic SamplerVulkan
#define ResourceListLayoutAPIGnostic ResourceListLayoutVulkan
#define ResourceListAPIGnostic ResourceListVulkan
#define RenderPassAPIGnostic RenderPassVulkan

#endif // HYDRA_OPENGL


// Main structs /////////////////////////////////////////////////////////////////

// Forward-declarations /////////////////////////////////////////////////////////
struct CommandBuffer;


struct ResourcePool {

    void                            init( uint32_t pool_size, uint32_t resource_size );
    void                            terminate();

    uint32_t                        obtain_resource();
    void                            release_resource( uint32_t handle );
    
    void*                           access_resource( uint32_t handle );
    const void*                     access_resource( uint32_t handle ) const;

    uint8_t*                        memory              = nullptr;
    uint32_t*                       free_indices        = nullptr;

    uint32_t                        free_indices_head   = 0;
    uint32_t                        size                = 16;
    uint32_t                        resource_size       = 4;

}; // struct ResourcePool


struct Device {

    // Init/Terminate methods
    void                            init( const DeviceCreation& creation );
    void                            terminate();

    // Creation/Destruction of resources ////////////////////////////////////////
    BufferHandle                    create_buffer( const BufferCreation& creation );
    TextureHandle                   create_texture( const TextureCreation& creation );
    PipelineHandle                  create_pipeline( const PipelineCreation& creation );
    SamplerHandle                   create_sampler( const SamplerCreation& creation );
    ResourceListLayoutHandle        create_resource_list_layout( const ResourceListLayoutCreation& creation );
    ResourceListHandle              create_resource_list( const ResourceListCreation& creation );
    RenderPassHandle				create_render_pass( const RenderPassCreation& creation );

    void                            destroy_buffer( BufferHandle buffer );
    void                            destroy_texture( TextureHandle texture );
    void                            destroy_pipeline( PipelineHandle pipeline );
    void                            destroy_sampler( SamplerHandle sampler );
    void                            destroy_resource_list_layout( ResourceListLayoutHandle resource_layout );
    void                            destroy_resource_list( ResourceListHandle resource_list );
    void                            destroy_render_pass( RenderPassHandle render_pass );

    // Query Description ////////////////////////////////////////////////////////
    void                            query_buffer( BufferHandle buffer, BufferDescription& out_description );
    void                            query_texture( TextureHandle texture, TextureDescription& out_description );
    void                            query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description );
    void                            query_sampler( SamplerHandle sampler, SamplerDescription& out_description );
    void                            query_resource_list_layout( ResourceListLayoutHandle resource_layout, ResourceListLayoutDescription& out_description );
    void                            query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description );

    // Map/Unmap ////////////////////////////////////////////////////////////////
    void*                           map_buffer( const MapBufferParameters& parameters );
    void                            unmap_buffer( const MapBufferParameters& parameters );

    // Command Buffers //////////////////////////////////////////////////////////
    CommandBuffer*                  get_command_buffer( QueueType::Enum type, uint32_t size, bool baked );    // Request a command buffer with a certain size. If baked reset will affect only the read offset.
    void                            free_command_buffer( CommandBuffer* command_buffer );

    void                            queue_command_buffer( CommandBuffer* command_buffer );          // Queue command buffer that will not be executed until present is called.

    // Rendering ////////////////////////////////////////////////////////////////
    void                            present();

    void                            resize( uint16_t width, uint16_t height );

    void                            resize_output_textures( RenderPassHandle render_pass, uint16_t width, uint16_t height );

    BufferHandle                    get_fullscreen_vertex_buffer() const;           // Returns a vertex buffer usable for fullscreen shaders that uses no vertices.
    RenderPassHandle                get_swapchain_pass() const;                     // Returns what is considered the final pass that writes to the swapchain.

    TextureHandle                   get_dummy_texture() const;
    BufferHandle                    get_dummy_constant_buffer() const;

    // Internals ////////////////////////////////////////////////////////////////
    void                            backend_init( const DeviceCreation& creation );
    void                            backend_terminate();

    ResourcePool                    buffers;
    ResourcePool                    textures;
    ResourcePool                    pipelines;
    ResourcePool                    samplers;
    ResourcePool                    resource_list_layouts;
    ResourcePool                    resource_lists;
	ResourcePool                    render_passes;
    ResourcePool                    command_buffers;
    ResourcePool                    shaders;

    // Primitive resources
    BufferHandle                    fullscreen_vertex_buffer;
    RenderPassHandle                swapchain_pass;
    // Dummy resources
    TextureHandle                   dummy_texture;
    BufferHandle                    dummy_constant_buffer;

    CommandBuffer**                 queued_command_buffers              = nullptr;
    uint32_t                        num_allocated_command_buffers       = 0;
    uint32_t                        num_queued_command_buffers          = 0;

    uint16_t                        swapchain_width                     = 1;
    uint16_t                        swapchain_height                    = 1;


    ShaderStateAPIGnostic*          access_shader( ShaderHandle shader );
    const ShaderStateAPIGnostic*    access_shader( ShaderHandle shader ) const;

    TextureAPIGnostic*              access_texture( TextureHandle texture );
    const TextureAPIGnostic*        access_texture( TextureHandle texture ) const;

    BufferAPIGnostic*               access_buffer( BufferHandle buffer );
    const BufferAPIGnostic*         access_buffer( BufferHandle buffer ) const;

    PipelineAPIGnostic*             access_pipeline( PipelineHandle pipeline );
    const PipelineAPIGnostic*       access_pipeline( PipelineHandle pipeline ) const;

    SamplerAPIGnostic*              access_sampler( SamplerHandle sampler );
    const SamplerAPIGnostic*        access_sampler( SamplerHandle sampler ) const;

    ResourceListLayoutAPIGnostic*   access_resource_list_layout( ResourceListLayoutHandle resource_layout );
    const ResourceListLayoutAPIGnostic* access_resource_list_layout( ResourceListLayoutHandle resource_layout ) const;

    ResourceListAPIGnostic*         access_resource_list( ResourceListHandle resource_list );
    const ResourceListAPIGnostic*   access_resource_list( ResourceListHandle resource_list ) const;

    RenderPassAPIGnostic*           access_render_pass( RenderPassHandle render_pass );
    const RenderPassAPIGnostic*     access_render_pass( RenderPassHandle render_pass ) const;


#if defined (HYDRA_OPENGL)

    // API Specific resource methods. Ideally they should not exist, but needed.

    // Shaders are considered resources in the OpenGL device!
    ShaderHandle                    create_shader( const ShaderCreation& creation );
    void                            destroy_shader( ShaderHandle shader );
    void                            query_shader( ShaderHandle shader, ShaderStateDescription& out_description );


    // Specialized methods
    void                            link_texture_sampler( TextureHandle texture, SamplerHandle sampler );

    DeviceStateGL*                  device_state            = nullptr;
#elif defined (HYDRA_VULKAN)

    ShaderHandle                    create_shader( const ShaderCreation& creation );
    void                            destroy_shader( ShaderHandle shader );
    void                            query_shader( ShaderHandle shader, ShaderStateDescription& out_description );
    
    VkAllocationCallbacks*          vulkan_allocation_callbacks;
    VkInstance                      vulkan_instance;
    VkPhysicalDevice                vulkan_physical_device;
    VkDevice                        vulkan_device;
    VkQueue                         vulkan_queue;
    uint32_t                        vulkan_queue_family;
    VkDescriptorPool                vulkan_descriptor_pool;
    VkSurfaceKHR                    vulkan_window_surface;
    VkImage                         vulkan_swapchain_images[k_max_swapchain_images];
    VkImageView*                    vulkan_swapchain_image_views;
    VkFramebuffer                   vulkan_swapchain_framebuffers[k_max_swapchain_images];
    VkCommandPool                   vulkan_command_pool;
    VkCommandBuffer                 vulkan_command_buffer[k_max_swapchain_images];
    VkCommandBuffer                 vulkan_command_buffer_immediate;

    static const uint32_t           k_max_frames = 3;

    VkSemaphore                     vulkan_image_available_semaphores[k_max_frames];
    VkSemaphore                     vulkan_render_finished_semaphores[k_max_frames];
    VkFence                         vulkan_in_flight_fences[k_max_frames];

    // Windows specific
    VkSurfaceFormatKHR              vulkan_surface_format;
    VkPresentModeKHR                vulkan_present_mode;
    VkSwapchainKHR                  vulkan_swapchain;

    uint32_t                        vulkan_swapchain_image_count;

    VkDebugReportCallbackEXT        vulkan_debug_callback;

    uint32_t                        vulkan_image_index;
    uint32_t                        current_frame;

    VmaAllocator                    vma_allocator;
#elif defined (HYDRA_OPENGL)
#endif // HYDRA_VULKAN

}; // struct Device


// Commands list ////////////////////////////////////////////////////////////////

namespace commands {


    // NEW
    struct BindPassData {
        RenderPassHandle                handle;
    }; // struct BindPassData

    
    struct BindPipelineData {
        PipelineHandle                  handle;
    }; // struct BindPipelineData

    struct BindResourceListData {

        ResourceListHandle              handles[k_max_resource_layouts];
        uint32_t                        offsets[k_max_resource_layouts];

        uint32_t                        num_lists;
        uint32_t                        num_offsets;
    }; // struct BindResourceSet

    struct BindVertexBufferData {

        BufferHandle                    buffer;
        uint32_t                        binding;
        uint32_t                        byte_offset;
    }; // struct BindVertexBuffer

    struct BindIndexBufferData {

        BufferHandle                    buffer;
    }; // struct BindIndexBuffer


    struct DrawData {

        TopologyType::Enum              topology;
        uint32_t                        first_vertex;
        uint32_t                        vertex_count;
        uint32_t                        instance_count;
    }; // struct Draw

    struct DrawIndexedData {

        TopologyType::Enum              topology;
        uint32_t                        index_count;
        uint32_t                        instance_count;
        uint32_t                        first_index;
        int32_t                         vertex_offset;
        uint32_t                        first_instance;

    }; // struct DrawIndexed

    struct DispatchData {

        uint16_t                        group_x;
        uint16_t                        group_y;
        uint16_t                        group_z;
    }; // struct Dispatch

    struct CopyResourceData {


    }; // struct CopyResource

    struct SetViewportData {

        Viewport                        viewport;

    }; // struct SetViewport

    struct SetScissorData {

        Rect2DInt                       rect;

    }; // struct SetScissor

    struct ClearData {

        float                           clear_color[4];

    }; // struct Clear

    struct ClearDepthData {

        float                           value;

    }; // struct Clear

    struct ClearStencilData {

        uint8_t                         value;
    }; // struct Clear

} // namespace Commands

// CommandBuffer ////////////////////////////////////////////////////////////////

//
//
struct CommandBuffer {

    void                            init( QueueType::Enum type, uint32_t buffer_size, uint32_t submit_size, bool baked );
    void                            terminate();

    //
    // Commands interface
    //

    void                            bind_pass( uint64_t sort_key, RenderPassHandle handle );
    void                            bind_pipeline( uint64_t sort_key, PipelineHandle handle );
    void                            bind_vertex_buffer( uint64_t sort_key, BufferHandle handle, uint32_t binding, uint32_t offset );
    void                            bind_index_buffer( uint64_t sort_key, BufferHandle handle );
    void                            bind_resource_list( uint64_t sort_key, ResourceListHandle* handles, uint32_t num_lists, uint32_t* offsets, uint32_t num_offsets );

    void                            set_viewport( uint64_t sort_key, const Viewport& viewport );
    void                            set_scissor( uint64_t sort_key, const Rect2DInt& rect );

    void                            clear( uint64_t sort_key, float red, float green, float blue, float alpha );
    void                            clear_depth( uint64_t sort_key, float value );
    void                            clear_stencil( uint64_t sort_key, uint8_t value );

    void                            draw( uint64_t sort_key, TopologyType::Enum topology, uint32_t first_vertex, uint32_t vertex_count, uint32_t instance_count = 0 );
    void                            drawIndexed( uint64_t sort_key, TopologyType::Enum topology, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance );

    void                            dispatch( uint64_t sort_key, uint32_t group_x, uint32_t group_y, uint32_t group_z );

    //bool                            has_commands()      { return write_offset > 0; }
    //bool                            end_of_stream()     { return read_offset >= write_offset; }

    void                            reset();

#if defined (HYDRA_VULKAN)
    VkCommandBuffer                 vk_command_buffer;
    Device*                         device;
#endif // HYDRA_VULKAN

    // NEW INTERFACE
    uint64_t*                       keys;
    uint8_t*                        types;
    void**                          datas;

    uint32_t                        current_command;

    ResourceHandle                  resource_handle;
    uint32_t                        swapchain_frame_issued;
    uint32_t                        frames_in_flight;

    QueueType::Enum                 type                = QueueType::Graphics;

    uint8_t*                        buffer_data                = nullptr;
    uint32_t                        read_offset         = 0;
    uint32_t                        write_offset        = 0;
    uint32_t                        buffer_size         = 0;

    bool                            baked               = false;        // If baked reset will affect only the read of the commands.

}; // struct CommandBuffer

// Inlines //////////////////////////////////////////////////////////////////////



} // namespace graphics
} // namespace hydra
