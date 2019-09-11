#pragma once

#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////////
//  
//  Hydra Graphics: 3D API wrapper around Vulkan/Direct3D12/OpenGL.
//
//  Mostly based on the amazing Sokol library (https://github.com/floooh/sokol),
//  but with a different target (wrapping Vulkan/Direct3D12).
//
//  Source code     : https://www.github.com/jorenjoestar/HydraGraphics
//
//  Date            : 22/05/2019, 18.50
//  Version         : 0.020
//
/////////////////////////////////////////////////////////////////////////////////
//
//      GUIDE
//
//  To choose the different rendering backend, define one of the following:
//
//  #define HYDRA_VULKAN
//  #define HYDRA_D3D12
//  #define HYDRA_OPENGL
//
//  The API is divided in two sections: resource handling and rendering.
//
//  hydra::gfx_device::Device             -- responsible for resource handling and main rendering operations.
//  hydra::gfx_device::CommandBuffer      -- commands for rendering, compute and copy/transfer
//
//
//      USAGE
//
//  1. Define hydra::gfx_device::Device in your code.
//  2. call device.init();
//
//      DEFINES
//
//  Different defines can be used to use custom code:
//
//  HYDRA_LOG( message, ... )
//  HYDRA_MALLOC( size )
//  HYDRA_FREE( pointer )
//  HYDRA_ASSERT( condition, message, ... )
//
//
//      CODE PHILOSOPHY
//
//  1. Create healthy defaults for each struct.
//  2. init/terminate initialize or terminate a class/struct.
//  3. Create/Destroy are used for actual creation/destruction of resources.
//
//
//      GSTODO
//
//  * Substitute enums from HDF format
//  * Init methods for creation classes - to remove more boilerplate code

//////////////////////////////////
//// Api selection

//#define HYDRA_VULKAN
//#define HYDRA_D3D12
#define HYDRA_OPENGL

//////////////////////////////////
// Window management selection

//#define HYDRA_SDL
#define HYDRA_GLFW

// API includes
#if defined (HYDRA_OPENGL)
// 
#include <GL/glew.h>

#elif defined (HYDRA_VULKAN)

#include <vulkan/vulkan.h>

#endif // HYDRA_VULKAN

namespace hydra {
namespace graphics {


/////////////////////////////////////////////////////////////////////////////////
// Resources
/////////////////////////////////////////////////////////////////////////////////

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

struct ResourceSetLayoutHandle {
    ResourceHandle                  handle;
}; // struct ResourceSetLayoutHandle

struct ResourceSetHandle {
    ResourceHandle                  handle;
}; // struct ResourceSetHandle

struct PipelineHandle {
    ResourceHandle                  handle;
}; // struct PipelineHandle

/////////////////////////////////////////////////////////////////////////////////
// Enums
/////////////////////////////////////////////////////////////////////////////////

// !!! WARNING !!!
// THIS CODE IS GENERATED WITH HYDRA DATA FORMAT CODE GENERATOR.

/////////////////////////////////////////////////////////////////////////////////

namespace Blend {
    enum Enum {
        Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor, SrcAlphaSta, BlendFactor, InvBlendFactor, Src1Color, InvSrc1Color, Src1Alpha, InvSrc1Alpha, Count
    };

    enum Mask {
        Zero_mask = 1 << 0, One_mask = 1 << 1, SrcColor_mask = 1 << 2, InvSrcColor_mask = 1 << 3, SrcAlpha_mask = 1 << 4, InvSrcAlpha_mask = 1 << 5, DestAlpha_mask = 1 << 6, InvDestAlpha_mask = 1 << 7, DestColor_mask = 1 << 8, InvDestColor_mask = 1 << 9, SrcAlphaSta_mask = 1 << 10, BlendFactor_mask = 1 << 11, InvBlendFactor_mask = 1 << 12, Src1Color_mask = 1 << 13, InvSrc1Color_mask = 1 << 14, Src1Alpha_mask = 1 << 15, InvSrc1Alpha_mask = 1 << 16, Count_mask = 1 << 17
    };

