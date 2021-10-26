
//
// Hydra HFX v0.49
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/05/22, 18.50
//
//
// Revision history //////////////////////
//
//      0.49  (2021/10/25): + Fixed a reflection duplication bug coming from the generated json.
//      0.48  (2021/10/24): + Added support for Structured Buffers in layouts. + Added support for automatic layout from reflection data.
//      0.47  (2021/10/23): + Added textures and ubos indices in reflection generated c++ header.
//      0.46  (2021/10/22): + Restored reflection generation.
//      0.45  (2021/10/21): + Updated HFX blueprint to use the updated blob serializer.
//      0.44  (2021/10/11): + Fixed binary header magic in V2 format and restored caching.
//      0.43  (2021/09/29): + Rewrite of new binary format based on Relative data structures.
//      0.42  (2021/06/15): + Removed output file from hfx_compile method to remove memory leaks. + Implemented allocator aware shader_effect_init method.
//      0.41  (2021/06/10): + Separated in different files and updated to HydraNext.
//      0.40  (2021/02/16): + Added proper include injection.
//      0.39  (2021/02/03): + Fixed parsing of shader code with error with newer and more robus lexer parsing instead of manually doing it. + Removed \r injected in shader generator.
//      0.38  (2020/01/14): + Added resource list method that initalize also a name map.
//      0.37  (2020/01/06): + Compile binary only if source file has changed.
//      0.36  (2020/01/05): + Changed namespace output of reflection c++ header to be pass_name_stage instead of shader_name_stage to avoid conflics in reuse of shaders.
//      0.35  (2020/12/31): + Added constants c++ file generation from reflection. + Added option to retain temporary files. + Using spir-v for reflection data generation.
//      0.34  (2020/12/30): + Added glsl platform automatically into shader as first line. + Added defines as first thing after platform.
//      0.33  (2020/12/29): + Write binary ONLY IF compilation of all shaders succeded!
//      0.32  (2020/12/28): + Added parsing of compute dispatch sizes.
//      0.31  (2020/12/25): + Added parsing of sampler/images 3D and 1D.
//      0.30  (2020/12/21): + Fixed difference between append and variadic append methods. + Made bigger string buffers.
//      0.29  (2020/12/20): + Added shader debug output to show previous lines than error in generated shaders.
//      0.28  (2020/12/18): + Fixed bug in parsing some texts.
//      0.27  (2020/12/15): + Fixed embedded SpirV flagging for shader compilation. + Added gl_VertexID/Index defines for increased compatibility.
//      0.26  (2020/12/14): + Simplification of interfaces for compilation and shader effects.
//      0.25  (2020/04/14): + Fixed filename parsing problem with '/'. + Removed pipeline name from HFX file.
//      0.24  (2020/03/24): + Added permutation parsing.
//      0.23  (2020/03/23): + Added output directory to Parser. + Greatly improved output directory of intermediate files.
//      0.22  (2020/03/20): + Updated parsing of vertex input rate - now is in the vertex stream.
//      0.21  (2020/03/20): + Added ImGui inspection. + Fixed allocator assignment in parser.
//      0.20  (2020/03/18): + Added allocator usage.
//      0.19  (2020/03/16): + Added ImGui debug view.
//      0.18  (2020/03/12): + Removed get_shader_creation from API interface. It is included get_pipeline already.
//      0.17  (2020/03/08): + Improving documentation and API. + Renamed 'Lexer' to 'hydra_lexer'. + Fixed all warnings.
//      0.16  (2020/03/06): + Small fix for loading includes from other HFX. That part needs to be reworked. + Added a couple of error messages.
//      0.15  (2020/03/05): + Renamed almost all methods in lexer and hydra_shaderfx. + Removed CodeGenerator header (unused).
//      0.14  (2020/03/02): + Added compiler options as api input and in CodeGenerator class. + Added glsl compiler path.
//                          + Reworked internals to use the new options.
//      0.13  (2020/02/28): + Added initial compilation step using glslangvalidator.
//      0.12  (2020/02/27): + Added binary file inspection.
//      0.11  (2020/02/06): + Added revision history.
//
//
// Example ///////////////////////////////
//
// 1. To compile an HFX file into an embedded binary one:
//      uint32_t options = hfx::CompileOptions_OpenGL | hfx::CompileOptions_Embedded;
//      hfx::hfx_compile( "simple.hfx", "simple.bhfx", options );
//
//
// API Documentation /////////////////////
//
//      The library is divided in 2 main parts: shader compiler and shader fx reader.
//
//      Shader Compiler
//          The methods to compile a HFX file are:
//
//          hfx_compile( input, output, options )
//              Compile a HFX file to either multiple files (for each permutation, pass and stage) or an embedded binary HFX.
//              
//          hfx_inspect( binary hfx file )
//              Shows a written dump of data inside the binary hfx file.
//
//          hfx_inspect_imgui( binary hfx file )
//              Use ImGui to show binary hfx file contents.
//
//
//      Shader FX Reader
//
// Customization /////////////////////////
//
//      Different defines can be used to use custom code.
//      Being this a library defined in h/cpp files, you need to modify the cpp to customize it.
//      So far the following functions are changable (at the top of hydra_shaderfx.cpp):
//
//      HYDRA_LOG( message, ... )                       hydra::print_format
//
//      HYDRA_MALLOC( size )                            hydra::hy_malloc( size )
//      HYDRA_FREE( pointer )                           hydra::hy_free( pointer )
//
//      HYDRA_ASSERT( condition, message, ... )         assert( condition )
//
//      Additional options:
//
//      To enable debug drawing using ImGui
//      #define HYDRA_IMGUI

