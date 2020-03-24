
//
// Hydra HFX v0.24
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/05/22, 18.50
//
//
// Revision history //////////////////////
//
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
//      1. Shader FX Reader
//          
//
//      2. Shader Compiler
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

#include "hydra_lexer.h"
#include "hydra_graphics.h"

#define HYDRA_IMGUI
#define HYDRA_LIB

#if defined (HYDRA_LIB)

using HfxMemoryAllocator = hydra::MemoryAllocator;

#else

struct HfxMemoryAllocator {

};

#endif // HYDRA_LIB

namespace hfx {

    typedef hydra::StringRef                                    StringRef;
    typedef hydra::StringBuffer                                 StringBuffer;
    typedef hydra::graphics::ShaderStage::Enum                  Stage;
    typedef hydra::graphics::ResourceListLayoutCreation::Binding ResourceBinding;

    enum PropertyType {
        Float, Int, Range, Color, Vector, Texture1D, Texture2D, Texture3D, TextureVolume, Unknown
    };

    

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
            char                        pipeline_name[32];
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
            uint16_t                    has_resource_state;
            uint16_t                    shader_list_offset;
            uint32_t                    resource_table_offset;
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
    void                                shader_effect_init( ShaderEffectFile& file, const char* full_filename );
    void                                shader_effect_init( ShaderEffectFile& file, char* memory );

    ShaderEffectFile::PassHeader*       shader_effect_get_pass( char* hfx_memory, uint32_t index );
    ShaderEffectFile::MaterialProperty* shader_effect_get_property( char* properties_data, uint32_t index );

    void                                shader_effect_pass_get_pipeline( ShaderEffectFile::PassHeader* pass_header, hydra::graphics::PipelineCreation& pipeline );

    const hydra::graphics::ResourceListLayoutCreation::Binding* shader_effect_pass_get_layout_bindings( ShaderEffectFile::PassHeader* pass_header, uint32_t layout_index, uint8_t& out_num_bindings );


#define HFX_COMPILER

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
    }; // enum CompileOptions

    //
    // Main compile function. Input_filename is the hfx input file, output name can be either a file or a folder. Options dictate the behaviour.
    bool                            hfx_compile( const char* input_filename, const char* output_name, uint32_t options );

    void                            hfx_inspect( const char* binary_filename );
    void                            hfx_inspect_imgui( ShaderEffectFile& bhfx_file );

    //
    // Parsing classes //////////////////////////////////////////////////////////

    //
    //
    struct CodeFragment {

        struct Resource {

            hydra::graphics::ResourceType::Enum type;
            StringRef               name;

        }; // struct Resource

        std::vector<StringRef>      includes;
        std::vector<uint32_t>       includes_flags;     // Stage mask + File/Local include, used for referencing other hfx.
        std::vector<Resource>       resources;          // Used to generate the layout table.

        StringRef                   name;
        StringRef                   code;
        Stage                       current_stage = Stage::Count;
        uint32_t                    ifdef_depth = 0;
        uint32_t                    stage_ifdef_depth[Stage::Count];

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
        std::vector<hydra::graphics::VertexStream> streams;
        std::vector<hydra::graphics::VertexAttribute> attributes;

    }; // struct VertexLayout

    //
    //
    struct RenderState {

        StringRef                   name;

        hydra::graphics::RasterizationCreation  rasterization;
        hydra::graphics::DepthStencilCreation   depth_stencil;
        hydra::graphics::BlendStateCreation     blend_state;

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

        std::vector<const ResourceList*> resource_lists;         // List used by the pass
        const VertexLayout*         vertex_layout;
        const RenderState*          render_state;

    }; // struct Pass

    //
    //
    struct SamplerState {

        StringRef                   name;
        hydra::graphics::SamplerCreation sampler;

    }; // struct SamplerState

    //
    //
    struct Shader {

        StringRef                       name;
        StringRef                       pipeline_name;

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

        Lexer*                      lexer = nullptr;
        HfxMemoryAllocator*         allocator = nullptr;

        StringBuffer                string_buffer;
        Shader                      shader;

        char                        source_path[512];
        char                        source_filename[512];

        char                        destination_path[512];

    }; // struct Parser

    void                            parser_init( Parser* parser, Lexer* lexer, HfxMemoryAllocator* allocator, const char* source_path, const char* source_filename, const char* destination_path );
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
    void                            vertex_attribute_identifier( Parser* parser, Token& token, hydra::graphics::VertexAttribute& attribute );
    void                            vertex_binding_identifier( Parser* parser, Token& token, hydra::graphics::VertexStream& stream );

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
    void                            declaration_includes( Parser* parser );
    void                            declaration_render_states( Parser* parser );
    void                            declaration_render_state( Parser* parser, RenderState& render_state );
    void                            declaration_sampler_states( Parser* parser );
    void                            declaration_sampler_state( Parser* parser, SamplerState& state );

    //
    // CodeGenerator ////////////////////////////////////////////////////

    //
    //
    struct CodeGenerator {

        const Parser*               parser = nullptr;
        uint32_t                    buffer_count;

        StringBuffer*               string_buffers;

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