    static const char* s_value_names[] = {
        "Zero", "One", "SrcColor", "InvSrcColor", "SrcAlpha", "InvSrcAlpha", "DestAlpha", "InvDestAlpha", "DestColor", "InvDestColor", "SrcAlphaSta", "BlendFactor", "InvBlendFactor", "Src1Color", "InvSrc1Color", "Src1Alpha", "InvSrc1Alpha", "Count"
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
        Red_mask = 1 << 0, Green_mask = 1 << 1, Blue_mask = 1 << 2, Alpha_mask = 1 << 3, All_mask = 1 << 4, Count_mask = 1 << 5
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
        UNKNOWN, R32G32B32A32_TYPELESS, R32G32B32A32_FLOAT, R32G32B32A32_UINT, R32G32B32A32_SINT, R32G32B32_TYPELESS, R32G32B32_FLOAT, R32G32B32_UINT, R32G32B32_SINT, R16G16B16A16_TYPELESS, R16G16B16A16_FLOAT, R16G16B16A16_UNORM, R16G16B16A16_UINT, R16G16B16A16_SNORM, R16G16B16A16_SINT, R32G32_TYPELESS, R32G32_FLOAT, R32G32_UINT, R32G32_SINT, R10G10B10A2_TYPELESS, R10G10B10A2_UNORM, R10G10B10A2_UINT, R11G11B10_FLOAT, R8G8B8A8_TYPELESS, R8G8B8A8_UNORM, R8G8B8A8_UNORM_SRGB, R8G8B8A8_UINT, R8G8B8A8_SNORM, R8G8B8A8_SINT, R16G16_TYPELESS, R16G16_FLOAT, R16G16_UNORM, R16G16_UINT, R16G16_SNORM, R16G16_SINT, R32_TYPELESS, R32_FLOAT, R32_UINT, R32_SINT, R8G8_TYPELESS, R8G8_UNORM, R8G8_UINT, R8G8_SNORM, R8G8_SINT, R16_TYPELESS, R16_FLOAT, R16_UNORM, R16_UINT, R16_SNORM, R16_SINT, R8_TYPELESS, R8_UNORM, R8_UINT, R8_SNORM, R8_SINT, R9G9B9E5_SHAREDEXP, D32_FLOAT_S8X24_UINT, D32_FLOAT, D24_UNORM_S8_UINT, D24_UNORM_X8_UINT, D16_UNORM, S8_UINT, BC1_TYPELESS, BC1_UNORM, BC1_UNORM_SRGB, BC2_TYPELESS, BC2_UNORM, BC2_UNORM_SRGB, BC3_TYPELESS, BC3_UNORM, BC3_UNORM_SRGB, BC4_TYPELESS, BC4_UNORM, BC4_SNORM, BC5_TYPELESS, BC5_UNORM, BC5_SNORM, B5G6R5_UNORM, B5G5R5A1_UNORM, B8G8R8A8_UNORM, B8G8R8X8_UNORM, R10G10B10_XR_BIAS_A2_UNORM, B8G8R8A8_TYPELESS, B8G8R8A8_UNORM_SRGB, B8G8R8X8_TYPELESS, B8G8R8X8_UNORM_SRGB, BC6H_TYPELESS, BC6H_UF16, BC6H_SF16, BC7_TYPELESS, BC7_UNORM, BC7_UNORM_SRGB, FORCE_UINT, Count
    };

