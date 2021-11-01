#include "graphics/gpu_device_vulkan.hpp"
#include "graphics/command_buffer.hpp"

#include "kernel/file.hpp"
#include "kernel/process.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/log.hpp"

#if defined(HYDRA_VULKAN)

template<class T>
constexpr const T& hydra_min( const T& a, const T& b ) {
    return ( a < b ) ? a : b;
}

template<class T>
constexpr const T& hydra_max( const T& a, const T& b ) {
    return ( a < b ) ? b : a;
}

#define VMA_MAX hydra_max
#define VMA_MIN hydra_min
#define VMA_USE_STL_CONTAINERS 0
#define VMA_USE_STL_VECTOR 0
#define VMA_USE_STL_UNORDERED_MAP 0
#define VMA_USE_STL_LIST 0

#if defined (_MSC_VER)
#pragma warning (disable: 4127)
#pragma warning (disable: 4189)
#pragma warning (disable: 4191)
#pragma warning (disable: 4296)
#pragma warning (disable: 4324)
#pragma warning (disable: 4355)
#pragma warning (disable: 4365)
#pragma warning (disable: 4625)
#pragma warning (disable: 4626)
#pragma warning (disable: 4668)
#pragma warning (disable: 5026)
#pragma warning (disable: 5027)
#endif // _MSC_VER

#define VMA_IMPLEMENTATION
#include "external/vk_mem_alloc.h"


#if defined (HYDRA_GFX_SDL)
#include <SDL.h>
#include <SDL_vulkan.h>
#endif // HYDRA_GFX_SDL


namespace hydra {
namespace gfx {



static void                 check( VkResult result );


struct CommandBufferRing {

    void                    init( GpuDeviceVulkan* gpu );
    void                    shutdown();

    void                    reset_pools( u32 frame_index );

    CommandBuffer*          get_command_buffer( u32 frame, bool begin );
    CommandBuffer*          get_command_buffer_instant( u32 frame, bool begin );

    static u16              pool_from_index( u32 index ) { return (u16)index / k_buffer_per_pool; }

    static const u16        k_max_threads = 1;
    static const u16        k_max_pools = k_max_swapchain_images * k_max_threads;
    static const u16        k_buffer_per_pool = 4;
    static const u16        k_max_buffers = k_buffer_per_pool * k_max_pools;

    GpuDeviceVulkan*        gpu;
    VkCommandPool           vulkan_command_pools[ k_max_pools ];
    CommandBuffer           command_buffers[ k_max_buffers ];
    u8                      next_free_per_thread_frame[ k_max_pools ];

}; // struct CommandBufferRing

void CommandBufferRing::init( GpuDeviceVulkan* gpu_ ) {

    gpu = gpu_;
    
    for ( u32 i = 0; i < k_max_pools; i++ ) {
        VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
        cmd_pool_info.queueFamilyIndex = gpu->vulkan_queue_family;
        cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        check( vkCreateCommandPool( gpu->vulkan_device, &cmd_pool_info, gpu->vulkan_allocation_callbacks, &vulkan_command_pools[ i ] ) );
    }

    for ( u32 i = 0; i < k_max_buffers; i++ ) {
        VkCommandBufferAllocateInfo cmd = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        const u32 pool_index = pool_from_index( i );
        cmd.commandPool = vulkan_command_pools[ pool_index ];
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        check( vkAllocateCommandBuffers( gpu->vulkan_device, &cmd, &command_buffers[ i ].vk_command_buffer ) );

        command_buffers[ i ].device = gpu;
        command_buffers[ i ].handle = i;
        command_buffers[ i ].reset();
    }
}

void CommandBufferRing::shutdown() {
    for ( u32 i = 0; i < k_max_swapchain_images * k_max_threads; i++ ) {
        vkDestroyCommandPool( gpu->vulkan_device, vulkan_command_pools[ i ], gpu->vulkan_allocation_callbacks );
    }
}

void CommandBufferRing::reset_pools( u32 frame_index ) {

    for ( u32 i = 0; i < k_max_threads; i++ ) {
        vkResetCommandPool( gpu->vulkan_device, vulkan_command_pools[ frame_index * k_max_threads + i ], 0 );
    }
}

CommandBuffer* CommandBufferRing::get_command_buffer( u32 frame, bool begin ) {
    // TODO: take in account threads
    CommandBuffer* cb = &command_buffers[ frame * k_buffer_per_pool ];

    if ( begin ) {
        cb->reset();

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer( cb->vk_command_buffer, &beginInfo );
    }

    return cb;
}

CommandBuffer* CommandBufferRing::get_command_buffer_instant( u32 frame, bool begin ) {
    CommandBuffer* cb = &command_buffers[ frame * k_buffer_per_pool + 1 ];
    return cb;
}
    
// Device implementation //////////////////////////////////////////////////

// Methods //////////////////////////////////////////////////////////////////////

// Enable this to add debugging capabilities.
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html
#define VULKAN_DEBUG_REPORT

static const char* s_requested_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    // Platform specific extension
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_MACOS_MVK
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XLIB_KHR
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_WAYLAND_KHR
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_MIR_KHR || VK_USE_PLATFORM_DISPLAY_KHR
        VK_KHR_DISPLAY_EXTENSION_NAME,
#elif VK_USE_PLATFORM_ANDROID_KHR
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_IOS_MVK
        VK_MVK_IOS_SURFACE_EXTENSION_NAME,
#endif // VK_USE_PLATFORM_WIN32_KHR

#if defined (VULKAN_DEBUG_REPORT)
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // VULKAN_DEBUG_REPORT
};

static const char* s_requested_layers[] = {
#if defined (VULKAN_DEBUG_REPORT)
    "VK_LAYER_KHRONOS_validation",
    //"VK_LAYER_LUNARG_core_validation",
    //"VK_LAYER_LUNARG_image",
    //"VK_LAYER_LUNARG_parameter_validation",
    //"VK_LAYER_LUNARG_object_tracker"
#else
    "",
#endif // VULKAN_DEBUG_REPORT
};

#ifdef VULKAN_DEBUG_REPORT

// Old debug callback.
//static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData ) {
//    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
//    HYDRA_LOG( "[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
//    return VK_FALSE;
//}

static VkBool32 debug_utils_callback( VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT types,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                            void* user_data ) {
    hprint( " MessageID: %s %i\nMessage: %s\n\n", callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage );
    return VK_FALSE;
}

#endif // VULKAN_DEBUG_REPORT

static SDL_Window*                  sdl_window;

PFN_vkSetDebugUtilsObjectNameEXT    pfnSetDebugUtilsObjectNameEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT    pfnCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT      pfnCmdEndDebugUtilsLabelEXT;


#if defined(VULKAN_DEBUG_REPORT)


// TODO:
// GPU Timestamps ///////////////////////////////////////////////////

VkDebugUtilsMessengerCreateInfoEXT create_debug_utils_messenger_info() {
    VkDebugUtilsMessengerCreateInfoEXT creation_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    creation_info.pfnUserCallback = debug_utils_callback;
    creation_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    creation_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

    return creation_info;
}
#endif // VULKAN_DEBUG_REPORT

static GpuDeviceVulkan s_vulkan_device;
static hydra::FlatHashMap<u64, VkRenderPass> render_pass_cache;

Device* Device::instance() {
    return &s_vulkan_device;
}

void Device::backend_init( const DeviceCreation& creation ) {
    s_vulkan_device.internal_init( creation );
}

void Device::backend_shutdown() {
    s_vulkan_device.internal_shutdown();
}

// Resource Creation ////////////////////////////////////////////////////////////

BufferHandle Device::create_buffer( const BufferCreation& creation ) {
    return s_vulkan_device.create_buffer( creation );
}

TextureHandle Device::create_texture( const TextureCreation& creation ) {
    return s_vulkan_device.create_texture( creation );
}

PipelineHandle Device::create_pipeline( const PipelineCreation& creation ) {
    return s_vulkan_device.create_pipeline( creation );
}

SamplerHandle Device::create_sampler( const SamplerCreation& creation ) {
    return s_vulkan_device.create_sampler( creation );
}

ResourceLayoutHandle Device::create_resource_layout( const ResourceLayoutCreation& creation ) {
    return s_vulkan_device.create_resource_layout( creation );
}

ResourceListHandle Device::create_resource_list( const ResourceListCreation& creation ) {
    return s_vulkan_device.create_resource_list( creation );
}

RenderPassHandle Device::create_render_pass( const RenderPassCreation& creation ) {
    return s_vulkan_device.create_render_pass( creation );
}

ShaderStateHandle Device::create_shader_state( const ShaderStateCreation& creation ) {
    return s_vulkan_device.create_shader_state( creation );
}

// Resource Destruction /////////////////////////////////////////////////////////

void Device::destroy_buffer( BufferHandle buffer ) {
    s_vulkan_device.destroy_buffer( buffer );
}

void Device::destroy_texture( TextureHandle texture ) {
    s_vulkan_device.destroy_texture( texture );
}

void Device::destroy_pipeline( PipelineHandle pipeline ) {
    s_vulkan_device.destroy_pipeline( pipeline );
}

void Device::destroy_sampler( SamplerHandle sampler ) {
    s_vulkan_device.destroy_sampler( sampler );
}

void Device::destroy_resource_layout( ResourceLayoutHandle resource_layout ) {
    s_vulkan_device.destroy_resource_layout( resource_layout );
}

void Device::destroy_resource_list( ResourceListHandle resource_list ) {
    s_vulkan_device.destroy_resource_list( resource_list );
}

void Device::destroy_render_pass( RenderPassHandle render_pass ) {
    s_vulkan_device.destroy_render_pass( render_pass );
}

void Device::destroy_shader_state( ShaderStateHandle shader ) {
    s_vulkan_device.destroy_shader_state( shader );
}

// Misc ///////////////////////////////////////////////////////////////////
void Device::resize_output_textures( RenderPassHandle render_pass, u16 width, u16 height ) {
    s_vulkan_device.resize_output_textures( render_pass, width, height );
}

void Device::link_texture_sampler( TextureHandle texture, SamplerHandle sampler ) {
    s_vulkan_device.link_texture_sampler( texture, sampler );
}

void Device::fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier ) {
    s_vulkan_device.fill_barrier( render_pass, out_barrier );
}

void Device::new_frame() {
    s_vulkan_device.new_frame();
}

void Device::present() {
    s_vulkan_device.present();
}

void Device::set_presentation_mode( PresentMode::Enum mode ) {
    s_vulkan_device.set_present_mode( mode );
    s_vulkan_device.resize_swapchain();
}

void* Device::map_buffer( const MapBufferParameters& parameters ) {
    return s_vulkan_device.map_buffer( parameters );
}

void Device::unmap_buffer( const MapBufferParameters& parameters ) {
    s_vulkan_device.unmap_buffer( parameters );
}

static size_t pad_uniform_buffer_size( size_t originalSize ) {
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = 256;// _gpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if ( minUboAlignment > 0 ) {
        alignedSize = ( alignedSize + minUboAlignment - 1 ) & ~( minUboAlignment - 1 );
    }
    return alignedSize;
}

void* Device::dynamic_allocate( u32 size ) {
    void* mapped_memory = dynamic_mapped_memory + dynamic_allocated_size;
    dynamic_allocated_size += (u32)pad_uniform_buffer_size( size );
    return mapped_memory;
}

void Device::set_buffer_global_offset( BufferHandle buffer, u32 offset ) {
    s_vulkan_device.set_buffer_global_offset( buffer, offset );
}

void Device::queue_command_buffer( CommandBuffer* command_buffer ) {

    s_vulkan_device.queue_command_buffer( command_buffer );
}

CommandBuffer* Device::get_command_buffer( QueueType::Enum type, bool begin ) {
    return s_vulkan_device.get_command_buffer( type, begin );
}

CommandBuffer* Device::get_instant_command_buffer() {
    return s_vulkan_device.get_instant_command_buffer();
}

void Device::update_resource_list( const ResourceListUpdate& update ) {
    s_vulkan_device.update_resource_list( update );
}

u32 Device::get_gpu_timestamps( GPUTimestamp* out_timestamps ) {
    return s_vulkan_device.get_gpu_timestamps( out_timestamps );
}

void Device::push_gpu_timestamp( CommandBuffer* command_buffer, const char* name ) {
    s_vulkan_device.push_gpu_timestamp( command_buffer, name );
}

void Device::pop_gpu_timestamp( CommandBuffer* command_buffer ) {
    s_vulkan_device.pop_gpu_timestamp( command_buffer );
}

// GpuDeviceVulkan ////////////////////////////////////////////////////////

static CommandBufferRing command_buffer_ring;

//#define HYDRA_BINDLESS

// gsBindless
static const u32        k_bindless_texture_binding = 10;
static const u32        k_max_bindless_resources = 32;

void GpuDeviceVulkan::internal_init( const DeviceCreation& creation ) {

    //////// Init Vulkan instance.
    VkResult result;
    vulkan_allocation_callbacks = nullptr;

    VkApplicationInfo application_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Hydra Graphics Device", 1, "Hydra", 1, VK_MAKE_VERSION( 1, 2, 0 ) };

    VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &application_info,
#if defined(VULKAN_DEBUG_REPORT)
                                         ArraySize( s_requested_layers ), s_requested_layers,
#else
                                         0, nullptr,
#endif
                                         ArraySize( s_requested_extensions ), s_requested_extensions };
#ifdef VULKAN_DEBUG_REPORT
    const VkDebugUtilsMessengerCreateInfoEXT debug_create_info = create_debug_utils_messenger_info();
    create_info.pNext = &debug_create_info;
#endif
    //// Create Vulkan Instance
    result = vkCreateInstance( &create_info, vulkan_allocation_callbacks, &vulkan_instance );
    check( result );

    swapchain_width = creation.width;
    swapchain_height = creation.height;

    //// Choose extensions
#ifdef VULKAN_DEBUG_REPORT
    {
        uint32_t num_instance_extensions;
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, nullptr );
        VkExtensionProperties* extensions = ( VkExtensionProperties* )halloca( sizeof( VkExtensionProperties ) * num_instance_extensions, allocator );
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, extensions );
        for ( size_t i = 0; i < num_instance_extensions; i++ ) {

            if ( !strcmp( extensions[ i ].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) ) {
                debug_utils_extension_present = true;
                break;
            }
        }

        hfree( extensions, allocator );

        if ( !debug_utils_extension_present ) {
            hprint( "Extension %s for debugging non present.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        } else {
            //// Create debug callback
            //auto vkCreateDebugReportCallbackEXT = ( PFN_vkCreateDebugReportCallbackEXT )vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugReportCallbackEXT" );
            //HYDRA_ASSERT( vkCreateDebugReportCallbackEXT != NULL, "" );

            //// Setup the debug report callback
            /*VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_callback;
            debug_report_ci.pUserData = NULL;
            result = vkCreateDebugReportCallbackEXT( vulkan_instance, &debug_report_ci, vulkan_allocation_callbacks, &vulkan_debug_callback );
            check( result );*/

            // Create new debug utils callback
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = ( PFN_vkCreateDebugUtilsMessengerEXT )vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugUtilsMessengerEXT" );
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = create_debug_utils_messenger_info();

            vkCreateDebugUtilsMessengerEXT( vulkan_instance, &debug_messenger_create_info, vulkan_allocation_callbacks, &vulkan_debug_utils_messenger );
        }
    }
#endif

    //////// Choose physical device
    uint32_t num_physical_device;
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, NULL );
    check( result );

    VkPhysicalDevice* gpus = ( VkPhysicalDevice* )halloca( sizeof( VkPhysicalDevice ) * num_physical_device, allocator );
    result = vkEnumeratePhysicalDevices( vulkan_instance, &num_physical_device, gpus );
    check( result );

    // TODO: improve - choose the first gpu.
    vulkan_physical_device = gpus[ 0 ];
    hfree( gpus, allocator );

    vkGetPhysicalDeviceProperties( vulkan_physical_device, &vulkan_physical_properties );
    gpu_timestamp_frequency = vulkan_physical_properties.limits.timestampPeriod / ( 1000 * 1000 );

    // Bindless support
#if defined (HYDRA_BINDLESS)
    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr };
    VkPhysicalDeviceFeatures2 device_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing_features };

    vkGetPhysicalDeviceFeatures2( vulkan_physical_device, &device_features );

    bindless_supported = indexing_features.descriptorBindingPartiallyBound && indexing_features.runtimeDescriptorArray;
#else
    bindless_supported = false;
#endif // HYDRA_BINDLESS

    //////// Create logical device
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( vulkan_physical_device, &queue_family_count, nullptr );

    VkQueueFamilyProperties* queue_families = ( VkQueueFamilyProperties* )halloca( sizeof( VkQueueFamilyProperties ) * queue_family_count, allocator );
    vkGetPhysicalDeviceQueueFamilyProperties( vulkan_physical_device, &queue_family_count, queue_families );

    uint32_t family_index = 0;
    for ( ; family_index < queue_family_count; ++family_index ) {
        VkQueueFamilyProperties queue_family = queue_families[ family_index ];
        if ( queue_family.queueCount > 0 && queue_family.queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) {
            //indices.graphicsFamily = i;
            break;
        }

        //VkBool32 presentSupport = false;
        //vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, i, _surface, &presentSupport );
        //if ( queue_family.queueCount && presentSupport ) {
        //    indices.presentFamily = i;
        //}

        //if ( indices.isComplete() ) {
        //    break;
        //}
    }

    hfree( queue_families, allocator );

    u32 device_extension_count = 1;
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[ 1 ] = {};
    queue_info[ 0 ].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[ 0 ].queueFamilyIndex = family_index;
    queue_info[ 0 ].queueCount = 1;
    queue_info[ 0 ].pQueuePriorities = queue_priority;

    // Enable all features: just pass the physical features 2 struct.
    VkPhysicalDeviceFeatures2 physical_features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    vkGetPhysicalDeviceFeatures2( vulkan_physical_device, &physical_features2 );

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = sizeof( queue_info ) / sizeof( queue_info[ 0 ] );
    device_create_info.pQueueCreateInfos = queue_info;
    device_create_info.enabledExtensionCount = device_extension_count;
    device_create_info.ppEnabledExtensionNames = device_extensions;
    device_create_info.pNext = &physical_features2;

#if defined(HYDRA_BINDLESS)
    if ( bindless_supported ) {
        indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
        indexing_features.runtimeDescriptorArray = VK_TRUE;

        physical_features2.pNext = &indexing_features;
    }
#endif // HYDRA_BINDLESS

    result = vkCreateDevice( vulkan_physical_device, &device_create_info, vulkan_allocation_callbacks, &vulkan_device );
    check( result );

    //  Get the function pointers to Debug Utils functions.
    if ( debug_utils_extension_present ) {
        pfnSetDebugUtilsObjectNameEXT = ( PFN_vkSetDebugUtilsObjectNameEXT )vkGetDeviceProcAddr( vulkan_device, "vkSetDebugUtilsObjectNameEXT" );
        pfnCmdBeginDebugUtilsLabelEXT = ( PFN_vkCmdBeginDebugUtilsLabelEXT )vkGetDeviceProcAddr( vulkan_device, "vkCmdBeginDebugUtilsLabelEXT" );
        pfnCmdEndDebugUtilsLabelEXT = ( PFN_vkCmdEndDebugUtilsLabelEXT )vkGetDeviceProcAddr( vulkan_device, "vkCmdEndDebugUtilsLabelEXT" );
    }

    vkGetDeviceQueue( vulkan_device, family_index, 0, &vulkan_queue );

    vulkan_queue_family = family_index;

    //////// Create drawable surface
#if defined (HYDRA_GFX_SDL)

    // Create surface
    SDL_Window* window = ( SDL_Window* )creation.window;
    if ( SDL_Vulkan_CreateSurface( window, vulkan_instance, &vulkan_window_surface ) == SDL_FALSE ) {
        hprint( "Failed to create Vulkan surface.\n" );
    }

    sdl_window = window;

    // Create Framebuffers
    int window_width, window_height;
    SDL_GetWindowSize( window, &window_width, &window_height );
#else
    static_assert( false, "Create surface manually!" );
#endif // HYDRA_GFX_SDL

    //// Select Surface Format
    const TextureFormat::Enum swapchain_formats[] = { TextureFormat::B8G8R8A8_UNORM, TextureFormat::R8G8B8A8_UNORM, TextureFormat::B8G8R8X8_UNORM, TextureFormat::B8G8R8X8_UNORM };
    const VkFormat surface_image_formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    uint32_t supported_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, NULL );
    VkSurfaceFormatKHR* supported_formats = ( VkSurfaceFormatKHR* )halloca( sizeof( VkSurfaceFormatKHR ) * supported_count, allocator );
    vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, supported_formats );

    // Cache render pass output
    swapchain_output.reset();

    //// Check for supported formats
    bool format_found = false;
    const uint32_t surface_format_count = ArraySize( surface_image_formats );

    for ( int i = 0; i < surface_format_count; i++ ) {
        for ( uint32_t j = 0; j < supported_count; j++ ) {
            if ( supported_formats[ j ].format == surface_image_formats[ i ] && supported_formats[ j ].colorSpace == surface_color_space ) {
                vulkan_surface_format = supported_formats[ j ];
                swapchain_output.color( swapchain_formats[ j ] );
                format_found = true;
                break;
            }
        }

        if ( format_found )
            break;
    }

    // Default to the first format supported.
    if ( !format_found ) {
        vulkan_surface_format = supported_formats[ 0 ];
        hy_assert( false );
    }
    hfree( supported_formats, allocator );

    set_present_mode( present_mode );

    //////// Create swapchain
    create_swapchain();

    //////// Create VMA Allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = vulkan_physical_device;
    allocatorInfo.device = vulkan_device;
    allocatorInfo.instance = vulkan_instance;

    result = vmaCreateAllocator( &allocatorInfo, &vma_allocator );
    check( result );

    ////////  Create pools
    static const u32 k_global_pool_elements = 128;
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, k_global_pool_elements },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, k_global_pool_elements}
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = k_global_pool_elements * ArraySize( pool_sizes );
    pool_info.poolSizeCount = ( u32 )ArraySize( pool_sizes );
    pool_info.pPoolSizes = pool_sizes;
    result = vkCreateDescriptorPool( vulkan_device, &pool_info, vulkan_allocation_callbacks, &vulkan_descriptor_pool );
    check( result );

    // Create timestamp query pool used for GPU timings.
    VkQueryPoolCreateInfo vqpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP, creation.gpu_time_queries_per_frame * 2 * k_max_frames, 0 };
    vkCreateQueryPool( vulkan_device, &vqpci, vulkan_allocation_callbacks, &vulkan_timestamp_query_pool );

#if defined (HYDRA_GRAPHICS_TEST)
    test_texture_creation( *this );
    test_pool( *this );
    test_command_buffer( *this );
#endif // HYDRA_GRAPHICS_TEST


    //// Init pools
    buffers.init( allocator, 128, sizeof( BufferVulkan ) );
    textures.init( allocator, 128, sizeof( TextureVulkan ) );
    render_passes.init( allocator, 256, sizeof( RenderPassVulkan ) );
    resource_layouts.init( allocator, 128, sizeof( ResourceLayoutVulkan ) );
    pipelines.init( allocator, 128, sizeof( PipelineVulkan ) );
    shaders.init( allocator, 128, sizeof( ShaderStateVulkan ) );
    resource_lists.init( allocator, 128, sizeof( ResourceListVulkan ) );
    samplers.init( allocator, 32, sizeof( SamplerVulkan ) );
    //command_buffers.init( allocator, 128, sizeof( CommandBuffer ) );

    // Init render frame informations. This includes fences, semaphores, command buffers, ...
    // TODO: memory - allocate memory of all Device render frame stuff
    u8* memory = hallocam( sizeof(GPUTimestampManager) + sizeof(CommandBuffer*) * 128, allocator);

    VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore( vulkan_device, &semaphore_info, vulkan_allocation_callbacks, &vulkan_image_acquired_semaphore );

    for ( size_t i = 0; i < k_max_swapchain_images; i++ ) {

        vkCreateSemaphore( vulkan_device, &semaphore_info, vulkan_allocation_callbacks, &vulkan_render_complete_semaphore[ i ] );

        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence( vulkan_device, &fenceInfo, vulkan_allocation_callbacks, &vulkan_command_buffer_executed_fence[i] );
    }

    gpu_timestamp_manager = ( GPUTimestampManager* )( memory );
    gpu_timestamp_manager->init( allocator, creation.gpu_time_queries_per_frame, k_max_frames );

    command_buffer_ring.init( this );

    // Allocate queued command buffers array
    queued_command_buffers = ( CommandBuffer** )( gpu_timestamp_manager + 1 );
    CommandBuffer** correctly_allocated_buffer = ( CommandBuffer** )( memory + sizeof( GPUTimestampManager ) );
    hy_assertm( queued_command_buffers == correctly_allocated_buffer, "Wrong calculations for queued command buffers arrays. Should be %p, but it is %p.", correctly_allocated_buffer, queued_command_buffers);

    //
    // Init primitive resources
    BufferCreation fullscreen_vb_creation = { BufferType::Vertex_mask, ResourceUsageType::Immutable, 0, nullptr, "Fullscreen_vb" };
    fullscreen_vertex_buffer = create_buffer( fullscreen_vb_creation );

    // Create depth image
    TextureCreation depth_texture_creation = { nullptr, swapchain_width, swapchain_height, 1, 1, 0, TextureFormat::D32_FLOAT, TextureType::Texture2D, "DepthImage_Texture" };
    depth_texture = create_texture( depth_texture_creation );

    // Cache depth texture format
    swapchain_output.depth( TextureFormat::D32_FLOAT );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type( RenderPassType::Swapchain ).set_name( "Swapchain" );
    swapchain_pass_creation.set_operations( RenderPassOperation::Clear, RenderPassOperation::Clear, RenderPassOperation::Clear );
    swapchain_pass = create_render_pass( swapchain_pass_creation );

    // Init Dummy resources
    TextureCreation dummy_texture_creation = { nullptr, 1, 1, 1, 1, 0, TextureFormat::R8_UINT, TextureType::Texture2D };
    dummy_texture = create_texture( dummy_texture_creation );

    SamplerCreation sc{};
    sc.set_address_mode_uvw( TextureAddressMode::Repeat, TextureAddressMode::Repeat, TextureAddressMode::Repeat )
        .set_min_mag_mip( TextureFilter::Linear, TextureFilter::Linear, TextureMipFilter::Linear ).set_name( "Sampler Default" );
    default_sampler = create_sampler( sc );

    BufferCreation dummy_constant_buffer_creation = { BufferType::Constant_mask, ResourceUsageType::Immutable, 16, nullptr, "Dummy_cb" };
    dummy_constant_buffer = create_buffer( dummy_constant_buffer_creation );

    vulkan_image_index = 0;
    current_frame = 1;
    previous_frame = 0;
    absolute_frame = 0;
    timestamps_enabled = false;
    num_deletion_queue = 0;
    num_update_queue = 0;

    // Get binaries path
    char* vulkan_env = string_buffer.reserve( 512 );
    ExpandEnvironmentStringsA( "%VULKAN_SDK%", vulkan_env, 512 );
    char* compiler_path = string_buffer.append_use_f( "%s\\Bin\\", vulkan_env );

    strcpy( vulkan_binaries_path, compiler_path );
    string_buffer.clear();

    // Dynamic buffer handling
    // TODO:
    dynamic_per_frame_size = 1024 * 1024 * 10;
    BufferCreation bc;
    bc.set( (BufferType::Mask)(BufferType::Vertex_mask | BufferType::Index_mask | BufferType::Constant_mask), ResourceUsageType::Immutable, dynamic_per_frame_size * k_max_frames ).set_name( "Dynamic_Persistent_Buffer" );
    dynamic_buffer = create_buffer( bc );

    MapBufferParameters cb_map = { dynamic_buffer, 0, 0 };
    dynamic_mapped_memory = ( u8* )map_buffer( cb_map );

    // Init render pass cache
    render_pass_cache.init( allocator, 16 );
}

void GpuDeviceVulkan::internal_shutdown() {

    vkDeviceWaitIdle( vulkan_device );

    command_buffer_ring.shutdown();

    for ( size_t i = 0; i < k_max_swapchain_images; i++ ) {
        vkDestroySemaphore( vulkan_device, vulkan_render_complete_semaphore[i], vulkan_allocation_callbacks );
        vkDestroyFence( vulkan_device, vulkan_command_buffer_executed_fence[i], vulkan_allocation_callbacks );
    }

    vkDestroySemaphore( vulkan_device, vulkan_image_acquired_semaphore, vulkan_allocation_callbacks );

    gpu_timestamp_manager->shutdown();

    MapBufferParameters cb_map = { dynamic_buffer, 0, 0 };
    unmap_buffer( cb_map );

    // Memory: this contains allocations for gpu timestamp memory, queued command buffers and render frames.
    hfree( gpu_timestamp_manager, allocator );

    destroy_texture( depth_texture );
    destroy_buffer( fullscreen_vertex_buffer );
    destroy_buffer( dynamic_buffer );
    destroy_render_pass( swapchain_pass );
    destroy_texture( dummy_texture );
    destroy_buffer( dummy_constant_buffer );
    destroy_sampler( default_sampler );

    // Destroy all pending resources.
    for ( size_t i = 0; i < num_deletion_queue; i++ ) {
        ResourceDeletion& resource_deletion = resource_deletion_queue[ i ];

        // Skip just freed resources.
        if ( resource_deletion.current_frame == -1 )
            continue;

        switch ( resource_deletion.type ) {

            case ResourceDeletionType::Buffer:
            {
                destroy_buffer_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Pipeline:
            {
                destroy_pipeline_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::RenderPass:
            {
                destroy_render_pass_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ResourceList:
            {
                destroy_resource_list_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ResourceLayout:
            {
                destroy_resource_layout_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Sampler:
            {
                destroy_sampler_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::ShaderState:
            {
                destroy_shader_state_instant( resource_deletion.handle );
                break;
            }

            case ResourceDeletionType::Texture:
            {
                destroy_texture_instant( resource_deletion.handle );
                break;
            }
        }
    }
    num_deletion_queue = 0;


    // Destroy render passes from the cache.
    FlatHashMapIterator it = render_pass_cache.iterator_begin();
    while ( it.is_valid() ) {
        VkRenderPass vk_render_pass = render_pass_cache.get( it );
        vkDestroyRenderPass( vulkan_device, vk_render_pass, vulkan_allocation_callbacks );
        render_pass_cache.iterator_advance( it );
    }
    render_pass_cache.shutdown();

    // Destroy swapchain render pass, not present in the cache.
    RenderPassVulkan* vk_swapchain_pass = access_render_pass( swapchain_pass );
    vkDestroyRenderPass( vulkan_device, vk_swapchain_pass->vk_render_pass, vulkan_allocation_callbacks );

    // Destroy swapchain
    destroy_swapchain();
    vkDestroySurfaceKHR( vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks );

    vmaDestroyAllocator( vma_allocator );

    //command_buffers.shutdown();
    pipelines.shutdown();
    buffers.shutdown();
    shaders.shutdown();
    textures.shutdown();
    samplers.shutdown();
    resource_layouts.shutdown();
    resource_lists.shutdown();
    render_passes.shutdown();
#ifdef VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    //auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkDestroyDebugReportCallbackEXT" );
    //vkDestroyDebugReportCallbackEXT( vulkan_instance, vulkan_debug_callback, vulkan_allocation_callbacks );

    auto vkDestroyDebugUtilsMessengerEXT = ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( vulkan_instance, "vkDestroyDebugUtilsMessengerEXT" );
    vkDestroyDebugUtilsMessengerEXT( vulkan_instance, vulkan_debug_utils_messenger, vulkan_allocation_callbacks );
#endif // IMGUI_VULKAN_DEBUG_REPORT

    vkDestroyDescriptorPool( vulkan_device, vulkan_descriptor_pool, vulkan_allocation_callbacks );
    vkDestroyQueryPool( vulkan_device, vulkan_timestamp_query_pool, vulkan_allocation_callbacks );

    vkDestroyDevice( vulkan_device, vulkan_allocation_callbacks );

    vkDestroyInstance( vulkan_instance, vulkan_allocation_callbacks );
    
}

// GpuDeviceVulkan ////////////////////////////////////////////////////////


static void transition_image_layout( VkCommandBuffer command_buffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, bool is_depth ) {

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        //throw std::invalid_argument( "unsupported layout transition!" );
    }

    vkCmdPipelineBarrier( command_buffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}

// Resource Creation ////////////////////////////////////////////////////////////
static void vulkan_create_texture( GpuDeviceVulkan& gpu, const TextureCreation& creation, TextureHandle handle, TextureVulkan* texture ) {

    texture->width = creation.width;
    texture->height = creation.height;
    texture->depth = creation.depth;
    texture->mipmaps = creation.mipmaps;
    texture->format = creation.format;
    texture->type = creation.type;
    texture->render_target = creation.flags & TextureCreationFlags::RenderTarget_mask;
    texture->name = creation.name;
    texture->vk_format = to_vk_format( creation.format );
    texture->sampler = nullptr;
    texture->flags = creation.flags;

    texture->handle = handle;

    //// Create the image
    VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.format = texture->vk_format;
    image_info.flags = 0;
    image_info.imageType = to_vk_image_type( creation.type );
    image_info.extent.width = creation.width;
    image_info.extent.height = creation.height;
    image_info.extent.depth = creation.depth;
    image_info.mipLevels = creation.mipmaps;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    if ( TextureFormat::has_depth_or_stencil( creation.format ) ) {
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.usage |= ( texture->render_target ) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
        image_info.usage |= ( creation.flags & TextureCreationFlags::ComputeOutput_mask ) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    } else {
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TODO
        image_info.usage |= ( texture->render_target ) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        image_info.usage |= ( creation.flags & TextureCreationFlags::ComputeOutput_mask ) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    }

    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check( vmaCreateImage( gpu.vma_allocator, &image_info, &memory_info,
                           &texture->vk_image, &texture->vma_allocation, nullptr ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE, ( uint64_t )texture->vk_image, creation.name );

    //// Create the image view
    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    info.image = texture->vk_image;
    info.viewType = to_vk_image_view_type( creation.type );
    info.format = image_info.format;

    if ( TextureFormat::has_depth_or_stencil( creation.format ) ) {

        info.subresourceRange.aspectMask = TextureFormat::has_depth( creation.format ) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        // TODO:gs
        //info.subresourceRange.aspectMask |= TextureFormat::has_stencil( creation.format ) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    } else {
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;
    check( vkCreateImageView( gpu.vulkan_device, &info, gpu.vulkan_allocation_callbacks, &texture->vk_image_view ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE_VIEW, ( uint64_t )texture->vk_image_view, creation.name );

    texture->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

TextureHandle GpuDeviceVulkan::create_texture( const TextureCreation& creation ) {

    uint32_t resource_index = textures.obtain_resource();
    TextureHandle handle = { resource_index };
    if ( resource_index == k_invalid_index ) {
        return handle;
    }

    TextureVulkan* texture = access_texture( handle );

    vulkan_create_texture( *this, creation, handle, texture );

    //// Copy buffer_data if present
    if ( creation.initial_data ) {
        // Create stating buffer
        VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        uint32_t image_size = creation.width * creation.height * 4;
        buffer_info.size = image_size;

        VmaAllocationCreateInfo memory_info{};
        memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VmaAllocationInfo allocation_info{};
        VkBuffer staging_buffer;
        VmaAllocation staging_allocation;
        check( vmaCreateBuffer( vma_allocator, &buffer_info, &memory_info,
                                &staging_buffer, &staging_allocation, &allocation_info ) );

        // Copy buffer_data
        void* destination_data;
        vmaMapMemory( vma_allocator, staging_allocation, &destination_data );
        memcpy( destination_data, creation.initial_data, static_cast< size_t >( image_size ) );
        vmaUnmapMemory( vma_allocator, staging_allocation );

        // Execute command buffer
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        CommandBuffer* command_buffer = get_instant_command_buffer();
        vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { creation.width, creation.height, creation.depth };

        // Transition
        transition_image_layout( command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false );
        // Copy
        vkCmdCopyBufferToImage( command_buffer->vk_command_buffer, staging_buffer, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
        // Transition
        transition_image_layout( command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false );

        vkEndCommandBuffer( command_buffer->vk_command_buffer );

        // Submit command buffer
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command_buffer->vk_command_buffer;

        vkQueueSubmit( vulkan_queue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( vulkan_queue );

        vmaDestroyBuffer( vma_allocator, staging_buffer, staging_allocation );

        // TODO: free command buffer
        vkResetCommandBuffer( command_buffer->vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT );

        texture->vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    return handle;
}

static const char* s_shader_compiler_stage[ ShaderStage::Enum::Count ] = { "vert", "frag", "geom", "comp", "tesc", "tese" };

ShaderStateHandle GpuDeviceVulkan::create_shader_state( const ShaderStateCreation& creation ) {

    ShaderStateHandle handle = { k_invalid_index };

    if ( creation.stages_count == 0 || creation.stages == nullptr ) {
        hprint( "Shader %s does not contain shader stages.\n", creation.name );
        return handle;
    }

    handle.index = shaders.obtain_resource();
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    // For each shader stage, compile them individually.
    uint32_t compiled_shaders = 0;

    ShaderStateVulkan* shader_state = access_shader_state( handle );
    shader_state->graphics_pipeline = true;
    shader_state->active_shaders = 0;

    for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
        const ShaderStateCreation::Stage& stage = creation.stages[ compiled_shaders ];

        // Gives priority to compute: if any is present (and it should not be) then it is not a graphics pipeline.
        if ( stage.type == ShaderStage::Compute ) {
            shader_state->graphics_pipeline = false;
        }

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        bool compiled = false;

        if ( creation.spv_input ) {
            createInfo.codeSize = stage.code_size;
            createInfo.pCode = reinterpret_cast< const uint32_t* >( stage.code );
        } else {
            // Compile from glsl to SpirV.
            // TODO: detect if input is HLSL.
            const char* temp_filename = "temp.shader";

            // gstodo
            // Write current shader to file.
            FILE* temp_shader_file = fopen( temp_filename, "w" );
            fwrite( stage.code, stage.code_size, 1, temp_shader_file );
            fclose( temp_shader_file );

            // Compile to SPV
            char* glsl_compiler_path = string_buffer.append_use_f( "%sglslangValidator.exe", vulkan_binaries_path );
            char* final_shader_filename = string_buffer.append_use( "shader_final.spv" );
            char* arguments = string_buffer.append_use_f( "glslangValidator.exe %s -V -o %s -S %s", temp_filename, final_shader_filename, s_shader_compiler_stage[ stage.type ] );
            process_execute( ".", glsl_compiler_path, arguments, "" );

            // Read back SPV file.
            createInfo.pCode = reinterpret_cast< const uint32_t* >( file_read_binary( final_shader_filename, allocator, &createInfo.codeSize ) );

            // Temporary files cleanup
            file_delete( temp_filename );
            file_delete( final_shader_filename );

            compiled = true;
        }

        // Compile shader module
        VkPipelineShaderStageCreateInfo& shader_stage_info = shader_state->shader_stage_info[ compiled_shaders ];
        memset( &shader_stage_info, 0, sizeof( VkPipelineShaderStageCreateInfo ) );
        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info.pName = "main";
        shader_stage_info.stage = to_vk_shader_stage( stage.type );

        if ( vkCreateShaderModule( vulkan_device, &createInfo, nullptr, &shader_state->shader_stage_info[ compiled_shaders ].module ) != VK_SUCCESS ) {

            break;
        }

        if ( compiled ) {
            hfree( ( void* )createInfo.pCode, allocator );
        }

        set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( uint64_t )shader_state->shader_stage_info[ compiled_shaders ].module, creation.name );
    }

    bool creation_failed = compiled_shaders != creation.stages_count;
    if ( !creation_failed ) {
        shader_state->active_shaders = compiled_shaders;
        shader_state->name = creation.name;
    }

    if ( creation_failed ) {
        destroy_shader_state( handle );
        handle.index = k_invalid_index;

        // Dump shader code
        hprint( "Error in creation of shader %s. Dumping all shader informations.\n", creation.name );
        for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
            const ShaderStateCreation::Stage& stage = creation.stages[ compiled_shaders ];
            hprint( "%s:\n%s\n", ShaderStage::ToString( stage.type ), stage.code );
        }
    }

    return handle;
}

PipelineHandle GpuDeviceVulkan::create_pipeline( const PipelineCreation& creation ) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    ShaderStateHandle shader_state = create_shader_state( creation.shaders );
    if ( shader_state.index == k_invalid_index ) {
        // Shader did not compile.
        pipelines.release_resource( handle.index );
        handle.index = k_invalid_index;

        return handle;
    }

    // Now that shaders have compiled we can create the pipeline.
    PipelineVulkan* pipeline = access_pipeline( handle );
    ShaderStateVulkan* shader_state_data = access_shader_state( shader_state );

    pipeline->shader_state = shader_state;

    VkDescriptorSetLayout vk_layouts[ k_max_resource_layouts ];

    // Create VkPipelineLayout
    for ( uint32_t l = 0; l < creation.num_active_layouts; ++l ) {
        pipeline->resource_layout[ l ] = access_resource_layout( creation.resource_layout[ l ] );
        pipeline->resource_layout_handle[ l ] = creation.resource_layout[ l ];

        vk_layouts[ l ] = pipeline->resource_layout[ l ]->vk_descriptor_set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_info.pSetLayouts = vk_layouts;
    pipeline_layout_info.setLayoutCount = creation.num_active_layouts;

    VkPipelineLayout pipeline_layout;
    check( vkCreatePipelineLayout( vulkan_device, &pipeline_layout_info, vulkan_allocation_callbacks, &pipeline_layout ) );
    // Cache pipeline layout
    pipeline->vk_pipeline_layout = pipeline_layout;
    pipeline->num_active_layouts = creation.num_active_layouts;

    // Create full pipeline
    if ( shader_state_data->graphics_pipeline ) {
        VkGraphicsPipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

        //// Shader stage
        pipeline_info.pStages = shader_state_data->shader_stage_info;
        pipeline_info.stageCount = shader_state_data->active_shaders;
        //// PipelineLayout
        pipeline_info.layout = pipeline_layout;

        //// Vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        // Vertex attributes.
        VkVertexInputAttributeDescription vertex_attributes[ 8 ];
        if ( creation.vertex_input.num_vertex_attributes ) {

            for ( uint32_t i = 0; i < creation.vertex_input.num_vertex_attributes; ++i ) {
                const VertexAttribute& vertex_attribute = creation.vertex_input.vertex_attributes[ i ];
                vertex_attributes[ i ] = { vertex_attribute.location, vertex_attribute.binding, to_vk_vertex_format( vertex_attribute.format ), vertex_attribute.offset };
            }

            vertex_input_info.vertexAttributeDescriptionCount = creation.vertex_input.num_vertex_attributes;
            vertex_input_info.pVertexAttributeDescriptions = vertex_attributes;
        } else {
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }
        // Vertex bindings
        VkVertexInputBindingDescription vertex_bindings[ 8 ];
        if ( creation.vertex_input.num_vertex_streams ) {
            vertex_input_info.vertexBindingDescriptionCount = creation.vertex_input.num_vertex_streams;

            for ( uint32_t i = 0; i < creation.vertex_input.num_vertex_streams; ++i ) {
                const VertexStream& vertex_stream = creation.vertex_input.vertex_streams[ i ];
                VkVertexInputRate vertex_rate = vertex_stream.input_rate == VertexInputRate::PerVertex ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX : VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE;
                vertex_bindings[ i ] = { vertex_stream.binding, vertex_stream.stride, vertex_rate };
            }
            vertex_input_info.pVertexBindingDescriptions = vertex_bindings;
        } else {
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
        }

        pipeline_info.pVertexInputState = &vertex_input_info;

        //// Input Assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        pipeline_info.pInputAssemblyState = &input_assembly;

        //// Color Blending
        VkPipelineColorBlendAttachmentState color_blend_attachment[ 8 ];

        if ( creation.blend_state.active_states ) {
            for ( size_t i = 0; i < creation.blend_state.active_states; i++ ) {
                const BlendState& blend_state = creation.blend_state.blend_states[ i ];

                color_blend_attachment[ i ].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                color_blend_attachment[ i ].blendEnable = blend_state.blend_enabled ? VK_TRUE : VK_FALSE;
                color_blend_attachment[ i ].srcColorBlendFactor = to_vk_blend_factor( blend_state.source_color );
                color_blend_attachment[ i ].dstColorBlendFactor = to_vk_blend_factor( blend_state.destination_color );
                color_blend_attachment[ i ].colorBlendOp = to_vk_blend_operation( blend_state.color_operation );

                if ( blend_state.separate_blend ) {
                    color_blend_attachment[ i ].srcAlphaBlendFactor = to_vk_blend_factor( blend_state.source_alpha );
                    color_blend_attachment[ i ].dstAlphaBlendFactor = to_vk_blend_factor( blend_state.destination_alpha );
                    color_blend_attachment[ i ].alphaBlendOp = to_vk_blend_operation( blend_state.alpha_operation );
                } else {
                    color_blend_attachment[ i ].srcAlphaBlendFactor = to_vk_blend_factor( blend_state.source_color );
                    color_blend_attachment[ i ].dstAlphaBlendFactor = to_vk_blend_factor( blend_state.destination_color );
                    color_blend_attachment[ i ].alphaBlendOp = to_vk_blend_operation( blend_state.color_operation );
                }
            }
        } else {
            // Default non blended state
            color_blend_attachment[ 0 ] = {};
            color_blend_attachment[ 0 ].blendEnable = VK_FALSE;
            color_blend_attachment[ 0 ].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo color_blending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blending.attachmentCount = creation.blend_state.active_states ? creation.blend_state.active_states : 1; // Always have 1 blend defined.
        color_blending.pAttachments = color_blend_attachment;
        color_blending.blendConstants[ 0 ] = 0.0f; // Optional
        color_blending.blendConstants[ 1 ] = 0.0f; // Optional
        color_blending.blendConstants[ 2 ] = 0.0f; // Optional
        color_blending.blendConstants[ 3 ] = 0.0f; // Optional

        pipeline_info.pColorBlendState = &color_blending;

        //// Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depth_stencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

        depth_stencil.depthWriteEnable = creation.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.stencilTestEnable = creation.depth_stencil.stencil_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthTestEnable = creation.depth_stencil.depth_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = to_vk_compare_operation( creation.depth_stencil.depth_comparison );
        if ( creation.depth_stencil.stencil_enable ) {
            // TODO: add stencil
            hy_assert( false );
        }

        pipeline_info.pDepthStencilState = &depth_stencil;

        //// Multisample
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        pipeline_info.pMultisampleState = &multisampling;

        //// Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = to_vk_cull_mode( creation.rasterization.cull_mode );
        rasterizer.frontFace = to_vk_front_face( creation.rasterization.front );
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        pipeline_info.pRasterizationState = &rasterizer;

        //// Tessellation
        pipeline_info.pTessellationState;


        //// Viewport state
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = ( float )swapchain_width;
        viewport.height = ( float )swapchain_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { swapchain_width, swapchain_height };

        VkPipelineViewportStateCreateInfo viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        pipeline_info.pViewportState = &viewport_state;

        //// Render Pass
        pipeline_info.renderPass = get_vulkan_render_pass( creation.render_pass, creation.name );

        //// Dynamic states
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic_state.dynamicStateCount = ArraySize( dynamic_states );
        dynamic_state.pDynamicStates = dynamic_states;

        pipeline_info.pDynamicState = &dynamic_state;

        vkCreateGraphicsPipelines( vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline );

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
    } else {
        VkComputePipelineCreateInfo pipeline_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

        pipeline_info.stage = shader_state_data->shader_stage_info[ 0 ];
        pipeline_info.layout = pipeline_layout;

        vkCreateComputePipelines( vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline );

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    return handle;
}

BufferHandle GpuDeviceVulkan::create_buffer( const BufferCreation& creation ) {
    BufferHandle handle = { buffers.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    BufferVulkan* buffer = access_buffer( handle );

    buffer->name = creation.name;
    buffer->size = creation.size;
    buffer->type = creation.type;
    buffer->usage = creation.usage;
    buffer->handle = handle;
    buffer->global_offset = 0;
    buffer->parent_buffer = creation.parent_buffer;

    // Cache and calculate if dynamic buffer can be used.
    static const u32 k_dynamic_buffer_mask = BufferType::Vertex_mask | BufferType::Index_mask | BufferType::Constant_mask;
    const bool use_global_buffer = ( creation.type & k_dynamic_buffer_mask ) != 0;
    if ( creation.usage == ResourceUsageType::Dynamic && use_global_buffer ) {
        buffer->parent_buffer = dynamic_buffer;
        return handle;
    }

    if ( creation.parent_buffer.index != k_invalid_buffer.index ) {
        return handle;
    }

    VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if ( ( creation.type & BufferType::Constant_mask ) == BufferType::Constant_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    
    if ( ( creation.type & BufferType::Structured_mask ) == BufferType::Structured_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if ( ( creation.type & BufferType::Indirect_mask ) == BufferType::Indirect_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    if ( ( creation.type & BufferType::Vertex_mask ) == BufferType::Vertex_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    
    if ( ( creation.type & BufferType::Index_mask ) == BufferType::Index_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.usage = buffer_usage;
    buffer_info.size = creation.size > 0 ? creation.size : 1;       // 0 sized creations are not permitted.

    VmaAllocationCreateInfo memory_info{};
    memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;


    VmaAllocationInfo allocation_info{};
    check( vmaCreateBuffer( vma_allocator, &buffer_info, &memory_info,
                            &buffer->vk_buffer, &buffer->vma_allocation, &allocation_info ) );

    set_resource_name( VK_OBJECT_TYPE_BUFFER, ( uint64_t )buffer->vk_buffer, creation.name );

    buffer->vk_device_memory = allocation_info.deviceMemory;

    if ( creation.initial_data ) {
        void* data;
        vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );
        memcpy( data, creation.initial_data, ( size_t )creation.size );
        vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
    }

    // TODO
    //if ( persistent )
    //{
    //    mapped_data = static_cast<uint8_t *>(allocation_info.pMappedData);
    //}

    return handle;
}

SamplerHandle GpuDeviceVulkan::create_sampler( const SamplerCreation& creation ) {
    SamplerHandle handle = { samplers.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    SamplerVulkan* sampler = access_sampler( handle );

    sampler->address_mode_u = creation.address_mode_u;
    sampler->address_mode_v = creation.address_mode_v;
    sampler->address_mode_w = creation.address_mode_w;
    sampler->min_filter = creation.min_filter;
    sampler->mag_filter = creation.mag_filter;
    sampler->mip_filter = creation.mip_filter;
    sampler->name = creation.name;

    VkSamplerCreateInfo create_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    create_info.addressModeU = to_vk_address_mode( creation.address_mode_u );
    create_info.addressModeV = to_vk_address_mode( creation.address_mode_v );
    create_info.addressModeW = to_vk_address_mode( creation.address_mode_w );
    create_info.minFilter = to_vk_filter( creation.min_filter );
    create_info.magFilter = to_vk_filter( creation.mag_filter );
    create_info.mipmapMode = to_vk_mipmap( creation.mip_filter );
    create_info.anisotropyEnable = 0;
    create_info.compareEnable = 0;
    create_info.unnormalizedCoordinates = 0;
    create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    // TODO:
    /*float                   mipLodBias;
    float                   maxAnisotropy;
    VkCompareOp             compareOp;
    float                   minLod;
    float                   maxLod;
    VkBorderColor           borderColor;
    VkBool32                unnormalizedCoordinates;*/

    vkCreateSampler( vulkan_device, &create_info, vulkan_allocation_callbacks, &sampler->vk_sampler );

    set_resource_name( VK_OBJECT_TYPE_SAMPLER, ( uint64_t )sampler->vk_sampler, creation.name );

    return handle;
}

ResourceLayoutHandle GpuDeviceVulkan::create_resource_layout( const ResourceLayoutCreation& creation ) {
    ResourceLayoutHandle handle = { resource_layouts.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    ResourceLayoutVulkan* resource_layout = access_resource_layout( handle );

    // TODO: add support for multiple sets.
    // Create flattened binding list
    resource_layout->num_bindings = creation.num_bindings;
    resource_layout->bindings = ( ResourceBindingVulkan* )halloca( sizeof( ResourceBindingVulkan ) * creation.num_bindings, allocator );
    resource_layout->vk_binding = ( VkDescriptorSetLayoutBinding* )halloca( sizeof( VkDescriptorSetLayoutBinding ) * creation.num_bindings, allocator );
    resource_layout->handle = handle;

    bool bindless_descriptor = false;

    for ( uint32_t r = 0; r < creation.num_bindings; ++r ) {
        ResourceBindingVulkan& binding = resource_layout->bindings[ r ];
        const ResourceLayoutCreation::Binding& input_binding = creation.bindings[ r ];
        binding.start = input_binding.start == u16_max ? ( u16 )r : input_binding.start;
        binding.count = 1;
        binding.type = ( u16 )input_binding.type;
        binding.name = input_binding.name;

        VkDescriptorSetLayoutBinding& vk_binding = resource_layout->vk_binding[ r ];
        vk_binding.binding = binding.start;
        vk_binding.descriptorType = to_vk_descriptor_type( input_binding.type );
#if defined (HYDRA_BINDLESS)
        if ( binding.type == ResourceType::Texture ) {
            // TODO: hardcoded bindless values to test functionality
            bindless_descriptor = true;
            vk_binding.descriptorCount = k_max_bindless_resources;
            vk_binding.binding = k_bindless_texture_binding;
        }
        else {
            vk_binding.descriptorCount = 1;
            // TODO: still need to improve this!
            vk_binding.descriptorType = vk_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : vk_binding.descriptorType;
        }
        
#else
        vk_binding.descriptorCount = 1;
        // TODO: default to dynamic constants
        vk_binding.descriptorType = vk_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : vk_binding.descriptorType;
#endif // HYDRA_BINDLESS

        // TODO:
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layout_info.bindingCount = creation.num_bindings;
    layout_info.pBindings = resource_layout->vk_binding;

#if defined (HYDRA_BINDLESS)
    if ( bindless_descriptor ) {
        VkDescriptorBindingFlags binding_flag[] = { VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT };
        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
        extended_info.bindingCount = creation.num_bindings;
        extended_info.pBindingFlags = binding_flag;
        // Bindless: need to have partially bound flags to be on all descriptors.
        // Find a way to overcome this.
        hy_assert( creation.num_bindings <= ArraySize( binding_flag ) );

        layout_info.pNext = &extended_info;
    }
#endif // HYDRA_BINDLESS

    vkCreateDescriptorSetLayout( vulkan_device, &layout_info, vulkan_allocation_callbacks, &resource_layout->vk_descriptor_set_layout );

    return handle;
}

//
//
static void vulkan_fill_write_descriptor_sets( GpuDeviceVulkan& gpu, const ResourceLayoutVulkan* resource_layout, VkDescriptorSet vk_descriptor_set,
                                               VkWriteDescriptorSet* descriptor_write, VkDescriptorBufferInfo* buffer_info, VkDescriptorImageInfo* image_info,
                                               VkSampler vk_default_sampler, u32 num_resources, const ResourceHandle* resources, const SamplerHandle* samplers, const u16* bindings ) {

    for ( u32 r = 0; r < num_resources; r++ ) {

        u32 i = r;
        // Binding array contains the index into the resource layout binding to retrieve
        // the correct binding informations.
        u32 layout_binding_index = bindings[ r ];

        const ResourceBindingVulkan& binding = resource_layout->bindings[ layout_binding_index ];

        descriptor_write[ i ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptor_write[ i ].dstSet = vk_descriptor_set;
        // Use binding array to get final binding point.
        const u32 binding_point = binding.start;
        descriptor_write[ i ].dstBinding = binding_point;
        descriptor_write[ i ].dstArrayElement = 0;
        descriptor_write[ i ].descriptorCount = 1;

        switch ( binding.type ) {
            case ResourceType::Texture:
            {
                descriptor_write[ i ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                TextureHandle texture_handle = { resources[ r ] };
                TextureVulkan* texture_data = gpu.access_texture( texture_handle );

                // Find proper sampler.
                // TODO: improve. Remove the single texture interface ?
                image_info[ i ].sampler = vk_default_sampler;
                if ( texture_data->sampler ) {
                    image_info[ i ].sampler = texture_data->sampler->vk_sampler;
                }
                // TODO: else ?
                if ( samplers[ r ].index != k_invalid_index ) {
                    SamplerVulkan* sampler = gpu.access_sampler( { samplers[ r ] } );
                    image_info[ i ].sampler = sampler->vk_sampler;
                }

                image_info[ i ].imageLayout = TextureFormat::has_depth_or_stencil( texture_data->format ) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_info[ i ].imageView = texture_data->vk_image_view;

                descriptor_write[ i ].pImageInfo = &image_info[ i ];

#if defined (HYDRA_BINDLESS)
                descriptor_write[ i ].dstArrayElement = texture_handle.index;
#endif

                break;
            }

            case ResourceType::Image:
            {
                descriptor_write[ i ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { resources[ r ] };
                TextureVulkan* texture_data = gpu.access_texture( texture_handle );

                image_info[ i ].sampler = nullptr;
                image_info[ i ].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[ i ].imageView = texture_data->vk_image_view;

                descriptor_write[ i ].pImageInfo = &image_info[ i ];

                break;
            }

            case ResourceType::ImageRW:
            {
                descriptor_write[ i ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { resources[ r ] };
                TextureVulkan* texture_data = gpu.access_texture( texture_handle );

                image_info[ i ].sampler = nullptr;
                image_info[ i ].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[ i ].imageView = texture_data->vk_image_view;

                descriptor_write[ i ].pImageInfo = &image_info[ i ];

                break;
            }

            case ResourceType::Constants:
            {
                BufferHandle buffer_handle = { resources[ r ] };
                BufferVulkan* buffer = gpu.access_buffer( buffer_handle );

                descriptor_write[ i ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                // gsbindless
//#if !defined( HYDRA_BINDLESS )
                descriptor_write[ i ].descriptorType = buffer->usage == ResourceUsageType::Dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//#endif // HYDRA_BINDLESS
                // Bind parent buffer if present, used for dynamic resources.
                if ( buffer->parent_buffer.index != k_invalid_index ) {
                    BufferVulkan* parent_buffer = gpu.access_buffer( buffer->parent_buffer );

                    buffer_info[ i ].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[ i ].buffer = buffer->vk_buffer;
                }

                buffer_info[ i ].offset = 0;
                buffer_info[ i ].range = buffer->size;

                descriptor_write[ i ].pBufferInfo = &buffer_info[ i ];

                break;
            }

            case ResourceType::StructuredBuffer:
            {
                BufferHandle buffer_handle = { resources[ r ] };
                BufferVulkan* buffer = gpu.access_buffer( buffer_handle );

                descriptor_write[ i ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                // gsbindless
//#if !defined( HYDRA_BINDLESS )
                //descriptor_write[ i ].descriptorType = buffer->usage == ResourceUsageType::Dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//#endif // HYDRA_BINDLESS
                // Bind parent buffer if present, used for dynamic resources.
                if ( buffer->parent_buffer.index != k_invalid_index ) {
                    BufferVulkan* parent_buffer = gpu.access_buffer( buffer->parent_buffer );

                    buffer_info[ i ].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[ i ].buffer = buffer->vk_buffer;
                }

                buffer_info[ i ].offset = 0;
                buffer_info[ i ].range = buffer->size;

                descriptor_write[ i ].pBufferInfo = &buffer_info[ i ];

                break;
            }

            default:
            {
                hy_assertm( false, "Resource type %d not supported in resource list creation!\n", binding.type );
                break;
            }
        }
    }
}

ResourceListHandle GpuDeviceVulkan::create_resource_list( const ResourceListCreation& creation ) {
    ResourceListHandle handle = { resource_lists.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    ResourceListVulkan* resource_list = access_resource_list( handle );
    const ResourceLayoutVulkan* resource_list_layout = access_resource_layout( creation.layout );

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = vulkan_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &resource_list_layout->vk_descriptor_set_layout;

    vkAllocateDescriptorSets( vulkan_device, &allocInfo, &resource_list->vk_descriptor_set );
    // Cache data
    resource_list->resources = ( ResourceHandle* )halloca( sizeof( ResourceHandle ) * creation.num_resources, allocator );
    resource_list->samplers = ( SamplerHandle* )halloca( sizeof( SamplerHandle ) * creation.num_resources, allocator );
    resource_list->bindings = ( u16* )halloca( sizeof( u16 ) * creation.num_resources, allocator );
    resource_list->num_resources = creation.num_resources;
    resource_list->layout = resource_list_layout;

    // Update descriptor set
    VkWriteDescriptorSet descriptor_write[ 8 ];
    VkDescriptorBufferInfo buffer_info[ 8 ];
    VkDescriptorImageInfo image_info[ 8 ];

    SamplerVulkan* vk_default_sampler = access_sampler( default_sampler );

    vulkan_fill_write_descriptor_sets( *this, resource_list_layout, resource_list->vk_descriptor_set, descriptor_write, buffer_info, image_info, vk_default_sampler->vk_sampler,
                                       creation.num_resources, creation.resources, creation.samplers, creation.bindings );

    // Cache resources
    for ( u32 r = 0; r < creation.num_resources; r++ ) {
        resource_list->resources[ r ] = creation.resources[ r ];
        resource_list->samplers[ r ] = creation.samplers[ r ];
        resource_list->bindings[ r ] = creation.bindings[ r ];
    }

    vkUpdateDescriptorSets( vulkan_device, creation.num_resources, descriptor_write, 0, nullptr );

    return handle;
}

static void vulkan_create_swapchain_pass( GpuDeviceVulkan& gpu, const RenderPassCreation& creation, RenderPassVulkan* render_pass ) {
    // Color attachment
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = gpu.vulkan_surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depth_attachment{};
    TextureVulkan* depth_texture_vk = gpu.access_texture( gpu.depth_texture );
    depth_attachment.format = to_vk_format( depth_texture_vk->format );
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
    VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    check( vkCreateRenderPass( gpu.vulkan_device, &render_pass_info, nullptr, &render_pass->vk_render_pass ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_RENDER_PASS, ( uint64_t )render_pass->vk_render_pass, creation.name );

    // Create framebuffer into the device.
    VkFramebufferCreateInfo framebuffer_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_info.renderPass = render_pass->vk_render_pass;
    framebuffer_info.attachmentCount = 2;
    framebuffer_info.width = gpu.swapchain_width;
    framebuffer_info.height = gpu.swapchain_height;
    framebuffer_info.layers = 1;

    VkImageView framebuffer_attachments[ 2 ];
    framebuffer_attachments[ 1 ] = depth_texture_vk->vk_image_view;

    for ( size_t i = 0; i < gpu.vulkan_swapchain_image_count; i++ ) {
        framebuffer_attachments[ 0 ] = gpu.vulkan_swapchain_image_views[ i ];
        framebuffer_info.pAttachments = framebuffer_attachments;

        vkCreateFramebuffer( gpu.vulkan_device, &framebuffer_info, nullptr, &gpu.vulkan_swapchain_framebuffers[ i ] );
        gpu.set_resource_name( VK_OBJECT_TYPE_FRAMEBUFFER, ( uint64_t )gpu.vulkan_swapchain_framebuffers[ i ], creation.name );
    }

    render_pass->width = gpu.swapchain_width;
    render_pass->height = gpu.swapchain_height;

    // Manually transition the texture
    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    CommandBuffer* command_buffer = gpu.get_instant_command_buffer();
    vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { gpu.swapchain_width, gpu.swapchain_height, 1 };

    // Transition
    for ( size_t i = 0; i < gpu.vulkan_swapchain_image_count; i++ ) {
        transition_image_layout( command_buffer->vk_command_buffer, gpu.vulkan_swapchain_images[ i ], gpu.vulkan_surface_format.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false );
    }
    transition_image_layout( command_buffer->vk_command_buffer, depth_texture_vk->vk_image, depth_texture_vk->vk_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, true );

    vkEndCommandBuffer( command_buffer->vk_command_buffer );

    // Submit command buffer
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer->vk_command_buffer;

    vkQueueSubmit( gpu.vulkan_queue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( gpu.vulkan_queue );
}

static void vulkan_create_framebuffer( GpuDeviceVulkan& gpu, RenderPassVulkan* render_pass, const TextureHandle* output_textures, u32 num_render_targets, TextureHandle depth_stencil_texture ) {
    // Create framebuffer
    VkFramebufferCreateInfo framebuffer_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_info.renderPass = render_pass->vk_render_pass;
    framebuffer_info.width = render_pass->width;
    framebuffer_info.height = render_pass->height;
    framebuffer_info.layers = 1;

    VkImageView framebuffer_attachments[ k_max_image_outputs + 1 ]{};
    u32 active_attachments = 0;
    for ( ; active_attachments < num_render_targets; ++active_attachments ) {
        TextureVulkan* texture_vk = gpu.access_texture( output_textures[ active_attachments ] );
        framebuffer_attachments[ active_attachments ] = texture_vk->vk_image_view;
    }

    if ( depth_stencil_texture.index != k_invalid_index ) {
        TextureVulkan* depth_texture_vk = gpu.access_texture( depth_stencil_texture );
        framebuffer_attachments[ active_attachments++ ] = depth_texture_vk->vk_image_view;
    }
    framebuffer_info.pAttachments = framebuffer_attachments;
    framebuffer_info.attachmentCount = active_attachments;

    vkCreateFramebuffer( gpu.vulkan_device, &framebuffer_info, nullptr, &render_pass->vk_frame_buffer );
    gpu.set_resource_name( VK_OBJECT_TYPE_FRAMEBUFFER, ( uint64_t )render_pass->vk_frame_buffer, render_pass->name );
}

//
//
static VkRenderPass vulkan_create_render_pass( GpuDeviceVulkan& gpu, const RenderPassOutput& output, cstring name ) {
    VkAttachmentDescription color_attachments[ 8 ] = {};
    VkAttachmentReference color_attachments_ref[ 8 ] = {};

    VkAttachmentLoadOp color_op, depth_op, stencil_op;
    VkImageLayout color_initial, depth_initial;

    switch ( output.color_operation ) {
        case RenderPassOperation::Load:
            color_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOperation::Clear:
            color_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        default:
            color_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }

    switch ( output.depth_operation ) {
        case RenderPassOperation::Load:
            depth_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOperation::Clear:
            depth_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        default:
            depth_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }

    switch ( output.stencil_operation ) {
        case RenderPassOperation::Load:
            stencil_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case RenderPassOperation::Clear:
            stencil_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        default:
            stencil_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
    }

    // Color attachments
    uint32_t c = 0;
    for ( ; c < output.num_color_formats; ++c ) {
        TextureFormat::Enum format = output.color_formats[ c ];
        VkAttachmentDescription& color_attachment = color_attachments[ c ];
        color_attachment.format = to_vk_format( format );
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = color_op;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = stencil_op;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = color_initial;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference& color_attachment_ref = color_attachments_ref[ c ];
        color_attachment_ref.attachment = c;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Depth attachment
    VkAttachmentDescription depth_attachment{};
    VkAttachmentReference depth_attachment_ref{};

    if ( output.depth_stencil_format != TextureFormat::UNKNOWN ) {

        depth_attachment.format = to_vk_format( output.depth_stencil_format );
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = depth_op;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.stencilLoadOp = stencil_op;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = depth_initial;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = c;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Create subpass.
    // TODO: for now is just a simple subpass, evolve API.
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Calculate active attachments for the subpass
    VkAttachmentDescription attachments[ k_max_image_outputs + 1 ]{};
    uint32_t active_attachments = 0;
    for ( ; active_attachments < output.num_color_formats; ++active_attachments ) {
        attachments[ active_attachments ] = color_attachments[ active_attachments ];
        ++active_attachments;
    }
    subpass.colorAttachmentCount = active_attachments ? active_attachments - 1 : 0;
    subpass.pColorAttachments = color_attachments_ref;

    subpass.pDepthStencilAttachment = nullptr;

    uint32_t depth_stencil_count = 0;
    if ( output.depth_stencil_format != TextureFormat::UNKNOWN ) {
        attachments[ subpass.colorAttachmentCount ] = depth_attachment;

        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        depth_stencil_count = 1;
    }

    VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

    render_pass_info.attachmentCount = ( active_attachments ? active_attachments - 1 : 0 ) + depth_stencil_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass;
    check( vkCreateRenderPass( gpu.vulkan_device, &render_pass_info, nullptr, &vk_render_pass ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_RENDER_PASS, ( uint64_t )vk_render_pass, name );

    return vk_render_pass;
}

//
//
static RenderPassOutput fill_render_pass_output( GpuDeviceVulkan& gpu, const RenderPassCreation& creation ) {
    RenderPassOutput output;
    output.reset();

    for ( u32 i = 0; i < creation.num_render_targets; ++i ) {
        TextureVulkan* texture_vk = gpu.access_texture( creation.output_textures[ i ] );
        output.color( texture_vk->format );
    }
    if ( creation.depth_stencil_texture.index != k_invalid_index ) {
        TextureVulkan* texture_vk = gpu.access_texture( creation.depth_stencil_texture );
        output.depth( texture_vk->format );
    }

    output.color_operation = creation.color_operation;
    output.depth_operation = creation.depth_operation;
    output.stencil_operation = creation.stencil_operation;

    return output;
}

RenderPassHandle GpuDeviceVulkan::create_render_pass( const RenderPassCreation& creation ) {
    RenderPassHandle handle = { render_passes.obtain_resource() };
    if ( handle.index == k_invalid_index ) {
        return handle;
    }

    RenderPassVulkan* render_pass = access_render_pass( handle );
    render_pass->type = creation.type;
    // Init the rest of the struct.
    render_pass->num_render_targets = ( u8 )creation.num_render_targets;
    render_pass->dispatch_x = 0;
    render_pass->dispatch_y = 0;
    render_pass->dispatch_z = 0;
    render_pass->name = creation.name;
    render_pass->vk_frame_buffer = nullptr;
    render_pass->vk_render_pass = nullptr;
    render_pass->scale_x = creation.scale_x;
    render_pass->scale_y = creation.scale_y;
    render_pass->resize = creation.resize;

    // Cache texture handles
    uint32_t c = 0;
    for ( ; c < creation.num_render_targets; ++c ) {
        TextureVulkan* texture_vk = access_texture( creation.output_textures[ c ] );

        render_pass->width = texture_vk->width;
        render_pass->height = texture_vk->height;

        // Cache texture handles
        render_pass->output_textures[ c ] = creation.output_textures[ c ];
    }

    render_pass->output_depth = creation.depth_stencil_texture;

    switch ( creation.type ) {
        case RenderPassType::Swapchain:
        {
            vulkan_create_swapchain_pass( *this, creation, render_pass );

            break;
        }

        case RenderPassType::Compute:
        {
            break;
        }

        case RenderPassType::Standard:
        {
            render_pass->output = fill_render_pass_output( *this, creation );
            render_pass->vk_render_pass = get_vulkan_render_pass( render_pass->output, creation.name );

            vulkan_create_framebuffer( *this, render_pass, creation.output_textures, creation.num_render_targets, creation.depth_stencil_texture );

            break;
        }
    }

    return handle;
}

// Resource Destruction /////////////////////////////////////////////////////////

void GpuDeviceVulkan::destroy_buffer( BufferHandle buffer ) {
    if ( buffer.index < buffers.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Buffer, buffer.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid Buffer %u\n", buffer.index );
    }
}

void GpuDeviceVulkan::destroy_texture( TextureHandle texture ) {
    if ( texture.index < textures.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Texture, texture.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid Texture %u\n", texture.index );
    }
}

void GpuDeviceVulkan::destroy_pipeline( PipelineHandle pipeline ) {
    if ( pipeline.index < pipelines.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Pipeline, pipeline.index, current_frame };
        // Shader state creation is handled internally when creating a pipeline, thus add this to track correctly.
        PipelineVulkan* v_pipeline = access_pipeline( pipeline );
        destroy_shader_state( v_pipeline->shader_state );
    } else {
        hprint( "Graphics error: trying to free invalid Pipeline %u\n", pipeline.index );
    }
}

void GpuDeviceVulkan::destroy_sampler( SamplerHandle sampler ) {
    if ( sampler.index < samplers.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::Sampler, sampler.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid Sampler %u\n", sampler.index );
    }
}

void GpuDeviceVulkan::destroy_resource_layout( ResourceLayoutHandle resource_layout ) {
    if ( resource_layout.index < resource_layouts.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ResourceLayout, resource_layout.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid ResourceLayout %u\n", resource_layout.index );
    }
}

void GpuDeviceVulkan::destroy_resource_list( ResourceListHandle resource_list ) {
    if ( resource_list.index < resource_lists.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ResourceList, resource_list.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid ResourceList %u\n", resource_list.index );
    }
}

void GpuDeviceVulkan::destroy_render_pass( RenderPassHandle render_pass ) {
    if ( render_pass.index < render_passes.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::RenderPass, render_pass.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid RenderPass %u\n", render_pass.index );
    }
}

void GpuDeviceVulkan::destroy_shader_state( ShaderStateHandle shader ) {
    if ( shader.index < shaders.pool_size ) {
        resource_deletion_queue[ num_deletion_queue++ ] = { ResourceDeletionType::ShaderState, shader.index, current_frame };
    } else {
        hprint( "Graphics error: trying to free invalid Shader %u\n", shader.index );
    }
}

// Real destruction methods - the other enqueue only the resources.
void GpuDeviceVulkan::destroy_buffer_instant( ResourceHandle buffer ) {

    BufferVulkan* v_buffer = ( BufferVulkan* )buffers.access_resource( buffer );

    if ( v_buffer && v_buffer->parent_buffer.index == k_invalid_buffer.index ) {
        vmaDestroyBuffer( vma_allocator, v_buffer->vk_buffer, v_buffer->vma_allocation );
    }
    buffers.release_resource( buffer );
}

void GpuDeviceVulkan::destroy_texture_instant( ResourceHandle texture ) {
    TextureVulkan* v_texture = ( TextureVulkan* )textures.access_resource( texture );

    if ( v_texture ) {
        vkDestroyImageView( vulkan_device, v_texture->vk_image_view, vulkan_allocation_callbacks );
        vmaDestroyImage( vma_allocator, v_texture->vk_image, v_texture->vma_allocation );
    }
    textures.release_resource( texture );
}

void GpuDeviceVulkan::destroy_pipeline_instant( ResourceHandle pipeline ) {
    PipelineVulkan* v_pipeline = ( PipelineVulkan* )pipelines.access_resource( pipeline );

    if ( v_pipeline ) {
        vkDestroyPipeline( vulkan_device, v_pipeline->vk_pipeline, vulkan_allocation_callbacks );

        vkDestroyPipelineLayout( vulkan_device, v_pipeline->vk_pipeline_layout, vulkan_allocation_callbacks );
    }
    pipelines.release_resource( pipeline );
}

void GpuDeviceVulkan::destroy_sampler_instant( ResourceHandle sampler ) {
    SamplerVulkan* v_sampler = ( SamplerVulkan* )samplers.access_resource( sampler );

    if ( v_sampler ) {
        vkDestroySampler( vulkan_device, v_sampler->vk_sampler, vulkan_allocation_callbacks );
    }
    samplers.release_resource( sampler );
}

void GpuDeviceVulkan::destroy_resource_layout_instant( ResourceHandle resource_layout ) {
    ResourceLayoutVulkan* v_resource_list_layout = ( ResourceLayoutVulkan* )resource_layouts.access_resource( resource_layout );

    if ( v_resource_list_layout ) {
        vkDestroyDescriptorSetLayout( vulkan_device, v_resource_list_layout->vk_descriptor_set_layout, vulkan_allocation_callbacks );

        hfree( v_resource_list_layout->bindings, allocator );
        hfree( v_resource_list_layout->vk_binding, allocator );
    }
    resource_layouts.release_resource( resource_layout );
}

void GpuDeviceVulkan::destroy_resource_list_instant( ResourceHandle resource_list ) {
    ResourceListVulkan* v_resource_list = ( ResourceListVulkan* )resource_lists.access_resource( resource_list );

    if ( v_resource_list ) {
        hfree( v_resource_list->resources, allocator );
        hfree( v_resource_list->samplers, allocator );
        hfree( v_resource_list->bindings, allocator );
        // This is freed with the DescriptorSet pool.
        //vkFreeDescriptorSets
    }
    resource_lists.release_resource( resource_list );
}

void GpuDeviceVulkan::destroy_render_pass_instant( ResourceHandle render_pass ) {
    RenderPassVulkan* v_render_pass = ( RenderPassVulkan* )render_passes.access_resource( render_pass );

    if ( v_render_pass ) {

        if ( v_render_pass->num_render_targets )
            vkDestroyFramebuffer( vulkan_device, v_render_pass->vk_frame_buffer, vulkan_allocation_callbacks );

        // NOTE: this is now destroyed with the render pass cache, to avoid double deletes.
        //vkDestroyRenderPass( vulkan_device, v_render_pass->vk_render_pass, vulkan_allocation_callbacks );
    }
    render_passes.release_resource( render_pass );
}

void GpuDeviceVulkan::destroy_shader_state_instant( ResourceHandle shader ) {
    ShaderStateVulkan* v_shader_state = ( ShaderStateVulkan* )shaders.access_resource( shader );
    if ( v_shader_state ) {

        for ( size_t i = 0; i < v_shader_state->active_shaders; i++ ) {
            vkDestroyShaderModule( vulkan_device, v_shader_state->shader_stage_info[ i ].module, vulkan_allocation_callbacks );
        }
    }
    shaders.release_resource( shader );
}

void GpuDeviceVulkan::set_resource_name( VkObjectType type, uint64_t handle, const char* name ) {

    if ( !debug_utils_extension_present ) {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name;
    pfnSetDebugUtilsObjectNameEXT( vulkan_device, &name_info );
}

void GpuDeviceVulkan::push_marker( VkCommandBuffer command_buffer, cstring name ) {

    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    label.color[ 0 ] = 1.0f;
    label.color[ 1 ] = 1.0f;
    label.color[ 2 ] = 1.0f;
    label.color[ 3 ] = 1.0f;
    pfnCmdBeginDebugUtilsLabelEXT( command_buffer, &label );
}

void GpuDeviceVulkan::pop_marker( VkCommandBuffer command_buffer ) {
    pfnCmdEndDebugUtilsLabelEXT( command_buffer );
}

// Swapchain //////////////////////////////////////////////////////////////

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi ) {
    hy_assert( !( hi < lo ) );
    return ( v < lo ) ? lo : ( hi < v ) ? hi : v;
}

void GpuDeviceVulkan::create_swapchain() {

    //// Check if surface is supported
    // TODO: Windows only!
    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, vulkan_queue_family, vulkan_window_surface, &surface_supported );
    if ( surface_supported != VK_TRUE ) {
        hprint( "Error no WSI support on physical device 0\n" );
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vulkan_physical_device, vulkan_window_surface, &surface_capabilities );

    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    if ( swapchain_extent.width == UINT32_MAX ) {
        swapchain_extent.width = clamp( swapchain_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width );
        swapchain_extent.height = clamp( swapchain_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height );
    }

    hprint( "Create swapchain %u %u - saved %u %u, min image %u\n", swapchain_extent.width, swapchain_extent.height, swapchain_width, swapchain_height, surface_capabilities.minImageCount );
    
    swapchain_width = (u16)swapchain_extent.width;
    swapchain_height = (u16)swapchain_extent.height;

    //vulkan_swapchain_image_count = surface_capabilities.minImageCount + 2;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = vulkan_window_surface;
    swapchain_create_info.minImageCount = vulkan_swapchain_image_count;
    swapchain_create_info.imageFormat = vulkan_surface_format.format;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = vulkan_present_mode;

    VkResult result = vkCreateSwapchainKHR( vulkan_device, &swapchain_create_info, 0, &vulkan_swapchain );
    check( result );

    //// Cache swapchain images
    vkGetSwapchainImagesKHR( vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, NULL );
    vkGetSwapchainImagesKHR( vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, vulkan_swapchain_images );

    for ( size_t iv = 0; iv < vulkan_swapchain_image_count; iv++ ) {
        // Create an image view which we can render into.
        VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = vulkan_surface_format.format;
        view_info.image = vulkan_swapchain_images[ iv ];
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;

        check( vkCreateImageView( vulkan_device, &view_info, vulkan_allocation_callbacks, &vulkan_swapchain_image_views[ iv ] ) );
    }
}

void GpuDeviceVulkan::destroy_swapchain() {

    for ( size_t iv = 0; iv < vulkan_swapchain_image_count; iv++ ) {
        vkDestroyImageView( vulkan_device, vulkan_swapchain_image_views[ iv ], vulkan_allocation_callbacks );
        vkDestroyFramebuffer( vulkan_device, vulkan_swapchain_framebuffers[ iv ], vulkan_allocation_callbacks );
    }

    vkDestroySwapchainKHR( vulkan_device, vulkan_swapchain, vulkan_allocation_callbacks );
}

VkRenderPass GpuDeviceVulkan::get_vulkan_render_pass( const RenderPassOutput& output, cstring name ) {
    
    // Hash the memory output and find a compatible VkRenderPass.
    // In current form RenderPassOutput should track everything needed, including load operations.
    u64 hashed_memory = hydra::hash_bytes( (void*)&output, sizeof( RenderPassOutput ) );
    VkRenderPass vulkan_render_pass = render_pass_cache.get( hashed_memory );
    if ( vulkan_render_pass ) {
        return vulkan_render_pass;
    }
    vulkan_render_pass = vulkan_create_render_pass( *this, output, name );
    render_pass_cache.insert( hashed_memory, vulkan_render_pass );

    return vulkan_render_pass;
}

void GpuDeviceVulkan::resize_swapchain() {

    vkDeviceWaitIdle( vulkan_device );

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vulkan_physical_device, vulkan_window_surface, &surface_capabilities );
    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    
    // Skip zero-sized swapchain
    //hprint( "Requested swapchain resize %u %u\n", swapchain_extent.width, swapchain_extent.height );
    if ( swapchain_extent.width == 0 || swapchain_extent.height == 0 ) {
        //hprint( "Cannot create a zero-sized swapchain\n" );
        return;
    }
    
    // Internal destroy of swapchain pass to retain the same handle.
    RenderPassVulkan* vk_swapchain_pass = access_render_pass( swapchain_pass );
    vkDestroyRenderPass( vulkan_device, vk_swapchain_pass->vk_render_pass, vulkan_allocation_callbacks );
    // Destroy depth texture
    destroy_texture( depth_texture );
    // Destroy swapchain images and framebuffers
    destroy_swapchain();
    vkDestroySurfaceKHR( vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks );

    // Recreate window surface
    if ( SDL_Vulkan_CreateSurface( sdl_window, vulkan_instance, &vulkan_window_surface ) == SDL_FALSE ) {
        hprint( "Failed to create Vulkan surface.\n" );
    }

    // Create swapchain
    create_swapchain();

    TextureCreation depth_texture_creation = { nullptr, swapchain_width, swapchain_height, 1, 1, 0, TextureFormat::D32_FLOAT, TextureType::Texture2D };
    depth_texture = create_texture( depth_texture_creation );

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type( RenderPassType::Swapchain ).set_name( "Swapchain" );
    vulkan_create_swapchain_pass( *this, swapchain_pass_creation, vk_swapchain_pass );

    vkDeviceWaitIdle( vulkan_device );
}

// Resource list //////////////////////////////////////////////////////////

void GpuDeviceVulkan::update_resource_list( const ResourceListUpdate& update ) {

    if ( update.resource_list.index < resource_lists.pool_size ) {

        resource_list_update_queue[ num_update_queue ] = update;
        resource_list_update_queue[ num_update_queue ].frame_issued = current_frame;
        ++num_update_queue;
    } else {
        hprint( "Graphics error: trying to update invalid ResourceList %u\n", update.resource_list.index );
    }
}

void GpuDeviceVulkan::update_resource_list_instant( const ResourceListUpdate& update ) {

    // Delete descriptor set.
    ResourceListHandle new_resouce_list_handle = { resource_lists.obtain_resource() };
    ResourceListVulkan* new_resource_list = access_resource_list( new_resouce_list_handle );

    ResourceListVulkan* resource_list = access_resource_list( update.resource_list );
    const ResourceLayoutVulkan* resource_layout = resource_list->layout;

    new_resource_list->vk_descriptor_set = resource_list->vk_descriptor_set;
    new_resource_list->bindings = nullptr;
    new_resource_list->resources = nullptr;
    new_resource_list->samplers = nullptr;
    new_resource_list->num_resources = resource_list->num_resources;

    destroy_resource_list( new_resouce_list_handle );

    VkWriteDescriptorSet descriptor_write[ 8 ];
    VkDescriptorBufferInfo buffer_info[ 8 ];
    VkDescriptorImageInfo image_info[ 8 ];

    SamplerVulkan* vk_default_sampler = access_sampler( default_sampler );

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = vulkan_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &resource_list->layout->vk_descriptor_set_layout;
    vkAllocateDescriptorSets( vulkan_device, &allocInfo, &resource_list->vk_descriptor_set );

    vulkan_fill_write_descriptor_sets( *this, resource_layout, resource_list->vk_descriptor_set, descriptor_write, buffer_info, image_info, vk_default_sampler->vk_sampler,
                                       resource_layout->num_bindings, resource_list->resources, resource_list->samplers, resource_list->bindings );

    vkUpdateDescriptorSets( vulkan_device, resource_layout->num_bindings, descriptor_write, 0, nullptr );
}

//
//
static void vulkan_resize_texture( GpuDeviceVulkan& gpu, TextureVulkan* v_texture, TextureVulkan* v_texture_to_delete, u16 width, u16 height, u16 depth ) {

    // Cache handles to be delayed destroyed
    v_texture_to_delete->vk_image_view = v_texture->vk_image_view;
    v_texture_to_delete->vk_image = v_texture->vk_image;
    v_texture_to_delete->vma_allocation = v_texture->vma_allocation;

    // Re-create image in place.
    TextureCreation tc;
    tc.set_flags( v_texture->mipmaps, v_texture->flags ).set_format_type( v_texture->format, v_texture->type ).set_name( v_texture->name ).set_size( width, height, depth );
    vulkan_create_texture( gpu, tc, v_texture->handle, v_texture );
}

//
//
void GpuDeviceVulkan::resize_output_textures( RenderPassHandle render_pass, u16 width, u16 height ) {

    // For each texture, create a temporary pooled texture and cache the handles to delete.
    // This is because we substitute just the Vulkan texture when resizing so that
    // external users don't need to update the handle.

    RenderPassVulkan* vk_render_pass = access_render_pass( render_pass );
    if ( vk_render_pass ) {
        // No need to resize!
        if ( !vk_render_pass->resize ) {
            return;
        }

        // Calculate new width and height based on render pass sizing informations.
        u16 new_width = ( u16 )( width * vk_render_pass->scale_x );
        u16 new_height = ( u16 )( height * vk_render_pass->scale_y );

        // Resize textures
        const u32 rts = vk_render_pass->num_render_targets;
        for ( u32 i = 0; i < rts; ++i ) {
            TextureHandle texture = vk_render_pass->output_textures[ i ];
            TextureVulkan* vk_texture = access_texture( texture );

            // Queue deletion of texture by creating a temporary one
            TextureHandle texture_to_delete = { textures.obtain_resource() };
            TextureVulkan* vk_texture_to_delete = access_texture( texture_to_delete );

            vulkan_resize_texture( *this, vk_texture, vk_texture_to_delete, new_width, new_height, 1 );

            destroy_texture( texture_to_delete );
        }

        if ( vk_render_pass->output_depth.index != k_invalid_index ) {
            TextureVulkan* vk_texture = access_texture( vk_render_pass->output_depth );

            // Queue deletion of texture by creating a temporary one
            TextureHandle texture_to_delete = { textures.obtain_resource() };
            TextureVulkan* vk_texture_to_delete = access_texture( texture_to_delete );

            vulkan_resize_texture( *this, vk_texture, vk_texture_to_delete, new_width, new_height, 1 );

            destroy_texture( texture_to_delete );
        }

        // Again: create temporary resource to use the standard deferred deletion mechanism.
        RenderPassHandle render_pass_to_destroy = { render_passes.obtain_resource() };
        RenderPassVulkan* vk_render_pass_to_destroy = access_render_pass( render_pass_to_destroy );

        vk_render_pass_to_destroy->vk_frame_buffer = vk_render_pass->vk_frame_buffer;
        // This is checked in the destroy method to proceed with frame buffer destruction.
        vk_render_pass_to_destroy->num_render_targets = 1;
        // Set this to 0 so deletion won't be performed.
        vk_render_pass_to_destroy->vk_render_pass = 0;

        destroy_render_pass( render_pass_to_destroy );

        // Recreate framebuffer
        vk_render_pass->width = new_width;
        vk_render_pass->height = new_height;

        vulkan_create_framebuffer( *this, vk_render_pass, vk_render_pass->output_textures, vk_render_pass->num_render_targets, vk_render_pass->output_depth );
    }
}

//
//

void GpuDeviceVulkan::fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier ) {

    RenderPassVulkan* vk_render_pass = access_render_pass( render_pass );

    out_barrier.num_image_barriers = 0;

    if ( vk_render_pass ) {
        const u32 rts = vk_render_pass->num_render_targets;
        for ( u32 i = 0; i < rts; ++i ) {
            out_barrier.image_barriers[ out_barrier.num_image_barriers++ ].texture = vk_render_pass->output_textures[ i ];
        }

        if ( vk_render_pass->output_depth.index != k_invalid_index ) {
            out_barrier.image_barriers[ out_barrier.num_image_barriers++ ].texture = vk_render_pass->output_depth;
        }
    }
}

void GpuDeviceVulkan::new_frame() {

    // Fence wait and reset
    VkFence* render_complete_fence = &vulkan_command_buffer_executed_fence[ current_frame ];

    if ( vkGetFenceStatus( vulkan_device, *render_complete_fence ) != VK_SUCCESS ) {
        vkWaitForFences( vulkan_device, 1, render_complete_fence, VK_TRUE, UINT64_MAX );
    }

    vkResetFences( vulkan_device, 1, render_complete_fence );
    // Command pool reset
    command_buffer_ring.reset_pools( current_frame );
    // Dynamic memory update
    const u32 used_size = dynamic_allocated_size - ( dynamic_per_frame_size * previous_frame );
    dynamic_max_per_frame_size = max( used_size, dynamic_max_per_frame_size );
    dynamic_allocated_size = dynamic_per_frame_size * current_frame;
}

void GpuDeviceVulkan::present() {

    VkResult result = vkAcquireNextImageKHR( vulkan_device, vulkan_swapchain, UINT64_MAX, vulkan_image_acquired_semaphore, VK_NULL_HANDLE, &vulkan_image_index );
    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        resize_swapchain();

        // Advance frame counters that are skipped during this frame.
        frame_counters_advance();

        return;
    }
    VkFence* render_complete_fence = &vulkan_command_buffer_executed_fence[ current_frame ];
    VkSemaphore* render_complete_semaphore = &vulkan_render_complete_semaphore[ current_frame ];
    
    // Copy all commands
    VkCommandBuffer enqueued_command_buffers[ 4 ];
    for ( uint32_t c = 0; c < num_queued_command_buffers; c++ ) {

        CommandBuffer* command_buffer = queued_command_buffers[ c ];

        enqueued_command_buffers[ c ] = command_buffer->vk_command_buffer;
        // NOTE: why it was needing current_pipeline to be setup ?
        if ( command_buffer->is_recording && command_buffer->current_render_pass && ( command_buffer->current_render_pass->type != RenderPassType::Compute ) )
            vkCmdEndRenderPass( command_buffer->vk_command_buffer );

        vkEndCommandBuffer( command_buffer->vk_command_buffer );
    }

    // Submit command buffers
    VkSemaphore wait_semaphores[] = { vulkan_image_acquired_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = num_queued_command_buffers;
    submit_info.pCommandBuffers = enqueued_command_buffers;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = render_complete_semaphore;

    vkQueueSubmit( vulkan_queue, 1, &submit_info, *render_complete_fence );

    VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = render_complete_semaphore;

    VkSwapchainKHR swap_chains[] = { vulkan_swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &vulkan_image_index;
    present_info.pResults = nullptr; // Optional
    result = vkQueuePresentKHR( vulkan_queue, &present_info );

    num_queued_command_buffers = 0;

    //
    // GPU Timestamp resolve
    if ( timestamps_enabled ) {
        if ( gpu_timestamp_manager->has_valid_queries() ) {
        // Query GPU for all timestamps.
            const u32 query_offset = ( current_frame * gpu_timestamp_manager->queries_per_frame ) * 2;
            const u32 query_count = gpu_timestamp_manager->current_query * 2;
            vkGetQueryPoolResults( vulkan_device, vulkan_timestamp_query_pool, query_offset, query_count,
                                   sizeof( uint64_t ) * query_count * 2, &gpu_timestamp_manager->timestamps_data[ query_offset ],
                                   sizeof( gpu_timestamp_manager->timestamps_data[ 0 ] ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );

            // Calculate and cache the elapsed time
            for ( u32 i = 0; i < gpu_timestamp_manager->current_query; i++ ) {
                uint32_t index = ( current_frame * gpu_timestamp_manager->queries_per_frame ) + i;

                GPUTimestamp& timestamp = gpu_timestamp_manager->timestamps[ index ];

                double start = ( double )gpu_timestamp_manager->timestamps_data[ ( index * 2 ) ];
                double end = ( double )gpu_timestamp_manager->timestamps_data[ ( index * 2 ) + 1 ];
                double range = end - start;
                double elapsed_time = range * gpu_timestamp_frequency;

                timestamp.elapsed_ms = elapsed_time;
                timestamp.frame_index = absolute_frame;

                //print_format( "%s: %2.3f d(%u) - ", timestamp.name, elapsed_time, timestamp.depth );
            }
            //print_format( "\n" );
        }
        else if ( gpu_timestamp_manager->current_query ) {
            hprint( "Asymmetrical GPU queries, missing pop of some markers!\n" );
        }

        gpu_timestamp_manager->reset();
        gpu_timestamp_reset = true;
    } else {
        gpu_timestamp_reset = false;
    }


    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized ) {
        resized = false;
        resize_swapchain();

        // Advance frame counters that are skipped during this frame.
        frame_counters_advance();

        return;
    }

    //hydra::print_format( "Index %u, %u, %u\n", current_frame, previous_frame, vulkan_image_index );

    // This is called inside resize_swapchain as well to correctly work.
    frame_counters_advance();

    // Resource deletion using reverse iteration and swap with last element.
    if ( num_deletion_queue > 0 ) {
        for ( i32 i = num_deletion_queue - 1; i >= 0; i-- ) {
            ResourceDeletion& resource_deletion = resource_deletion_queue[ i ];

            if ( resource_deletion.current_frame == current_frame ) {

                switch ( resource_deletion.type ) {

                case ResourceDeletionType::Buffer:
                {
                    destroy_buffer_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Pipeline:
                {
                    destroy_pipeline_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::RenderPass:
                {
                    destroy_render_pass_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ResourceList:
                {
                    destroy_resource_list_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ResourceLayout:
                {
                    destroy_resource_layout_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Sampler:
                {
                    destroy_sampler_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::ShaderState:
                {
                    destroy_shader_state_instant( resource_deletion.handle );
                    break;
                }

                case ResourceDeletionType::Texture:
                {
                    destroy_texture_instant( resource_deletion.handle );
                    break;
                }
                }

                // Mark resource as free
                resource_deletion.current_frame = u32_max;

                // Swap element
                resource_deletion_queue[ i ] = resource_deletion_queue[ --num_deletion_queue ];
            }
        }
    }

    // Resource List Updates
    if ( num_update_queue ) {
        for ( i32 i = num_update_queue - 1; i >= 0; i-- ) {
            ResourceListUpdate& update = resource_list_update_queue[ i ];

            if ( update.frame_issued == current_frame ) {

                update_resource_list_instant( update );

                update.frame_issued = u32_max;
                resource_list_update_queue[ i ] = resource_list_update_queue[ num_update_queue-- ];
            }
        }
    }
}

static VkPresentModeKHR to_vk_present_mode( PresentMode::Enum mode ) {
    switch ( mode ) {
        case PresentMode::VSyncFast:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case PresentMode::VSyncRelaxed:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case PresentMode::Immediate:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::VSync:
        default:
            return VK_PRESENT_MODE_FIFO_KHR;
    }
}

void GpuDeviceVulkan::set_present_mode( PresentMode::Enum mode ) {

    // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
    u32 supported_count = 0;

    static VkPresentModeKHR supported_mode_allocated[ 8 ];
    vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, NULL );
    hy_assert( supported_count < 8 );
    vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_physical_device, vulkan_window_surface, &supported_count, supported_mode_allocated );

    bool mode_found = false;
    VkPresentModeKHR requested_mode = to_vk_present_mode( mode );
    for ( u32 j = 0; j < supported_count; j++ ) {
        if ( requested_mode == supported_mode_allocated[ j ] ) {
            mode_found = true;
            break;
        }
    }

    // Default to VK_PRESENT_MODE_FIFO_KHR that is guaranteed to always be supported
    vulkan_present_mode = mode_found ? requested_mode : VK_PRESENT_MODE_FIFO_KHR;
    // Use 4 for immediate ?
    vulkan_swapchain_image_count = 3;// vulkan_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? 2 : 3;

    present_mode = mode_found ? mode : PresentMode::VSync;
}

void GpuDeviceVulkan::link_texture_sampler( TextureHandle texture, SamplerHandle sampler ) {

    TextureVulkan* texture_vk = access_texture( texture );
    SamplerVulkan* sampler_vk = access_sampler( sampler );

    texture_vk->sampler = sampler_vk;
}

void GpuDeviceVulkan::frame_counters_advance() {
    previous_frame = current_frame;
    current_frame = ( current_frame + 1 ) % vulkan_swapchain_image_count;

    ++absolute_frame;
}

//
//
void GpuDeviceVulkan::queue_command_buffer( CommandBuffer* command_buffer ) {

    queued_command_buffers[ num_queued_command_buffers++ ] = command_buffer;
}

//
//
CommandBuffer* GpuDeviceVulkan::get_command_buffer( QueueType::Enum type, bool begin ) {
    CommandBuffer* cb = command_buffer_ring.get_command_buffer( current_frame, begin );

    // The first commandbuffer issued in the frame is used to reset the timestamp queries used.
    if ( gpu_timestamp_reset && begin ) {
        // These are currently indices!
        vkCmdResetQueryPool( cb->vk_command_buffer, vulkan_timestamp_query_pool, current_frame * gpu_timestamp_manager->queries_per_frame * 2, gpu_timestamp_manager->queries_per_frame );

        gpu_timestamp_reset = false;
    }

    return cb;
}

//
//
CommandBuffer* GpuDeviceVulkan::get_instant_command_buffer() {
    CommandBuffer* cb = command_buffer_ring.get_command_buffer_instant( current_frame, false );
    return cb;
}

// Resource Description Query ///////////////////////////////////////////////////

void Device::query_buffer( BufferHandle buffer, BufferDescription& out_description ) {
    if ( buffer.index != k_invalid_index ) {
        const BufferVulkan* buffer_data = access_buffer( buffer );

        out_description.name = buffer_data->name;
        out_description.size = buffer_data->size;
        out_description.type = buffer_data->type;
        out_description.usage = buffer_data->usage;
        out_description.parent_handle = buffer_data->parent_buffer;
        out_description.native_handle = (void*)&buffer_data->vk_buffer;
    }
}

void Device::query_texture( TextureHandle texture, TextureDescription& out_description ) {
    if ( texture.index != k_invalid_index ) {
        const TextureVulkan* texture_data = access_texture( texture );

        out_description.width = texture_data->width;
        out_description.height = texture_data->height;
        out_description.depth = texture_data->depth;
        out_description.format = texture_data->format;
        out_description.mipmaps = texture_data->mipmaps;
        out_description.type = texture_data->type;
        out_description.render_target = texture_data->render_target;
        out_description.native_handle = (void*)&texture_data->vk_image;
        out_description.name = texture_data->name;
    }
}

void Device::query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description ) {
    if ( pipeline.index != k_invalid_index ) {
        const PipelineVulkan* pipeline_data = access_pipeline( pipeline );

        out_description.shader = pipeline_data->shader_state;
    }
}

void Device::query_sampler( SamplerHandle sampler, SamplerDescription& out_description ) {
    if ( sampler.index != k_invalid_index ) {
        //const SamplerVulkan* sampler_data = access_sampler( sampler );
    }
}

void Device::query_resource_layout( ResourceLayoutHandle resource_list_layout, ResourceLayoutDescription& out_description ) {
    if ( resource_list_layout.index != k_invalid_index ) {
        const ResourceLayoutVulkan* resource_list_layout_data = access_resource_layout( resource_list_layout );

        const uint32_t num_bindings = resource_list_layout_data->num_bindings;
        for ( size_t i = 0; i < num_bindings; i++ ) {
            out_description.bindings[i].name = resource_list_layout_data->bindings[i].name;
            out_description.bindings[i].type = resource_list_layout_data->bindings[i].type;
        }

        out_description.num_active_bindings = resource_list_layout_data->num_bindings;
    }
}

void Device::query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description ) {
    if ( resource_list.index != k_invalid_index ) {
        const ResourceListVulkan* resource_list_data = access_resource_list( resource_list );

        out_description.num_active_resources = resource_list_data->num_resources;
        for ( u32 i = 0; i < out_description.num_active_resources; ++i ) {
            //out_description.resources[ i ].data = resource_list_data->resources[ i ].data;
        }
    }
}

const RenderPassOutput& Device::get_render_pass_output( RenderPassHandle render_pass ) const {
    const RenderPassVulkan* vulkan_render_pass = access_render_pass( render_pass );
    return vulkan_render_pass->output;
}

// Resource Map/Unmap ///////////////////////////////////////////////////////////


void* GpuDeviceVulkan::map_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.index == k_invalid_index )
        return nullptr;

    BufferVulkan* buffer = access_buffer( parameters.buffer );

    if ( buffer->parent_buffer.index == dynamic_buffer.index ) {

        buffer->global_offset = dynamic_allocated_size;

        return dynamic_allocate( parameters.size == 0 ? buffer->size : parameters.size );
    }
    
    void* data;
    vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );

    return data;
}

void GpuDeviceVulkan::unmap_buffer( const MapBufferParameters& parameters ) {
    if ( parameters.buffer.index == k_invalid_index )
        return;

    BufferVulkan* buffer = access_buffer( parameters.buffer );
    if ( buffer->parent_buffer.index == dynamic_buffer.index )
        return;

    vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
}


void GpuDeviceVulkan::set_buffer_global_offset( BufferHandle buffer, u32 offset ) {
    if ( buffer.index == k_invalid_index )
        return;

    BufferVulkan* vulkan_buffer = access_buffer( buffer );
    vulkan_buffer->global_offset = offset;
}

u32 GpuDeviceVulkan::get_gpu_timestamps( GPUTimestamp* out_timestamps ) {
    return gpu_timestamp_manager->resolve( previous_frame, out_timestamps );

}

void GpuDeviceVulkan::push_gpu_timestamp( CommandBuffer* command_buffer, const char* name ) {
    if ( !timestamps_enabled )
        return;

    u32 query_index = gpu_timestamp_manager->push( current_frame, name );
    vkCmdWriteTimestamp( command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkan_timestamp_query_pool, query_index );
}

void GpuDeviceVulkan::pop_gpu_timestamp( CommandBuffer* command_buffer ) {
    if ( !timestamps_enabled )
        return;

    u32 query_index = gpu_timestamp_manager->pop( current_frame );
    vkCmdWriteTimestamp( command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkan_timestamp_query_pool, query_index );
}


// Utility methods //////////////////////////////////////////////////////////////

void check( VkResult result ) {
    if ( result == VK_SUCCESS ) {
        return;
    }

    hprint( "Vulkan error: code(%u)", result );
    if ( result < 0 ) {
        hy_assertm( false, "Vulkan error: aborting." );
    }
}

} // namespace gfx
} // namespace hydra

#endif // HYDRA_VULKAN