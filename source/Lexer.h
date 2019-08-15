#pragma once


#include <stdio.h>
#include <vector>

//
// INTERFACE
//

// Simple string that references another one. Used to not allocate strings if not needed.
struct StringRef {

    size_t                          length;
    char*                           text;

}; // struct StringRef

bool                                equals( const StringRef& a, const StringRef& b );
void                                copy( const StringRef& a, char* buffer, uint32_t buffer_size );


//
// Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination.
struct StringBuffer {

    void                            init( uint32_t size );

    void                            append( const char* format, ... );
    void                            append( char c );
    void                            append( const StringRef& text );

    void                            clear();

    char*                           data                = nullptr;
    uint32_t                        buffer_size         = 1024;
    uint32_t                        current_size        = 0;

}; // struct StringBuffer

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

}; // struct Lexer

//
// Lexer-related methods
//
static void                         initLexer( Lexer* lexer, char* text );
static void                         nextToken( Lexer* lexer, Token& out_token );    // Retrieve the next token. Most importand method!
static void                         skipWhitespace( Lexer* lexer );

static bool                         equalToken( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check for token, no error if different
static bool                         expectToken( Lexer* lexer, Token& out_token, Token::Type expected_type );   // Expect a token, error if not present


//
// IMPLEMENTATION
//

//
// StringRef
inline bool equals( const StringRef& a, const StringRef& b ) {
    if ( a.length != b.length )
        return false;

    for ( uint32_t i = 0; i < a.length; ++i ) {
        if ( a.text[i] != b.text[i] ) {
            return false;
        }
    }

    return true;
}

inline void copy( const StringRef& a, char* buffer, uint32_t buffer_size ) {
    const uint32_t max_length = buffer_size - 1 < a.length ? buffer_size - 1 : a.length;
    memcpy( buffer, a.text, max_length );
    buffer[a.length] = 0;
}

//
// StringBuffer
void StringBuffer::init( uint32_t size ) {
    if ( data ) {
        free( data );
    }

    if ( size < 1 ) {
        printf( "ERROR: Buffer cannot be empty!\n" );
        return;
    }

    data = (char*)malloc( size + 1 );
    buffer_size = size;
    current_size = 0;
}

void StringBuffer::append( const char* format, ... ) {
    if ( current_size >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return;
    }

    va_list args;
    va_start( args, format );
    int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
    current_size += written_chars > 0 ? written_chars : 0;
    va_end( args );
}

void StringBuffer::append( char c ) {
    if ( current_size + 1 == buffer_size )
        return;

    data[current_size++] = c;
    data[current_size] = 0;
}

void StringBuffer::append( const StringRef& text ) {
    const uint32_t max_length = current_size + text.length < buffer_size ? text.length : buffer_size - current_size;
    if ( max_length == 0 || max_length >= buffer_size ) {
        printf( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[current_size], text.text, max_length );
    current_size += max_length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[current_size] = 0;
}

void StringBuffer::clear() {
    current_size = 0;
}

//
// Lexer
void initLexer( Lexer* lexer, char* text ) {
    lexer->position = text;
    lexer->line = 1;
    lexer->column = 0;
    lexer->error = false;
    lexer->error_line = 1;
}

inline bool IsEndOfLine( char c ) {
    bool Result = ((c == '\n') || (c == '\r'));
    return(Result);
}

inline bool IsWhitespace( char c ) {
    bool Result = ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f') || IsEndOfLine( c ));
    return(Result);
}

inline bool IsAlpha( char c ) {
    bool Result = (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
    return(Result);
}

inline bool IsNumber( char c ) {
    bool Result = ((c >= '0') && (c <= '9'));
    return(Result);
}

void nextToken( Lexer* lexer, Token& token ) {

    // Skip all whitespace first so that the token is without them.
    skipWhitespace( lexer );

    // Initialize token
    token.type = Token::Token_Unknown;
    token.text.text = lexer->position;
    token.text.length = 1;
    token.line = lexer->line;

    char c = lexer->position[0];
    ++lexer->position;

    switch ( c ) {
        case '\0':
        {
            token.type = Token::Token_EndOfStream;
        } break;
        case '(':
        {
            token.type = Token::Token_OpenParen;
        } break;
        case ')':
        {
            token.type = Token::Token_CloseParen;
        } break;
        case ':':
        {
            token.type = Token::Token_Colon;
        } break;
        case ';':
        {
            token.type = Token::Token_Semicolon;
        } break;
        case '*':
        {
            token.type = Token::Token_Asterisk;
        } break;
        case '[':
        {
            token.type = Token::Token_OpenBracket;
        } break;
        case ']':
        {
            token.type = Token::Token_CloseBracket;
        } break;
        case '{':
        {
            token.type = Token::Token_OpenBrace;
        } break;
        case '}':
        {
            token.type = Token::Token_CloseBrace;
        } break;
        case '=':
        {
            token.type = Token::Token_Equals;
        } break;
        case '#':
        {
            token.type = Token::Token_Hash;
        }break;

        case '"':
        {
            token.type = Token::Token_String;

            token.text.text = lexer->position;

            while ( lexer->position[0] &&
                    lexer->position[0] != '"' )
            {
                if ( (lexer->position[0] == '\\') &&
                     lexer->position[1] )
                {
                    ++lexer->position;
                }
                ++lexer->position;
            }

            token.text.length = lexer->position - token.text.text;
            if ( lexer->position[0] == '"' ) {
                ++lexer->position;
            }
        } break;

        default:
        {
            // Identifier/keywords
            if ( IsAlpha( c ) ) {
                token.type = Token::Token_Identifier;

                while ( IsAlpha( lexer->position[0] ) || IsNumber( lexer->position[0] ) || (lexer->position[0] == '_') ) {
                    ++lexer->position;
                }

                token.text.length = lexer->position - token.text.text;
            } // Numbers
            else if ( IsNumber( c ) ) {
                token.type = Token::Token_Number;
            }
            else {
                token.type = Token::Token_Unknown;
            }
        } break;
    }
}

void skipWhitespace( Lexer* lexer ) {
    // Scan text until whitespace is finished.
    for ( ;; ) {
        // Check if it is a pure whitespace first.
        if ( IsWhitespace( lexer->position[0] ) ) {
            // Handle change of line
            if ( IsEndOfLine( lexer->position[0] ) )
                ++lexer->line;

            // Advance to next character
            ++lexer->position;

        } // Check for single line comments ("//")
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '/') ) {
            lexer->position += 2;
            while ( lexer->position[0] && !IsEndOfLine( lexer->position[0] ) ) {
                ++lexer->position;
            }
        } // Check for c-style comments
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '*') ) {
            lexer->position += 2;

            // Advance until the string is closed. Remember to check if line is changed.
            while ( !((lexer->position[0] == '*') && (lexer->position[1] == '/')) ) {
                // Handle change of line
                if ( IsEndOfLine( lexer->position[0] ) )
                    ++lexer->line;

                // Advance to next character
                ++lexer->position;
            }

            if ( lexer->position[0] == '*' ) {
                lexer->position += 2;
            }
        }
        else {
            break;
        }
    }
}

inline bool equalToken( Lexer* lexer, Token& token, Token::Type expected_type ) {
    nextToken( lexer, token );
    return token.type == expected_type;
}

inline bool expectToken( Lexer* lexer, Token& token, Token::Type expected_type ) {
    if ( lexer->error )
        return true;

    nextToken( lexer, token );

    const bool error = token.type != expected_type;
    lexer->error = error;

    if ( error ) {
        // Save line of error
        lexer->error_line = lexer->line;
    }

    return !error;
}


//
// Utils

#define ArrayLength(array) ( sizeof(array)/sizeof((array)[0]) )

// Utility method
static char* ReadEntireFileIntoMemory( const char* fileName, size_t* size ) {
    char *text = 0;

    FILE *file = fopen( fileName, "rb" );

    if ( file ) {

        fseek( file, 0, SEEK_END );
        size_t filesize = ftell( file );
        fseek( file, 0, SEEK_SET );

        text = (char *)malloc( filesize + 1 );
        size_t result = fread( text, filesize, 1, file );
        text[filesize] = 0;

        if ( size )
            *size = filesize;

        fclose( file );
    }

    return(text);
}