    static const char* s_value_names[] = {
        "UNKNOWN", "R32G32B32A32_TYPELESS", "R32G32B32A32_FLOAT", "R32G32B32A32_UINT", "R32G32B32A32_SINT", "R32G32B32_TYPELESS", "R32G32B32_FLOAT", "R32G32B32_UINT", "R32G32B32_SINT", "R16G16B16A16_TYPELESS", "R16G16B16A16_FLOAT", "R16G16B16A16_UNORM", "R16G16B16A16_UINT", "R16G16B16A16_SNORM", "R16G16B16A16_SINT", "R32G32_TYPELESS", "R32G32_FLOAT", "R32G32_UINT", "R32G32_SINT", "R10G10B10A2_TYPELESS", "R10G10B10A2_UNORM", "R10G10B10A2_UINT", "R11G11B10_FLOAT", "R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "R8G8B8A8_UNORM_SRGB", "R8G8B8A8_UINT", "R8G8B8A8_SNORM", "R8G8B8A8_SINT", "R16G16_TYPELESS", "R16G16_FLOAT", "R16G16_UNORM", "R16G16_UINT", "R16G16_SNORM", "R16G16_SINT", "R32_TYPELESS", "R32_FLOAT", "R32_UINT", "R32_SINT", "R8G8_TYPELESS", "R8G8_UNORM", "R8G8_UINT", "R8G8_SNORM", "R8G8_SINT", "R16_TYPELESS", "R16_FLOAT", "R16_UNORM", "R16_UINT", "R16_SNORM", "R16_SINT", "R8_TYPELESS", "R8_UNORM", "R8_UINT", "R8_SNORM", "R8_SINT", "R9G9B9E5_SHAREDEXP", "D32_FLOAT_S8X24_UINT", "D32_FLOAT", "D24_UNORM_S8_UINT", "D24_UNORM_X8_UINT", "D16_UNORM", "S8_UINT", "BC1_TYPELESS", "BC1_UNORM", "BC1_UNORM_SRGB", "BC2_TYPELESS", "BC2_UNORM", "BC2_UNORM_SRGB", "BC3_TYPELESS", "BC3_UNORM", "BC3_UNORM_SRGB", "BC4_TYPELESS", "BC4_UNORM", "BC4_SNORM", "BC5_TYPELESS", "BC5_UNORM", "BC5_SNORM", "B5G6R5_UNORM", "B5G5R5A1_UNORM", "B8G8R8A8_UNORM", "B8G8R8X8_UNORM", "R10G10B10_XR_BIAS_A2_UNORM", "B8G8R8A8_TYPELESS", "B8G8R8A8_UNORM_SRGB", "B8G8R8X8_TYPELESS", "B8G8R8X8_UNORM_SRGB", "BC6H_TYPELESS", "BC6H_UF16", "BC6H_SF16", "BC7_TYPELESS", "BC7_UNORM", "BC7_UNORM_SRGB", "FORCE_UINT", "Count"
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
        Float, Float2, Float3, Float4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Count
    };

    enum Mask {
        Float_mask = 1 << 0, Float2_mask = 1 << 1, Float3_mask = 1 << 2, Float4_mask = 1 << 3, Byte_mask = 1 << 4, Byte4N_mask = 1 << 5, UByte_mask = 1 << 6, UByte4N_mask = 1 << 7, Short2_mask = 1 << 8, Short2N_mask = 1 << 9, Short4_mask = 1 << 10, Short4N_mask = 1 << 11, Count_mask = 1 << 12
    };

    static const char* s_value_names[] = {
        "Float", "Float2", "Float3", "Float4", "Byte", "Byte4N", "UByte", "UByte4N", "Short2", "Short2N", "Short4", "Short4N", "Count"
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
        BindPipeline, BindResourceTable, BindVertexBuffer, BindIndexBuffer, BindResourceSet, Draw, DrawIndexed, DrawInstanced, DrawIndexedInstanced, Dispatch, CopyResource, Count
    };

    static const char* s_value_names[] = {
        "BindPipeline", "BindResourceTable", "BindVertexBuffer", "BindIndexBuffer", "BindResourceSet", "Draw", "DrawIndexed", "DrawInstanced", "DrawIndexedInstanced", "Dispatch", "CopyResource", "Count"
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
    DeviceExtensions_DebugCallback          = 1 << 0,
};

// TODO: Error enum?
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// Resource creation structs
/////////////////////////////////////////////////////////////////////////////////

//
//
struct DeviceCreation {

    void*                           window              = nullptr; // Pointer to API-specific window: SDL_Window, GLFWWindow

    uint64_t                        extensions_mask     = 0;

    // Vulkan specific parameters
    // TODO: add them as flags ?
    const char**                    extensions          = nullptr;
    uint32_t                        extensions_count    = 0;

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

    uint16_t                        width               = 1;
    uint16_t                        height              = 1;
    uint16_t                        depth               = 1;
    uint8_t                         mipmaps             = 1;
    uint8_t                         render_target       = 0;

