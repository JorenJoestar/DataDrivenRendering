#include "hydra_lexer.h"

#include <Windows.h>

//
// DataBuffer ///////////////////////////////////////////////////////////////////

//
//
void data_buffer_init( DataBuffer* data_buffer, uint32_t max_entries, uint32_t buffer_size ) {
    data_buffer->data = (char*)malloc( buffer_size );
    data_buffer->current_size = 0;
    data_buffer->buffer_size = buffer_size;

    data_buffer->entries = (DataBuffer::Entry*)malloc( sizeof( DataBuffer::Entry ) * max_entries );
    data_buffer->current_entries = 0;
    data_buffer->max_entries = max_entries;
}

//
//
void data_buffer_terminate( DataBuffer* data_buffer ) {
    free( data_buffer->data );
    free( data_buffer->entries );
}

//
//
void data_buffer_reset( DataBuffer* data_buffer ) {
    data_buffer->current_size = 0;
    data_buffer->current_entries = 0;
}

//
//
uint32_t data_buffer_add( DataBuffer* data_buffer, double data ) {

    if ( data_buffer->current_entries >= data_buffer->max_entries )
        return k_invalid_entry;

    if ( data_buffer->current_size + sizeof( double ) >= data_buffer->buffer_size )
        return k_invalid_entry;

    // Init entry
    DataBuffer::Entry& entry = data_buffer->entries[data_buffer->current_entries++];
    entry.offset = data_buffer->current_size;
    // Copy data
    memcpy( &data_buffer->data[data_buffer->current_size], &data, sizeof( double ) );
    data_buffer->current_size += sizeof( double );

    return data_buffer->current_entries - 1;
}

//
//
void data_buffer_get( const DataBuffer& data_buffer, uint32_t entry_index, float& value ) {
    value = 0.0f;
    if ( entry_index >= data_buffer.current_entries )
        return;

    const DataBuffer::Entry& entry = data_buffer.entries[entry_index];
    double* value_data = (double*)&data_buffer.data[entry.offset];

    value = (float)(*value_data);
}

//
// Lexer ////////////////////////////////////////////////////////////////////////

//
//
void lexer_init( Lexer* lexer, char* text, DataBuffer* data_buffer ) {
    lexer->position = text;
    lexer->line = 1;
    lexer->column = 0;
    lexer->error = false;
    lexer->error_line = 1;

    lexer->data_buffer = data_buffer;
    data_buffer_reset( data_buffer );
}

//
//
void lexer_terminate( Lexer* lexer ) {

}

//
//
static bool is_end_of_line( char c ) {
    bool Result = ((c == '\n') || (c == '\r'));
    return(Result);
}

//
//
static bool is_whitespace( char c ) {
    bool Result = ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f') || is_end_of_line( c ));
    return(Result);
}

//
//
static bool is_alpha( char c ) {
    bool Result = (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
    return(Result);
}

//
//
static bool is_number( char c ) {
    bool Result = ((c >= '0') && (c <= '9'));
    return(Result);
}

//
//
void lexer_next_token( Lexer* lexer, Token& token ) {

    // Skip all whitespace first so that the token is without them.
    lexer_skip_whitespace( lexer );

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
            if ( is_alpha( c ) ) {
                token.type = Token::Token_Identifier;

                while ( is_alpha( lexer->position[0] ) || is_number( lexer->position[0] ) || (lexer->position[0] == '_') ) {
                    ++lexer->position;
                }

                token.text.length = lexer->position - token.text.text;
            } // Numbers: handle also negative ones!
            else if ( is_number( c ) || c == '-' ) {
                // Backtrack to start properly parsing the number
                --lexer->position;
                lexer_parse_number( lexer );
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

//
//
void lexer_parse_number( Lexer* lexer ) {
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

        while ( *lexer->position != '.' && is_number( *lexer->position ) ) {

            decimal_part = (decimal_part * 10) + (*lexer->position - '0');

            ++lexer->position;
        }

    }
    // 4. Fractional part
    int32_t fractional_part = 0;
    int32_t fractional_divisor = 1;

    if ( *lexer->position == '.' ) {
        ++lexer->position;

        while ( is_number( *lexer->position ) ) {

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
    data_buffer_add( lexer->data_buffer, parsed_number );
}

//
//
void lexer_skip_whitespace( Lexer* lexer ) {
    // Scan text until whitespace is finished.
    for ( ;; ) {
        // Check if it is a pure whitespace first.
        if ( is_whitespace( lexer->position[0] ) ) {
            // Handle change of line
            if ( is_end_of_line( lexer->position[0] ) )
                ++lexer->line;

            // Advance to next character
            ++lexer->position;

        } // Check for single line comments ("//")
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '/') ) {
            lexer->position += 2;
            while ( lexer->position[0] && !is_end_of_line( lexer->position[0] ) ) {
                ++lexer->position;
            }
        } // Check for c-style comments
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '*') ) {
            lexer->position += 2;

            // Advance until the string is closed. Remember to check if line is changed.
            while ( !((lexer->position[0] == '*') && (lexer->position[1] == '/')) ) {
                // Handle change of line
                if ( is_end_of_line( lexer->position[0] ) )
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

//
//
bool lexer_equals_token( Lexer* lexer, Token& token, Token::Type expected_type ) {
    lexer_next_token( lexer, token );
    return token.type == expected_type;
}

//
//
bool lexer_expect_token( Lexer* lexer, Token& token, Token::Type expected_type ) {
    if ( lexer->error )
        return true;

    lexer_next_token( lexer, token );

    const bool error = token.type != expected_type;
    lexer->error = error;

    if ( error ) {
        // Save line of error
        lexer->error_line = lexer->line;
    }

    return !error;
}

//
//
bool lexer_check_token( Lexer* lexer, Token& token, Token::Type expected_type ) {
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

//
//
double lexer_get_float_from_string( char* text ) {

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

        while ( *text != '.' && is_number( *text ) ) {

            decimal_part = (decimal_part * 10) + (*text - '0');

            ++text;
        }

    }
    // 4. Fractional part
    int32_t fractional_part = 0;
    int32_t fractional_divisor = 1;

    if ( *text == '.' ) {
        ++text;

        while ( is_number( *text ) ) {

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
//
bool lexer_expect_keyword( const StringRef& text, uint32_t length, const char* expected_keyword ) {
    if ( text.length != length )
        return false;

    return memcmp( text.text, expected_keyword, length ) == 0;
}
