#pragma once

#if defined(HYDRA_VULKAN)
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "external/vk_mem_alloc.h"

#include "graphics/gpu_device.hpp"
#include "graphics/gpu_resources_vulkan.hpp"
#include "graphics/command_buffer.hpp"

#include "kernel/array.hpp"

namespace hydra {
namespace gfx {


// GpuDeviceVulkan
struct GpuDeviceVulkan : public Device {

    void                            internal_init( const DeviceCreation& creation );
    void                            internal_shutdown();

    
    // Creation/Destruction of resources ////////////////////////////////////////
    BufferHandle                    create_buffer( const BufferCreation& creation );
    TextureHandle                   create_texture( const TextureCreation& creation );
    PipelineHandle                  create_pipeline( const PipelineCreation& creation );
    SamplerHandle                   create_sampler( const SamplerCreation& creation );
    ResourceLayoutHandle            create_resource_layout( const ResourceLayoutCreation& creation );
    ResourceListHandle              create_resource_list( const ResourceListCreation& creation );
    RenderPassHandle                create_render_pass( const RenderPassCreation& creation );
    ShaderStateHandle               create_shader_state( const ShaderStateCreation& creation );

    void                            destroy_buffer( BufferHandle buffer );
    void                            destroy_texture( TextureHandle texture );
    void                            destroy_pipeline( PipelineHandle pipeline );
    void                            destroy_sampler( SamplerHandle sampler );
    void                            destroy_resource_layout( ResourceLayoutHandle resource_layout );
    void                            destroy_resource_list( ResourceListHandle resource_list );
    void                            destroy_render_pass( RenderPassHandle render_pass );
    void                            destroy_shader_state( ShaderStateHandle shader );

    // Query Description //////////////////////////////////////////////////
    void                            query_buffer( BufferHandle buffer, BufferDescription& out_description );
    void                            query_texture( TextureHandle texture, TextureDescription& out_description );
    void                            query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description );
    void                            query_sampler( SamplerHandle sampler, SamplerDescription& out_description );
    void                            query_resource_layout( ResourceLayoutHandle resource_layout, ResourceLayoutDescription& out_description );
    void                            query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description );
    void                            query_shader_state( ShaderStateHandle shader, ShaderStateDescription& out_description );

    // Map/unmap //////////////////////////////////////////////////////////
    void*                           map_buffer( const MapBufferParameters& parameters );
    void                            unmap_buffer( const MapBufferParameters& parameters );
    void                            set_buffer_global_offset( BufferHandle buffer, u32 offset );

    // Resource list //////////////////////////////////////////////////////
    void                            update_resource_list( ResourceListHandle resource_list );
    void                            update_resource_list_instant( const ResourceListUpdate& update );

    // Swapchain //////////////////////////////////////////////////////////
    void                            create_swapchain();
    void                            destroy_swapchain();
    void                            resize_swapchain();

    // Command buffers ////////////////////////////////////////////////////
    CommandBuffer*                  get_command_buffer( QueueType::Enum type, bool begin );
    CommandBuffer*                  get_instant_command_buffer();

    void                            queue_command_buffer( CommandBuffer* command_buffer );

    // GPU Timestamps

    u32                             get_gpu_timestamps( GPUTimestamp* out_timestamps );
    void                            push_gpu_timestamp( CommandBuffer* command_buffer, const char* name );
    void                            pop_gpu_timestamp( CommandBuffer* command_buffer );

    // Instant methods 
    void                            destroy_buffer_instant( ResourceHandle buffer );
    void                            destroy_texture_instant( ResourceHandle texture );
    void                            destroy_pipeline_instant( ResourceHandle pipeline );
    void                            destroy_sampler_instant( ResourceHandle sampler );
    void                            destroy_resource_layout_instant( ResourceHandle resource_layout );
    void                            destroy_resource_list_instant( ResourceHandle resource_list );
    void                            destroy_render_pass_instant( ResourceHandle render_pass );
    void                            destroy_shader_state_instant( ResourceHandle shader );

    // Names and markers
    void                            set_resource_name( VkObjectType object_type, uint64_t handle, const char* name );
    void                            push_marker( VkCommandBuffer command_buffer, cstring name );
    void                            pop_marker( VkCommandBuffer command_buffer );

    VkRenderPass                    get_vulkan_render_pass( const RenderPassOutput& output, cstring name );

    //
    void                            new_frame();
    void                            present();
    void                            set_present_mode( PresentMode::Enum mode );

    void                            fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier );
    void                            resize_output_textures( RenderPassHandle render_pass, u32 width, u32 height );
    void                            link_texture_sampler( TextureHandle texture, SamplerHandle sampler );

    void                            frame_counters_advance();

    VkAllocationCallbacks*          vulkan_allocation_callbacks;
    VkInstance                      vulkan_instance;
    VkPhysicalDevice                vulkan_physical_device;
    VkPhysicalDeviceProperties      vulkan_physical_properties;
    VkDevice                        vulkan_device;
    VkQueue                         vulkan_queue;
    uint32_t                        vulkan_queue_family;
    VkDescriptorPool                vulkan_descriptor_pool;
    VkDescriptorPool                vulkan_descriptor_pool_bindless;
    VkDescriptorSetLayout           vulkan_bindless_descriptor_layout;      // Global bindless descriptor layout.
    VkDescriptorSet                 vulkan_bindless_descriptor_set;         // Global bindless descriptor set.

    // Swapchain
    VkImage                         vulkan_swapchain_images[ k_max_swapchain_images ];
    VkImageView                     vulkan_swapchain_image_views[ k_max_swapchain_images ];
    VkFramebuffer                   vulkan_swapchain_framebuffers[ k_max_swapchain_images ];
    
    VkQueryPool                     vulkan_timestamp_query_pool;
    // Per frame synchronization
    VkSemaphore                     vulkan_render_complete_semaphore[ k_max_swapchain_images ];
    VkSemaphore                     vulkan_image_acquired_semaphore;
    VkFence                         vulkan_command_buffer_executed_fence[ k_max_swapchain_images ];

    TextureHandle                   depth_texture;

    static const uint32_t           k_max_frames                    = 3;

    // Windows specific
    VkSurfaceKHR                    vulkan_window_surface;
    VkSurfaceFormatKHR              vulkan_surface_format;
    VkPresentModeKHR                vulkan_present_mode;
    VkSwapchainKHR                  vulkan_swapchain;
    u32                             vulkan_swapchain_image_count;

    VkDebugReportCallbackEXT        vulkan_debug_callback;
    VkDebugUtilsMessengerEXT        vulkan_debug_utils_messenger;

    u32                             vulkan_image_index;
    
    VmaAllocator                    vma_allocator;

    // These are dynamic - so that workload can be handled correctly.
    Array<ResourceUpdate>           resource_deletion_queue;
    Array<ResourceListUpdate>       resource_list_update_queue;
    Array<ResourceUpdate>           texture_to_update_bindless;

    f32                             gpu_timestamp_frequency;
    bool                            gpu_timestamp_reset             = true;
    bool                            debug_utils_extension_present   = false;

    char                            vulkan_binaries_path[ 512 ];
}; // struct GpuDeviceVulkan

} // namespace gfx
} // namespace hydra

#endif // HYDRA_VULKAN