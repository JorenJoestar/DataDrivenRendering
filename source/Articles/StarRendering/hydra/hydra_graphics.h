#pragma once

#include <stdint.h>

//
//  Hydra Graphics - v0.28
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
//      0.28  (2020/12/30): + Implemented query methods for resources.
//      0.27  (2020/12/29): + Added barrier textures from Render Pass. + Improved resize of RenderPass.
//      0.26  (2020/12/28): + Added resize handling inside Render Pass.
//      0.25  (2020/12/27): + Added reset methods to Creation structures.
//      0.24  (2020/12/23): + Added GPU timestamp queries.
//      0.23  (2020/12/22): + Added support for samplers resources.
//      0.22  (2020/12/22): + Fixed support for depth stencil in barriers and render pass creation for Vulkan backend.
//      0.21  (2020/12/15): + Implemented resource deletion methods for Vulkan backend.
//      0.20  (2020/09/24): + Added render frame to handle multi threaded rendering for Vulkan backend.
//      0.19  (2020/09/24): + Added resize handling for Vulkan backend.
//      0.18  (2020/09/23): + Added barriers and transitions API.
//      0.17  (2020/09/23): + Added texture, sampler and buffer helper methods + Added texture creation flags. + Added support for compute shader textures outputs.
//      0.16  (2020/09/17): + Added Vulkan backend render pass creation. Compute still missing. + Added helpers for render pass and depth test.
//      0.15  (2020/09/16): + Fixed blend and depth test, clear colors for Vulkan backend. + Added swapchain depth.
//      0.14  (2020/09/15): + Fixing Vulkan device back to working to a minimum set.
//      0.13  (2020/09/15): + Added building helper methods to Resource List, Layouts and Shader State creations structs.
//      0.12  (2020/05/13): + Renamed Shader in ShaderState. + Renamed ShaderCreation to ShaderStateCreation.
//      0.11  (2020/04/12): + Removed size from GetCommandBuffer device method.
//      0.10  (2020/04/05): + Bumped up version - to follow other libraries versions. + Added base sort-key helper class.
//      0.052 (2020/04/05): + Fixed cases where no optick or enkits are defined.
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


#if !defined (HYDRA_PRIMITIVE_TYPES)

    #define HYDRA_PRIMITIVE_TYPES

    typedef uint8_t                 u8;
    typedef uint16_t                u16;
    typedef uint32_t                u32;
    typedef uint64_t                u64;

    typedef int8_t                  i8;
    typedef int16_t                 i16;
    typedef int32_t                 i32;
    typedef int64_t                 i64;

    typedef float                   f32;
    typedef double                  f64;

    typedef size_t                  sizet;
    typedef UINT_PTR                uintptr;
    typedef INT_PTR                 intptr;

    typedef const char* cstring;

#endif // HYDRA_PRIMITIVE_TYPES


namespace hydra {
namespace graphics {


// Resources ////////////////////////////////////////////////////////////////////

typedef u32                         ResourceHandle;

struct BufferHandle {
    ResourceHandle                  handle;
}; // struct BufferHandle

struct TextureHandle {
    ResourceHandle                  handle;
}; // struct TextureHandle

struct ShaderStateHandle {
    ResourceHandle                  handle;
}; // struct ShaderStateHandle

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
        Sampler, Texture, Image, ImageRW, Constants, Buffer, BufferRW, Count
    };

    enum Mask {
        Sampler_mask = 1 << 0, Texture_mask = 1 << 1, Image_mask = 1 << 2, ImageRW_mask = 1 << 3, Constants_mask = 1 << 4, Buffer_mask = 1 << 5, BufferRW_mask = 1 << 6, Count_mask = 1 << 7
    };

    static const char* s_value_names[] = {
        "Sampler", "Texture", "Image", "ImageRW", "Constants", "Buffer", "BufferRW", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }
} // namespace ResourceType

// Manually typed enums /////////////////////////////////////////////////////////

enum DeviceExtensions {
    DeviceExtensions_DebugCallback                      = 1 << 0,
};

