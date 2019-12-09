#pragma once


#include <stdio.h>
#include <vector>

#include "hydra/hydra_lib.h"

typedef hydra::StringRef StringRef;

//
// INTERFACE
//

//
// DataBuffer class used to store data from the lexer. Used mostly for numbers.
struct DataBuffer {

    struct Entry {

        uint32_t                    offset              : 30;
        uint32_t                    type                : 2;

    }; // struct Entry

    Entry*                          entries             = nullptr;
    uint32_t                        max_entries         = 256;
    uint32_t                        current_entries     = 0;

    char*                           data                = nullptr;
    uint32_t                        buffer_size         = 1024;
    uint32_t                        current_size        = 0;

}; // struct DataBuffer


void                                init_data_buffer( DataBuffer* data_buffer, uint32_t max_entries, uint32_t buffer_size );
void                                terminate_data_buffer( DataBuffer* data_buffer );

void                                reset( DataBuffer* data_buffer );

uint32_t                            add_data( DataBuffer* data_buffer, double data );
void                                get_data( const DataBuffer& data_buffer, uint32_t entry_index, float& value );

//
// Token class, used to classify character groups.
struct Token {

    enum Type {
        Token_Unknown,

        Token_OpenParen,
        Token_CloseParen,
        Token_Colon,
        Token_Semicolon,
        Token_Asterisk,
        Token_OpenBracket,
        Token_CloseBracket,
        Token_OpenBrace,
        Token_CloseBrace,
        Token_OpenAngleBracket,
        Token_CloseAngleBracket,
        Token_Equals,
        Token_Hash,
        Token_Comma,

        Token_String,
        Token_Identifier,
        Token_Number,

        Token_EndOfStream,
    }; // enum Type

    Type                            type;
    StringRef                       text;
    uint32_t                        line;

}; // struct Token


//
// The role of the Lexer is to divide the input string into a list of Tokens.
struct Lexer {

    char*                           position            = nullptr;
    uint32_t                        line                = 0;
    uint32_t                        column              = 0;

    bool                            error               = false;
    uint32_t                        error_line          = 0;

    DataBuffer*                     data_buffer         = nullptr;

}; // struct Lexer

//
// Lexer-related methods
//
void                                init_lexer( Lexer* lexer, char* text, DataBuffer* data_buffer );
void                                next_token( Lexer* lexer, Token& out_token );    // Retrieve the next token. Most importand method!
void                                skip_whitespace( Lexer* lexer );
void                                parse_number( Lexer* lexer );

bool                                equals_token( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check for token, no error if different
bool                                expect_token( Lexer* lexer, Token& out_token, Token::Type expected_type );   // Advance to next token and expect a type, error if not present
bool                                check_token( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check the current token for errors.

double                              get_float_from_string( char* text );
bool                                expect_keyword( const StringRef& text, uint32_t length, const char* expected_keyword );


//
// Utils

#define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )

