#pragma once

#include "Lexer.h"
#include "CodeGenerator.h"  // To reuse struct and variable parsing
#include "hydra_graphics.h"

namespace hfx {

    typedef hydra::graphics::ShaderStage::Enum Stage;
    typedef hydra::graphics::ResourceListLayoutCreation::Binding ResourceBinding;

    //
    //
    struct CodeFragment {

        struct Resource {
            hydra::graphics::ResourceType::Enum type;
            StringRef               name;
        }; // struct Resource

        std::vector<StringRef>      includes;
        std::vector<Stage>          includes_stage;     // Used to separate which include is in which shader stage.
        std::vector<Resource>       resources;          // Used to generate the layout table.

        StringRef                   name;
        StringRef                   code;
        Stage                       current_stage       = Stage::Count;
        uint32_t                    ifdef_depth         = 0;
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

        Type                        type                = Unknown;
        
        uint32_t                    data_index          = 0xffffffff;     // Index into the DataBuffer holding the value
    
    }; // struct Properties

    //
    //
    struct ResourceList {

        StringRef                   name;
        std::vector<ResourceBinding> resources;

    }; // struct ResourceList

    //
    //
    struct Pass {

        struct ShaderStage {

            const CodeFragment*     code = nullptr;
            Stage                   stage = Stage::Count;

        }; // struct ShaderStage

        StringRef                   name;
        std::vector<ShaderStage>    shader_stages;
        std::vector<const ResourceList*> resource_lists;         // List used by the pass
    }; // struct Pass


    //
    //
    struct Shader {

        StringRef                       name;

        std::vector<Pass*>              passes;
        std::vector<Property*>          properties;
        std::vector<const ResourceList*> resource_lists;    // All declared lists
    }; // struct Shader

    //
    //
    struct Parser {

        Lexer*                          lexer           = nullptr;

        std::vector<CodeFragment>       code_fragments;
        std::vector<Pass>               passes;
        Shader                          shader;

    }; // struct Parser

    static void                         initParser( Parser* parser, Lexer* lexer );
    static void                         generateAST( Parser* parser );

    static const CodeFragment*          findCodeFragment( Parser* parser, const StringRef& name );
    static const ResourceList*          findResourceList( Parser* parser, const StringRef& name );

    static void                         identifier( Parser* parser, const Token& token );
    static void                         passIdentifier( Parser* parser, const Token& token, Pass& pass );
    static void                         directiveIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    static void                         uniformIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    static Property::Type               propertyTypeIdentifier( const Token& token );
    static void                         resourceBindingIdentifier( Parser* parser, const Token& token, ResourceBinding& binding );
    
    static void                         declarationShader( Parser* parser );
    static void                         declarationGlsl( Parser* parser );
    static void                         declarationPass( Parser* parser );
    static void                         declarationShaderStage( Parser* parser, Pass::ShaderStage& out_stage );
    static void                         declarationProperties( Parser* parser );
    static void                         declarationProperty( Parser* parser, const StringRef& name );
    static void                         declarationLayout( Parser* parser );
    static void                         declarationResourceList( Parser* parser, ResourceList& resource_list );
    static void                         declarationPassResources( Parser* parser, Pass& pass );

    //
    //
    struct CodeGenerator {
        
        const Parser*                   parser = nullptr;
        uint32_t                        buffer_count;

        StringBuffer*                   string_buffers;
        
    }; // struct CodeGenerator

    //
    // Following ideas from https://github.com/niklasfrykholm/blog/blob/master/2014/entity-4.md
    // The file contains 2 integers, array of passes, array of offsets to shader chunks and the shader chunks themselves.
    struct ShaderEffectFile {

        struct Chunk {
            uint32_t                    start;
            uint32_t                    size;
        }; // struct Chunk

        struct PassHeader {
            uint16_t                    num_shader_chunks;
            uint16_t                    num_resources;
            uint32_t                    resource_table_offset;
            char                        name[32];
        }; // struct PassHeader

        struct ChunkHeader {
            int8_t                      shader_stage;   // Compressed enum
        };

        uint32_t                        num_passes;

    }; // struct ShaderEffectFile