namespace TextureCreationFlags {
    enum Enum {
        None, RenderTarget, ComputeOutput, Count
    };

    enum Mask {
        None_mask = 1 << 0, RenderTarget_mask = 1 << 1, ComputeOutput_mask = 1 << 2
    };

    static const char* s_value_names[] = {
        "None", "RenderTarget", "ComputeOutput", "Count"
    };

    static const char* ToString( Enum e ) {
        return s_value_names[(int)e];
    }

} // namespace TextureCreationFlags


namespace PipelineStage {

    enum Enum {
        DrawIndirect = 0, VertexInput = 1, VertexShader = 2, FragmentShader = 3, RenderTarget = 4, ComputeShader = 5, Transfer = 6
    };

    enum Mask {
        DrawIndirect_mask = 1 << 0, VertexInput_mask = 1 << 1, VertexShader_mask = 1 << 2, FragmentShader_mask = 1 << 3, RenderTarget_mask = 1 << 4, ComputeShader_mask = 1 << 5, Transfer_mask = 1 << 6
    };

} // namespace PipelineStage

namespace RenderPassType {

    enum Enum {
        Standard, Swapchain, Compute
    };
} // namespace RenderPassType


namespace ResourceDeletionType {

    enum Enum {
        Buffer, Texture, Pipeline, Sampler, ResourceListLayout, ResourceList, RenderPass, ShaderState, Count
    };
} // namespace ResourceDeletionType

// TODO: Error enum?

// Consts ///////////////////////////////////////////////////////////////////////

static const u8                     k_max_image_outputs         = 8;        // Maximum number of images/render_targets/fbo attachments usable.
static const u8                     k_max_resource_layouts      = 8;        // Maximum number of layouts in the pipeline.
static const u8                     k_max_shader_stages         = 5;        // Maximum simultaneous shader stages. Applicable to all different type of pipelines.
static const u8                     k_max_resources_per_list    = 16;       // Maximum list elements for both resource list layout and resource lists.
static const u8                     k_max_vertex_streams        = 16;
static const u8                     k_max_vertex_attributes     = 16;

static const u32                    k_submit_header_sentinel    = 0xfefeb7ba;
static const u32                    k_invalid_handle            = 0xffffffff;
static const u32                    k_max_resource_deletions    = 64;

// Resource creation structs ////////////////////////////////////////////////////

//
//
struct Rect2D {
    f32                             x                   = 0.0f;
    f32                             y                   = 0.0f;
    f32                             width               = 0.0f;
    f32                             height              = 0.0f;
}; // struct Rect2D

//
//
struct Rect2DInt {
    i16                             x                   = 0;
    i16                             y                   = 0;
    u16                             width               = 0;
    u16                             height              = 0;
}; // struct Rect2D

//
//
struct Viewport {
    Rect2DInt                       rect;
    f32                             min_depth           = 0.0f;
    f32                             max_depth           = 0.0f;
}; // struct Viewport

//
//
struct ViewportState {
    u32                             num_viewports       = 0;
    u32                             num_scissors        = 0;

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
    u32                             compare_mask        = 0xff;
    u32                             write_mask          = 0xff;
    u32                             reference           = 0xff;

}; // struct StencilOperationState

//
//
struct DepthStencilCreation {

    StencilOperationState           front;
    StencilOperationState           back;
    ComparisonFunction::Enum        depth_comparison    = ComparisonFunction::Always;

    u8                              depth_enable        : 1;
    u8                              depth_write_enable  : 1;
    u8                              stencil_enable      : 1;
    u8                              pad                 : 5;

    // Default constructor
    DepthStencilCreation() : depth_enable( 0 ), depth_write_enable( 0 ), stencil_enable( 0 ), depth_comparison( ComparisonFunction::Less ) {
    }

    DepthStencilCreation&           set_depth( bool write, ComparisonFunction::Enum comparison_test );

}; // struct DepthStencilCreation

struct BlendState {
    
    Blend::Enum                     source_color        = Blend::One;
    Blend::Enum                     destination_color   = Blend::One;
    BlendOperation::Enum            color_operation     = BlendOperation::Add;

