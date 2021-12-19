#pragma once

#include "graphics/gpu_device.hpp"

#if defined(HYDRA_VULKAN)
#include <vulkan/vulkan.h>
#endif // HYDRA_VULKAN

namespace hydra {
namespace gfx {

struct Device;
struct GpuDeviceVulkan;
    
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
    void                            clear_depth_stencil( u64 sort_key, f32 depth, u8 stencil );

    void                            draw( u64 sort_key, TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count );
    void                            draw_indexed( u64 sort_key, TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance );
    void                            draw_indirect( u64 sort_key, BufferHandle handle, u32 offset, u32 stride );
    void                            draw_indexed_indirect( u64 sort_key, BufferHandle handle, u32 offset, u32 stride );

    void                            dispatch( u64 sort_key, u32 group_x, u32 group_y, u32 group_z );
    void                            dispatch_indirect( u64 sort_key, BufferHandle handle, u32 offset );

    void                            barrier( const ExecutionBarrier& barrier );

    void                            fill_buffer( BufferHandle buffer, u32 offset, u32 size, u32 data );

    void                            push_marker( const char* name );
    void                            pop_marker();

    void                            reset();

#if defined (HYDRA_VULKAN)
    VkCommandBuffer                 vk_command_buffer;

    GpuDeviceVulkan*                device;

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
    uint32_t                        allocated_offset        = 0;
#endif // HYDRA_VULKAN

    // NEW INTERFACE
    u32                             current_command;
    ResourceHandle                  resource_handle;
    QueueType::Enum                 type                = QueueType::Graphics;
    u32                             buffer_size         = 0;

    bool                            baked               = false;        // If baked reset will affect only the read of the commands.

}; // struct CommandBuffer


} // namespace gfx
} // namespace hydra