    static void                         initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size, uint32_t buffer_count );
    static void                         generateShaderPermutations( CodeGenerator* code_generator, const char* path );
    static void                         compileShaderEffectFile( CodeGenerator* code_generator, const char* path, const char* filename );
    static void                         generateShaderResourceHeader( CodeGenerator* code_generator, const char* path );

    static void                         outputShaderStage( CodeGenerator* code_generator, const char* path, const Pass::ShaderStage& stage );

    bool                                isResourceLayoutAutomatic( const Shader& shader );

    char*                               getPassMemory( char* hfx_file_memory, uint32_t index );
    void                                getShaderCreation( uint32_t shader_count, char* pass_memory, uint32_t index, hydra::graphics::ShaderCreation::Stage* shader_creation );


    //
    // IMPLEMENTATION
    //

    void initParser( Parser* parser, Lexer* lexer ) {

        parser->lexer = lexer;

        parser->shader.name.length = 0;
        parser->shader.name.text = nullptr;
        parser->shader.passes.clear();
        parser->shader.properties.clear();
        parser->shader.resource_lists.clear();

        parser->code_fragments.clear();
        parser->passes.clear();
    }

    void generateAST( Parser* parser ) {

        // Read source text until the end.
        // The main body can be a list of declarations.
        bool parsing = true;

        while ( parsing ) {

            Token token;
            nextToken( parser->lexer, token );

            switch ( token.type ) {

                case Token::Token_Identifier:
                {
                    identifier( parser, token );
                    break;
                }

                case Token::Type::Token_EndOfStream:
                {
                    parsing = false;
                    break;
                }
            }
        }
    }

    static bool expectKeyword( const StringRef& text, uint32_t length, const char* expected_keyword ) {
        if ( text.length != length )
            return false;

        return memcmp( text.text, expected_keyword, length ) == 0;
    }

    void identifier( Parser* parser, const Token& token ) {

        // Scan the name to know which 
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                case 's':
                {
                    if ( expectKeyword( token.text, 6, "shader" ) ) {
                        declarationShader( parser );
                        return;
                    }

                    break;
                }

                case 'g':
                {
                    if ( expectKeyword( token.text, 4, "glsl" ) ) {
                        declarationGlsl( parser );
                        return;
                    }
                    break;
                }

                case 'p':
                {
                    if ( expectKeyword( token.text, 4, "pass" ) ) {
                        declarationPass( parser );
                        return;
                    }
                    
                    if ( expectKeyword( token.text, 10, "properties" ) ) {
                        declarationProperties( parser );
                        return;
                    }
                    break;
                }

                case 'l':
                {
                    if ( expectKeyword( token.text, 6, "layout" ) ) {
                        declarationLayout( parser );
                        return;
                    }
                }
            }
        }
    }

    void passIdentifier( Parser* parser, const Token& token, Pass& pass ) {
        // Scan the name to know which 
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                
                case 'c':
                {
                    if ( expectKeyword( token.text, 7, "compute") ) {
                        Pass::ShaderStage stage = { nullptr, Stage::Compute };
                        declarationShaderStage( parser, stage );
                        
                        pass.shader_stages.emplace_back( stage );
                        return;
                    }
                    break;
                }

                case 'v':
                {
                    if ( expectKeyword( token.text, 6, "vertex" ) ) {
                        Pass::ShaderStage stage = { nullptr, Stage::Vertex };
                        declarationShaderStage( parser, stage );

                        pass.shader_stages.emplace_back( stage );
                        return;
                    }
                    break;
                }

                case 'f':
                {
                    if ( expectKeyword( token.text, 8, "fragment" ) ) {
                        Pass::ShaderStage stage = { nullptr, Stage::Fragment };
                        declarationShaderStage( parser, stage );

                        pass.shader_stages.emplace_back( stage );
                        return;
                    }
                    break;
                }

                case 'r':
                {
                    if ( expectKeyword( token.text, 9, "resources" ) ) {
                        declarationPassResources( parser, pass );
                        return;
                    }
                }
            }
        }
    }

    void directiveIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment ) {
        
        Token new_token;
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                case 'i':
                {
                    // Search for the pattern 'if defined'
                    if ( expectKeyword( token.text, 2, "if" ) ) {
                        nextToken( parser->lexer, new_token );

                        if ( expectKeyword( new_token.text, 7, "defined" ) ) {
                            nextToken( parser->lexer, new_token );

                            // Use 0 as not set value for the ifdef depth.
                            ++code_fragment.ifdef_depth;

                            if ( expectKeyword( new_token.text, 6, "VERTEX" ) ) {

                                code_fragment.stage_ifdef_depth[Stage::Vertex] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = Stage::Vertex;
                            }
                            else if ( expectKeyword( new_token.text, 8, "FRAGMENT" ) ) {

                                code_fragment.stage_ifdef_depth[Stage::Fragment] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = Stage::Fragment;
                            }
                            else if ( expectKeyword( new_token.text, 7, "COMPUTE" ) ) {

                                code_fragment.stage_ifdef_depth[Stage::Compute] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = Stage::Compute;
                            }
                        }

                        return;
                    }
                    break;
                }

                case 'p':
                {
                    if ( expectKeyword( token.text, 6, "pragma" ) ) {
                        nextToken( parser->lexer, new_token );

                        if ( expectKeyword( new_token.text, 7, "include" ) ) {
                            nextToken( parser->lexer, new_token );

                            code_fragment.includes.emplace_back( new_token.text );
                            code_fragment.includes_stage.emplace_back( code_fragment.current_stage );
                        }

                        return;
                    }
                    break;
                }

                case 'e':
                {
                    if ( expectKeyword( token.text, 5, "endif" ) ) {

                        if ( code_fragment.stage_ifdef_depth[Stage::Vertex] == code_fragment.ifdef_depth ) {
                            
                            code_fragment.stage_ifdef_depth[Stage::Vertex] = 0xffffffff;
                            code_fragment.current_stage = Stage::Count;
                        }
                        else if ( code_fragment.stage_ifdef_depth[Stage::Fragment] == code_fragment.ifdef_depth ) {

                            code_fragment.stage_ifdef_depth[Stage::Fragment] = 0xffffffff;
                            code_fragment.current_stage = Stage::Count;
                        }
                        else if ( code_fragment.stage_ifdef_depth[Stage::Compute] == code_fragment.ifdef_depth ) {

                            code_fragment.stage_ifdef_depth[Stage::Compute] = 0xffffffff;
                            code_fragment.current_stage = Stage::Count;
                        }

                        --code_fragment.ifdef_depth;

                        return;
                    }
                    break;
                }
            }
        }
    }

    void uniformIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment ) {
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                case 'i':
                {
                    if ( expectKeyword( token.text, 7, "image2D" ) ) {
                        // Advance to next token to get the name
                        Token name_token;
                        nextToken( parser->lexer, name_token );

                        CodeFragment::Resource resource = { hydra::graphics::ResourceType::TextureRW, name_token.text };
                        code_fragment.resources.emplace_back( resource );
                    }
                    break;
                }

                case 's':
                {
                    if ( expectKeyword( token.text, 9, "sampler2D" ) ) {
                        // Advance to next token to get the name
                        Token name_token;
                        nextToken( parser->lexer, name_token );

                        CodeFragment::Resource resource = { hydra::graphics::ResourceType::Texture, name_token.text };
                        code_fragment.resources.emplace_back( resource );
                    }
                    break;
                }
            }
        }
    }

    //
    //
    Property::Type propertyTypeIdentifier( const Token& token ) {
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            // Parse one of the following types:
            // Float, Int, Range, Color, Vector, 1D, 2D, 3D, Volume, Unknown

            switch ( c ) {
                case '1':
                {
                    if ( expectKeyword( token.text, 2, "1D" ) ) {
                        return Property::Texture1D;
                    }
                    break;
                }
                case '2':
                {
                    if ( expectKeyword( token.text, 2, "2D" ) ) {
                        return Property::Texture2D;
                    }
                    break;
                }
                case '3':
                {
                    if ( expectKeyword( token.text, 2, "3D" ) ) {
                        return Property::Texture3D;
                    }
                    break;
                }
                case 'V':
                {
                    if ( expectKeyword( token.text, 6, "Volume" ) ) {
                        return Property::TextureVolume;
                    }
                    else if ( expectKeyword( token.text, 6, "Vector" ) ) {
                        return Property::Vector;
                    }
                    break;
                }
                case 'I':
                {
                    if ( expectKeyword( token.text, 3, "Int" ) ) {
                        return Property::Int;
                    }
                    break;
                }
                case 'R':
                {
                    if ( expectKeyword( token.text, 5, "Range" ) ) {
                        return Property::Range;
                    }
                    break;
                }
                case 'F':
                {
                    if ( expectKeyword( token.text, 5, "Float" ) ) {
                        return Property::Float;
                    }
                    break;
                }
                case 'C':
                {
                    if ( expectKeyword( token.text, 5, "Color" ) ) {
                        return Property::Color;
                    }
                    break;
                }
                default:
                {
                    return Property::Unknown;
                    break;
                }
            }
        }

        return Property::Unknown;
    }

    //
    //
    void resourceBindingIdentifier( Parser* parser, const Token& token, ResourceBinding& binding ) {

        Token other_token;

        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                case 'c':
                {
                    if ( expectKeyword( token.text, 7, "cbuffer" ) ) {
                        binding.type = hydra::graphics::ResourceType::Constants;
                        binding.start = 0;
                        binding.count = 1;

                        nextToken( parser->lexer, other_token );
                        copy( other_token.text, binding.name, 32 );
                        
                        nextToken( parser->lexer, other_token );

                        return;
                    }
                    break;
                }

                case 't':
                {
                    if ( expectKeyword( token.text, 9, "texture2D" ) ) {
                        binding.type = hydra::graphics::ResourceType::Texture;
                        binding.start = 0;
                        binding.count = 1;

                        nextToken( parser->lexer, other_token );

                        copy( other_token.text, binding.name, 32 );

                        return;
                    }
                    else if ( expectKeyword( token.text, 11, "texture2Drw" ) ) {
                        binding.type = hydra::graphics::ResourceType::TextureRW;
                        binding.start = 0;
                        binding.count = 1;

                        nextToken( parser->lexer, other_token );
                        nextToken( parser->lexer, other_token );

                        copy( other_token.text, binding.name, 32 );

                        return;
                    }

                    break;
                }
            }
        }
    }

    //
    //
    const CodeFragment* findCodeFragment( Parser* parser, const StringRef& name ) {
        const uint32_t code_fragments_count = (uint32_t)parser->code_fragments.size();
        for ( uint32_t i = 0; i < code_fragments_count; ++i ) {
            const CodeFragment* type = &parser->code_fragments[i];
            if ( equals( name, type->name ) ) {
                return type;
            }
        }
        return nullptr;
    }

    //
    //
    const ResourceList* findResourceList( Parser* parser, const StringRef& name ) {
        std::vector<const ResourceList*>& declared_resources = parser->shader.resource_lists;

        for ( size_t i = 0; i < declared_resources.size(); i++ ) {
            const ResourceList* list = declared_resources[i];
            if ( equals( name, list->name ) ) {
                return list;
            }
        }
        return nullptr;
    }


    //
    //
    void declarationShader( Parser* parser ) {
        // Parse name
        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        parser->shader.name = token.text;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

            identifier( parser, token );
        }
    }

    void declarationGlsl( Parser* parser ) {

        // Parse name
        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        CodeFragment code_fragment = {};
        // Cache name string
        code_fragment.name = token.text;

        for ( size_t i = 0; i < Stage::Count; i++ ) {
            code_fragment.stage_ifdef_depth[i] = 0xffffffff;
        }

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        // Advance token and cache the starting point of the code.
        nextToken( parser->lexer, token );
        code_fragment.code = token.text;

        uint32_t open_braces = 1;

        // Scan until close brace token
        while ( open_braces ) {

            if ( token.type == Token::Token_OpenBrace )
                ++open_braces;
            else if ( token.type == Token::Token_CloseBrace )
                --open_braces;

            // Parse hash for includes and defines
            if ( token.type == Token::Token_Hash ) {
                // Get next token and check which directive is
                nextToken( parser->lexer, token );

                directiveIdentifier( parser, token, code_fragment );
            }
            else if ( token.type == Token::Token_Identifier ) {

                // Parse uniforms to add resource dependencies if not explicit in the HFX file.
                if ( expectKeyword( token.text, 7, "uniform" ) ) {
                    nextToken( parser->lexer, token );

                    uniformIdentifier( parser, token, code_fragment );
                }
            }

            // Only advance token when we are inside the glsl braces, otherwise will skip the following glsl part.
            if ( open_braces )
                nextToken( parser->lexer, token );
        }
        
        // Calculate code string length using the token before the last close brace.
        code_fragment.code.length = token.text.text - code_fragment.code.text;
        
        parser->code_fragments.emplace_back( code_fragment );
    }

    void declarationPass( Parser* parser ) {

        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        Pass pass = {};
        // Cache name string
        pass.name = token.text;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {
            passIdentifier( parser, token, pass );
        }

        parser->passes.emplace_back( pass );
    }

    void declarationShaderStage( Parser* parser, Pass::ShaderStage& out_stage ) {

        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Equals ) ) {
            return;
        }

        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        out_stage.code = findCodeFragment( parser, token.text );
    }

    void declarationProperties( Parser* parser ) {
        Token token;
        
        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        uint32_t open_braces = 1;
        // Advance to next token to avoid reading the open brace again.
        nextToken( parser->lexer, token );

        // Scan until close brace token
        while ( open_braces ) {

            if ( token.type == Token::Token_OpenBrace )
                ++open_braces;
            else if ( token.type == Token::Token_CloseBrace )
                --open_braces;

            if ( token.type == Token::Token_Identifier ) {
                declarationProperty( parser, token.text );
            }

            // Only advance token when we are inside the glsl braces, otherwise will skip the following glsl part.
            if ( open_braces )
                nextToken( parser->lexer, token );
        }
    }

    //
    // Parse the declaration of a property with the syntax:
    //
    //   identifier(string, identifier[(arguments)]) [= default_value]
    //
    // Arguments are optional and enclosed in () and dictate the UI of the parameter.
    // Default_value is optional and depends on the type.
    //
    void declarationProperty( Parser* parser, const StringRef& name ) {
        Property* property = new Property();

        // Cache name
        property->name = name;

        Token token;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenParen ) ) {
            return;
        }
        
        // Advance to the string representing the ui_name
        if ( !expectToken( parser->lexer, token, Token::Token_String ) ) {
            return;
        }

        property->ui_name = token.text;
        
        if ( !expectToken( parser->lexer, token, Token::Token_Comma ) ) {
            return;
        }

        // Next is the identifier representing the type name
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Parse property type and convert it to an enum
        property->type = propertyTypeIdentifier( token );

        // If an open parenthesis is present, then parse the ui arguments.
        nextToken( parser->lexer, token );
        if ( token.type == Token::Token_OpenParen ) {
            property->ui_arguments = token.text;

            while ( !equalToken( parser->lexer, token, Token::Token_CloseParen ) ) {
            }

            // Advance to the last close parenthesis
            nextToken( parser->lexer, token );

            property->ui_arguments.length = token.text.text - property->ui_arguments.text;
        }

        if ( !checkToken( parser->lexer, token, Token::Token_CloseParen ) ) {
            return;
        }

        // Cache lexer status and advance to next token.
        // If the token is '=' then we parse the default value.
        // Otherwise backtrack by one token.
        Lexer cached_lexer = *parser->lexer;

        nextToken( parser->lexer, token );
        // At this point only the optional default value is missing, otherwise the parsing is over.
        if ( token.type == Token::Token_Equals ) {
            nextToken( parser->lexer, token );
            
            if ( token.type == Token::Token_Number ) {
                // Cache the data buffer entry index into the property for later retrieval.
                property->data_index = parser->lexer->data_buffer->current_entries - 1;
            }
            else {
                // Handle vectors, colors and non single number default values
            }
        }
        else {
            *parser->lexer = cached_lexer;
        }

        parser->shader.properties.push_back( property );
    }

    //
    //
    void declarationLayout( Parser* parser ) {
        Token token;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

            if ( token.type == Token::Token_Identifier ) {

                if ( expectKeyword( token.text, 4, "list" ) ) {

                    // Advane to next token
                    nextToken( parser->lexer, token );

                    ResourceList* resource_list = new ResourceList();
                    resource_list->name = token.text;

                    declarationResourceList( parser, *resource_list );

                    parser->shader.resource_lists.emplace_back( resource_list );
                }
            }
        }
    }

    //
    //
    void declarationResourceList( Parser* parser, ResourceList& resource_list ) {
        Token token;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

            if ( token.type == Token::Token_Identifier ) {
                ResourceBinding binding;
                resourceBindingIdentifier( parser, token, binding );
                resource_list.resources.emplace_back( binding );
            }
        }
    }

    void declarationPassResources( Parser* parser, Pass& pass ) {
        Token token;

        if ( !expectToken( parser->lexer, token, Token::Token_Equals ) ) {
            return;
        }

        nextToken( parser->lexer, token );
        
        // Now token contains the name of the resource list
        const ResourceList* resource_list = findResourceList( parser, token.text );
        if ( resource_list ) {
            pass.resource_lists.emplace_back( resource_list );
        }
        else {
            // Error
        }
    }


    //
    // CodeGenerator
    void initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size, uint32_t buffer_count ) {
        code_generator->parser = parser;
        code_generator->buffer_count = buffer_count;
        code_generator->string_buffers = new StringBuffer[buffer_count];

        for ( uint32_t i = 0; i < buffer_count; i++ ) {
            code_generator->string_buffers[i].init( buffer_size );
        }
    }

    //
    // Generate single files for each shader stage.
    void generateShaderPermutations( CodeGenerator* code_generator, const char* path ) {

        code_generator->string_buffers[0].clear();
        code_generator->string_buffers[1].clear();
        code_generator->string_buffers[2].clear();

        // For each pass and for each pass generate permutation file.
        const uint32_t pass_count = (uint32_t)code_generator->parser->passes.size();
        for ( uint32_t i = 0; i < pass_count; i++ ) {

            // Create one file for each code fragment
            const Pass& pass = code_generator->parser->passes[i];

            for ( size_t s = 0; s < pass.shader_stages.size(); ++s ) {
                outputShaderStage( code_generator, path, pass.shader_stages[s] );
            }
        }
    }
    
    // Additional data to be added to output shaders.
    // Vertex, Fragment, Geometry, Compute, Hull, Domain, Count
    static const char*              s_shader_file_extension[Stage::Count + 1] = { ".vert", ".frag", ".geo", ".comp", ".hul", ".dom", ".h" };
    static const char*              s_shader_stage_defines[Stage::Count + 1] = { "#define VERTEX\r\n", "#define FRAGMENT\r\n", "#define GEOMETRY\r\n", "#define COMPUTE\r\n", "#define HULL\r\n", "#define DOMAIN\r\n", "\r\n" };

    static void generateConstantsCode( const Shader& shader, StringBuffer& out_buffer ) {
        if ( !shader.properties.size() ) {
            return;
        }

        // Add the local constants into the code.
        out_buffer.append( "\n\t\tlayout (std140, binding=7) uniform LocalConstants {\n\n" );

        // For GPU the struct must be 16 bytes aligned. Track alignment
        uint32_t gpu_struct_alignment = 0;

        const std::vector<Property*>& properties = shader.properties;
        for ( size_t i = 0; i < shader.properties.size(); i++ ) {
            hfx::Property* property = shader.properties[i];

            switch ( property->type ) {
                case Property::Float:
                {
                    out_buffer.append( "\t\t\tfloat\t\t\t\t\t" );
                    out_buffer.append( property->name );
                    out_buffer.append( ";\n" );

                    ++gpu_struct_alignment;
                    break;
                }

                case Property::Int:
                {
                    break;
                }

                case Property::Range:
                {
                    break;
                }

                case Property::Color:
                {
                    break;
                }

                case Property::Vector:
                {
                    break;
                }
            }
        }

        uint32_t tail_padding_size = 4 - (gpu_struct_alignment % 4);
        out_buffer.append( "\t\t\tfloat\t\t\t\t\tpad_tail[%u];\n\n", tail_padding_size );
        out_buffer.append( "\t\t} local_constants;\n\n" );
    }

    //
    // Finalize and append code to a code buffer.
    // For embedded code (into binary HFX), prepend the stage and null terminate.
    static void appendFinalizedCode( const char* path, Stage stage, const CodeFragment* code_fragment, StringBuffer& filename_buffer, StringBuffer& code_buffer, bool embedded, const StringBuffer& constants_buffer ) {

        // Append small header: type
        if ( embedded )
            code_buffer.append( (char)stage );

        // Append includes for the current stage.
        for ( size_t i = 0; i < code_fragment->includes.size(); i++ ) {
            if ( code_fragment->includes_stage[i] != stage && code_fragment->includes_stage[i] != Stage::Count ) {
                continue;
            }

            // Open and read file
            filename_buffer.clear();
            filename_buffer.append( path );
            filename_buffer.append( code_fragment->includes[i] );
            char* include_code = ReadEntireFileIntoMemory( filename_buffer.data, nullptr );

            code_buffer.append( include_code );
            code_buffer.append( "\n\n" );
        }

        // Add the per stage define.
        code_buffer.append( "\n\t\t" );
        code_buffer.append( s_shader_stage_defines[stage] );

        // Append local constants
        code_buffer.append( constants_buffer );

        // Add the code straight from the HFX file.
        code_buffer.append( "\r\n\t\t" );
        code_buffer.append( code_fragment->code );

        if ( embedded )
            code_buffer.append( '\0' );
    }

    //
    //
    void outputShaderStage( CodeGenerator* code_generator, const char* path, const Pass::ShaderStage& stage ) {
        // Create file
        FILE* output_file;

        StringBuffer& filename_buffer = code_generator->string_buffers[0];
        filename_buffer.clear();
        filename_buffer.append( path );
        filename_buffer.append( code_generator->parser->shader.name );
        filename_buffer.append( "_" );
        filename_buffer.append( stage.code->name );
        filename_buffer.append( s_shader_file_extension[stage.stage] );
        fopen_s( &output_file, filename_buffer.data, "wb" );

        if ( !output_file ) {
            printf( "Error opening file. Aborting. \n" );
            return;
        }

        StringBuffer& code_buffer = code_generator->string_buffers[1];
        code_buffer.clear();

        // Generate the constants code to inject into the GLSL shader.
        // For now this will be the numerical properties with padding, in the future either that
        // or the manual layout.
        StringBuffer& constants_buffer = code_generator->string_buffers[2];
        constants_buffer.clear();
        generateConstantsCode( code_generator->parser->shader, constants_buffer );

        appendFinalizedCode( path, stage.stage, stage.code, filename_buffer, code_buffer, false, constants_buffer );

        fprintf( output_file, "%s", code_generator->string_buffers[1].data );

        fclose( output_file );
    }

    //
    //
    static void updateOffsetTable( uint32_t* current_shader_offset, uint32_t pass_header_size, StringBuffer& offset_buffer, StringBuffer& code_buffer ) {

        ShaderEffectFile::Chunk chunk = { *current_shader_offset, code_buffer.current_size - *current_shader_offset };
        offset_buffer.append( &chunk, sizeof( ShaderEffectFile::Chunk ) );

        *current_shader_offset = code_buffer.current_size + pass_header_size;
    }

    //
    //
    static void writeAutomaticResourcesLayout( const hfx::Pass& pass, StringBuffer& pass_buffer, uint32_t& pass_offset ) {

        using namespace hydra::graphics;

        // Add the local constant buffer obtained from all the properties in the layout.
        hydra::graphics::ResourceListLayoutCreation::Binding binding = { hydra::graphics::ResourceType::Constants, 0, 1, "LocalConstants" };
        pass_buffer.append( (void*)&binding, sizeof( hydra::graphics::ResourceListLayoutCreation::Binding) );
        pass_offset += sizeof( hydra::graphics::ResourceListLayoutCreation::Binding );

        for ( size_t s = 0; s < pass.shader_stages.size(); ++s ) {
            const Pass::ShaderStage shader_stage = pass.shader_stages[s];

            for ( size_t p = 0; p < shader_stage.code->resources.size(); p++ ) {
                const hfx::CodeFragment::Resource& resource = shader_stage.code->resources[p];

                switch ( resource.type ) {
                    case ResourceType::Texture:
                    {
                        copy( resource.name, binding.name, 32 );
                        binding.type = hydra::graphics::ResourceType::Texture;

                        pass_buffer.append( (void*)&binding, sizeof( hydra::graphics::ResourceListLayoutCreation::Binding ) );
                        pass_offset += sizeof( hydra::graphics::ResourceListLayoutCreation::Binding );
                        break;
                    }

                    case ResourceType::TextureRW:
                    {
                        copy( resource.name, binding.name, 32 );
                        binding.type = hydra::graphics::ResourceType::TextureRW;

                        pass_buffer.append( (void*)&binding, sizeof( hydra::graphics::ResourceListLayoutCreation::Binding ) );
                        pass_offset += sizeof( hydra::graphics::ResourceListLayoutCreation::Binding );
                        break;
                    }
                }
            }
        }
    }

    //
    //
    static void writeResourcesLayout( const hfx::Pass& pass, StringBuffer& pass_buffer, uint32_t& pass_offset ) {

        using namespace hydra::graphics;

        for ( size_t r = 0; r < pass.resource_lists.size(); ++r ) {
            const ResourceList* resource_list = pass.resource_lists[r];

            const uint32_t resources_count = (uint32_t)resource_list->resources.size();
            pass_buffer.append( (void*)resource_list->resources.data(), sizeof(ResourceBinding) * resources_count );
            pass_offset += sizeof( ResourceBinding ) * resources_count;
        }
    }

    //
    //
    void compileShaderEffectFile( CodeGenerator* code_generator, const char* path, const char* filename ) {
        // Create file
        FILE* output_file;

        StringBuffer& filename_buffer = code_generator->string_buffers[0];
        filename_buffer.clear();
        filename_buffer.append( path );
        filename_buffer.append( filename );
        fopen_s( &output_file, filename_buffer.data, "wb" );

        if ( !output_file ) {
            printf( "Error opening file. Aborting. \n" );
            return;
        }

        // -----------------------------------------------------------------------------------------------------------------------------------
        // Shader Effect File Format
        // -----------------------------------------------------------------------------------------------------------------------------------
        // | Header     | Pass Offset List | Pass Section 0                                                                                    | Pass Section 1
        // -----------------------------------------------------------------------------------------------------------------------------------
        // |                               |                  Pass Header                     |                  Pass Data
        // -----------------------------------------------------------------------------------------------------------------------------------
        // |                               | Shaders count | Res Count | Res List Offset | name | Shader Offset+Count | Shader Code | Res List
        // -----------------------------------------------------------------------------------------------------------------------------------
        
        
        const uint32_t pass_count = (uint32_t)code_generator->parser->passes.size();
        
        ShaderEffectFile shader_effect_file;
        shader_effect_file.num_passes = pass_count;

        fwrite( &shader_effect_file, sizeof(ShaderEffectFile), 1, output_file );

        // Pass Section:
        // |                  Pass Header                     |
        // -------------------------------------------------------------------------------------------------
        // Shaders count | Res Count | Res List Offset | name | Shader Offset+Count | Shader Code | Res List
        // -------------------------------------------------------------------------------------------------
        StringBuffer& code_buffer = code_generator->string_buffers[1];
        StringBuffer& pass_offset_buffer = code_generator->string_buffers[2];
        StringBuffer& shader_offset_buffer = code_generator->string_buffers[3];
        StringBuffer& pass_buffer = code_generator->string_buffers[4];
        StringBuffer& constants_buffer = code_generator->string_buffers[5];
        
        pass_offset_buffer.clear();
        pass_buffer.clear();
        constants_buffer.clear();

        // Generate the constants code to inject into the GLSL shader.
        // For now this will be the numerical properties with padding, in the future either that
        // or the manual layout.
        generateConstantsCode( code_generator->parser->shader, constants_buffer );

        // Pass memory offset starts after header and list of passes offsets.
        uint32_t pass_offset = sizeof( ShaderEffectFile ) + sizeof(uint32_t) * pass_count;

        ShaderEffectFile::PassHeader pass_header;

        for ( uint32_t i = 0; i < pass_count; i++ ) {

            pass_offset_buffer.append( &pass_offset, sizeof( uint32_t ) );

            const Pass& pass = code_generator->parser->passes[i];
            const uint32_t pass_shader_stages = (uint32_t)pass.shader_stages.size();
            const uint32_t pass_header_size = pass_shader_stages * sizeof( ShaderEffectFile::Chunk ) + sizeof( ShaderEffectFile::PassHeader );
            uint32_t current_shader_offset = pass_header_size;
            
            shader_offset_buffer.clear();
            code_buffer.clear();

            const bool automatic_layout = isResourceLayoutAutomatic( code_generator->parser->shader );
            uint32_t total_resources = 0;

            for ( size_t s = 0; s < pass.shader_stages.size(); ++s ) {
                const Pass::ShaderStage shader_stage = pass.shader_stages[s];

                appendFinalizedCode( path, shader_stage.stage, shader_stage.code, filename_buffer, code_buffer, true, constants_buffer );
                updateOffsetTable( &current_shader_offset, pass_header_size, shader_offset_buffer, code_buffer );

                // Manually count resources for automatic layout.
                // This needs to be done on a per pass level.
                if ( automatic_layout ) {
                    for ( size_t p = 0; p < shader_stage.code->resources.size(); p++ ) {
                        const hfx::CodeFragment::Resource& resource = shader_stage.code->resources[p];

                        switch ( resource.type ) {
                            case hydra::graphics::ResourceType::TextureRW:
                            case hydra::graphics::ResourceType::Texture:
                            case hydra::graphics::ResourceType::Constants:
                            {
                                ++total_resources;
                                break;
                            }
                        }
                    }
                }
            }

            // Update pass offset to the resource list sub-section
            pass_offset += code_buffer.current_size + pass_header_size;

            // Add local constant buffer in the count only if automatic layout is needed.
            if ( automatic_layout ) {
                ++total_resources;
            }
            else {
                // Otherwise search the resource lists that are per pass and non per shader stage.
                for ( size_t i = 0; i < pass.resource_lists.size(); i++ ) {
                    total_resources += (uint32_t)pass.resource_lists[i]->resources.size();
                }
            }
            
            // Fill Pass Header
            copy( pass.name, pass_header.name, 32 );
            pass_header.num_shader_chunks = pass_shader_stages;
            pass_header.num_resources = total_resources;
            pass_header.resource_table_offset = code_buffer.current_size + pass_header_size;

            pass_buffer.append( (void*)&pass_header, sizeof( ShaderEffectFile::PassHeader ) );
            pass_buffer.append( shader_offset_buffer );
            pass_buffer.append( code_buffer );

            if ( automatic_layout ) {
                writeAutomaticResourcesLayout( pass, pass_buffer, pass_offset );
            }
            else {
                writeResourcesLayout( pass, pass_buffer, pass_offset );
            }
        }

        fwrite( pass_offset_buffer.data, pass_offset_buffer.current_size, 1, output_file );
        fwrite( pass_buffer.data, pass_buffer.current_size, 1, output_file );
        
        fclose( output_file );
    }

    //
    //
    char* getPassMemory( char* hfx_memory, uint32_t index ) {
        const uint32_t pass_offset = *(uint32_t*)(hfx_memory + sizeof( ShaderEffectFile ) + (index * sizeof( uint32_t )));
        return hfx_memory + pass_offset;
    }

    //
    // Helper method to create shader stages
    void getShaderCreation( uint32_t shader_count, char* pass_memory, uint32_t index, hydra::graphics::ShaderCreation::Stage* shader_creation ) {

        char* shader_offset_list_start = pass_memory + sizeof( ShaderEffectFile::PassHeader );
        const uint32_t shader_offset = *(uint32_t*)(shader_offset_list_start + (index * sizeof( ShaderEffectFile::Chunk )));
        char* shader_chunk_start = pass_memory + shader_offset;

        shader_creation->type = (hydra::graphics::ShaderStage::Enum)(*shader_chunk_start);
        shader_creation->code = (const char*)(shader_chunk_start + sizeof( hfx::ShaderEffectFile::ChunkHeader ));
    }

    //
    //
    void generateShaderResourceHeader( CodeGenerator* code_generator, const char* path ) {
        FILE* output_file;

        const hfx::Shader& shader = code_generator->parser->shader;

        code_generator->string_buffers[0].clear();
        code_generator->string_buffers[0].append( path );
        code_generator->string_buffers[0].append( shader.name );
        code_generator->string_buffers[0].append( ".h" );
        fopen_s( &output_file, code_generator->string_buffers[0].data, "wb" );

        if ( !output_file ) {
            printf( "Error opening file. Aborting. \n" );
            return;
        }

        code_generator->string_buffers[0].clear();
        code_generator->string_buffers[1].clear();
        code_generator->string_buffers[2].clear();
        code_generator->string_buffers[3].clear();

        // Alias string buffers for code clarity
        StringBuffer& cpu_constants = code_generator->string_buffers[0];
        StringBuffer& constants_ui = code_generator->string_buffers[1];
        StringBuffer& buffer_class = code_generator->string_buffers[2];
        StringBuffer& constants_ui_method = code_generator->string_buffers[3];

        // Beginning
        fprintf( output_file, "\n#pragma once\n#include <stdint.h>\n#include \"hydra_graphics.h\"\n\n// This file is autogenerated!\nnamespace " );

        fwrite( shader.name.text, shader.name.length, 1, output_file );
        fprintf( output_file, " {\n\n" );

        // Preliminary sections
        constants_ui.append( "struct LocalConstantsUI {\n\n" );
        
        cpu_constants.append( "struct LocalConstants {\n\n" );

        constants_ui_method.append("\tvoid reflectMembers() {\n");

        buffer_class.append( "struct LocalConstantsBuffer {\n\n\thydra::graphics::BufferHandle\tbuffer;\n" );
        buffer_class.append( "\tLocalConstants\t\t\t\t\tconstants;\n\tLocalConstantsUI\t\t\t\tconstantsUI;\n\n" );
        buffer_class.append( "\tvoid create( hydra::graphics::Device& device ) {\n\t\tusing namespace hydra;\n\n" );
        buffer_class.append( "\t\tgraphics::BufferCreation constants_creation = { graphics::BufferType::Constant, graphics::ResourceUsageType::Dynamic, sizeof( LocalConstants ), &constants, \"LocalConstants\" };\n" );
        buffer_class.append( "\t\tbuffer = device.create_buffer( constants_creation );\n\t}\n\n" );
        buffer_class.append( "\tvoid destroy( hydra::graphics::Device& device ) {\n\t\tdevice.destroy_buffer( buffer );\n\t}\n\n" );
        buffer_class.append( "\tvoid updateUI( hydra::graphics::Device& device ) {\n\t\t// Draw UI\n\t\tconstantsUI.reflectUI();\n\t\t// Update constants from UI\n" );
        buffer_class.append( "\t\thydra::graphics::MapBufferParameters map_parameters = { buffer.handle, 0, 0 };\n" );
        buffer_class.append( "\t\tLocalConstants* buffer_data = (LocalConstants*)device.map_buffer( map_parameters );\n\t\tif (buffer_data) {\n" );

        // For GPU the struct must be 16 bytes aligned. Track alignment
        uint32_t gpu_struct_alignment = 0;

        DataBuffer* data_buffer = code_generator->parser->lexer->data_buffer;
        // For each property write code
        for ( size_t i = 0; i < shader.properties.size(); i++ ) {
            hfx::Property* property = shader.properties[i];

            switch ( property->type ) {
                case Property::Float:
                {
                    constants_ui.append("\tfloat\t\t\t\t\t");
                    constants_ui.append( property->name );

                    cpu_constants.append( "\tfloat\t\t\t\t\t" );
                    cpu_constants.append( property->name );
                    
                    if ( property->data_index != 0xffffffff ) {
                        float value = 0.0f;
                        getData( data_buffer, property->data_index, value );
                        constants_ui.append( "\t\t\t\t= %ff", value );
                        cpu_constants.append( "\t\t\t\t= %ff", value );
                    }

                    constants_ui.append( ";\n" );

                    cpu_constants.append( ";\n" );

                    constants_ui_method.append("\t\tImGui::InputScalar( \"");
                    constants_ui_method.append( property->ui_name );
                    constants_ui_method.append( "\", ImGuiDataType_Float, &" );
                    constants_ui_method.append( property->name );
                    constants_ui_method.append( ");\n" );

                    // buffer_data->scale = constantsUI.scale;
                    buffer_class.append("\t\t\tbuffer_data->");
                    buffer_class.append( property->name );
                    buffer_class.append( " = constantsUI." );
                    buffer_class.append( property->name );
                    buffer_class.append( ";\n" );

                    ++gpu_struct_alignment;

                    break;
                }

                case Property::Int:
                {
                    break;
                }

                case Property::Range:
                {
                    break;
                }

                case Property::Color:
                {
                    break;
                }

                case Property::Vector:
                {
                    break;
                }
            }
        }

        // Post-property sections
        constants_ui.append( "\n" );

        constants_ui_method.append( "\t}\n\n" );
        constants_ui_method.append( "\tvoid reflectUI() {\n\t\tImGui::Begin( \"LocalConstants\" );\n" );
        constants_ui_method.append( "\t\treflectMembers();\n\t\tImGui::End();\n\t}\n\n" );
        constants_ui_method.append( "}; // struct LocalConstantsUI\n\n" );

        // Add tail padding data
        uint32_t tail_padding_size = 4 - (gpu_struct_alignment % 4);
        cpu_constants.append( "\tfloat\t\t\t\t\tpad_tail[%u];\n\n", tail_padding_size );

        cpu_constants.append( "}; // struct LocalConstants\n\n" );

        buffer_class.append( "\t\t\tdevice.unmap_buffer( map_parameters );\n\t\t}\n\t}\n}; // struct LocalConstantBuffer\n\n" );

        fwrite( constants_ui.data, constants_ui.current_size, 1, output_file );
        fwrite( constants_ui_method.data, constants_ui_method.current_size, 1, output_file );
        fwrite( cpu_constants.data, cpu_constants.current_size, 1, output_file );
        fwrite( buffer_class.data, buffer_class.current_size, 1, output_file );


        // End
        fprintf( output_file, "} // namespace " );
        fwrite( shader.name.text, shader.name.length, 1, output_file );
        fprintf( output_file, "\n\n" );

        fclose( output_file );
    }

    //
    //
    inline bool isResourceLayoutAutomatic( const Shader& shader ) {
        return shader.resource_lists.size() == 0;
    }

} // namespace hfx