    Blend::Enum                     source_alpha        = Blend::One;
    Blend::Enum                     destination_alpha   = Blend::One;
    BlendOperation::Enum            alpha_operation     = BlendOperation::Add;

    ColorWriteEnabled::Mask         color_write_mask    = ColorWriteEnabled::All_mask;

    u8                              blend_enabled       : 1;
    u8                              separate_blend      : 1;
    u8                              pad                 : 6;


    BlendState() : blend_enabled( 0 ), separate_blend( 0 ) {
    }

    BlendState&                     set_color( Blend::Enum source_color, Blend::Enum destination_color, BlendOperation::Enum color_operation );
    BlendState&                     set_alpha( Blend::Enum source_color, Blend::Enum destination_color, BlendOperation::Enum color_operation );
    BlendState&                     set_color_write_mask( ColorWriteEnabled::Mask value );

}; // struct BlendState

struct BlendStateCreation {

    BlendState                      blend_states[k_max_image_outputs];
    u32                             active_states       = 0;

    BlendStateCreation&             reset();
    BlendState&                     add_blend_state();

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
    u16                             width               = 1;
    u16                             height              = 1;

    u16                             gpu_time_queries_per_frame  = 32;
    bool                            enable_gpu_time_queries     = false;
    bool                            debug                       = false;

}; // struct DeviceCreation

//
//
struct BufferCreation {

    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    u32                             size                = 0;
    void*                           initial_data        = nullptr;

    const char*                     name                = nullptr;

    BufferCreation&                 set( BufferType::Enum type, ResourceUsageType::Enum usage, u32 size );
    BufferCreation&                 set_data( void* data );
    BufferCreation&                 set_name( const char* name );

}; // struct BufferCreation

//
//
struct TextureCreation {

    void*                           initial_data        = nullptr;
    u16                             width               = 1;
    u16                             height              = 1;
    u16                             depth               = 1;
    u8                              mipmaps             = 1;
    u8                              flags               = 0;    // TextureCreationFlags bitmasks

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

    const char*                     name                = nullptr;

    TextureCreation&                set_size( u16 width, u16 height, u16 depth );
    TextureCreation&                set_flags( u8 mipmaps, u8 flags );
    TextureCreation&                set_format_type( TextureFormat::Enum format, TextureType::Enum type );
    TextureCreation&                set_name( const char* name );
    TextureCreation&                set_data( void* data );

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

    SamplerCreation&                set_min_mag_mip( TextureFilter::Enum min, TextureFilter::Enum mag, TextureMipFilter::Enum mip );
    SamplerCreation&                set_address_mode_u( TextureAddressMode::Enum u );
    SamplerCreation&                set_address_mode_uv( TextureAddressMode::Enum u, TextureAddressMode::Enum v );
    SamplerCreation&                set_address_mode_uvw( TextureAddressMode::Enum u, TextureAddressMode::Enum v, TextureAddressMode::Enum w );
    SamplerCreation&                set_name( const char* name );

}; // struct SamplerCreation

//
//
struct ShaderStateCreation {

    struct Stage {

        const char*                 code                = nullptr;
        u32                         code_size           = 0;
        ShaderStage::Enum           type                = ShaderStage::Compute;

    }; // struct Stage

    Stage                           stages[k_max_shader_stages];

    const char*                     name                = nullptr;

    u32                             stages_count        = 0;
    u32                             spv_input           = 0;

    // Building helpers
    ShaderStateCreation&            reset();
    ShaderStateCreation&            set_name( const char* name );
    ShaderStateCreation&            add_stage( const char* code, u32 code_size, ShaderStage::Enum type );
    ShaderStateCreation&            set_spv_input( bool value );

}; // struct ShaderStateCreation

//
// 
struct ResourceListLayoutCreation {
    
    //
    // A single resource binding. It can be relative to one or more resources of the same type.
    //
    struct Binding {

        ResourceType::Enum          type                = ResourceType::Buffer;
        u16                         start               = 0;
        u16                         count               = 0;
        char                        name[32];
    }; // struct Binding

