#pragma once

#include "Lexer.h"

typedef hydra::StringRef StringRef;
typedef hydra::StringBuffer StringBuffer;

//
// Define the language specific structures.
namespace hdf {

    struct Type {

        enum Types {
            Types_Primitive, Types_Enum, Types_Struct, Types_Command, Types_None
        };

        enum PrimitiveTypes {
            Primitive_Int32, Primitive_Uint32, Primitive_Int16, Primitive_Uint16, Primitive_Int8, Primitive_Uint8, Primitive_Int64, Primitive_Uint64, Primitive_Float, Primitive_Double, Primitive_Bool, Primitive_None
        };

        Types                       type;
        PrimitiveTypes              primitive_type;
        StringRef                   name;

        std::vector<StringRef>      names;
        std::vector<const Type*>    types;
        //std::vector<Attribute> attributes;
        bool                        exportable = true;

    }; // struct Type


//
// The Parser parses Tokens using the Lexer and generate an Abstract Syntax Tree.
    struct Parser {

        Lexer*                          lexer = nullptr;

        Type*                           types = nullptr;
        uint32_t                        types_count = 0;
        uint32_t                        types_max = 0;

    }; // struct Parser

    static void                         init_parser( Parser* parser, Lexer* lexer, uint32_t max_types );
    static void                         generate_ast( Parser* parser );

    static void                         identifier( Parser* parser, const Token& token );
    static const Type*                  findType( Parser* parser, const StringRef& name );

    static void                         declaration_struct( Parser* parser );        // Follows the syntax 'struct name { (member)* };
    static void                         declaration_enum( Parser* parser );
    static void                         declaration_command( Parser* parser );
    static void                         declaration_variable( Parser* parser, const StringRef& type_name, Type& type );

    //
    // Given an AST the CodeGenerator will create the output code.
    struct CodeGenerator {

        const Parser*                   parser = nullptr;
        StringBuffer                    string_buffer_0;
        StringBuffer                    string_buffer_1;
        StringBuffer                    string_buffer_2;

        bool                            generate_imgui = false;

    }; // struct CodeGenerator