#pragma once

#include "kernel/lexer.hpp"
#include "kernel/string.hpp"
#include "kernel/memory.hpp"
#include "kernel/array.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/relative_data_structures.hpp"
#include "kernel/blob.hpp"

#include "graphics/gpu_resources.hpp"

#include <vector>

#define HFX_COMPILER
#define HFX_V2

namespace hfx {

    typedef hydra::StringView                                   StringRef;
    typedef hydra::StringBuffer                                 StringBuffer;
    typedef hydra::gfx::ShaderStage::Enum                       Stage;
    typedef hydra::gfx::ResourceLayoutCreation::Binding         ResourceBinding;

    enum PropertyType {
        Float, Int, Range, Color, Vector, Texture1D, Texture2D, Texture3D, TextureVolume, Unknown
    }; // enum PropertyType

    //
    //
    struct ComputeDispatch {
        u16                             x = 0;
        u16                             y = 0;
        u16                             z = 0;
    }; // struct ComputeDispatch

    // NEW VERSION ////////////////////////////////////////////////////////
    //
    // Rewriting of the serialization of the HFX files using Relative
    // data structures. It should give an easier to read binary layout.

    //
    //
    struct RenderStateBlueprint {

        hydra::gfx::RasterizationCreation   rasterization;
        hydra::gfx::DepthStencilCreation    depth_stencil;
        hydra::gfx::BlendStateCreation      blending;

        u64                         hash;   // Used for fast retrieval in runtime.

    }; // struct RenderStateBlueprint

    //
    //
    struct ShaderCodeBlueprint {

        hydra::RelativeArray<u8>    code;
        u8                          stage;  // hydra::gfx::ShaderStage enum

    }; // struct ShaderCodeBlueprint

    //
    //
    struct ResourceLayoutBlueprint {

        hydra::RelativeArray<ResourceBinding> bindings;
        u64                         hash;   // Used for fast retrieval in runtime.

    }; // struct ResourceLayoutBlueprint

    //
    //
    struct ShaderPassBlueprint {

        void                        fill_pipeline( hydra::gfx::PipelineCreation& out_pipeline );
        void                        fill_resource_layout( hydra::gfx::ResourceLayoutCreation& creation, u32 index );

        char                        name[ 32 ];
        char                        stage_name[ 32 ];

        ComputeDispatch             compute_dispatch;
        u8                          is_spirv;

        hydra::RelativeArray<ShaderCodeBlueprint> shaders;

        hydra::RelativePointer<RenderStateBlueprint> render_state;

        hydra::RelativeArray<hydra::gfx::VertexStream> vertex_streams;
        hydra::RelativeArray<hydra::gfx::VertexAttribute> vertex_attributes;
        
        hydra::RelativeArray<ResourceLayoutBlueprint> resource_layouts;

    }; // struct ShaderPassBlueprint

    //
    //
    struct ShaderEffectBlueprint : public hydra::Blob {

        char                        binary_header_magic[ 32 ];

        hydra::RelativeString       name;
        hydra::RelativeArray<ShaderPassBlueprint> passes;

        static constexpr u32        k_version = 0;

    }; // struct ShaderEffectBlueprint



