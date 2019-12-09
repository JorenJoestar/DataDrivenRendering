
//
// Hydra HFX v0.1
//

// Header-only library, but with definition/implementation separation.
//
// To use the library, write this in only one cpp file:
//
//      #define HFX_SCG_IMPLEMENTATION
//      #include "ShaderCodeGenerator.h"


#pragma once

#include "Lexer.h"
#include "CodeGenerator.h"  // To reuse struct and variable parsing

#include "hydra/hydra_graphics.h"

namespace hfx {

    typedef hydra::graphics::ShaderStage::Enum Stage;
    typedef hydra::graphics::ResourceListLayoutCreation::Binding ResourceBinding;

#define HFX_PARSING

#if defined (HFX_PARSING)


    //
    // HFX interface ////////////////////////////////////////////////////////////
    //

    bool                            compile_hfx( const char* full_filename, const char* out_folder, const char* out_filename );
    void                            generate_hfx_permutations( const char* file_path, const char* out_folder );


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

        enum Type {
            Float, Int, Range, Color, Vector, Texture1D, Texture2D, Texture3D, TextureVolume, Unknown
        };

        StringRef                   name;
        StringRef                   ui_name;
        StringRef                   ui_arguments;
        StringRef                   default_value;

        Type                        type = Unknown;
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
    struct Pass {

        struct ShaderStage {

            const CodeFragment*     code = nullptr;
            Stage                   stage = Stage::Count;

        }; // struct ShaderStage

        StringRef                   name;
        StringRef                   stage_name;
        std::vector<ShaderStage>    shader_stages;
        std::vector<const ResourceList*> resource_lists;         // List used by the pass
    }; // struct Pass


    //
    //
    struct Shader {

        StringRef                       name;
        StringRef                       pipeline_name;

        std::vector<Pass>               passes;
        std::vector<Property*>          properties;
        std::vector<const ResourceList*> resource_lists;        // All declared lists
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

        StringBuffer                string_buffer;
        Shader                      shader;

    }; // struct Parser

    void                            init_parser( Parser* parser, Lexer* lexer );
    void                            terminate_parser( Parser* parser );

    void                            generate_ast( Parser* parser );

    const CodeFragment*             find_code_fragment( const Parser* parser, const StringRef& name );
    const ResourceList*             find_resource_list( const Parser* parser, const StringRef& name );
    const Property*                 find_property( const Parser* parser, const StringRef& name );

    void                            identifier( Parser* parser, const Token& token );
    void                            pass_identifier( Parser* parser, const Token& token, Pass& pass );
    void                            directive_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    void                            uniform_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    Property::Type                  property_type_identifier( const Token& token );
    void                            resource_binding_identifier( Parser* parser, const Token& token, ResourceBinding& binding, uint32_t flags );

    void                            declaration_shader( Parser* parser );
    void                            declaration_glsl( Parser* parser );
    void                            declaration_pass( Parser* parser );
    void                            declaration_pipeline( Parser* parser );
    void                            declaration_shader_stage( Parser* parser, Pass::ShaderStage& out_stage );
    void                            declaration_properties( Parser* parser );
    void                            declaration_property( Parser* parser, const StringRef& name );
    void                            declaration_layout( Parser* parser );
    void                            declaration_resource_list( Parser* parser, ResourceList& resource_list );
    void                            declaration_pass_resources( Parser* parser, Pass& pass );
    void                            declaration_pass_stage( Parser* parser, Pass& pass );
    void                            declaration_includes( Parser* parser );

    //
    // CodeGenerator ////////////////////////////////////////////////////

    //
    //
    struct CodeGenerator {

        const Parser*               parser = nullptr;
        uint32_t                    buffer_count;

        StringBuffer*               string_buffers;

        char                        binary_header_magic[32];        // Memory used in individual headers when generating binary files.
    }; // struct CodeGenerator

    void                            init_code_generator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size, uint32_t buffer_count );
    void                            terminate_code_generator( CodeGenerator* code_generator );

    void                            generate_shader_permutations( CodeGenerator* code_generator, const char* path );
    void                            compile_shader_effect_file( CodeGenerator* code_generator, const char* path, const char* filename );
    void                            generate_shader_resource_header( CodeGenerator* code_generator, const char* path );

    void                            output_shader_stage( CodeGenerator* code_generator, const char* path, const Pass::ShaderStage& stage );

    bool                            is_resources_layout_automatic( const Shader& shader );

#else
    struct Property {

        enum Type {
            Float, Int, Range, Color, Vector, Texture1D, Texture2D, Texture3D, TextureVolume, Unknown
        };
    };

#endif // HFX_PARSING

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
            uint16_t                    num_shader_chunks;
            uint16_t                    num_resource_layouts;
            uint32_t                    resource_table_offset;
            char                        name[32];
            char                        stage_name[32];
        }; // struct PassHeader


        struct ChunkHeader {
            int8_t                      shader_stage;   // Compressed enum
        }; // struct ChunkHeader

        struct MaterialProperty {

            Property::Type              type;
            uint16_t                    offset;
            char                        name[64];
        }; // struct MaterialProperty


        char*                           memory                  = nullptr;  //
        Header*                         header                  = nullptr;

        uint16_t                        num_resource_defaults   = 0;
        uint16_t                        num_properties          = 0;
        uint32_t                        local_constants_size    = 0;

        char*                           resource_defaults_data  = nullptr;
        char*                           local_constants_default_data = nullptr;
        char*                           properties_data         = nullptr;

    }; // struct ShaderEffectFile

    // ShaderEffectFile methods /////////////////////////////////////////////////
    void                                init_shader_effect_file( ShaderEffectFile& file, const char* full_filename );
    void                                init_shader_effect_file( ShaderEffectFile& file, char* memory );

    ShaderEffectFile::PassHeader*       get_pass( ShaderEffectFile& file, uint32_t index );
    void                                get_shader_creation( ShaderEffectFile::PassHeader* pass_header, uint32_t index, hydra::graphics::ShaderCreation::Stage* shader_creation );
    ShaderEffectFile::MaterialProperty* get_property( char* properties_data, uint32_t index );

    const hydra::graphics::ResourceListLayoutCreation::Binding* get_pass_layout_bindings( ShaderEffectFile::PassHeader* pass_header, uint32_t layout_index, uint8_t& num_bindings );


} // namespace hfx
