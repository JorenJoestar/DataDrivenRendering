#pragma once


#include <stdio.h>
#include <vector>

//
// INTERFACE
//

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
    void                            append( void* memory, uint32_t size );
    void                            append( const StringBuffer& other_buffer );

    void                            clear();

    char*                           data                = nullptr;
    uint32_t                        buffer_size         = 1024;
    uint32_t                        current_size        = 0;

}; // struct StringBuffer

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


void                                initDataBuffer( DataBuffer* data_buffer, uint32_t max_entries, uint32_t buffer_size );
void                                reset( DataBuffer* data_buffer );

uint32_t                            addData( DataBuffer* data_buffer, double data );
void                                getData( DataBuffer* data_buffer, uint32_t entry_index, float& value );

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
void                                initLexer( Lexer* lexer, char* text, DataBuffer* data_buffer );
void                                nextToken( Lexer* lexer, Token& out_token );    // Retrieve the next token. Most importand method!
void                                skipWhitespace( Lexer* lexer );
void                                parseNumber( Lexer* lexer );

bool                                equalToken( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check for token, no error if different
bool                                expectToken( Lexer* lexer, Token& out_token, Token::Type expected_type );   // Advance to next token and expect a type, error if not present
bool                                checkToken( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check the current token for errors.

double                              parseNumber( char* text );

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

void StringBuffer::append( void* memory, uint32_t size ) {

    if ( current_size + size >= buffer_size )
        return;

    memcpy( &data[current_size], memory, size );
    current_size += size;
}

void StringBuffer::append( const StringBuffer& other_buffer ) {

    if ( other_buffer.current_size == 0 ) {
        return;
    }

    if ( current_size + other_buffer.current_size >= buffer_size ) {
        return;
    }

    memcpy( &data[current_size], other_buffer.data, other_buffer.current_size );
    current_size += other_buffer.current_size;
}

void StringBuffer::clear() {
    current_size = 0;
}

//
// DataBuffer
inline void initDataBuffer( DataBuffer* data_buffer, uint32_t max_entries, uint32_t buffer_size ) {
    data_buffer->data = (char*)malloc( buffer_size );
    data_buffer->current_size = 0;
    data_buffer->buffer_size = buffer_size;

    data_buffer->entries = (DataBuffer::Entry*)malloc( sizeof( DataBuffer::Entry ) * max_entries );
    data_buffer->current_entries = 0;
    data_buffer->max_entries = max_entries;
}

inline void reset( DataBuffer* data_buffer ) {
    data_buffer->current_size = 0;
    data_buffer->current_entries = 0;
}

inline uint32_t addData( DataBuffer* data_buffer, double data ) {

    if ( data_buffer->current_entries >= data_buffer->max_entries )
        return -1;

    if ( data_buffer->current_size + sizeof( double ) >= data_buffer->buffer_size )
        return -1;

    // Init entry
    DataBuffer::Entry& entry = data_buffer->entries[data_buffer->current_entries++];
    entry.offset = data_buffer->current_size;
    // Copy data
    memcpy( &data_buffer->data[data_buffer->current_size], &data, sizeof(double) );
    data_buffer->current_size += sizeof(double);

    return data_buffer->current_entries - 1;
}

inline void getData( DataBuffer* data_buffer, uint32_t entry_index, float& value ) {
    value = 0.0f;
    if ( entry_index >= data_buffer->current_entries )
        return;

    DataBuffer::Entry& entry = data_buffer->entries[entry_index];
    double* value_data = (double*)&data_buffer->data[entry.offset];

    value = (float)(*value_data);
}

//
// Lexer
void initLexer( Lexer* lexer, char* text, DataBuffer* data_buffer ) {
    lexer->position = text;
    lexer->line = 1;
    lexer->column = 0;
    lexer->error = false;
    lexer->error_line = 1;

    lexer->data_buffer = data_buffer;
    reset( data_buffer );
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
        case ',':
        {
            token.type = Token::Token_Comma;
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
            } // Numbers: handle also negative ones!
            else if ( IsNumber( c ) || c == '-' ) {
                // Backtrack to start properly parsing the number
                --lexer->position;
                parseNumber( lexer );
                // Update token and calculate correct length.
                token.type = Token::Token_Number;
                token.text.length = lexer->position - token.text.text;
            }
            else {
                token.type = Token::Token_Unknown;
            }
        } break;
    }
}

void parseNumber( Lexer* lexer ) {
    char c = lexer->position[0];

    // Parse the following literals:
    // 58, -58, 0.003, 4e2, 123.456e-67, 0.1E4f
    // 1. Sign detection
    int32_t sign = 1;
    if ( c == '-' ) {
        sign = -1;
        ++lexer->position;
    }
    
    // 2. Heading zeros (00.003)
    if ( *lexer->position == '0' ) {
        ++lexer->position;

        while ( *lexer->position == '0' )
            ++lexer->position;
    }
    // 3. Decimal part (until the point)
    int32_t decimal_part = 0;
    if ( *lexer->position > '0' && *lexer->position <= '9' ) {
        decimal_part = (*lexer->position - '0');
        ++lexer->position;

        while ( *lexer->position != '.' && IsNumber( *lexer->position ) ) {

            decimal_part = (decimal_part * 10) + (*lexer->position - '0');

            ++lexer->position;
        }

    }
    // 4. Fractional part
    int32_t fractional_part = 0;
    int32_t fractional_divisor = 1;

    if ( *lexer->position == '.' ) {
        ++lexer->position;

        while ( IsNumber( *lexer->position ) ) {

            fractional_part = (fractional_part * 10) + (*lexer->position - '0');
            fractional_divisor *= 10;

            ++lexer->position;
        }
    }

    // 5. Exponent (if present)
    if ( *lexer->position == 'e' || *lexer->position == 'E' ) {
        ++lexer->position;
    }

    double parsed_number = (double)sign * (decimal_part + ((double)fractional_part / fractional_divisor));
    addData( lexer->data_buffer, parsed_number );
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


inline bool checkToken( Lexer* lexer, Token& token, Token::Type expected_type ) {
    if ( lexer->error )
        return true;

    const bool error = token.type != expected_type;
    lexer->error = error;

    if ( error ) {
        // Save line of error
        lexer->error_line = lexer->line;
    }

    return !error;
}

inline double parseNumber( char* text ) {

    char c = text[0];

    // Parse the following literals:
    // 58, -58, 0.003, 4e2, 123.456e-67, 0.1E4f
    // 1. Sign detection
    int32_t sign = 1;
    if ( c == '-' ) {
        sign = -1;
        ++text;
    }

    // 2. Heading zeros (00.003)
    if ( *text == '0' ) {
        ++text;

        while ( *text == '0' )
            ++text;
    }
    // 3. Decimal part (until the point)
    int32_t decimal_part = 0;
    if ( *text > '0' && *text <= '9' ) {
        decimal_part = (*text - '0');
        ++text;

        while ( *text != '.' && IsNumber( *text ) ) {

            decimal_part = (decimal_part * 10) + (*text - '0');

            ++text;
        }

    }
    // 4. Fractional part
    int32_t fractional_part = 0;
    int32_t fractional_divisor = 1;

    if ( *text == '.' ) {
        ++text;

        while ( IsNumber( *text ) ) {

            fractional_part = (fractional_part * 10) + (*text - '0');
            fractional_divisor *= 10;

            ++text;
        }
    }

    // 5. Exponent (if present)
    if ( *text == 'e' || *text == 'E' ) {
        ++text;
    }

    double parsed_number = (double)sign * (decimal_part + ((double)fractional_part / fractional_divisor));
    return parsed_number;
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