    Binding                         bindings[k_max_resources_per_list];
    u32                             num_bindings        = 0;

    const char*                     name                = nullptr;

    // Building helpers
    ResourceListLayoutCreation&     reset();
    ResourceListLayoutCreation&     add_binding( const Binding& binding );
    ResourceListLayoutCreation&     set_name( const char* name );

}; // struct ResourceListLayoutCreation

//
//
struct ResourceListCreation {

    ResourceListLayoutHandle        layout;

    ResourceHandle                  resources[k_max_resources_per_list];
    u32                             num_resources       = 0;

    const char*                     name                = nullptr;

    // Building helpers
    ResourceListCreation&           reset();
    ResourceListCreation&           set_layout( ResourceListLayoutHandle layout );
    ResourceListCreation&           add_resource( ResourceHandle resource );
    ResourceListCreation&           set_resources( ResourceHandle* resources, u32 num_resources );
    ResourceListCreation&           set_name( const char* name );

}; // struct ResourceListCreation


//
//
struct VertexAttribute {

    u16                             location            = 0;
    u16                             binding             = 0;
    u32                             offset              = 0;
    VertexComponentFormat::Enum     format              = VertexComponentFormat::Count;

}; // struct VertexAttribute

//
//
struct VertexStream {

    u16                             binding             = 0;
    u16                             stride              = 0;
    VertexInputRate::Enum           input_rate          = VertexInputRate::Count;

}; // struct VertexStream

//
//
struct VertexInputCreation {

    u32                             num_vertex_streams = 0;
    u32                             num_vertex_attributes = 0;

    VertexStream                    vertex_streams[k_max_vertex_streams];
    VertexAttribute                 vertex_attributes[k_max_vertex_attributes];

    VertexInputCreation&            reset();
    VertexInputCreation&            add_vertex_stream( const VertexStream& stream );
    VertexInputCreation&            add_vertex_attribute( const VertexAttribute& attribute );
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

    u16                             num_render_targets  = 0;
    RenderPassType::Enum            type                = RenderPassType::Standard;

    TextureHandle                   output_textures[k_max_image_outputs];
    TextureHandle                   depth_stencil_texture;

    f32                             scale_x             = 1.f;
    f32                             scale_y             = 1.f;
    u8                              resize              = 1;

    const char*                     name                = nullptr;

    RenderPassCreation&             reset();
    RenderPassCreation&             add_render_texture( TextureHandle texture );
    RenderPassCreation&             set_scaling( f32 scale_x, f32 scale_y, u8 resize );
    RenderPassCreation&             set_depth_stencil_texture( TextureHandle texture );
    RenderPassCreation&             set_name( const char* name );
    RenderPassCreation&             set_type( RenderPassType::Enum type );

}; // struct RenderPassCreation

//
//
struct PipelineCreation {

    RasterizationCreation           rasterization;
    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    VertexInputCreation             vertex_input;
    ShaderStateCreation             shaders;

    RenderPassHandle                render_pass;
    ResourceListLayoutHandle        resource_list_layout[k_max_resource_layouts];
    const ViewportState*            viewport            = nullptr;

    u32                             num_active_layouts  = 0;

    const char*                     name                = nullptr;

    PipelineCreation&               add_resource_list_layout( ResourceListLayoutHandle handle );

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

    inline bool                     has_depth( Enum value )             { return value >= D32_FLOAT_S8X24_UINT && value < S8_UINT; }
    inline bool                     has_stencil( Enum value )           { return value == D32_FLOAT_S8X24_UINT || value == D24_UNORM_S8_UINT || value == S8_UINT; }
    inline bool                     has_depth_or_stencil( Enum value )  { return value >= D32_FLOAT_S8X24_UINT && value <= S8_UINT; }

} // namespace TextureFormat

struct ResourceData {

    void*                           data                = nullptr;

}; // struct ResourceData

