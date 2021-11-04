#pragma once


namespace hydra {
namespace gfx {

// Main structs /////////////////////////////////////////////////////////////////

//
//
struct BufferVulkan {

    VkBuffer                        vk_buffer;
    VmaAllocation                   vma_allocation;
    VkDeviceMemory                  vk_device_memory;
    VkDeviceSize                    vk_device_size;

    BufferType::Mask                type            = BufferType::Vertex_mask;
    ResourceUsageType::Enum         usage           = ResourceUsageType::Immutable;
    u32                             size            = 0;
    u32                             global_offset   = 0;    // Offset into global constant, if dynamic

    BufferHandle                    handle;
    BufferHandle                    parent_buffer;

    const char* name                = nullptr;

}; // struct BufferVulkan


//
//
struct SamplerVulkan {

    VkSampler                       vk_sampler;

    TextureFilter::Enum             min_filter = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter = TextureFilter::Nearest;
    TextureMipFilter::Enum          mip_filter = TextureMipFilter::Nearest;

    TextureAddressMode::Enum        address_mode_u = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_v = TextureAddressMode::Repeat;
    TextureAddressMode::Enum        address_mode_w = TextureAddressMode::Repeat;

    const char* name = nullptr;

}; // struct SamplerVulkan

//
//
struct TextureVulkan {

    VkImage                         vk_image;
    VkImageView                     vk_image_view;
    VkFormat                        vk_format;
    VkImageLayout                   vk_image_layout;
    VmaAllocation                   vma_allocation;

    u16                             width = 1;
    u16                             height = 1;
    u16                             depth = 1;
    u8                              mipmaps = 1;
    u8                              render_target = 0;
    u8                              flags = 0;

    TextureHandle                   handle;

    TextureFormat::Enum             format  = TextureFormat::UNKNOWN;
    TextureType::Enum               type    = TextureType::Texture2D;

    SamplerVulkan*                  sampler = nullptr;

    const char*                     name    = nullptr;
}; // struct TextureVulkan

//
//
struct ShaderStateVulkan {

    VkPipelineShaderStageCreateInfo shader_stage_info[ k_max_shader_stages ];

    const char*                     name = nullptr;

    u32                             active_shaders = 0;
    bool                            graphics_pipeline = false;
};

//
//
struct PipelineVulkan {

    VkPipeline                      vk_pipeline;
    VkPipelineLayout                vk_pipeline_layout;

    VkPipelineBindPoint             vk_bind_point;

    ShaderStateHandle               shader_state;

    const ResourceLayoutVulkan*     resource_layout[ k_max_resource_layouts ];
    ResourceLayoutHandle            resource_layout_handle[ k_max_resource_layouts ];
    u32                             num_active_layouts = 0;

    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    RasterizationCreation           rasterization;

    PipelineHandle                  handle;
    bool                            graphics_pipeline = true;

}; // struct PipelineVulkan


//
//
struct RenderPassVulkan {

    VkRenderPass                    vk_render_pass;
    VkFramebuffer                   vk_frame_buffer;

    RenderPassOutput                output;

    TextureHandle                   output_textures[ k_max_image_outputs ];
    TextureHandle                   output_depth;

    RenderPassType::Enum            type;

    f32                             scale_x     = 1.f;
    f32                             scale_y     = 1.f;
    u16                             width       = 0;
    u16                             height      = 0;
    u16                             dispatch_x  = 0;
    u16                             dispatch_y  = 0;
    u16                             dispatch_z  = 0;

    u8                              resize      = 0;
    u8                              num_render_targets = 0;

    const char*                     name        = nullptr;
}; // struct RenderPassVulkan

//
//
struct ResourceBindingVulkan {

    u16                             type    = 0;    // ResourceType
    u16                             start   = 0;
    u16                             count   = 0;
    u16                             set     = 0;

    const char*                     name    = nullptr;
}; // struct ResourceBindingVulkan

//
//
struct ResourceLayoutVulkan {

    VkDescriptorSetLayout           vk_descriptor_set_layout;

    VkDescriptorSetLayoutBinding*   vk_binding      = nullptr;
    ResourceBindingVulkan*          bindings        = nullptr;
    u16                             num_bindings    = 0;
    u16                             max_binding     = 0;

    ResourceLayoutHandle            handle;

}; // struct ResourceLayoutVulkan

//
//
struct ResourceListVulkan {

    VkDescriptorSet                 vk_descriptor_set;