    TextureFormat::Enum             format              = TextureFormat::UNKNOWN;
    TextureType::Enum               type                = TextureType::Texture2D;

}; // struct TextureCreation

struct SamplerCreation {
    
    TextureFilter::Enum             min_filter          = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter          = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter          = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w      = TextureAddressMode::Repeat;

}; // struct SamplerCreation

//
//
struct ShaderCreation {

    struct Stage {

        ShaderStage::Enum           type                = ShaderStage::Compute;
        const char*                 code                = nullptr;

    }; // struct Stage

    const Stage*                    stages              = nullptr;
    const char*                     name                = nullptr;

    uint32_t                        stages_count        = 0;

}; // struct ShaderCreation

//
// 
struct ResourceSetLayoutCreation {
    
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

}; // struct ResourceSetLayoutCreation

//
//
struct ResourceSetCreation {

    ResourceSetLayoutHandle            layout;

    // List of resources
    struct Resource {

        ResourceHandle              handle;

    }; // struct Resource

    const Resource*                 resources           = nullptr;
    uint32_t                        num_resources       = 0;

}; // struct ResourceSetCreation

//
//
struct PipelineCreation {

    ShaderHandle                    shader_state;
    ResourceSetLayoutHandle         resource_layout;

}; // struct PipelineCreation

/////////////////////////////////////////////////////////////////////////////////
// API-agnostic resources
/////////////////////////////////////////////////////////////////////////////////

//
//
struct ShaderState {

    const char*                     name                = nullptr;

}; // struct ShaderState

//
//
struct Buffer {

    BufferType::Enum                type                = BufferType::Vertex;
    ResourceUsageType::Enum         usage               = ResourceUsageType::Immutable;
    uint32_t                        size                = 0;
    const char*                     name                = nullptr;

}; // struct Buffer

//
//
struct Texture {
    
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
struct Sampler {

    TextureFilter::Enum             min_filter          = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter          = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter          = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v      = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w      = TextureAddressMode::Repeat;

}; // struct Sampler

//
// Resource management
//
struct ResourceBinding {
    uint16_t                        type                = 0;    // ResourceType
    uint16_t                        start               = 0;
    uint16_t                        count               = 0;
    uint16_t                        set                 = 0;

    const char*                     name                = nullptr;
}; // struct ResourceBinding

//
//
struct ResourceSetLayout {

    //Binding*                        bindings            = nullptr;
    //uint32_t                        num_bindings        = 0;

}; // struct ResourceSetLayout

//
//
struct ResourceSet {

    struct Resource {

        void*                       data                = nullptr;

    }; // struct Resource

    Resource*                       resources           = nullptr;
    uint32_t                        num_resources       = 0;

}; // struct ResourceSet

//
//
struct Pipeline {

    ShaderHandle                    shader_state;

}; // struct Pipeline

/////////////////////////////////////////////////////////////////////////////////
// API-agnostic resource modifications:
/////////////////////////////////////////////////////////////////////////////////

struct MapBufferParameters {
    BufferHandle                    buffer;
    uint32_t                        offset              = 0;
    uint32_t                        size                = 0;

}; // struct MapBufferParameters

/////////////////////////////////////////////////////////////////////////////////
// API-gnostic resources:
/////////////////////////////////////////////////////////////////////////////////
#if defined (HYDRA_OPENGL)

struct ShaderStateGL;
struct TextureGL;
struct BufferGL;
struct PipelineGL;
struct SamplerGL;
struct ResourceSetLayoutGL;
struct ResourceSetGL;

#elif defined (HYDRA_VULKAN)
#endif // HYDRA_OPENGL


/////////////////////////////////////////////////////////////////////////////////
// Main structs
/////////////////////////////////////////////////////////////////////////////////

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


//
// Command buffer interface
//

//
// Commands list
//
namespace commands {

    struct Command {
        uint16_t                        type = 0;
        uint16_t                        size = 0;

    }; // struct Commands

    struct BindPipeline : public Command {

        PipelineHandle                  handle;

        static uint16_t                 Type() { return CommandType::BindPipeline; }

    }; // struct BindPipeline

    struct BindResourceSet : public Command {

        ResourceSetHandle               handle;

