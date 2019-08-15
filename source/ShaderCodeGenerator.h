#pragma once

#include "Lexer.h"

namespace hfx {

    struct CodeFragment {

        enum Stage {
            Vertex = 0,
            Fragment,
            Compute,
            Common,
            Count
        };

        std::vector<StringRef>      includes;
        std::vector<Stage>          includes_stage;     // Used to separate which include is in which shader stage.

        StringRef                   name;
        StringRef                   code;
        Stage                       current_stage       = Common;
        uint32_t                    ifdef_depth         = 0;
        uint32_t                    stage_ifdef_depth[Count];
    }; // struct CodeFragment

    struct Pass {

        StringRef                   name;

        const CodeFragment*         vs                  = nullptr;
        const CodeFragment*         fs                  = nullptr;
        const CodeFragment*         cs                  = nullptr;
    }; // struct Pass

    struct Shader {

        StringRef                   name;

        std::vector<Pass*>          passes;
    };


    struct Parser {

        Lexer*                          lexer = nullptr;

        std::vector<CodeFragment>       code_fragments;
        std::vector<Pass>               passes;
        Shader                          shader;

    }; // struct ShaderEffectParser

    static void                         initParser( Parser* parser, Lexer* lexer );
    static void                         generateAST( Parser* parser );

    static const CodeFragment*          findCodeFragment( Parser* parser, const StringRef& name );

    static void                         identifier( Parser* parser, const Token& token );
    static void                         passIdentifier( Parser* parser, const Token& token, Pass& pass );
    static void                         directiveIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment );
    
    static void                         declarationShader( Parser* parser );
    static void                         declarationGlsl( Parser* parser );
    static void                         declarationPass( Parser* parser );
    static void                         declarationShaderStage( Parser* parser, const CodeFragment** out_fragment );

    struct CodeGenerator {
        
        const Parser*                   parser = nullptr;
        StringBuffer                    string_buffer_0;
        StringBuffer                    string_buffer_1;
        StringBuffer                    string_buffer_2;

    }; // struct CodeGenerator

    static void                         initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size );
    static void                         generateShaderPermutations( CodeGenerator* code_generator, const char* path );

    static void                         outputCodeFragment( CodeGenerator* code_generator, const char* path, CodeFragment::Stage stage, const CodeFragment* code_fragment );

    //
    // IMPLEMENTATION
    //

    void initParser( Parser* parser, Lexer* lexer ) {

        parser->lexer = lexer;

        parser->shader.name.length = 0;
        parser->shader.name.text = nullptr;
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

    inline void identifier( Parser* parser, const Token& token ) {

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
                    break;
                }

            }
        }
    }

    inline void passIdentifier( Parser* parser, const Token& token, Pass& pass ) {
        // Scan the name to know which 
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                
                case 'c':
                {
                    if ( expectKeyword( token.text, 7, "compute") ) {
                        declarationShaderStage( parser, &pass.cs );
                        return;
                    }
                    break;
                }

                case 'v':
                {
                    if ( expectKeyword( token.text, 6, "vertex" ) ) {
                        declarationShaderStage( parser, &pass.vs );
                        return;
                    }
                    break;
                }

                case 'f':
                {
                    if ( expectKeyword( token.text, 8, "fragment" ) ) {
                        declarationShaderStage( parser, &pass.fs );
                        return;
                    }
                    break;
                }
            }
        }
    }

    inline void directiveIdentifier( Parser* parser, const Token& token, CodeFragment& code_fragment ) {
        
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

                                code_fragment.stage_ifdef_depth[CodeFragment::Vertex] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = CodeFragment::Vertex;
                            }
                            else if ( expectKeyword( new_token.text, 8, "FRAGMENT" ) ) {

                                code_fragment.stage_ifdef_depth[CodeFragment::Fragment] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = CodeFragment::Fragment;
                            }
                            else if ( expectKeyword( new_token.text, 7, "COMPUTE" ) ) {

                                code_fragment.stage_ifdef_depth[CodeFragment::Compute] = code_fragment.ifdef_depth;
                                code_fragment.current_stage = CodeFragment::Compute;
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

                        if ( code_fragment.stage_ifdef_depth[CodeFragment::Vertex] == code_fragment.ifdef_depth ) {
                            
                            code_fragment.stage_ifdef_depth[CodeFragment::Vertex] = 0xffffffff;
                            code_fragment.current_stage = CodeFragment::Common;
                        }
                        else if ( code_fragment.stage_ifdef_depth[CodeFragment::Fragment] == code_fragment.ifdef_depth ) {

                            code_fragment.stage_ifdef_depth[CodeFragment::Fragment] = 0xffffffff;
                            code_fragment.current_stage = CodeFragment::Common;
                        }
                        else if ( code_fragment.stage_ifdef_depth[CodeFragment::Compute] == code_fragment.ifdef_depth ) {

                            code_fragment.stage_ifdef_depth[CodeFragment::Compute] = 0xffffffff;
                            code_fragment.current_stage = CodeFragment::Common;
                        }

                        --code_fragment.ifdef_depth;

                        return;
                    }
                    break;
                }
            }
        }
    }

    inline const CodeFragment* findCodeFragment( Parser* parser, const StringRef& name ) {
        const uint32_t code_fragments_count = (uint32_t)parser->code_fragments.size();
        for ( uint32_t i = 0; i < code_fragments_count; ++i ) {
            const CodeFragment* type = &parser->code_fragments[i];
            if ( equals( name, type->name ) ) {
                return type;
            }
        }
        return nullptr;
    }

    inline void declarationShader( Parser* parser ) {
        // Parse name
        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        StringRef name = token.text;

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

            identifier( parser, token );
        }
    }

    inline void declarationGlsl( Parser* parser ) {

        // Parse name
        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        CodeFragment code_fragment = {};
        // Cache name string
        code_fragment.name = token.text;

        for ( size_t i = 0; i < CodeFragment::Count; i++ ) {
            code_fragment.stage_ifdef_depth[i] = 0xffffffff;
        }

        if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        // Advance token and cache the starting point of the code.
        nextToken( parser->lexer, token );
        code_fragment.code = token.text;

        uint32_t open_braces = 1;

       // nextToken( parser->lexer, token );
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

            nextToken( parser->lexer, token );
        }
        
        // Calculate code string length
        code_fragment.code.length = token.text.text - code_fragment.code.text;
        
        parser->code_fragments.emplace_back( code_fragment );
    }

    inline void declarationPass( Parser* parser ) {

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

    inline void declarationShaderStage( Parser* parser, const CodeFragment** out_fragment ) {

        Token token;
        if ( !expectToken( parser->lexer, token, Token::Token_Equals ) ) {
            return;
        }

        if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        *out_fragment = findCodeFragment( parser, token.text );
    }


    //
    // CodeGenerator
    inline void initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size ) {
        code_generator->parser = parser;

        code_generator->string_buffer_0.init( buffer_size );
        code_generator->string_buffer_1.init( buffer_size );
        code_generator->string_buffer_2.init( buffer_size );
    }

    void generateShaderPermutations( CodeGenerator* code_generator, const char* path ) {

        code_generator->string_buffer_0.clear();
        code_generator->string_buffer_1.clear();
        code_generator->string_buffer_2.clear();

        // For each pass and for each pass generate permutation file.
        const uint32_t pass_count = (uint32_t)code_generator->parser->passes.size();
        for ( uint32_t i = 0; i < pass_count; i++ ) {

            // Create one file for each code fragment
            const Pass& pass = code_generator->parser->passes[i];
            
            if ( pass.cs ) {
                outputCodeFragment( code_generator, path, CodeFragment::Compute, pass.cs );
            }

            if ( pass.fs ) {
                outputCodeFragment( code_generator, path, CodeFragment::Fragment, pass.fs );
            }

            if ( pass.vs ) {
                outputCodeFragment( code_generator, path, CodeFragment::Vertex, pass.vs );
            }
        }
    }
    
    // Additional data to be added to output shaders.
    static const char*              s_shader_file_extension[CodeFragment::Count] = { ".vert", ".frag", ".compute", ".h" };
    static const char*              s_shader_stage_defines[CodeFragment::Count] = { "#define VERTEX\r\n", "#define FRAGMENT\r\n", "#define COMPUTE\r\n", "" };

    void outputCodeFragment( CodeGenerator* code_generator, const char* path, CodeFragment::Stage stage, const CodeFragment* code_fragment ) {
        // Create file
        FILE* output_file;

        code_generator->string_buffer_0.clear();
        code_generator->string_buffer_0.append( path );
        code_generator->string_buffer_0.append( code_fragment->name );
        code_generator->string_buffer_0.append( s_shader_file_extension[stage] );
        fopen_s( &output_file, code_generator->string_buffer_0.data, "wb" );

        if ( !output_file ) {
            printf( "Error opening file. Aborting. \n" );
            return;
        }

        code_generator->string_buffer_1.clear();

        // Append includes for the current stage.
        for ( size_t i = 0; i < code_fragment->includes.size(); i++ ) {
            if ( code_fragment->includes_stage[i] != stage && code_fragment->includes_stage[i] != CodeFragment::Common ) {
                continue;
            }

            // Open and read file
            code_generator->string_buffer_0.clear();
            code_generator->string_buffer_0.append( path );
            code_generator->string_buffer_0.append( code_fragment->includes[i] );
            char* include_code = ReadEntireFileIntoMemory( code_generator->string_buffer_0.data, nullptr );

            code_generator->string_buffer_1.append( include_code );
            code_generator->string_buffer_1.append( "\r\n" );
        }

        code_generator->string_buffer_1.append( "\t\t" );
        code_generator->string_buffer_1.append( s_shader_stage_defines[stage] );
        code_generator->string_buffer_1.append( "\r\n\t\t" );
        code_generator->string_buffer_1.append( code_fragment->code );

        fprintf( output_file, "%s", code_generator->string_buffer_1.data );

        fclose( output_file );
    }

} // namespace hfx

