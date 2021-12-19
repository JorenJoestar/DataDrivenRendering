#pragma once

#include "graphics/gpu_resources.hpp"

#include "kernel/data_structures.hpp"
#include "kernel/string.hpp"
#include "kernel/service.hpp"

namespace hydra {

struct Allocator;

namespace gfx {


// Forward-declarations /////////////////////////////////////////////////////////
struct CommandBuffer;
struct DeviceRenderFrame;
struct GPUTimestampManager;
struct Device;


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


struct GPUTimestampManager {

    void                            init( Allocator* allocator, u16 queries_per_frame, u16 max_frames );
    void                            shutdown();

    bool                            has_valid_queries() const;
    void                            reset();
    u32                             resolve( u32 current_frame, GPUTimestamp* timestamps_to_fill );    // Returns the total queries for this frame.

    u32                             push( u32 current_frame, const char* name );    // Returns the timestamp query index.
    u32                             pop( u32 current_frame );

    Allocator*                      allocator                   = nullptr;
    GPUTimestamp*                   timestamps                  = nullptr;
    u64*                            timestamps_data             = nullptr;

    u32                             queries_per_frame           = 0;
    u32                             current_query               = 0;
    u32                             parent_index                = 0;
    u32                             depth                       = 0;

    bool                            current_frame_resolved      = false;    // Used to query the GPU only once per frame if get_gpu_timestamps is called more than once per frame.

}; // struct GPUTimestampManager


//
//
struct DeviceCreation {

    Allocator*                      allocator       = nullptr;
    void*                           window          = nullptr; // Pointer to API-specific window: SDL_Window, GLFWWindow
    u16                             width           = 1;
    u16                             height          = 1;

    u16                             gpu_time_queries_per_frame = 32;
    bool                            enable_gpu_time_queries = false;
    bool                            debug           = false;

    DeviceCreation&                 set_window( u32 width, u32 height, void* handle );
    DeviceCreation&                 set_allocator( Allocator* allocator );

}; // struct DeviceCreation

//
//
struct Device : public Service {

    static Device*                  instance();
    
    // Init/Terminate methods
    void                            init( const DeviceCreation& creation );
    void                            shutdown();
    
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

    // Query Description ////////////////////////////////////////////////////////
    void                            query_buffer( BufferHandle buffer, BufferDescription& out_description );
    void                            query_texture( TextureHandle texture, TextureDescription& out_description );
    void                            query_pipeline( PipelineHandle pipeline, PipelineDescription& out_description );
    void                            query_sampler( SamplerHandle sampler, SamplerDescription& out_description );
    void                            query_resource_layout( ResourceLayoutHandle resource_layout, ResourceLayoutDescription& out_description );
    void                            query_resource_list( ResourceListHandle resource_list, ResourceListDescription& out_description );
    void                            query_shader_state( ShaderStateHandle shader, ShaderStateDescription& out_description );

    const RenderPassOutput&         get_render_pass_output( RenderPassHandle render_pass ) const;

    // Update/Reload resources
    void                            resize_output_textures( RenderPassHandle render_pass, u32 width, u32 height );

    void                            update_resource_list( ResourceListHandle resource_list );

    // Misc
    void                            link_texture_sampler( TextureHandle texture, SamplerHandle sampler );   // TODO: for now specify a sampler for a texture or use the default one.


    // Map/Unmap ////////////////////////////////////////////////////////////////
    void*                           map_buffer( const MapBufferParameters& parameters );
    void                            unmap_buffer( const MapBufferParameters& parameters );

    void*                           dynamic_allocate( u32 size );

    void                            set_buffer_global_offset( BufferHandle buffer, u32 offset );

    // Command Buffers //////////////////////////////////////////////////////////
    CommandBuffer*                  get_command_buffer( QueueType::Enum type, bool begin );
    CommandBuffer*                  get_instant_command_buffer();

    void                            queue_command_buffer( CommandBuffer* command_buffer );          // Queue command buffer that will not be executed until present is called.

    // Rendering ////////////////////////////////////////////////////////////////
    void                            new_frame();
    void                            present();
    void                            resize( u16 width, u16 height );
    void                            set_presentation_mode( PresentMode::Enum mode );

    void                            fill_barrier( RenderPassHandle render_pass, ExecutionBarrier& out_barrier );

    BufferHandle                    get_fullscreen_vertex_buffer() const;           // Returns a vertex buffer usable for fullscreen shaders that uses no vertices.
    RenderPassHandle                get_swapchain_pass() const;                     // Returns what is considered the final pass that writes to the swapchain.

    TextureHandle                   get_dummy_texture() const;
    BufferHandle                    get_dummy_constant_buffer() const;
    const RenderPassOutput&         get_swapchain_output() const                    { return swapchain_output; }
    
    // GPU Timings //////////////////////////////////////////////////////////////
    void                            set_gpu_timestamps_enable( bool value )         { timestamps_enabled = value; }

    u32                             get_gpu_timestamps( GPUTimestamp* out_timestamps );
    void                            push_gpu_timestamp( CommandBuffer* command_buffer, const char* name );
    void                            pop_gpu_timestamp( CommandBuffer* command_buffer );
    
    // Internals ////////////////////////////////////////////////////////////////
    void                            backend_init( const DeviceCreation& creation );
    void                            backend_shutdown();
    
    ResourcePool                    buffers;
    ResourcePool                    textures;
    ResourcePool                    pipelines;
    ResourcePool                    samplers;
    ResourcePool                    resource_layouts;
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
    
    RenderPassOutput                swapchain_output;

    StringBuffer                    string_buffer;

    Allocator*                      allocator;

    u32                             dynamic_max_per_frame_size;
    BufferHandle                    dynamic_buffer;
    u8*                             dynamic_mapped_memory;
    u32                             dynamic_allocated_size;
    u32                             dynamic_per_frame_size;

    CommandBuffer**                 queued_command_buffers              = nullptr;
    u32                             num_allocated_command_buffers       = 0;
    u32                             num_queued_command_buffers          = 0;

    //DeviceRenderFrame*              render_frames;

    PresentMode::Enum               present_mode                        = PresentMode::VSync;
    u32                             current_frame;
    u32                             previous_frame;

    u32                             absolute_frame;

    u16                             swapchain_width                     = 1;
    u16                             swapchain_height                    = 1;

    GPUTimestampManager*            gpu_timestamp_manager               = nullptr;

    bool                            bindless_supported                  = false;
    bool                            timestamps_enabled                  = false;
    bool                            resized                             = false;
    bool                            vertical_sync                       = false;

    static constexpr cstring        k_name = "hydra_gpu_service";


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

    ResourceLayoutAPIGnostic*       access_resource_layout( ResourceLayoutHandle resource_layout );
    const ResourceLayoutAPIGnostic* access_resource_layout( ResourceLayoutHandle resource_layout ) const;

    ResourceListAPIGnostic*         access_resource_list( ResourceListHandle resource_list );
    const ResourceListAPIGnostic*   access_resource_list( ResourceListHandle resource_list ) const;

    RenderPassAPIGnostic*           access_render_pass( RenderPassHandle render_pass );
    const RenderPassAPIGnostic*     access_render_pass( RenderPassHandle render_pass ) const;


}; // struct Device


} // namespace gfx
} // namespace hydra