    // OLDER VERSION //////////////////////////////////////////////////////
    //
    // Shader effect file containing all the informations to build a shader.
    struct ShaderEffectFile {

        //
        // Main header of the file.
        struct Header {
            uint32_t                    num_passes;
            uint32_t                    resource_defaults_offset;
            uint32_t                    properties_offset;
            char                        name[32];
            char                        binary_header_magic[32];
        }; // struct Header

        struct ShaderChunk {
            uint32_t                    start;
            uint32_t                    size;
        }; // struct ShaderChunk


        struct PassHeader {
            uint8_t                     num_shader_chunks;
            uint8_t                     num_vertex_streams;
            uint8_t                     num_vertex_attributes;
            uint8_t                     num_resource_layouts;
            uint8_t                     has_resource_state;
            uint8_t                     is_spirv;
            uint16_t                    shader_list_offset;
            uint32_t                    resource_table_offset;
            ComputeDispatch             compute_dispatch;
            char                        name[32];
            char                        stage_name[32];
        }; // struct PassHeader


        struct ChunkHeader {
            uint32_t                    code_size;
            int8_t                      shader_stage;   // Compressed enum
        }; // struct ChunkHeader

        struct MaterialProperty {

            PropertyType                type;
            uint16_t                    offset;
            char                        name[64];
        }; // struct MaterialProperty


        char*                           memory = nullptr;
        Header*                         header = nullptr;

        uint16_t                        num_resource_defaults = 0;
        uint16_t                        num_properties = 0;
        uint32_t                        local_constants_size = 0;

        char*                           resource_defaults_data = nullptr;
        char*                           local_constants_default_data = nullptr;
        char*                           properties_data = nullptr;

    }; // struct ShaderEffectFile

    // ShaderEffectFile methods /////////////////////////////////////////////////

    /*struct NameToIndex {
        char*                           key;
        u16                             value;
    }; // struct NameToResourceIndex*/

    typedef hydra::FlatHashMap<char*, u16> NameToIndex;


#if !defined (HFX_V2)
    //
    // Read file/memory of a binary compiled HFX file and initialize shader effect file struct.
    // For full compilation and init use hfx_compile specifying the output ShaderEffectFile.
    void                                shader_effect_init( ShaderEffectFile& out_file, const char* full_filename, hydra::Allocator* allocator );
    void                                shader_effect_init( ShaderEffectFile& out_file, char* memory );

    void                                shader_effect_shutdown( ShaderEffectFile& file, hydra::Allocator* allocator );

    void                                shader_effect_get_pipeline( ShaderEffectFile& hfx, uint32_t pass_index, hydra::gfx::PipelineCreation& out_pipeline );
    void                                shader_effect_get_resource_list_layout( ShaderEffectFile& hfx, uint32_t pass_index, uint32_t layout_index, hydra::gfx::ResourceLayoutCreation& out_list );
    void                                shader_effect_get_resource_layout( ShaderEffectFile& hfx, u32 pass_index, u32 layout_index, hydra::gfx::ResourceLayoutCreation& out_list, NameToIndex* out_map );
    u32                                 shader_effect_get_pass_index( ShaderEffectFile& hfx, const char* name );
    const char*                         shader_effect_get_pass_name( ShaderEffectFile& hfx, u32 index );

    // TODO[future]: this will become internal methods and be removed from here.
    ShaderEffectFile::PassHeader*       shader_effect_get_pass( char* hfx_memory, uint32_t index );
    ShaderEffectFile::MaterialProperty* shader_effect_get_property( char* properties_data, uint32_t index );

    void                                shader_effect_pass_get_pipeline( ShaderEffectFile::PassHeader* pass_header, hydra::gfx::PipelineCreation& out_pipeline );

    const hydra::gfx::ResourceLayoutCreation::Binding* shader_effect_pass_get_layout_bindings( ShaderEffectFile::PassHeader* pass_header, uint32_t layout_index, uint8_t& out_num_bindings );
#endif // HFX_V2

#if defined (HFX_COMPILER)

    //
    // HFX compiler interface ////////////////////////////////////////////////////////////
    //