    ResourceHandle*                 resources       = nullptr;
    SamplerHandle*                  samplers        = nullptr;
    u16*                            bindings        = nullptr;

    const ResourceLayoutVulkan*     layout          = nullptr;
    u32                             num_resources   = 0;
}; // struct ResourceListVulkan



// Enum translations. Use tables or switches depending on the case. ////////////

static VkFormat to_vk_format( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::R32G32B32A32_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::R32G32B32A32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case TextureFormat::R32G32B32A32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case TextureFormat::R32G32B32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::R32G32B32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case TextureFormat::R32G32B32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case TextureFormat::R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case TextureFormat::R16G16B16A16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case TextureFormat::R16G16B16A16_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case TextureFormat::R16G16B16A16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case TextureFormat::R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::R32G32_UINT:
            return VK_FORMAT_R32G32_UINT;
        case TextureFormat::R32G32_SINT:
            return VK_FORMAT_R32G32_SINT;
        //case TextureFormat::R10G10B10A2_TYPELESS:
        //    return GL_RGB10_A2;
        case TextureFormat::R10G10B10A2_UNORM:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R10G10B10A2_UINT:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R11G11B10_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        //case TextureFormat::R8G8B8A8_TYPELESS:
        //    return GL_RGBA8;
        case TextureFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        //case TextureFormat::R8G8B8A8_UNORM_SRGB:
        //    return GL_SRGB8_ALPHA8;
        case TextureFormat::R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::R8G8B8A8_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case TextureFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::B8G8R8X8_UNORM:
            return VK_FORMAT_B8G8R8_UNORM;
        //case TextureFormat::R16G16_TYPELESS:
        //    return GL_RG16UI;
        case TextureFormat::R16G16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::R16G16_UNORM:
            return VK_FORMAT_R16G16_UNORM;
        case TextureFormat::R16G16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case TextureFormat::R16G16_SNORM:
            return VK_FORMAT_R16G16_SNORM;
        case TextureFormat::R16G16_SINT:
            return VK_FORMAT_R16G16_SINT;
        //case TextureFormat::R32_TYPELESS:
        //    return GL_R32UI;
        case TextureFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case TextureFormat::R32_SINT:
            return VK_FORMAT_R32_SINT;
        //case TextureFormat::R8G8_TYPELESS:
        //    return GL_RG8UI;
        case TextureFormat::R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::R8G8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case TextureFormat::R8G8_SNORM:
            return VK_FORMAT_R8G8_SNORM;
        case TextureFormat::R8G8_SINT:
            return VK_FORMAT_R8G8_SINT;
        //case TextureFormat::R16_TYPELESS:
        //    return GL_R16UI;
        case TextureFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::R16_UNORM:
            return VK_FORMAT_R16_UNORM;
        case TextureFormat::R16_UINT:
            return VK_FORMAT_R16_UINT;
        case TextureFormat::R16_SNORM:
            return VK_FORMAT_R16_SNORM;
        case TextureFormat::R16_SINT:
            return VK_FORMAT_R16_SINT;
        //case TextureFormat::R8_TYPELESS:
        //    return GL_R8UI;
        case TextureFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::R8_UINT:
            return VK_FORMAT_R8_UINT;
        case TextureFormat::R8_SNORM:
            return VK_FORMAT_R8_SNORM;
        case TextureFormat::R8_SINT:
            return VK_FORMAT_R8_SINT;
        //case TextureFormat::R9G9B9E5_SHAREDEXP:
        //    return GL_RGB9_E5;
        //case TextureFormat::R32G32B32A32_TYPELESS:
        //    return GL_RGBA32UI;
        //case TextureFormat::R32G32B32_TYPELESS:
        //    return GL_RGB32UI;
        //case TextureFormat::R16G16B16A16_TYPELESS:
        //    return GL_RGBA16UI;
        //case TextureFormat::R32G32_TYPELESS:
        //    return GL_RG32UI;
        //// Depth formats  
        case TextureFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::D32_FLOAT_S8X24_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TextureFormat::D24_UNORM_X8_UINT:
            return VK_FORMAT_X8_D24_UNORM_PACK32;
        case TextureFormat::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case TextureFormat::S8_UINT:
            return VK_FORMAT_S8_UINT;

        //// Compressed
        //case TextureFormat::BC1_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC1_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC1_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC2_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC3_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC4_SNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC5_SNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B5G6R5_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B5G5R5A1_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::R10G10B10_XR_BIAS_A2_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8A8_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::B8G8R8X8_UNORM_SRGB:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_UF16:
        //    return GL_RGBA32F;
        //case TextureFormat::BC6H_SF16:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_TYPELESS:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_UNORM:
        //    return GL_RGBA32F;
        //case TextureFormat::BC7_UNORM_SRGB:
        //    return GL_RGBA32F;

        case TextureFormat::UNKNOWN:
        default:
            //HYDRA_ASSERT( false, "Format %s not supported on Vulkan.", EnumNamesTextureFormat()[format] );
            break;
    }