//
//
struct ResourceBinding {
    u16                             type                = 0;    // ResourceType
    u16                             start               = 0;
    u16                             count               = 0;
    u16                             set                 = 0;

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
    u32                             size                = 0;
    const char*                     name                = nullptr;

}; // struct BufferDescription

//
//
struct TextureDescription {

    void*                           native_handle       = nullptr;

    u16                             width               = 1;
    u16                             height              = 1;
    u16                             depth               = 1;
    u8                              mipmaps             = 1;
    u8                              render_target       = 0;

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

//
//
struct ResourceListLayoutDescription {

    ResourceBinding                 bindings[k_max_resources_per_list];
    u32                             num_active_bindings = 0;

}; // struct ResourceListLayoutDescription

//
//
struct ResourceListDescription {

    ResourceData                    resources[k_max_resources_per_list];
    u32                             num_active_resources = 0;

}; // struct ResourceListDescription

//
//
struct PipelineDescription {

    ShaderStateHandle               shader;

}; // struct PipelineDescription


// API-agnostic resource modifications //////////////////////////////////////////

struct MapBufferParameters {
    BufferHandle                    buffer;
    u32                             offset              = 0;
    u32                             size                = 0;

}; // struct MapBufferParameters

// Synchronization //////////////////////////////////////////////////////////////

//
//
struct ImageBarrier {

    TextureHandle                   texture;


}; // struct ImageBarrier

//
//
struct MemoryBarrier {

}; // struct MemoryBarrier

//
//
struct ExecutionBarrier {

    PipelineStage::Enum             source_pipeline_stage;
    PipelineStage::Enum             destination_pipeline_stage;

    u32                             num_image_barriers;
    u32                             num_memory_barriers;

    ImageBarrier                    image_barriers[8];
    MemoryBarrier                   memory_barriers[8];

    ExecutionBarrier&               set( PipelineStage::Enum source, PipelineStage::Enum destination );
    ExecutionBarrier&               add_image_barrier( const ImageBarrier& image_barrier );

}; // struct Barrier

//
//
struct ResourceDeletion {

    ResourceDeletionType::Enum      type;
    ResourceHandle                  handle;
    u32                             current_frame;
}; // struct ResourceDeletion

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

static const u32            k_max_swapchain_images = 3;

struct ShaderStateVulkan;
struct TextureVulkan;
struct BufferVulkan;
struct PipelineVulkan;
struct SamplerVulkan;
struct ResourceListLayoutVulkan;
struct ResourceListVulkan;
struct RenderPassVulkan;

struct DeviceStateVulkan;

#define ShaderStateAPIGnostic       ShaderStateVulkan
#define TextureAPIGnostic           TextureVulkan
#define BufferAPIGnostic            BufferVulkan
#define PipelineAPIGnostic          PipelineVulkan
#define SamplerAPIGnostic           SamplerVulkan
#define ResourceListLayoutAPIGnostic ResourceListLayoutVulkan
#define ResourceListAPIGnostic      ResourceListVulkan
#define RenderPassAPIGnostic        RenderPassVulkan

#endif // HYDRA_OPENGL


// Forward-declarations /////////////////////////////////////////////////////////
struct CommandBuffer;
struct DeviceRenderFrame;
struct GPUTimestampManager;

// Main structs /////////////////////////////////////////////////////////////////

//
//
struct ResourcePool {

    void                            init( u32 pool_size, u32 resource_size );
    void                            terminate();

    u32                             obtain_resource();
    void                            release_resource( u32 handle );
    void                            free_all_resources();
    
    void*                           access_resource( u32 handle );
    const void*                     access_resource( u32 handle ) const;

    u8*                             memory              = nullptr;
    u32*                            free_indices        = nullptr;

    u32                             free_indices_head   = 0;
    u32                             size                = 16;
    u32                             resource_size       = 4;

}; // struct ResourcePool

//
//
struct GPUTimestamp {

    u32                             start;
    u32                             end;

    f64                             elapsed_ms;

    u16                             parent_index;
    u16                             depth;

    u32                             color;
    u32                             frame_index;