    enum CompileOptions {
        CompileOptions_None         = 0,
        CompileOptions_OpenGL       = 1,                    // Compile for GLSL used by an OpenGL backend.
        CompileOptions_Vulkan       = 1 << 1,               // Compile for a Vulkan backend. It will automatically add CompileOptions_SpirV
        CompileOptions_SpirV        = 1 << 2,               // Compile in SpirV. Optional in OpenGL (some GPUs do not support it) but automatic for Vulkan.
        CompileOptions_Embedded     = 1 << 3,               // Embed all the different shaders and pipeline informations in an embedded file.
        CompileOptions_Output_Files = 1 << 4,               // Output intermediate files for inspection.
        CompileOptions_Reflection_CPP = 1 << 5,             // Generate .h/.cpp for constants.
        CompileOptions_Reflection_Reload = 1 << 6,          // Slower reflection using variants and blobs of memory.

        CompileOptions_VulkanStandard = CompileOptions_Vulkan | CompileOptions_Embedded | CompileOptions_Reflection_CPP
    }; // enum CompileOptions

    //
    // Main compile function. Input_filename is the hfx input file, output_filename can be either a file or a folder, options dictate the behaviour.
    // Optionally specify an output shader effect file.
    bool                            hfx_compile( const char* input_filename, const char* output_filename, u32 options, bool force_rebuild = false );

    void                            hfx_inspect( const char* binary_filename );
    void                            hfx_inspect_imgui( ShaderEffectFile& bhfx_file );

    //
    // Parsing classes //////////////////////////////////////////////////////////

    //
    //
    struct CodeFragment {

        struct Resource {

            hydra::gfx::ResourceType::Enum type;
            StringRef               name;

        }; // struct Resource

        struct Include {
            StringRef               filename;
            u32                     declaration_line;
            u16                     stage_mask;
            u8                      file_or_local;
        };

        std::vector<Include>        includes;
        std::vector<Resource>       resources;          // Used to generate the layout table.

        StringRef                   name;
        StringRef                   code;
        Stage                       current_stage = Stage::Count;
        uint32_t                    ifdef_depth = 0;
        uint32_t                    stage_ifdef_depth[Stage::Count];
        uint32_t                    starting_file_line;

    }; // struct CodeFragment

    //
    //
    struct Property {

        StringRef                   name;
        StringRef                   ui_name;
        StringRef                   ui_arguments;
        StringRef                   default_value;

        PropertyType                type = Unknown;
        uint32_t                    offset_in_bytes = 0;
        uint32_t                    data_index = 0xffffffff;     // Index into the DataBuffer holding the value

    }; // struct Properties

    //
    //
    struct ResourceList {

        StringRef                   name;
        std::vector<ResourceBinding> resources;
        std::vector<uint32_t>       flags;

    }; // struct ResourceList

    //
    //
    struct VertexLayout {

        StringRef                   name;
        std::vector<hydra::gfx::VertexStream> streams;
        std::vector<hydra::gfx::VertexAttribute> attributes;

    }; // struct VertexLayout

    //
    //
    struct RenderState {

        StringRef                   name;

        hydra::gfx::RasterizationCreation  rasterization;
        hydra::gfx::DepthStencilCreation   depth_stencil;
        hydra::gfx::BlendStateCreation     blend_state;

    }; // struct RenderState

    //
    //
    struct Pass {

        struct ShaderStage {

            const CodeFragment*     code = nullptr;
            Stage                   stage = Stage::Count;

        }; // struct ShaderStage

        StringRef                   name;
        StringRef                   stage_name;
        std::vector<ShaderStage>    shader_stages;
        std::vector<StringRef>      options;
        std::vector<uint16_t>       options_offsets;
        ComputeDispatch             compute_dispatch;

        std::vector<const ResourceList*> resource_lists;         // List used by the pass
        const VertexLayout*         vertex_layout;
        const RenderState*          render_state;

    }; // struct Pass

    //
    //
    struct SamplerState {

        StringRef                   name;
        hydra::gfx::SamplerCreation sampler;

    }; // struct SamplerState

    //
    //
    struct Shader {

        StringRef                       name;

        std::vector<Pass>               passes;
        std::vector<Property*>          properties;
        std::vector<const ResourceList*> resource_lists;        // All declared lists
        std::vector<const VertexLayout*> vertex_layouts;        // All declared vertex layouts
        std::vector<const RenderState*> render_states;          // All declared render states
        std::vector<const SamplerState*> sampler_states;        // All declared sampler states
        std::vector<StringRef>          hfx_includes;           // HFX files included with this.
        std::vector<CodeFragment>       code_fragments;

