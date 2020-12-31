
#pragma once

#include "hydra_lib.h"

//
//  Hydra Data Format - v0.01
//
//  Parser, code generator and serializer using Hydra Data Format schema.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2020/05/23, 23.36
//
//
// Revision history //////////////////////
//
//      0.01  (2020/05/23): + Initial version. Code coming from CodeGenerator.h file.


// Forward declarations
struct Lexer;
struct Token;

namespace hdf {

    //
    //
    struct Type {

        enum Types {
            Types_Primitive, Types_Enum, Types_Struct, Types_Command, Types_None
        };

        enum PrimitiveTypes {
            Primitive_Int32, Primitive_Uint32, Primitive_Int16, Primitive_Uint16, Primitive_Int8, Primitive_Uint8, Primitive_Int64, Primitive_Uint64, Primitive_Float, Primitive_Double, Primitive_Bool, Primitive_None
        };

        Types                       type;
        PrimitiveTypes              primitive_type;
        hydra::StringRef            name;

        array( hydra::StringRef )   names;
        array( const Type* )        types;
        //array(Attribute)          attributes;

        bool                        exportable          = true;

    }; // struct Type


    //
    //
    struct TypeMap {
        char*                       key;
        Type*                       value;
    }; // struct TypeMap


    //
    //
    struct Parser {

        Lexer*                      lexer               = nullptr;
        array( Type )               types;

        static void                 init( Parser* parser, Lexer* lexer );
        static void                 generate_ast( Parser* parser );

        static void                 identifier( Parser* parser, const Token& token );
        static const Type*          find_type( Parser* parser, const hydra::StringRef& name );

        static void                 declaration_struct( Parser* parser );
        static void                 declaration_enum( Parser* parser );
        static void                 declaration_variable( Parser* parser, const hydra::StringRef& type_name, Type& type );

    }; // struct Parser


    struct CodeGenerator {

        const Parser*               parser              = nullptr;

        array( hydra::StringBuffer* ) string_buffers;

        bool                        generate_imgui_code = false;

        static void                 init( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_count );
        static void                 generate_code( CodeGenerator* code_generator, const char* filename );


    }; // struct CodeGenerator


} // namespace hdf