        static uint16_t                 Type() {
            return CommandType::BindResourceSet;
        }

    }; // struct BindResourceSet

    struct BindVertexBuffer : public Command {

        BufferHandle                    buffer;

        static uint16_t                 Type() {
            return CommandType::BindVertexBuffer;
        }

    }; // struct BindVertexBuffer

    struct BindIndexBuffer : public Command {

        BufferHandle                    buffer;

        static uint16_t                 Type() {
            return CommandType::BindIndexBuffer;
        }

    }; // struct BindIndexBuffer


    struct Draw : public Command {

        TopologyType::Enum              topology;
        uint32_t                        first_vertex;
        uint32_t                        vertex_count;

        static uint16_t                 Type() {
            return CommandType::Draw;
        }

    }; // struct Draw

    struct DrawIndexed : public Command {

        TopologyType::Enum              topology;
        uint32_t                        first_vertex;
        uint32_t                        vertex_count;

        static uint16_t                 Type() {
            return CommandType::DrawIndexed;
        }

    }; // struct DrawIndexed

    struct DrawInstanced : public Command {

        TopologyType::Enum              topology;
        uint32_t                        first_vertex;
        uint32_t                        vertex_count;

        static uint16_t                 Type() {
            return CommandType::DrawInstanced;
        }

    }; // struct DrawInstanced

    struct DrawIndexedInstanced : public Command {

        TopologyType::Enum              topology;
        uint32_t                        first_vertex;
        uint32_t                        vertex_count;

        static uint16_t                 Type() {
            return CommandType::DrawIndexedInstanced;
        }

    }; // struct DrawIndexedInstanced

    struct Dispatch : public Command {

        uint8_t                         group_x;
        uint8_t                         group_y;
        uint8_t                         group_z;

        static uint16_t                 Type() {
            return CommandType::Dispatch;
        }

    }; // struct Dispatch

    struct CopyResource : public Command {

        static uint16_t                 Type() {
            return CommandType::CopyResource;
        }

    }; // struct CopyResource

} // namespace Commands


struct CommandBuffer {

    void                            init( QueueType::Enum type, uint32_t size );

    //
    // Commands interface
    //
    void                            bind_pipeline( PipelineHandle handle );
    void                            bind_vertex_buffer( BufferHandle handle );
    void                            bind_resource_set( ResourceSetHandle handle );

    void                            draw( TopologyType::Enum topology, uint32_t start, uint32_t count );
    void                            dispatch( uint8_t group_x, uint8_t group_y, uint8_t group_z );

    //
    // Internal interface
    //
    template <typename T>
    T*                              write_command();

    // Get command and proceed the reading
    template <typename T>
    const T&                        read_command();

    // Retrieve current command type
    CommandType::Enum               get_command_type() const {
        return (CommandType::Enum)(*(uint16_t*)(data + read_offset));
    }


    bool                            has_commands()      { return write_offset > 0; }
    void                            rewind()            { read_offset = 0; }
    bool                            end_of_stream()     { return read_offset >= write_offset; }
    void                            reset()             { write_offset = 0; read_offset = 0; }


    QueueType::Enum                 type                = QueueType::Graphics;

    uint8_t*                        data                = nullptr;
    uint32_t                        read_offset         = 0;
    uint32_t                        write_offset        = 0;
    uint32_t                        buffer_size         = 0;

}; // struct CommandBuffer


template <typename T>
T* CommandBuffer::write_command() {
    T* command = (T*)(data + write_offset);
    command->type = T::Type();
    command->size = sizeof( T );

    write_offset += sizeof( T );
    //HYDRA_ASSERT( write_offset < buffer_size, "" );
    return command;
}

template <typename T>
const T& CommandBuffer::read_command() {
    // Get current command
    T& command = *(T*)(data + read_offset);
    // Move offset to next command.
    read_offset += command.size;
    return command;
}

struct Device {

    // Init/Terminate methods
    void                            init( const DeviceCreation& creation );
    void                            terminate();