    static void                         init_code_generator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size );
    static void                         generate_code( CodeGenerator* code_generator, const char* filename );

    static void                         output_cpp_struct( CodeGenerator* code_generator, FILE* output, const Type& type );
    static void                         output_cpp_enum( CodeGenerator* code_generator, FILE* output, const Type& type );
    static void                         output_cpp_command( CodeGenerator* code_generator, FILE* output, const Type& type );

    //
    // Implementation ///////////////////////////////////////////////////////////
    //

    //
    // Parser
    // Allocate the string in one contiguous - and add the string length as a prefix (like a run length encoding)
    static const char*          s_primitive_types_names = "6int32 7uint32 6int16 7uint16 5int8 6uint8 6int64 7uint64 6float 7double 5bool";

    void init_parser( Parser* parser, Lexer* lexer, uint32_t max_types ) {

        parser->lexer = lexer;

        // Add primitive types
        parser->types_count = 11;
        parser->types_max = max_types;
        parser->types = new Type[max_types];

        // Use a single string with run-length encoding of names.
        char* names = (char*)s_primitive_types_names;

        for ( uint32_t i = 0; i < parser->types_count; ++i ) {
            Type& primitive_type = parser->types[i];

            primitive_type.type = Type::Types_Primitive;
            // Get the length encoded as first character of the name and remove the space (length - 1)
            const uint32_t length = names[0] - '0';
            primitive_type.name.length = length - 1;
            // Skip first character and let this string point into the master one.
            primitive_type.name.text = ++names;
            primitive_type.primitive_type = (Type::PrimitiveTypes)i;
            // Advance to next name
            names += length;
        }
    }

    //
    //
    void generate_ast( Parser* parser ) {

        // Read source text until the end.
        // The main body can be a list of declarations.
        bool parsing = true;

        while ( parsing ) {

            Token token;
            next_token( parser->lexer, token );

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

    //
    //
    inline void identifier( Parser* parser, const Token& token ) {

        // Scan the name to know which 
        for ( uint32_t i = 0; i < token.text.length; ++i ) {
            char c = *(token.text.text + i);

            switch ( c ) {
                case 's':
                {
                    if ( expect_keyword( token.text, 6, "struct" ) ) {
                        declaration_struct( parser );
                        return;
                    }

                    break;
                }

                case 'e':
                {
                    if ( expect_keyword( token.text, 4, "enum" ) ) {
                        declaration_enum( parser );
                        return;
                    }
                    break;
                }

                case 'c':
                {
                    if ( expect_keyword( token.text, 7, "command" ) ) {
                        declaration_command( parser );
                        return;
                    }
                    break;
                }
            }
        }
    }

    //
    //
    inline const Type* findType( Parser* parser, const StringRef& name ) {

        for ( uint32_t i = 0; i < parser->types_count; ++i ) {
            const Type* type = &parser->types[i];
            if ( equals( name, type->name ) ) {
                return type;
            }
        }
        return nullptr;
    }

    //
    //
    inline void declaration_struct( Parser* parser ) {
        // name
        Token token;
        if ( !expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        StringRef name = token.text;

        if ( !expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        // Add new type
        Type& type = parser->types[parser->types_count++];
        type.name = name;
        type.type = Type::Types_Struct;
        type.exportable = true;

        // Parse struct internals
        while ( !equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

            if ( token.type == Token::Token_Identifier ) {
                declaration_variable( parser, token.text, type );
            }
        }
    }

    //
    //
    inline void declaration_variable( Parser* parser, const StringRef& type_name, Type& type ) {
        const Type* variable_type = findType( parser, type_name );
        Token token;
        // Name
        if ( !expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        StringRef name = token.text;

        if ( !expect_token( parser->lexer, token, Token::Token_Semicolon ) ) {
            return;
        }

        type.types.emplace_back( variable_type );
        type.names.emplace_back( name );
    }

    //
    //
    inline void declaration_enum( Parser* parser ) {
        Token token;
        // Name
        if ( !expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        StringRef name = token.text;

        // Optional ': type' for the enum
        next_token( parser->lexer, token );
        if ( token.type == Token::Token_Colon ) {
            // Skip to open brace
            next_token( parser->lexer, token );
            // Token now contains type_name
            next_token( parser->lexer, token );
            // Token now contains open brace.
        }

        if ( token.type != Token::Token_OpenBrace ) {
            return;
        }

        // Add new type
        Type& type = parser->types[parser->types_count++];
        type.name = name;
        type.type = Type::Types_Enum;
        type.exportable = true;

        // Parse struct internals
        while ( !equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

            if ( token.type == Token::Token_Identifier ) {
                type.names.emplace_back( token.text );
            }
        }
    }

    //
    //
    inline void declaration_command( Parser* parser ) {
        // name
        Token token;
        if ( !expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
            return;
        }

        // Cache name string
        StringRef name = token.text;

        if ( !expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
            return;
        }

        // Add new type
        Type& command_type = parser->types[parser->types_count++];
        command_type.name = name;
        command_type.type = Type::Types_Command;
        command_type.exportable = true;

        // Parse struct internals
        while ( !equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

            if ( token.type == Token::Token_Identifier ) {
                // Create a new type for each command
                // Add new type
                Type& type = parser->types[parser->types_count++];
                type.name = token.text;
                type.type = Type::Types_Struct;
                type.exportable = false;

                while ( !equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {
                    if ( token.type == Token::Token_Identifier ) {
                        declaration_variable( parser, token.text, type );
                    }
                }

                command_type.names.emplace_back( type.name );
                command_type.types.emplace_back( &type );
            }
        }
    }

    //
    // CodeGenerator methods ////////////////////////////////////////////////////
    //

    static const char* s_primitive_type_cpp[] = { "int32_t", "uint32_t", "int16_t", "uint16_t", "int8_t", "uint8_t", "int64_t", "uint64_t", "float", "double", "bool" };
    static const char* s_primitive_type_imgui[] = { "ImGuiDataType_S32", "ImGuiDataType_U32", "ImGuiDataType_S16", "ImGuiDataType_U16", "ImGuiDataType_S8", "ImGuiDataType_U8", "ImGuiDataType_S64", "ImGuiDataType_U64", "ImGuiDataType_Float", "ImGuiDataType_Double" };

    //
    //
    inline void init_code_generator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size ) {
        code_generator->parser = parser;

        code_generator->string_buffer_0.init( buffer_size );
        code_generator->string_buffer_1.init( buffer_size );
        code_generator->string_buffer_2.init( buffer_size );
    }

    //
    //
    void generate_code( CodeGenerator* code_generator, const char* filename ) {

        // Create file
        FILE* output_file;
        fopen_s( &output_file, filename, "w" );

        if ( !output_file ) {
            printf( "Error opening file. Aborting. \n" );
            return;
        }

        fprintf( output_file, "\n#pragma once\n#include <stdint.h>\n\n// This file is autogenerated!\n" );

        // Output all the types needed
        const Parser& parser = *code_generator->parser;
        for ( uint32_t i = 0; i < parser.types_count; ++i ) {
            const Type& type = parser.types[i];

            if ( !type.exportable )
                continue;

            switch ( type.type ) {
                case Type::Types_Struct:
                {
                    output_cpp_struct( code_generator, output_file, type );
                    break;
                }

                case Type::Types_Enum:
                {
                    output_cpp_enum( code_generator, output_file, type );
                    break;
                }

                case Type::Types_Command:
                {
                    output_cpp_command( code_generator, output_file, type );
                    break;
                }
            }
        }

        fclose( output_file );
    }

    //
    //
    void output_cpp_struct( CodeGenerator* code_generator, FILE* output, const Type& type ) {
        const char* tabs = "";

        code_generator->string_buffer_0.clear();
        code_generator->string_buffer_1.clear();
        code_generator->string_buffer_2.clear();

        StringBuffer& ui_code = code_generator->string_buffer_0;

        char name_buffer[256], member_name_buffer[256], member_type_buffer[256];
        copy( type.name, name_buffer, 256 );

        if ( code_generator->generate_imgui ) {
            ui_code.append( "\n\tvoid reflectMembers() {\n" );
        }

        fprintf( output, "%sstruct %s {\n\n", tabs, name_buffer );

        for ( int i = 0; i < type.types.size(); ++i ) {
            const Type& member_type = *type.types[i];
            const StringRef& member_name = type.names[i];

            copy( member_name, member_name_buffer, 256 );

            // Translate type name based on output language.
            switch ( member_type.type ) {
                case Type::Types_Primitive:
                {
                    strcpy_s( member_type_buffer, 256, s_primitive_type_cpp[member_type.primitive_type] );
                    fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );

                    if ( code_generator->generate_imgui ) {
                        switch ( member_type.primitive_type ) {
                            case Type::Primitive_Int8:
                            case Type::Primitive_Uint8:
                            case Type::Primitive_Int16:
                            case Type::Primitive_Uint16:
                            case Type::Primitive_Int32:
                            case Type::Primitive_Uint32:
                            case Type::Primitive_Int64:
                            case Type::Primitive_Uint64:
                            case Type::Primitive_Float:
                            case Type::Primitive_Double:
                            {
                                ui_code.append( "\t\tImGui::InputScalar( \"%s\", %s, &%s );\n", member_name_buffer, s_primitive_type_imgui[member_type.primitive_type], member_name_buffer );

                                break;
                            }

                            case Type::Primitive_Bool:
                            {
                                ui_code.append( "\t\tImGui::Checkbox( \"%s\", &%s );\n", member_name_buffer, member_name_buffer );
                                break;
                            }
                        }
                    }

                    break;
                }

                case Type::Types_Struct:
                {
                    copy( member_type.name, member_type_buffer, 256 );
                    fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );

                    if ( code_generator->generate_imgui ) {
                        ui_code.append( "\t\tImGui::Text(\"%s\");\n", member_name_buffer );
                        ui_code.append( "\t\t%s.reflectMembers();\n", member_name_buffer );
                    }

                    break;
                }

                case Type::Types_Enum:
                {
                    copy( member_type.name, member_type_buffer, 256 );
                    fprintf( output, "%s\t%s::Enum %s;\n", tabs, member_type_buffer, member_name_buffer );

                    if ( code_generator->generate_imgui ) {
                        ui_code.append( "\t\tImGui::Combo( \"%s\", (int32_t*)&%s, %s::s_value_names, %s::Count );\n", member_name_buffer, member_name_buffer, member_type_buffer, member_type_buffer );
                    }

                    break;
                }

                default:
                {
                    break;
                }
            }
        }

        ui_code.append( "\t}" );
        ui_code.append( "\n\n\tvoid reflectUI() {\n\t\tImGui::Begin(\"%s\");\n\t\treflectMembers();\n\t\tImGui::End();\n\t}\n", name_buffer );

        fprintf( output, "%s\n", ui_code.data );

        fprintf( output, "\n%s}; // struct %s\n\n", tabs, name_buffer );
    }

    //
    //
    void output_cpp_enum( CodeGenerator* code_generator, FILE* output, const Type& type ) {

        // Empty enum: skip output.
        if ( type.names.size() == 0 )
            return;

        code_generator->string_buffer_0.clear();
        code_generator->string_buffer_1.clear();
        code_generator->string_buffer_2.clear();

        StringBuffer& values = code_generator->string_buffer_0;
        StringBuffer& value_names = code_generator->string_buffer_1;
        StringBuffer& value_masks = code_generator->string_buffer_2;

        value_names.append( "\"" );

        bool add_max = true;
        bool add_mask = true;

        char name_buffer[256];

        // Enums with more than 1 values
        if ( type.names.size() > 1 ) {
            const uint32_t max_values = (uint32_t)type.names.size() - 1;
            for ( uint32_t v = 0; v < max_values; ++v ) {

                if ( add_mask ) {
                    value_masks.append( type.names[v] );
                    value_masks.append( "_mask = 1 << " );
                    value_masks.append( _itoa( v, name_buffer, 10 ) );
                    value_masks.append( ", " );
                }

                values.append( type.names[v] );
                values.append( ", " );

                value_names.append( type.names[v] );
                value_names.append( "\", \"" );
            }

            if ( add_mask ) {
                value_masks.append( type.names[max_values] );
                value_masks.append( "_mask = 1 << " );
                value_masks.append( _itoa( max_values, name_buffer, 10 ) );
            }

            values.append( type.names[max_values] );

            value_names.append( type.names[max_values] );
            value_names.append( "\"" );
        }
        else {

            if ( add_mask ) {
                value_masks.append( type.names[0] );
                value_masks.append( "_mask = 1 << " );
                value_masks.append( _itoa( 0, name_buffer, 10 ) );
            }

            values.append( type.names[0] );

            value_names.append( type.names[0] );
            value_names.append( "\"" );
        }

        if ( add_max ) {
            values.append( ", Count" );

            value_names.append( ", \"Count\"" );

            if ( add_mask ) {
                value_masks.append( ", Count_mask = 1 << " );
                value_masks.append( _itoa( (int32_t)type.names.size(), name_buffer, 10 ) );
            }
        }

        copy( type.name, name_buffer, 256 );

        fprintf( output, "namespace %s {\n", name_buffer );

        fprintf( output, "\tenum Enum {\n" );
        fprintf( output, "\t\t%s\n", values.data );
        fprintf( output, "\t};\n" );

        // Write the mask
        if ( add_mask ) {
            fprintf( output, "\n\tenum Mask {\n" );
            fprintf( output, "\t\t%s\n", value_masks.data );
            fprintf( output, "\t};\n" );
        }

        // Write the string values
        fprintf( output, "\n\tstatic const char* s_value_names[] = {\n" );
        fprintf( output, "\t\t%s\n", value_names.data );
        fprintf( output, "\t};\n" );

        fprintf( output, "\n\tstatic const char* ToString( Enum e ) {\n" );
        fprintf( output, "\t\treturn s_value_names[(int)e];\n" );
        fprintf( output, "\t}\n" );

        fprintf( output, "} // namespace %s\n\n", name_buffer );
    }

    //
    //
    void output_cpp_command( CodeGenerator* code_generator, FILE* output, const Type& type ) {

        char name_buffer[256], member_name_buffer[256], member_type_buffer[256];
        copy( type.name, name_buffer, 256 );

        fprintf( output, "namespace %s {\n", name_buffer );

        // Add enum with all types
        fprintf( output, "\tenum Type {\n" );
        fprintf( output, "\t\t" );
        for ( int i = 0; i < type.types.size() - 1; ++i ) {
            const Type& command_type = *type.types[i];
            copy( command_type.name, name_buffer, 256 );
            fprintf( output, "Type_%s, ", name_buffer );
        }

        const Type* last_type = type.types[type.types.size() - 1];
        copy( last_type->name, name_buffer, 256 );
        fprintf( output, "Type_%s", name_buffer );
        fprintf( output, "\n\t};\n\n" );

        const char* tabs = "\t";

        for ( int i = 0; i < type.types.size(); ++i ) {
            const Type& command_type = *type.types[i];

            copy( command_type.name, member_type_buffer, 256 );
            fprintf( output, "%sstruct %s {\n\n", tabs, member_type_buffer );

            for ( int j = 0; j < command_type.types.size(); ++j ) {
                const Type& member_type = *command_type.types[j];
                const StringRef& member_name = command_type.names[j];

                copy( member_name, member_name_buffer, 256 );

                // Translate type name based on output language.
                switch ( member_type.type ) {
                    case Type::Types_Primitive:
                    {
                        strcpy_s( member_type_buffer, 256, s_primitive_type_cpp[member_type.primitive_type] );
                        fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );


                        break;
                    }

                    case Type::Types_Struct:
                    {
                        copy( member_type.name, member_type_buffer, 256 );
                        fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );

                        break;
                    }

                    case Type::Types_Enum:
                    {
                        copy( member_type.name, member_type_buffer, 256 );
                        fprintf( output, "%s\t%s::Enum %s;\n", tabs, member_type_buffer, member_name_buffer );

                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }

            copy( command_type.name, member_type_buffer, 256 );

            fprintf( output, "\n%s\tstatic Type GetType() { return Type_%s; }\n", tabs, member_type_buffer );
            fprintf( output, "\n%s}; // struct %s\n\n", tabs, name_buffer );
        }

        copy( type.name, name_buffer, 256 );
        fprintf( output, "}; // namespace %s\n\n", name_buffer );
    }

} // namespace hdf