    const char*                     name;
}; // struct GPUTimestamp


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
    RenderPassHandle                create_render_pass( const RenderPassCreation& creation );
    ShaderStateHandle               create_shader_state( const ShaderStateCreation& creation );

    void                            destroy_buffer( BufferHandle buffer );
    void                            destroy_texture( TextureHandle texture );
    void                            destroy_pipeline( PipelineHandle pipeline );
    void                            destroy_sampler( SamplerHandle sampler );
    void                            destroy_resource_list_layout( ResourceListLayoutHandle resource_layout );
    void                            destroy_resource_list( ResourceListHandle resource_list );
    void                            destroy_render_pass( RenderPassHandle render_pass );
    void                            destroy_shader_state( ShaderStateHandle shader );

    // Query Description ////////////////////////////////////////////////////////
    void                            query_buffer( BufferHandle buffer, BufferDescription& out_description );
    void                            query_texture( TextureHandle texture, TextureDescription& out_description );
    void                            query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description );
    void                            query_sampler( SamplerHandle sampler, SamplerDescription& out_description );
    void                            query_resource_list_layout( ResourceListLayoutHandle resource_layout, ResourceListLayoutDescription& out_description );
    void                            query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description );
    void                            query_shader_state( ShaderStateHandle shader, ShaderStateDescription& out_description );

    // Map/Unmap ////////////////////////////////////////////////////////////////
    void*                           map_buffer( const MapBufferParameters& parameters );
    void                            unmap_buffer( const MapBufferParameters& parameters );

    // Command Buffers //////////////////////////////////////////////////////////
    CommandBuffer*                  get_command_buffer( QueueType::Enum type, bool baked, bool begin );         // Request a command buffer with a certain size. If baked reset will affect only the read offset.
    void                            free_command_buffer( CommandBuffer* command_buffer );

    void                            queue_command_buffer( CommandBuffer* command_buffer );          // Queue command buffer that will not be executed until present is called.

    // Rendering ////////////////////////////////////////////////////////////////
    void                            present();

    void                            resize( u16 width, u16 height );
    void                            resize_swapchain();
    void                            resize_output_textures( RenderPassHandle render_pass, u16 width, u16 height );

    void                            fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier );

    BufferHandle                    get_fullscreen_vertex_buffer() const;           // Returns a vertex buffer usable for fullscreen shaders that uses no vertices.
    RenderPassHandle                get_swapchain_pass() const;                     // Returns what is considered the final pass that writes to the swapchain.

    TextureHandle                   get_dummy_texture() const;
    BufferHandle                    get_dummy_constant_buffer() const;

    // GPU Timings
    void                            set_gpu_timestamps_enable( bool value )         { timestamps_enabled = value; }

    u32                             get_gpu_timestamps( GPUTimestamp* out_timestamps );
    void                            push_gpu_timestamp( CommandBuffer* command_buffer, const char* name );
    void                            pop_gpu_timestamp( CommandBuffer* command_buffer );

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
    SamplerHandle                   default_sampler;
    // Dummy resources
    TextureHandle                   dummy_texture;
    BufferHandle                    dummy_constant_buffer;

    CommandBuffer**                 queued_command_buffers              = nullptr;
    u32                             num_allocated_command_buffers       = 0;
    u32                             num_queued_command_buffers          = 0;

    DeviceRenderFrame*              render_frames;

    u16                             swapchain_width                     = 1;
    u16                             swapchain_height                    = 1;

    GPUTimestampManager*            gpu_timestamp_manager               = nullptr;

    bool                            timestamps_enabled                  = false;
    bool                            resized                             = false;


    ShaderStateAPIGnostic*          access_shader_state( ShaderStateHandle shader );
    const ShaderStateAPIGnostic*    access_shader_state( ShaderStateHandle shader ) const;

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


    // Specialized methods
    void                            link_texture_sampler( TextureHandle texture, SamplerHandle sampler );   // TODO: for now specify a sampler for a texture or use the default one.

#if defined (HYDRA_OPENGL)

    // API Specific resource methods. Ideally they should not exist, but needed.

    DeviceStateGL*                  device_state            = nullptr;
