#pragma once

//
// Hydra Lexer v0.02
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/05/22, 18.50
//
//
// Revision history //////////////////////
//
//      0.02  (2021/06/10): + Updated to new HydraNext framework.
//      0.01  (2021/02/03): + Initial tracking of version. + Added lexer_goto_line and lexer_next_line. + Added possibility to use lexer without data_buffer.

#include "kernel/string.hpp"

typedef hydra::StringView           StringRef;

static const uint32_t               k_invalid_entry = 0xffffffff;
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


void                                data_buffer_init( DataBuffer* data_buffer, uint32_t max_entries, uint32_t buffer_size );
void                                data_buffer_terminate( DataBuffer* data_buffer );

void                                data_buffer_reset( DataBuffer* data_buffer );

uint32_t                            data_buffer_add( DataBuffer* data_buffer, double data );
void                                data_buffer_get_current( const DataBuffer& data_buffer, float& value );
void                                data_buffer_get( const DataBuffer& data_buffer, uint32_t entry_index, float& value );

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
// The role of the Lexer is to separate the input string into a list of Tokens.
struct Lexer {

    char*                           position            = nullptr;
    uint32_t                        line                = 0;
    uint32_t                        column              = 0;

    bool                            error               = false;
    uint32_t                        error_line          = 0;

    DataBuffer*                     data_buffer         = nullptr;

}; // struct Lexer

//
// Lexer-related methods //////////////////////////////////////////////////
//
void                                lexer_init( Lexer* lexer, char* text, DataBuffer* data_buffer );
void                                lexer_terminate( Lexer* lexer );

void                                lexer_next_token( Lexer* lexer, Token& out_token );    // Retrieve the next token. Most importand method!
void                                lexer_skip_whitespace( Lexer* lexer );
void                                lexer_parse_number( Lexer* lexer );

bool                                lexer_equals_token( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check for token, no error if different
bool                                lexer_expect_token( Lexer* lexer, Token& out_token, Token::Type expected_type );   // Advance to next token and expect a type, error if not present
bool                                lexer_check_token( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check the current token for errors.

double                              lexer_get_float_from_string( char* text );
bool                                lexer_expect_keyword( const StringRef& text, uint32_t length, const char* expected_keyword );

void                                lexer_goto_line( Lexer* lexer, i32 line );
void                                lexer_next_line( Lexer* lexer );

//
// Char-related methods ///////////////////////////////////////////////////

bool                                is_end_of_line( char c );
bool                                is_whitespace( char c );
bool                                is_alpha( char c );
bool                                is_number( char c );


//
// Utils //////////////////////////////////////////////////////////////////

#define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )


//
// Inlines ///////////////////////////////////////////////////////////////
//

//
//
inline bool is_end_of_line( char c ) {
    bool Result = ( ( c == '\n' ) || ( c == '\r' ) );
    return( Result );
}

//
//
inline bool is_whitespace( char c ) {
    bool Result = ( ( c == ' ' ) || ( c == '\t' ) || ( c == '\v' ) || ( c == '\f' ) || is_end_of_line( c ) );
    return( Result );
}

//
//
inline bool is_alpha( char c ) {
    bool Result = ( ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) );
    return( Result );
}

//
//
inline bool is_number( char c ) {
    bool Result = ( ( c >= '0' ) && ( c <= '9' ) );
    return( Result );
}

