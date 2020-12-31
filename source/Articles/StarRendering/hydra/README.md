# HydraLibs
Central repository for personal libraries.<br>
Used as subtree by other repositories.

Includes vk_mem_alloc by AMD.

=======
Library    | Lastest version | Category | LoC | Description
--------------------- | ---- | -------- | --- | --------------------------------
**[hydra_application.h](hydra_application.h)** | 0.11 | app | -1 | Simple window application loop.
**[hydra_graphics.h](hydra_graphics.h)** | 0.12 | gfx | -1 | OpenGL/Vulkan Render API Abstraction.
**[hydra_imgui.h](hydra_imgui.h)** | 0.05 | gfx | -1 | ImGui Renderer using Hydra Graphics.
**[hydra_lib.h](hydra_lib.h)** | 0.12 | ker | -1 | Logging, files, strings, process, stb array and hash maps.
**[hydra_rendering.h](hydra_rendering.h)** | 0.15 | gfx | -1 | High level rendering. CGLM, GLTF, Mesh, Materials, Render Pipelines.
**[hydra_resources.h](hydra_resources.h)** | 0.01 | gfx | -1 | Resource Manager for graphics.
**[hydra_shaderfx.h](hydra_shaderfx.h)** | 0.25 | gfx | -1 | Compiler of HFX.

# TODO

## Serialization
The actual idea is to convert all the serializable data to use structs/classes defined from a Data Definition Format, like HDF.
Serialization can be created on top of that by generating automatically the code using an external serialization library.
Some possibilities:

[Bitsery](https://github.com/fraillt/bitsery)
[MPack](https://github.com/ludocode/mpack)

## Allocation

Still to decide the implementation details: save the allocator or pass it ?

* Add allocators to all data structures (array and maps)

## Resource/Asset Management

* Add an asynchronous resource/asset manager.

## High Level Rendering

* Using serialization and resource/asset manager, high level rendering can be implemented.
* Material + Shaders
* Render Frame/Graph
* Mesh/Scene