#elif defined (HYDRA_VULKAN)

    void                            create_swapchain();
    void                            destroy_swapchain();

    void                            destroy_buffer_instant( ResourceHandle buffer );
    void                            destroy_texture_instant( ResourceHandle texture );
    void                            destroy_pipeline_instant( ResourceHandle pipeline );
    void                            destroy_sampler_instant( ResourceHandle sampler );
    void                            destroy_resource_list_layout_instant( ResourceHandle resource_layout );
    void                            destroy_resource_list_instant( ResourceHandle resource_list );
    void                            destroy_render_pass_instant( ResourceHandle render_pass );
    void                            destroy_shader_state_instant( ResourceHandle shader );

    void                            set_resource_name( VkObjectType object_type, uint64_t handle, const char* name );

    VkAllocationCallbacks*          vulkan_allocation_callbacks;
    VkInstance                      vulkan_instance;
    VkPhysicalDevice                vulkan_physical_device;
    VkPhysicalDeviceProperties      vulkan_physical_properties;
    VkDevice                        vulkan_device;
    VkQueue                         vulkan_queue;
    uint32_t                        vulkan_queue_family;
    VkDescriptorPool                vulkan_descriptor_pool;
    VkSurfaceKHR                    vulkan_window_surface;
    VkImage                         vulkan_swapchain_images[ k_max_swapchain_images ];
    VkImageView                     vulkan_swapchain_image_views[ k_max_swapchain_images ];
    VkFramebuffer                   vulkan_swapchain_framebuffers[ k_max_swapchain_images ];
    VkQueryPool                     vulkan_timestamp_query_pool;

    TextureHandle                   depth_texture;

    static const uint32_t           k_max_frames = 3;

    // Windows specific
    VkSurfaceFormatKHR              vulkan_surface_format;
    VkPresentModeKHR                vulkan_present_mode;
    VkSwapchainKHR                  vulkan_swapchain;

    u32                             vulkan_swapchain_image_count;

    VkDebugReportCallbackEXT        vulkan_debug_callback;
    VkDebugUtilsMessengerEXT        vulkan_debug_utils_messenger;

    u32                             vulkan_image_index;
    u32                             current_frame;
    u32                             previous_frame;

    u32                             absolute_frame;

    VmaAllocator                    vma_allocator;

    ResourceDeletion                resource_deletion_queue[ k_max_resource_deletions ];
    u32                             num_deletion_queue              = 0;

    f32                             gpu_timestamp_frequency;
    bool                            gpu_timestamp_reset             = true;
    bool                            debug_utils_extension_present   = false;

    char                            vulkan_binaries_path[ 512 ];
#elif defined (HYDRA_OPENGL)
#endif // HYDRA_VULKAN

}; // struct Device

// SortKey //////////////////////////////////////////////////////////////////////

//
//
struct SortKey {

    static uint64_t                     get_key( uint64_t stage_index );

}; // struct SortKey


// Commands list ////////////////////////////////////////////////////////////////

namespace commands {


    struct BindPassData {
        RenderPassHandle                handle;
    }; // struct BindPassData

    
    struct BindPipelineData {
        PipelineHandle                  handle;
    }; // struct BindPipelineData

    struct BindResourceListData {

        ResourceListHandle              handles[k_max_resource_layouts];
        u32                             offsets[k_max_resource_layouts];

        u32                             num_lists;
        u32                             num_offsets;
    }; // struct BindResourceSet

    struct BindVertexBufferData {

        BufferHandle                    buffer;
        u32                             binding;
        u32                             byte_offset;
    }; // struct BindVertexBuffer

    struct BindIndexBufferData {

        BufferHandle                    buffer;
    }; // struct BindIndexBuffer


    struct DrawData {

        TopologyType::Enum              topology;
        u32                             first_vertex;
        u32                             vertex_count;
        u32                             instance_count;
    }; // struct Draw

    struct DrawIndexedData {

        TopologyType::Enum              topology;
        u32                             index_count;
        u32                             instance_count;
        u32                             first_index;
        i32                             vertex_offset;
        u16                             first_instance;

    }; // struct DrawIndexed