        bool                            has_local_resource_list = false;

    }; // struct Shader

    //
    // Parser ///////////////////////////////////////////////////////////////////

    //
    //
    struct Parser {

        Lexer*                      lexer       = nullptr;
        hydra::Allocator*           allocator   = nullptr;

        StringBuffer                string_buffer;
        Shader                      shader;

        char                        source_path[512];
        char                        source_filename[512];

        char                        destination_path[512];

    }; // struct Parser

    void                            parser_init( Parser* parser, Lexer* lexer, hydra::Allocator* allocator, const char* source_path, const char* source_filename, const char* destination_path );
    void                            parser_terminate( Parser* parser );

    void                            parser_generate_ast( Parser* parser );

    const CodeFragment*             find_code_fragment( const Parser* parser, const StringRef& name );
    const ResourceList*             find_resource_list( const Parser* parser, const StringRef& name );
    const Property*                 find_property( const Parser* parser, const StringRef& name );
    const VertexLayout*             find_vertex_layout( const Parser* parser, const StringRef& name );
    const RenderState*              find_render_state( const Parser* parser, const StringRef& name );
    const SamplerState*             find_sampler_state( const Parser* parser, const StringRef& name );

    void                            identifier( Parser* parser, const Token& token );
    void                            pass_identifier( Parser* parser, const Token& token, Pass& pass );
    void                            directive_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    void                            uniform_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    PropertyType                    property_type_identifier( const Token& token );
    void                            resource_binding_identifier( Parser* parser, const Token& token, ResourceBinding& binding, uint32_t flags );
    void                            vertex_attribute_identifier( Parser* parser, Token& token, hydra::gfx::VertexAttribute& attribute );
    void                            vertex_binding_identifier( Parser* parser, Token& token, hydra::gfx::VertexStream& stream );

    void                            declaration_shader( Parser* parser );
    void                            declaration_glsl( Parser* parser );
    void                            declaration_pass( Parser* parser );
    void                            declaration_pipeline( Parser* parser );
    void                            declaration_shader_stage( Parser* parser, Pass::ShaderStage& out_stage );
    void                            declaration_properties( Parser* parser );
    void                            declaration_property( Parser* parser, const StringRef& name );
    void                            declaration_layout( Parser* parser );
    void                            declaration_resource_list( Parser* parser, ResourceList& resource_list );
    void                            declaration_vertex_layout( Parser* parser, VertexLayout& vertex_layout );
    void                            declaration_pass_resources( Parser* parser, Pass& pass );
    void                            declaration_pass_stage( Parser* parser, Pass& pass );
    void                            declaration_pass_vertex_layout( Parser* parser, Pass& pass );
    void                            declaration_pass_render_states( Parser* parser, Pass& pass );
    void                            declaration_pass_options( Parser* parser, Pass& pass );
    void                            declaration_pass_dispatch( Parser* parser, Pass& pass );
    void                            declaration_includes( Parser* parser );
    void                            declaration_render_states( Parser* parser );
    void                            declaration_render_state( Parser* parser, RenderState& render_state );
    void                            declaration_sampler_states( Parser* parser );
    void                            declaration_sampler_state( Parser* parser, SamplerState& state );

    //
    // CodeGenerator ////////////////////////////////////////////////////


    //
    // Used to retrieve reflection types.
    struct TypeDefinitionAlias {
        char* key;
        char* value;
    }; // struct TypeDefinitionAlias


    //
    //
    struct CodeGenerator {

        const Parser*               parser          = nullptr;
        uint32_t                    buffer_count    = 0;

        StringBuffer*               string_buffers  = nullptr;
        hydra::FlatHashMap<u64, cstring> name_to_type;

        char                        shader_binaries_path[512];      // Path of the shader compiler used, if needed.

        char                        binary_header_magic[32];        // Memory used in individual headers when generating binary files.

        uint32_t                    options;                        // CompileOption flags cache.
    }; // struct CodeGenerator

    void                            code_generator_init( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size, uint32_t buffer_count );
    void                            code_generator_terminate( CodeGenerator* code_generator );

    void                            code_generator_output_shader_files( CodeGenerator* code_generator, const char* path );
    void                            code_generator_generate_embedded_file( CodeGenerator* code_generator, const char* output_filename );
    void                            code_generator_generate_shader_cpp_header( CodeGenerator* code_generator, const char* path );

#endif // HFX_COMPILER


} // namespace hfx
