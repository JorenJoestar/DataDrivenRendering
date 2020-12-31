//
//  Hydra Data Format - v0.01

#include "hydra_data_format.h"
#include "hydra_lexer.h"

namespace hdf {

static const char* s_primitive_types_names = "6int32 7uint32 6int16 7uint16 5int8 6uint8 6int64 7uint64 6float 7double 5bool";

void Parser::init( Parser* parser, Lexer* lexer ) {

    parser->lexer = lexer;

    // Use a single string with run-length encoding of names.
    char* names = (char*)s_primitive_types_names;

    const uint32_t k_primitive_types = 11;
    for ( uint32_t i = 0; i < k_primitive_types; ++i ) {
        Type primitive_type {};

        primitive_type.type = Type::Types_Primitive;
        // Get the length encoded as first character of the name and remove the space (length - 1)
        const uint32_t length = names[0] - '0';
        primitive_type.name.length = length - 1;
        // Skip first character and let this string point into the master one.
        primitive_type.name.text = ++names;
        primitive_type.primitive_type = ( Type::PrimitiveTypes )i;
        // Advance to next name
        names += length;

        array_push( parser->types, primitive_type );
    }
}

void Parser::generate_ast( Parser* parser ) {

    // Read source text until the end.
        // The main body can be a list of declarations.
    bool parsing = true;

    while ( parsing ) {

        Token token;
        lexer_next_token( parser->lexer, token );

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

void Parser::identifier( Parser* parser, const Token& token ) {

    // Scan the name to know which 
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *( token.text.text + i );

        switch ( c ) {
            case 's':
            {
                if ( lexer_expect_keyword( token.text, 6, "struct" ) ) {
                    declaration_struct( parser );
                    return;
                }

                break;
            }

            case 'e':
            {
                if ( lexer_expect_keyword( token.text, 4, "enum" ) ) {
                    declaration_enum( parser );
                    return;
                }
                break;
            }

        }
    }
}

const Type* Parser::find_type( Parser* parser, const hydra::StringRef& name ) {

    return nullptr;
}

void Parser::declaration_struct( Parser* parser ) {

    // name
    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    // Add new type
    Type type {};
    type.name = name;
    type.type = Type::Types_Struct;
    type.exportable = true;

    // Parse struct internals
    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            declaration_variable( parser, token.text, type );
        }
    }

    array_push( parser->types, type );
}

void Parser::declaration_variable( Parser* parser, const hydra::StringRef& type_name, Type& type ) {
    const Type* variable_type = find_type( parser, type_name );
    Token token;
    // Name
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Semicolon ) ) {
        return;
    }

    array_push( type.types, variable_type );
    array_push( type.names, name );
}


void Parser::declaration_enum( Parser* parser ) {
    Token token;
    // Name
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    // Optional ': type' for the enum
    lexer_next_token( parser->lexer, token );
    if ( token.type == Token::Token_Colon ) {
        // Skip to open brace
        lexer_next_token( parser->lexer, token );
        // Token now contains type_name
        lexer_next_token( parser->lexer, token );
        // Token now contains open brace.
    }

    if ( token.type != Token::Token_OpenBrace ) {
        return;
    }

    // Add new type
    Type type {};
    type.name = name;
    type.type = Type::Types_Enum;
    type.exportable = true;

    // Parse struct internals
    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            array_push( type.names, token.text );
        }
    }

    array_push( parser->types, type );
}

// Code Generator ///////////////////////////

void CodeGenerator::init( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_count ) {

    code_generator->parser = parser;

    for ( size_t i = 0; i < buffer_count; i++ ) {
        //hydra::StringBuffer* string_buffer = new hydra::StringBuffer();
    }
}

void CodeGenerator::generate_code( CodeGenerator* code_generator, const char* filename ) {

    const Parser& parser = *code_generator->parser;
    const uint32_t types_count = array_size( parser.types );
    for ( uint32_t i = 0; i < types_count; ++i ) {
        const Type& type = parser.types[i];

        if ( !type.exportable )
            continue;

        switch ( type.type ) {
            case Type::Types_Struct:
            {
                //output_cpp_struct( code_generator, output_file, type );
                break;
            }

            case Type::Types_Enum:
            {
                //output_cpp_enum( code_generator, output_file, type );
                break;
            }

            case Type::Types_Command:
            {
                //output_cpp_command( code_generator, output_file, type );
                break;
            }
        }
    }
}

} // namespace hdf