    // Creation/Destruction
    BufferHandle                    create_buffer( const BufferCreation& creation );
    TextureHandle                   create_texture( const TextureCreation& creation );
    ShaderHandle                    create_shader( const ShaderCreation& creation );
    PipelineHandle                  create_pipeline( const PipelineCreation& creation );
    SamplerHandle                   create_sampler( const SamplerCreation& creation );
    ResourceSetLayoutHandle         create_resource_set_layout( const ResourceSetLayoutCreation& creation );
    ResourceSetHandle               create_resource_set( const ResourceSetCreation& creation );

    void                            destroy_buffer( BufferHandle buffer );
    void                            destroy_texture( TextureHandle texture );
    void                            destroy_shader( ShaderHandle shader );
    void                            destroy_pipeline( PipelineHandle pipeline );
    void                            destroy_sampler( SamplerHandle sampler );
    void                            destroy_resource_layout( ResourceSetLayoutHandle resource_layout );
    void                            destroy_resource_set( ResourceSetHandle resource_set );

    const Buffer*                   query_buffer( BufferHandle buffer );
    const Texture*                  query_texture( TextureHandle texture );
    const ShaderState*              query_shader( ShaderHandle shader );
    const Pipeline*                 query_pipeline( PipelineHandle pipeline );
    const Sampler*                  query_sampler( SamplerHandle sampler );
    const ResourceSetLayout*        query_resource_set_layout( ResourceSetLayoutHandle resource_layout );
    const ResourceSet*              query_resource_set( ResourceSetHandle resource_set );

    void*                           map_buffer( const MapBufferParameters& parameters );
    void                            unmap_buffer( const MapBufferParameters& parameters );


    // Command buffers
    CommandBuffer*                  get_command_buffer( QueueType::Enum type, uint32_t size );
    void                            execute_command_buffer( CommandBuffer* command_buffer );

    // Rendering
    void                            present();

    BufferHandle                    get_fullscreen_vertex_buffer() const;

    // Internals
    
    void                            backend_init( const DeviceCreation& creation );
    void                            backend_terminate();

    ResourcePool                    buffers;
    ResourcePool                    shaders;
    ResourcePool                    textures;
    ResourcePool                    pipelines;
    ResourcePool                    samplers;
    ResourcePool                    resource_layouts;
    ResourcePool                    resource_sets;

    // Primitive resources
    BufferHandle                    fullscreen_vertex_buffer;
    
#if defined (HYDRA_OPENGL)

    ShaderStateGL*                  access_shader( ShaderHandle shader );
    const ShaderStateGL*            access_shader( ShaderHandle shader ) const;

    TextureGL*                      access_texture( TextureHandle texture );
    const TextureGL*                access_texture( TextureHandle texture ) const;

    BufferGL*                       access_buffer( BufferHandle buffer );
    const BufferGL*                 access_buffer( BufferHandle buffer ) const;

    PipelineGL*                     access_pipeline( PipelineHandle pipeline );
    const PipelineGL*               access_pipeline( PipelineHandle pipeline ) const;

    SamplerGL*                      access_sampler( SamplerHandle sampler );
    const SamplerGL*                access_sampler( SamplerHandle sampler ) const;

    ResourceSetLayoutGL*            access_resource_set_layout( ResourceSetLayoutHandle resource_layout );
    const ResourceSetLayoutGL*      access_resource_set_layout( ResourceSetLayoutHandle resource_layout ) const;

    ResourceSetGL*                  access_resource_set( ResourceSetHandle resource_set );
    const ResourceSetGL*            access_resource_set( ResourceSetHandle resource_set ) const;


#elif defined (HYDRA_VULKAN)
    

    VkAllocationCallbacks*          v_allocation_callbacks;
    VkInstance                      v_instance;
    VkPhysicalDevice                v_physical_device;
    VkDevice                        v_device;
    VkQueue                         v_queue;
    uint32_t                        v_queue_family;
    VkDescriptorPool                v_descriptor_pool;
    VkSurfaceKHR                    v_window_surface;
    VkSurfaceFormatKHR              v_surface_format;
    VkPresentModeKHR                v_present_mode;

    VkDebugReportCallbackEXT        v_debug_callback;
#elif defined (HYDRA_OPENGL)
#endif // HYDRA_VULKAN

}; // struct Device


} // namespace gfx_device
} // namespace hydra
