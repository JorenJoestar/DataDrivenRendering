#pragma once

//
//  Hydra Graphics - v0.54
//  3D API wrapper around Vulkan/Direct3D12/OpenGL.
//  Mostly based on the amazing Sokol library (https://github.com/floooh/sokol), but with a different target (wrapping Vulkan/Direct3D12).
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/05/22, 18.50
//
// Files /////////////////////////////////
//
// command_buffer.hpp/.cpp, gpu_device.hpp/.cpp, gpu_device_vulkan.hpp/.cpp, gpu_enum.hpp, gpu_enum_vulkan.hpp,
// gpu_resources.hpp/.cpp, gpu_resources_vulkan.hpp.
//
// Revision history //////////////////////
//
//      0.48  (2021/11/15): + Handled update after bind for bindless descriptor set layout. + Added deferred bindless textures descriptors update, and batched them.
//      0.47  (2021/11/07): + Changed ResourceLayoutCreation::Binding to use a cstring instead of an array. Backend implementation are using a cstring,
//                            so they were losing the data after the creation struct was going out of scope.
//      0.46  (2021/11/06): + Added initial support for Vulkan bindless by creating custom descriptor set, layout and pools.
//      0.45  (2021/10/26): + Added VkRenderPass cache based on RenderPassOutput struct hash. + Fixed resize GPU query problems.
//      0.44  (2021/10/25): + Added support for color/depth operation for each render pass.
//      0.43  (2021/10/24): + Added Vulkan support for Storage/Structured and Indirect buffers. + Enabled all hardware features by default on Vulkan device creation.
//                          + Fixed destination binding in vulkan_fill_write_descriptor_sets.
//      0.42  (2021/10/22): + Vulkan vkEndRenderPass called always if the command buffer is recording, instead of needing a pipeline to be bound.
//      0.41  (2021/09/20): + Deletion of intermediate shader files created when creating a shader state.
//      0.40  (2021/06/10): + Separated gpu device in multiple files. + Added memory allocators to all allocations.
//      0.39  (2021/05/11): + Started bindless support on Vulkan. Still working on interface.
//      0.38  (2021/04/12): + Fixed resource list binding bug.
//      0.37  (2021/01/28): + Fixed unordered binding of resource list based on index.
//      0.36  (2021/01/26): + Added RenderPassOutput class to separate between Pipeline texture format needs and actual binding.
//      0.35  (2021/01/23): + Fixed recreation of render passes by deferring resource deletion with temporary textures/render passes to be deleted.
//      0.34  (2021/01/21): + BREAKING: renamed add_buffer and add_texture methods to simply 'buffer' and 'texture'. + Added binding to methods.
//      0.33  (2021/01/20): + Fixed GPU queries result wrong on non 0 frames.
//      0.32  (2021/01/14): + BREAKING: renamed resource_list_layout to resource_layout.
//      0.31  (2021/01/07): + Added internal recreation of swapchain during resize to maintain same resource handle.
//      0.30  (2021/01/02): + BREAKING: renamed handle member in handle to index. + Added invalid handles for resources.
//      0.29  (2021/01/02): + BREAKING: added typed handles to resource lists. + BIG: added DEFERRED update resource list method. + Fixed iteration for resource deletion.
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