    return VK_FORMAT_UNDEFINED;
}

//
//
static VkImageType to_vk_image_type( TextureType::Enum type ) {
    static VkImageType s_vk_target[ TextureType::Count ] = { VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D };
    return s_vk_target[ type ];
}

//
//
static VkImageViewType to_vk_image_view_type( TextureType::Enum type ) {
    static VkImageViewType s_vk_data[] = { VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY };
    return s_vk_data[ type ];
}

//
//
static VkDescriptorType to_vk_descriptor_type( ResourceType::Enum type ) {
    // Sampler, Texture, Image, ImageRW, Constants, StructuredBuffer, BufferRW, Count
    static VkDescriptorType s_vk_type[ ResourceType::Count ] = { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    return s_vk_type[ type ];
}

//
//
static VkShaderStageFlagBits to_vk_shader_stage( ShaderStage::Enum value ) {
    // Vertex, Fragment, Geometry, Compute, Hull, Domain, Count
    static VkShaderStageFlagBits s_vk_stage[ ShaderStage::Count ] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT };

    return s_vk_stage[ value ];
}

//
//
static VkFormat to_vk_vertex_format( VertexComponentFormat::Enum value ) {
    // Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Uint, Count
    static VkFormat s_vk_vertex_formats[ VertexComponentFormat::Count ] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, /*MAT4 TODO*/VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                          VK_FORMAT_R8_SINT, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SNORM,
                                                                          VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R32_UINT };

    return s_vk_vertex_formats[ value ];
}

//
//
static VkCullModeFlags to_vk_cull_mode( CullMode::Enum value ) {
    // TODO: there is also front_and_back!
    static VkCullModeFlags s_vk_cull_mode[ CullMode::Count ] = { VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT };
    return s_vk_cull_mode[ value ];
}

//
//
static VkFrontFace to_vk_front_face( FrontClockwise::Enum value ) {

    return value == FrontClockwise::True ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

//
//
static VkBlendFactor to_vk_blend_factor( Blend::Enum value ) {
    // Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor, SrcAlphasat, Src1Color, InvSrc1Color, Src1Alpha, InvSrc1Alpha, Count
    static VkBlendFactor s_vk_blend_factor[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                                                 VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
                                                 VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_SRC1_COLOR,
                                                 VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA };

    return s_vk_blend_factor[ value ];
}

//
//
static VkBlendOp to_vk_blend_operation( BlendOperation::Enum value ) {
    //Add, Subtract, RevSubtract, Min, Max, Count
    static VkBlendOp s_vk_blend_op[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
    return s_vk_blend_op[ value ];
}

//
//
static VkCompareOp to_vk_compare_operation( ComparisonFunction::Enum value ) {
    static VkCompareOp s_vk_values[] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS };
    return s_vk_values[ value ];
}

//
//
static VkPipelineStageFlags to_vk_pipeline_stage( PipelineStage::Enum value ) {
    static VkPipelineStageFlags s_vk_values[] = { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
    return s_vk_values[ value ];
}

//
//
// Repeat, Mirrored_Repeat, Clamp_Edge, Clamp_Border, Count
static VkSamplerAddressMode to_vk_address_mode( TextureAddressMode::Enum value ) {
    static VkSamplerAddressMode s_vk_values[] = { VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER };
    return s_vk_values[ value ];
}

//
//
static VkFilter to_vk_filter( TextureFilter::Enum value ) {
    static VkFilter s_vk_values[] = { VK_FILTER_NEAREST, VK_FILTER_LINEAR };
    return s_vk_values[ value ];
}

//
//
static VkSamplerMipmapMode to_vk_mipmap( TextureMipFilter::Enum value ) {
    static VkSamplerMipmapMode s_vk_values[] = { VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR };
    return s_vk_values[ value ];
}

} // namespace gfx
} // namespace hydra