    struct DispatchData {

        u16                             group_x;
        u16                             group_y;
        u16                             group_z;
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

        f32                            clear_color[4];

    }; // struct Clear

    struct ClearDepthData {

        f32                            value;

    }; // struct Clear

    struct ClearStencilData {

        u8                              value;
    }; // struct Clear

} // namespace Commands

// CommandBuffer ////////////////////////////////////////////////////////////////

//
//
struct CommandBuffer {

    void                            init( QueueType::Enum type, u32 buffer_size, u32 submit_size, bool baked );
    void                            terminate();

    //
    // Commands interface
    //

    void                            bind_pass( u64 sort_key, RenderPassHandle handle );
    void                            bind_pipeline( u64 sort_key, PipelineHandle handle );
    void                            bind_vertex_buffer( u64 sort_key, BufferHandle handle, u32 binding, u32 offset );
    void                            bind_index_buffer( u64 sort_key, BufferHandle handle );
    void                            bind_resource_list( u64 sort_key, ResourceListHandle* handles, u32 num_lists, u32* offsets, u32 num_offsets );

    void                            set_viewport( u64 sort_key, const Viewport* viewport );
    void                            set_scissor( u64 sort_key, const Rect2DInt* rect );

    void                            clear( u64 sort_key, f32 red, f32 green, f32 blue, f32 alpha );
    void                            clear_depth_stencil( u64 sort_key, f32 depth, uint8_t stencil );

    void                            draw( u64 sort_key, TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count );
    void                            draw_indexed( u64 sort_key, TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance );

    void                            dispatch( u64 sort_key, u32 group_x, u32 group_y, u32 group_z );

    void                            barrier( const ExecutionBarrier& barrier );

    void                            push_marker( const char* name );
    void                            pop_marker();

    void                            reset();

#if defined (HYDRA_VULKAN)
    VkCommandBuffer                 vk_command_buffer;

    Device*                         device;

    VkDescriptorSet                 vk_descriptor_sets[16];

    RenderPassVulkan*               current_render_pass;
    PipelineVulkan*                 current_pipeline;
    VkClearValue                    clears[2];          // 0 = color, 1 = depth stencil
    bool                            is_recording;

    u32                             handle;
#elif defined (HYDRA_OPENGL)
    uint64_t*                       keys;
    uint8_t*                        types;
    void**                          datas;

    uint8_t*                        buffer_data         = nullptr;
    uint32_t                        read_offset         = 0;
    uint32_t                        write_offset        = 0;
#endif // HYDRA_VULKAN

    // NEW INTERFACE
    u32                             current_command;
    ResourceHandle                  resource_handle;
    QueueType::Enum                 type                = QueueType::Graphics;
    u32                             buffer_size         = 0;

    bool                            baked               = false;        // If baked reset will affect only the read of the commands.

}; // struct CommandBuffer

// DeviceRenderFrame //////////////////////////////////////////////////////////////////

struct DeviceRenderFrame {

    void                            init( Device* gpu_device, u32 thread_count );
    void                            terminate();

    CommandBuffer*                  get_command_buffer( u32 thread_index, bool begin );

    void                            new_frame();
    void                            on_resize();

    Device*                         gpu_device          = nullptr;

    u32                             thread_count        = 1;            // IMPORTANT: this determines the size of pools for the frame.

#if defined (HYDRA_VULKAN)
    // Fences and semaphores
    VkFence                         vulkan_in_flight_fence;
    VkSemaphore                     vulkan_image_available_semaphore;
    VkSemaphore                     vulkan_render_finished_semaphore;

    // Command buffers
    VkCommandPool*                  vulkan_command_pools    = nullptr;
    VkCommandBuffer*                vulkan_command_buffers  = nullptr;
    CommandBuffer**                 command_buffers         = nullptr;

#endif // HYDRA_VULKAN

}; // struct DeviceRenderFrame

// Inlines //////////////////////////////////////////////////////////////////////



} // namespace graphics
} // namespace hydra
