// Hydra HFX v0.51

#include "hydra_shaderfx.h"

#include "kernel/numerics.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"
#include "kernel/process.hpp"
#include "kernel/file.hpp"
#include "kernel/blob_serialization.hpp"

#include "external/json.hpp"

// Customization /////////////////////////

#if defined (HYDRA_IMGUI)
    #include "imgui/imgui.h"
#endif // HYDRA_IMGUI


// Use shared Hydra library to enhance different functions.
//#include "hydra_lib.h"

#define HYDRA_LOG                                           hprint

#define HYDRA_ASSERT( condition, message, ... )             hy_assert( condition )

namespace hfx {

#if defined(HFX_COMPILER)

static const uint32_t                                       k_local_hfx_code_fragment_flag = 0x10;

void parser_init( Parser* parser, Lexer* lexer, hydra::Allocator* allocator, const char* source_path, const char* source_filename, const char* destination_path ) {

    parser->lexer = lexer;
    parser->allocator = allocator;

    parser->string_buffer.init(1024 * 16, allocator);
    strcpy( parser->source_path, source_path );
    strcpy( parser->source_filename, source_filename );
    strcpy( parser->destination_path, destination_path );

    parser->shader.name.length = 0;
    parser->shader.name.text = nullptr;
    parser->shader.passes.clear();
    parser->shader.properties.clear();
    parser->shader.resource_lists.clear();
    parser->shader.code_fragments.clear();
}

void parser_terminate( Parser* parser ) {
    parser->string_buffer.shutdown();
}

void parser_generate_ast( Parser* parser ) {

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

void identifier( Parser* parser, const Token& token ) {

    // Scan the name to know which 
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 's':
            {
                if ( lexer_expect_keyword( token.text, 6, "shader" ) ) {
                    declaration_shader( parser );
                    return;
                }
                else if ( lexer_expect_keyword( token.text, 14, "sampler_states" ) ) {
                    declaration_sampler_states( parser );
                    return;
                }

                break;
            }

            case 'g':
            {
                if ( lexer_expect_keyword( token.text, 4, "glsl" ) ) {
                    declaration_glsl( parser );
                    return;
                }
                break;
            }

            case 'p':
            {
                if ( lexer_expect_keyword( token.text, 4, "pass" ) ) {
                    declaration_pass( parser );
                    return;
                }
                else if ( lexer_expect_keyword( token.text, 10, "properties" ) ) {
                    declaration_properties( parser );
                    return;
                }
                else if ( lexer_expect_keyword( token.text, 8, "pipeline" ) ) {
                    declaration_pipeline( parser );
                    return;
                }

                break;
            }

            case 'l':
            {
                if ( lexer_expect_keyword( token.text, 6, "layout" ) ) {
                    declaration_layout( parser );
                    return;
                }
                break;
            }

            case 'i':
            {
                if ( lexer_expect_keyword( token.text, 8, "includes" ) ) {
                    declaration_includes( parser );
                    return;
                }
                break;
            }

            case 'r':
            {
                if ( lexer_expect_keyword( token.text, 13, "render_states" ) ) {
                    declaration_render_states( parser );
                    return;
                }
                break;
            }
        }
    }
}

void pass_identifier( Parser* parser, const Token& token, Pass& pass ) {
    // Scan the name to know which 
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {

            case 'c':
            {
                if ( lexer_expect_keyword( token.text, 7, "compute" ) ) {
                    Pass::ShaderStage stage = { nullptr, Stage::Compute };
                    declaration_shader_stage( parser, stage );

                    pass.shader_stages.emplace_back( stage );
                    return;
                }
                break;
            }

            case 'v':
            {
                if ( lexer_expect_keyword( token.text, 6, "vertex" ) ) {
                    Pass::ShaderStage stage = { nullptr, Stage::Vertex };
                    declaration_shader_stage( parser, stage );

                    pass.shader_stages.emplace_back( stage );
                    return;
                }
                else if ( lexer_expect_keyword( token.text, 13, "vertex_layout" ) ) {
                    declaration_pass_vertex_layout( parser, pass );
                    return;
                }
                break;
            }

            case 'f':
            {
                if ( lexer_expect_keyword( token.text, 8, "fragment" ) ) {
                    Pass::ShaderStage stage = { nullptr, Stage::Fragment };
                    declaration_shader_stage( parser, stage );

                    pass.shader_stages.emplace_back( stage );
                    return;
                }
                break;
            }

            case 'r':
            {
                if ( lexer_expect_keyword( token.text, 9, "resources" ) ) {
                    declaration_pass_resources( parser, pass );
                    return;
                }
                else if ( lexer_expect_keyword( token.text, 13, "render_states" ) ) {
                    declaration_pass_render_states( parser, pass );
                    return;
                }
                break;
            }

            case 's':
            {
                if ( lexer_expect_keyword( token.text, 5, "stage" ) ) {
                    declaration_pass_stage( parser, pass );
                    return;
                }
                break;
            }

            case 'o':
            {
                if ( lexer_expect_keyword( token.text, 7, "options" ) ) {
                    declaration_pass_options( parser, pass );
                    return;
                }

                break;
            }

            case 'd':
            {
                if ( lexer_expect_keyword( token.text, 8, "dispatch" ) ) {
                    declaration_pass_dispatch( parser, pass );
                    return;
                }
                break;
            }
        }
    }
}

void directive_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment ) {

    Token new_token;
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 'i':
            {
                // Search for the pattern 'if defined'
                if ( lexer_expect_keyword( token.text, 2, "if" ) ) {
                    lexer_next_token( parser->lexer, new_token );

                    if ( lexer_expect_keyword( new_token.text, 7, "defined" ) ) {
                        lexer_next_token( parser->lexer, new_token );

                        // Use 0 as not set value for the ifdef depth.
                        ++code_fragment.ifdef_depth;

                        if ( lexer_expect_keyword( new_token.text, 6, "VERTEX" ) ) {

                            code_fragment.stage_ifdef_depth[Stage::Vertex] = code_fragment.ifdef_depth;
                            code_fragment.current_stage = Stage::Vertex;
                        }
                        else if ( lexer_expect_keyword( new_token.text, 8, "FRAGMENT" ) ) {

                            code_fragment.stage_ifdef_depth[Stage::Fragment] = code_fragment.ifdef_depth;
                            code_fragment.current_stage = Stage::Fragment;
                        }
                        else if ( lexer_expect_keyword( new_token.text, 7, "COMPUTE" ) ) {

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
                if ( lexer_expect_keyword( token.text, 6, "pragma" ) ) {
                    lexer_next_token( parser->lexer, new_token );

                    if ( lexer_expect_keyword( new_token.text, 7, "include" ) ) {
                        lexer_next_token( parser->lexer, new_token );

                        CodeFragment::Include include;
                        include.filename = new_token.text;
                        include.stage_mask = ( u16 )code_fragment.current_stage;
                        include.declaration_line = parser->lexer->line + 1;
                        include.file_or_local = 0;

                        code_fragment.includes.emplace_back( include );
                    }
                    else if ( lexer_expect_keyword( new_token.text, 11, "include_hfx" ) ) {
                        lexer_next_token( parser->lexer, new_token );

                        CodeFragment::Include include;
                        include.filename = new_token.text;
                        include.stage_mask = ( u16 )code_fragment.current_stage;
                        include.declaration_line = parser->lexer->line + 1;
                        include.file_or_local = 1;

                        code_fragment.includes.emplace_back( include );
                    }

                    return;
                }
                break;
            }

            case 'e':
            {
                if ( lexer_expect_keyword( token.text, 5, "endif" ) ) {

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

//
//
void uniform_identifier( Parser* parser, const Token& token, CodeFragment& code_fragment ) {

    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 'i':
            {
                if ( lexer_expect_keyword( token.text, 7, "image1D" ) ||
                     lexer_expect_keyword( token.text, 7, "image2D" ) ||
                     lexer_expect_keyword( token.text, 7, "image3D" ) ) {
                    // Advance to next token to get the name
                    Token name_token;
                    lexer_next_token( parser->lexer, name_token );

                    CodeFragment::Resource resource = { hydra::gfx::ResourceType::ImageRW, name_token.text };
                    code_fragment.resources.emplace_back( resource );
                }
                break;
            }

            case 's':
            {
                if ( lexer_expect_keyword( token.text, 9, "sampler1D" ) ||
                     lexer_expect_keyword( token.text, 9, "sampler2D" ) ||
                     lexer_expect_keyword( token.text, 9, "sampler3D" ) ) {
                    // Advance to next token to get the name
                    Token name_token;
                    lexer_next_token( parser->lexer, name_token );

                    CodeFragment::Resource resource = { hydra::gfx::ResourceType::Texture, name_token.text };
                    code_fragment.resources.emplace_back( resource );
                }
                break;
            }
        }
    }
}

//
//
PropertyType property_type_identifier( const Token& token ) {

    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        // Parse one of the following types:
        // Float, Int, Range, Color, Vector, 1D, 2D, 3D, Volume, Unknown

        switch ( c ) {
            case '1':
            {
                if ( lexer_expect_keyword( token.text, 2, "1D" ) ) {
                    return PropertyType::Texture1D;
                }
                break;
            }
            case '2':
            {
                if ( lexer_expect_keyword( token.text, 2, "2D" ) ) {
                    return PropertyType::Texture2D;
                }
                break;
            }
            case '3':
            {
                if ( lexer_expect_keyword( token.text, 2, "3D" ) ) {
                    return PropertyType::Texture3D;
                }
                break;
            }
            case 'V':
            {
                if ( lexer_expect_keyword( token.text, 6, "Volume" ) ) {
                    return PropertyType::TextureVolume;
                }
                else if ( lexer_expect_keyword( token.text, 6, "Vector" ) ) {
                    return PropertyType::Vector;
                }
                break;
            }
            case 'I':
            {
                if ( lexer_expect_keyword( token.text, 3, "Int" ) ) {
                    return PropertyType::Int;
                }
                break;
            }
            case 'R':
            {
                if ( lexer_expect_keyword( token.text, 5, "Range" ) ) {
                    return PropertyType::Range;
                }
                break;
            }
            case 'F':
            {
                if ( lexer_expect_keyword( token.text, 5, "Float" ) ) {
                    return PropertyType::Float;
                }
                break;
            }
            case 'C':
            {
                if ( lexer_expect_keyword( token.text, 5, "Color" ) ) {
                    return PropertyType::Color;
                }
                break;
            }
            default:
            {
                return PropertyType::Unknown;
                break;
            }
        }
    }

    return PropertyType::Unknown;
}

//
//
void resource_binding_identifier( Parser* parser, const Token& token, ResourceBinding& binding, uint32_t flags ) {

    Token other_token;

    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 'c':
            {
                if ( lexer_expect_keyword( token.text, 7, "cbuffer" ) ) {
                    binding.type = hydra::gfx::ResourceType::Constants;
                    binding.start = u16_max;
                    binding.count = 1;

                    lexer_next_token( parser->lexer, other_token );
                    StringRef::copy_to( other_token.text, binding.name, 32 );

                    flags = find_property( parser, other_token.text ) ? 1 : 0;
                    // Skip next token - just variable name
                    lexer_next_token( parser->lexer, other_token );

                    return;
                }
                break;
            }

            case 't':
            {
                if ( lexer_expect_keyword( token.text, 9, "texture1D" ) ||
                     lexer_expect_keyword( token.text, 9, "texture2D" ) ||
                     lexer_expect_keyword( token.text, 9, "texture3D" ) ) {
                    binding.type = hydra::gfx::ResourceType::Texture;
                    binding.start = u16_max;
                    binding.count = 1;

                    lexer_next_token( parser->lexer, other_token );

                    StringRef::copy_to( other_token.text, binding.name, 32 );

                    flags = find_property( parser, other_token.text ) ? 1 : 0;

                    return;
                }
                else if ( lexer_expect_keyword( token.text, 11, "texture1Drw" ) ||
                          lexer_expect_keyword( token.text, 11, "texture2Drw" ) ||
                          lexer_expect_keyword( token.text, 11, "texture3Drw" ) ) {
                    binding.type = hydra::gfx::ResourceType::ImageRW;
                    binding.start = u16_max;
                    binding.count = 1;

                    lexer_next_token( parser->lexer, other_token );
                    lexer_next_token( parser->lexer, other_token );

                    StringRef::copy_to( other_token.text, binding.name, 32 );

                    flags = find_property( parser, other_token.text ) ? 1 : 0;

                    return;
                }

                break;
            }

            case 's':
            {
                if ( lexer_expect_keyword( token.text, 9, "sampler1D" ) || 
                     lexer_expect_keyword( token.text, 9, "sampler2D" ) ||
                     lexer_expect_keyword( token.text, 9, "sampler3D" ) ) {
                    binding.type = hydra::gfx::ResourceType::Sampler;
                    binding.start = u16_max;
                    binding.count = 1;

                    lexer_next_token( parser->lexer, other_token );

                    StringRef::copy_to( other_token.text, binding.name, 32 );

                    flags = find_property( parser, other_token.text ) ? 1 : 0;

                    return;
                }

                if ( lexer_expect_keyword( token.text, 7, "sbuffer" ) ) {
                    binding.type = hydra::gfx::ResourceType::StructuredBuffer;
                    binding.start = u16_max;
                    binding.count = 1;

                    lexer_next_token( parser->lexer, other_token );
                    StringRef::copy_to( other_token.text, binding.name, 32 );

                    flags = find_property( parser, other_token.text ) ? 1 : 0;
                    // Skip next token - just variable name
                    lexer_next_token( parser->lexer, other_token );

                    return;
                }
                break;
            }
        }
    }
}

//
//
void vertex_attribute_identifier( Parser* parser, Token& token, hydra::gfx::VertexAttribute& attribute ) {
    
    attribute.format = hydra::gfx::VertexComponentFormat::Count;

    // Parse Type
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 'f':
            {
                if ( lexer_expect_keyword( token.text, 6, "float4" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Float4;
                }
                else if ( lexer_expect_keyword( token.text, 6, "float3" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Float3;
                }
                else if ( lexer_expect_keyword( token.text, 6, "float2" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Float2;
                }
                else if ( lexer_expect_keyword( token.text, 5, "float" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Float;
                }

                break;
            }

            case 'b':
            {
                if ( lexer_expect_keyword( token.text, 4, "byte" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Byte;
                }
                else if ( lexer_expect_keyword( token.text, 6, "byte4n" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Byte4N;
                }

                break;
            }

            case 'u':
            {
                if ( lexer_expect_keyword( token.text, 5, "ubyte" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::UByte;
                }
                else if ( lexer_expect_keyword( token.text, 7, "ubyte4n" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::UByte4N;
                }
                else if ( lexer_expect_keyword( token.text, 4, "uint" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Uint;
                }

                break;
            }

            case 's':
            {
                if ( lexer_expect_keyword( token.text, 6, "short2" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Short2;
                }
                else if ( lexer_expect_keyword( token.text, 7, "short2n" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Short2N;
                }
                else if ( lexer_expect_keyword( token.text, 6, "short4" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Short4;
                }
                else if ( lexer_expect_keyword( token.text, 7, "short4n" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Short4N;
                }

                break;
            }

            case 'm':
            {
                if ( lexer_expect_keyword( token.text, 4, "mat4" ) ) {
                    attribute.format = hydra::gfx::VertexComponentFormat::Mat4;
                }

                break;
            }

            default:
            {
                //hprint( "Unsupported format %s\n", token.text );
                break;
            }
        }
    }

    if ( attribute.format == hydra::gfx::VertexComponentFormat::Count ) {
        // Error format not found!
    }

    // Goto next token
    lexer_next_token( parser->lexer, token );
    // Skip name
    lexer_next_token( parser->lexer, token );
    // Parse binding
    uint32_t data_index = parser->lexer->data_buffer->current_entries - 1;
    float value;
    data_buffer_get( *parser->lexer->data_buffer, data_index, value );

    attribute.binding = (uint16_t)value;

    lexer_next_token( parser->lexer, token );

    // Parse location
    data_index = parser->lexer->data_buffer->current_entries - 1;
    data_buffer_get( *parser->lexer->data_buffer, data_index, value );

    attribute.location = (uint16_t)value;

    lexer_next_token( parser->lexer, token );
    // Parse offset
    data_index = parser->lexer->data_buffer->current_entries - 1;
    data_buffer_get( *parser->lexer->data_buffer, data_index, value );

    attribute.offset = (uint16_t)value;
}

//
//
void vertex_binding_identifier( Parser* parser, Token& token, hydra::gfx::VertexStream& stream ) {

    // Parse binding
    float value;
    uint32_t data_index = parser->lexer->data_buffer->current_entries - 1;
    data_buffer_get( *parser->lexer->data_buffer, data_index, value );
    stream.binding = (uint16_t)value;

    // Parse stride
    lexer_next_token( parser->lexer, token );
    data_index = parser->lexer->data_buffer->current_entries - 1;
    data_buffer_get( *parser->lexer->data_buffer, data_index, value );
    stream.stride = (uint16_t)value;

    // Parse frequency (vertex or instance)
    lexer_next_token( parser->lexer, token );
    if ( lexer_expect_keyword( token.text, 6, "vertex" ) ) {
        stream.input_rate = hydra::gfx::VertexInputRate::PerVertex;
    } else if ( lexer_expect_keyword( token.text, 8, "instance" ) ) {
        stream.input_rate = hydra::gfx::VertexInputRate::PerInstance;
    }
}

//
//
const CodeFragment* find_code_fragment( const Parser* parser, const StringRef& name ) {
    const uint32_t code_fragments_count = (uint32_t)parser->shader.code_fragments.size();
    for ( uint32_t i = 0; i < code_fragments_count; ++i ) {
        const CodeFragment* type = &parser->shader.code_fragments[i];
        if ( StringRef::equals( name, type->name ) ) {
            return type;
        }
    }
    return nullptr;
}

//
//
const ResourceList* find_resource_list( const Parser* parser, const StringRef& name ) {
    const std::vector<const ResourceList*>& declared_resources = parser->shader.resource_lists;

    for ( size_t i = 0; i < declared_resources.size(); i++ ) {
        const ResourceList* list = declared_resources[i];
        if ( StringRef::equals( name, list->name ) ) {
            return list;
        }
    }
    return nullptr;
}

//
//
const Property* find_property( const Parser* parser, const StringRef& name ) {
    const std::vector<Property*>& properties = parser->shader.properties;
    for ( size_t i = 0; i < properties.size(); i++ ) {
        const Property* list = properties[i];
        if ( StringRef::equals( name, list->name ) ) {
            return list;
        }
    }
    return nullptr;
}

//
//
const VertexLayout* find_vertex_layout( const Parser* parser, const StringRef& name ) {
    const std::vector<const VertexLayout*>& layouts = parser->shader.vertex_layouts;
    for ( size_t i = 0; i < layouts.size(); i++ ) {
        const VertexLayout* layout = layouts[i];
        if ( StringRef::equals( name, layout->name ) ) {
            return layout;
        }
    }
    return nullptr;
}

const RenderState* find_render_state( const Parser* parser, const StringRef& name ) {
    const std::vector<const RenderState*>& render_states = parser->shader.render_states;
    for ( size_t i = 0; i < render_states.size(); i++ ) {
        const RenderState* render_state = render_states[i];
        if ( StringRef::equals( name, render_state->name ) ) {
            return render_state;
        }
    }
    return nullptr;
}

const SamplerState* find_sampler_state( const Parser* parser, const StringRef& name ) {
    const std::vector<const SamplerState*>& sampler_states = parser->shader.sampler_states;
    for ( size_t i = 0; i < sampler_states.size(); i++ ) {
        const SamplerState* state = sampler_states[i];
        if ( StringRef::equals( name, state->name ) ) {
            return state;
        }
    }
    return nullptr;
}


//
//
void declaration_shader( Parser* parser ) {
    // Parse name
    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    parser->shader.name = token.text;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        identifier( parser, token );
    }
}

//
//
void declaration_glsl( Parser* parser ) {

    // Parse name
    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    CodeFragment code_fragment = {};
    // Cache name string
    code_fragment.name = token.text;
    code_fragment.starting_file_line = parser->lexer->line + 1;

    for ( size_t i = 0; i < Stage::Count; i++ ) {
        code_fragment.stage_ifdef_depth[i] = 0xffffffff;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    // Advance token and cache the starting point of the code.
    lexer_next_token( parser->lexer, token );
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
            lexer_next_token( parser->lexer, token );

            directive_identifier( parser, token, code_fragment );
        }
        else if ( token.type == Token::Token_Identifier ) {

            // Parse uniforms to add resource dependencies if not explicit in the HFX file.
            if ( lexer_expect_keyword( token.text, 7, "uniform" ) ) {
                lexer_next_token( parser->lexer, token );

                uniform_identifier( parser, token, code_fragment );
            }
        }

        // Only advance token when we are inside the glsl braces, otherwise will skip the following glsl part.
        if ( open_braces )
            lexer_next_token( parser->lexer, token );
    }

    // Calculate code string length using the token before the last close brace.
    code_fragment.code.length = token.text.text - code_fragment.code.text;

    parser->shader.code_fragments.emplace_back( code_fragment );
}

//
//
void declaration_pass( Parser* parser ) {

    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    Pass pass = {};
    // Cache name string
    pass.name = token.text;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {
        pass_identifier( parser, token, pass );
    }

    parser->shader.passes.emplace_back( pass );
}

//
//
void declaration_pipeline( Parser* parser ) {
    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }
}

//
//
void declaration_shader_stage( Parser* parser, Pass::ShaderStage& out_stage ) {

    Token token;
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    out_stage.code = find_code_fragment( parser, token.text );
}

//
//
void declaration_properties( Parser* parser ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    uint32_t open_braces = 1;
    // Advance to next token to avoid reading the open brace again.
    lexer_next_token( parser->lexer, token );

    // Scan until close brace token
    while ( open_braces ) {

        if ( token.type == Token::Token_OpenBrace )
            ++open_braces;
        else if ( token.type == Token::Token_CloseBrace )
            --open_braces;

        if ( token.type == Token::Token_Identifier ) {
            declaration_property( parser, token.text );
        }

        // Only advance token when we are inside the glsl braces, otherwise will skip the following glsl part.
        if ( open_braces )
            lexer_next_token( parser->lexer, token );
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
void declaration_property( Parser* parser, const StringRef& name ) {
    Property* property = new Property();

    // Cache name
    property->name = name;

    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenParen ) ) {
        return;
    }

    // Advance to the string representing the ui_name
    if ( !lexer_expect_token( parser->lexer, token, Token::Token_String ) ) {
        return;
    }

    property->ui_name = token.text;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Comma ) ) {
        return;
    }

    // Next is the identifier representing the type name
    // There are 2 cases:
    // 1) Identifier
    // 2) Number+Identifier
    lexer_next_token( parser->lexer, token );
    if ( token.type == Token::Token_Number ) {
        Token number_token = token;
        lexer_next_token( parser->lexer, token );

        // Extend current token to include the number.
        token.text.text = number_token.text.text;
        token.text.length += number_token.text.length;
    }
    
    if ( token.type != Token::Token_Identifier ) {
        return;
    }
    
    // Parse property type and convert it to an enum
    property->type = property_type_identifier( token );

    // If an open parenthesis is present, then parse the ui arguments.
    lexer_next_token( parser->lexer, token );
    if ( token.type == Token::Token_OpenParen ) {
        property->ui_arguments = token.text;

        while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseParen ) ) {
        }

        // Advance to the last close parenthesis
        lexer_next_token( parser->lexer, token );

        property->ui_arguments.length = token.text.text - property->ui_arguments.text;
    }

    if ( !lexer_check_token( parser->lexer, token, Token::Token_CloseParen ) ) {
        return;
    }

    // Cache lexer status and advance to next token.
    // If the token is '=' then we parse the default value.
    // Otherwise backtrack by one token.
    Lexer cached_lexer = *parser->lexer;

    lexer_next_token( parser->lexer, token );
    // At this point only the optional default value is missing, otherwise the parsing is over.
    if ( token.type == Token::Token_Equals ) {
        lexer_next_token( parser->lexer, token );

        if ( token.type == Token::Token_Number ) {
            // Cache the data buffer entry index into the property for later retrieval.
            property->data_index = parser->lexer->data_buffer->current_entries - 1;
        }
        else if ( token.type == Token::Token_OpenParen ) {
            // TODO: Colors and Vectors
            // (number0, number1, ...)
        }
        else if ( token.type == Token::Token_String ) {
            // Texture.
            property->default_value = token.text;
        }
        else {
            // Error!
        }
    }
    else {
        *parser->lexer = cached_lexer;
    }

    parser->shader.properties.push_back( property );
}

//
//
void declaration_layout( Parser* parser ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 4, "list" ) ) {

                // Advance to next token
                lexer_next_token( parser->lexer, token );

                ResourceList* resource_list = new ResourceList();
                resource_list->name = token.text;

                declaration_resource_list( parser, *resource_list );

                parser->shader.resource_lists.emplace_back( resource_list );

                // Having at least one list declared, disable automatic list generation.
                parser->shader.has_local_resource_list = true;
            }
            else if ( lexer_expect_keyword( token.text, 6, "vertex" ) ) {

                lexer_next_token( parser->lexer, token );

                VertexLayout* vertex_layout = new VertexLayout();
                vertex_layout->name = token.text;

                declaration_vertex_layout( parser, *vertex_layout );

                parser->shader.vertex_layouts.emplace_back( vertex_layout );
            }
        }
    }
}

//
//
void declaration_resource_list( Parser* parser, ResourceList& resource_list ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            ResourceBinding binding;
            uint32_t flags = 0;
            resource_binding_identifier( parser, token, binding, flags );
            resource_list.resources.emplace_back( binding );
            resource_list.flags.emplace_back( flags );
        }
    }
}

//
//
void declaration_vertex_layout( Parser* parser, VertexLayout& vertex_layout ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 9, "attribute" ) ) {
                hydra::gfx::VertexAttribute vertex_attribute;
                
                // Advance to the token after the initial keyword.
                lexer_next_token( parser->lexer, token );

                vertex_attribute_identifier( parser, token, vertex_attribute );
                vertex_layout.attributes.emplace_back( vertex_attribute );
            }
            else if ( lexer_expect_keyword( token.text, 7, "binding" ) ) {
                hydra::gfx::VertexStream vertex_stream_binding;

                // Advance to the token after the initial keyword.
                lexer_next_token( parser->lexer, token );

                vertex_binding_identifier( parser, token, vertex_stream_binding );
                vertex_layout.streams.emplace_back( vertex_stream_binding );
            }
        }
    }
}


//
//
void declaration_render_states( Parser* parser ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 5, "state" ) ) {
                // Advance to next token
                lexer_next_token( parser->lexer, token );

                RenderState* render_state = new RenderState();
                render_state->name = token.text;

                declaration_render_state( parser, *render_state );

                parser->shader.render_states.emplace_back( render_state );
            }
        }
    }
}

//
//
void declaration_render_state( Parser* parser, RenderState& render_state ) {
    Token token;

    // Set render state in a default state
    render_state.blend_state.active_states = 0;

    render_state.depth_stencil.depth_enable = 0;
    render_state.depth_stencil.depth_write_enable = 0;
    render_state.depth_stencil.stencil_enable = 0;

    render_state.rasterization.cull_mode = hydra::gfx::CullMode::None;
    render_state.rasterization.front = hydra::gfx::FrontClockwise::False;
    render_state.rasterization.fill = hydra::gfx::FillMode::Solid;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 4, "Cull" ) ) {

                // Advance to the token after the initial keyword.
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 4, "Back" ) ) {
                    render_state.rasterization.cull_mode = hydra::gfx::CullMode::Back;
                }
                else if ( lexer_expect_keyword( token.text, 5, "Front" ) ) {
                    render_state.rasterization.cull_mode = hydra::gfx::CullMode::Front;
                }
                else if ( lexer_expect_keyword( token.text, 4, "None" ) ) {
                    render_state.rasterization.cull_mode = hydra::gfx::CullMode::None;
                }
            }
            else if ( lexer_expect_keyword( token.text, 5, "ZTest" ) ) {

                // Advance to the token after the initial keyword.
                lexer_next_token( parser->lexer, token );

                // ZTest (Less | Greater | LEqual | GEqual | Equal | NotEqual | Always)
                if ( lexer_expect_keyword( token.text, 4, "Less" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::Less;
                }
                else if ( lexer_expect_keyword( token.text, 7, "Greater" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::Greater;
                }
                else if ( lexer_expect_keyword( token.text, 6, "LEqual" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::LessEqual;
                }
                else if ( lexer_expect_keyword( token.text, 6, "GEqual" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::GreaterEqual;
                }
                else if ( lexer_expect_keyword( token.text, 5, "Equal" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::Equal;
                }
                else if ( lexer_expect_keyword( token.text, 8, "NotEqual" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::NotEqual;
                }
                else if ( lexer_expect_keyword( token.text, 6, "Always" ) ) {
                    render_state.depth_stencil.depth_comparison = hydra::gfx::ComparisonFunction::Always;
                }

                render_state.depth_stencil.depth_enable = 1;
            }
            else if ( lexer_expect_keyword( token.text, 6, "ZWrite" ) ) {

                // Advance to the token after the initial keyword.
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 2, "On" ) ) {
                    render_state.depth_stencil.depth_write_enable = 1;
                }
                else if ( lexer_expect_keyword( token.text, 3, "Off" ) ) {
                    render_state.depth_stencil.depth_write_enable = 0;
                }
            }
            else if ( lexer_expect_keyword( token.text, 9, "BlendMode" ) ) {

                lexer_next_token( parser->lexer, token );
                
                hydra::gfx::BlendState& bs = render_state.blend_state.blend_states[ render_state.blend_state.active_states ];

                if ( lexer_expect_keyword( token.text, 5, "Alpha" ) ) {    
                    bs.blend_enabled = 1;
                    bs.color_operation = hydra::gfx::BlendOperation::Add;
                    bs.source_color = hydra::gfx::Blend::SrcAlpha;
                    bs.destination_color = hydra::gfx::Blend::InvSrcAlpha;
                }
                else if ( lexer_expect_keyword( token.text, 13, "Premultiplied" ) ) {
                    bs.blend_enabled = 1;
                    bs.color_operation = hydra::gfx::BlendOperation::Add;
                    bs.source_color = hydra::gfx::Blend::One;
                    bs.destination_color = hydra::gfx::Blend::InvSrcAlpha;
                }
                else if ( lexer_expect_keyword( token.text, 8, "Additive" ) ) {
                    // TODO
                }

                ++render_state.blend_state.active_states;
            }
        }
    }
}

//
//
void declaration_sampler_states( Parser* parser ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 5, "state" ) ) {
                // Advance to next token
                lexer_next_token( parser->lexer, token );

                SamplerState* state = new SamplerState();
                state->name = token.text;

                declaration_sampler_state( parser, *state );

                parser->shader.sampler_states.emplace_back( state );
            }
        }
    }
}

//
//
void declaration_sampler_state( Parser* parser, SamplerState& state ) {

    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {

            if ( lexer_expect_keyword( token.text, 6, "Filter" ) ) {
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 15, "MinMagMipLinear" ) ) {
                    state.sampler.min_filter = hydra::gfx::TextureFilter::Linear;
                    state.sampler.mag_filter = hydra::gfx::TextureFilter::Linear;
                    state.sampler.mip_filter = hydra::gfx::TextureMipFilter::Linear;
                }
            }
            else if ( lexer_expect_keyword( token.text, 8, "AddressU" ) ) {
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 5, "Clamp" ) ) {
                    state.sampler.address_mode_u = hydra::gfx::TextureAddressMode::Clamp_Border;
                }
            }
            else if ( lexer_expect_keyword( token.text, 8, "AddressV" ) ) {
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 5, "Clamp" ) ) {
                    state.sampler.address_mode_v = hydra::gfx::TextureAddressMode::Clamp_Border;
                }
            }
            else if ( lexer_expect_keyword( token.text, 8, "AddressW" ) ) {
                lexer_next_token( parser->lexer, token );

                if ( lexer_expect_keyword( token.text, 5, "Clamp" ) ) {
                    state.sampler.address_mode_w = hydra::gfx::TextureAddressMode::Clamp_Border;
                }
            }
        }
    }
}

//
//
void declaration_pass_resources( Parser* parser, Pass& pass ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    lexer_next_token( parser->lexer, token );

    // Now token contains the name of the resource list
    const ResourceList* resource_list = find_resource_list( parser, token.text );
    if ( resource_list ) {
        pass.resource_lists.emplace_back( resource_list );
    }
    else {
        // Error
    }
}

//
//
void declaration_pass_stage( Parser* parser, Pass& pass ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    pass.stage_name = token.text;
}

//
//
void declaration_pass_vertex_layout( Parser* parser, Pass& pass ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    lexer_next_token( parser->lexer, token );
    const VertexLayout* vertex_layout = find_vertex_layout( parser, token.text );
    if ( vertex_layout ) {
        pass.vertex_layout = vertex_layout;
    }
}

//
//
void declaration_pass_render_states( Parser* parser, Pass& pass ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    lexer_next_token( parser->lexer, token );
    const RenderState* render_state = find_render_state( parser, token.text );
    if ( render_state ) {
        pass.render_state = render_state;
    }
}

//
//
void declaration_pass_options( Parser* parser, Pass& pass ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    // Include the 'off' option for this option group
    uint16_t count = 1;

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseParen ) ) {
        lexer_next_token( parser->lexer, token );

        if ( token.type == Token::Token_Identifier ) {
            pass.options.emplace_back( token.text );
            
            ++count;
        }
    }

    pass.options_offsets.emplace_back( count );
}

//
//
void declaration_pass_dispatch( Parser* parser, Pass& pass ) {

    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Equals ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Number ) ) {
        return;
    }

    pass.compute_dispatch.x = hydra::roundu16(atof(token.text.text));

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Comma ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Number ) ) {
        return;
    }

    pass.compute_dispatch.y = hydra::roundu16( atof( token.text.text ));

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Comma ) ) {
        return;
    }

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_Number ) ) {
        return;
    }

    pass.compute_dispatch.z = hydra::roundu16( atof( token.text.text ) );
}

//
//
void declaration_includes( Parser* parser ) {
    Token token;

    if ( !lexer_expect_token( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    while ( !lexer_equals_token( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_String ) {
            StringBuffer path_buffer;
            path_buffer.init( 256, parser->allocator );

            path_buffer.append( parser->source_path );
            path_buffer.append( token.text );

            char* text = hydra::file_read_binary( path_buffer.data, parser->allocator, nullptr );
            if ( text ) {
                Lexer lexer;
                DataBuffer data_buffer;

                data_buffer_init( &data_buffer, 256, 2048 );
                lexer_init( &lexer, text, &data_buffer );

                Parser local_parser;
                hfx::parser_init( &local_parser, &lexer, parser->allocator, parser->source_path, path_buffer.data, "." );
                hfx::parser_generate_ast( &local_parser );

                // TODO: cleanup code!

                // Merge parsing results
                hfx::Shader& shader = local_parser.shader;
                // Merge resource lists
                std::vector<const ResourceList*>& local_resource_lists = shader.resource_lists;
                for ( size_t i = 0; i < local_resource_lists.size(); ++i ) {
                    const ResourceList* resource_list = local_resource_lists[i];
                    // Rename this resource list to give context.

                    char* new_name = parser->string_buffer.reserve( resource_list->name.length + shader.name.length + 2 );   // +1 for the point, +1 for the null terminator.
                    memcpy( new_name, shader.name.text, shader.name.length );
                    memcpy( new_name + shader.name.length, ".", 1 );
                    memcpy( new_name + shader.name.length + 1, resource_list->name.text, resource_list->name.length );
                    new_name[resource_list->name.length + shader.name.length + 1] = 0;

                    ResourceList* new_resource_list = (ResourceList*)resource_list;
                    new_resource_list->name.length = resource_list->name.length + shader.name.length + 1; // "." added to string.
                    new_resource_list->name.text = new_name;

                    parser->shader.resource_lists.emplace_back( new_resource_list );
                }

                // Merge code fragments
                std::vector<CodeFragment>& local_code_fragments = shader.code_fragments;
                for ( size_t i = 0; i < local_code_fragments.size(); ++i ) {
                    CodeFragment& code_fragment = local_code_fragments[i];

                    char* new_name = parser->string_buffer.reserve( code_fragment.name.length + shader.name.length + 2 );   // +1 for the point, +1 for the null terminator.
                    memcpy( new_name, shader.name.text, shader.name.length );
                    memcpy( new_name + shader.name.length, ".", 1 );
                    memcpy( new_name + shader.name.length + 1, code_fragment.name.text, code_fragment.name.length );
                    new_name[code_fragment.name.length + shader.name.length + 1] = 0;

                    code_fragment.name.length = code_fragment.name.length + shader.name.length + 1;
                    code_fragment.name.text = new_name;

                    parser->shader.code_fragments.emplace_back( code_fragment );
                }

                hfx::parser_terminate( &local_parser );
            }
            else {
                HYDRA_LOG( "Cannot find include file %s\n", path_buffer.data );
            }

            //parser->shader.hfx_includes.emplace_back( token.text );
        }
    }
}

//
// CodeGenerator //////////////////////////////////////////////////////////////////////////////
//

void output_shader_stage( CodeGenerator* code_generator, const char* path, const Pass::ShaderStage& stage );

void code_generator_init( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size, uint32_t buffer_count ) {
    code_generator->parser = parser;
    code_generator->buffer_count = buffer_count;
    code_generator->string_buffers = new StringBuffer[buffer_count];

    for ( uint32_t i = 0; i < buffer_count; i++ ) {
        code_generator->string_buffers[i].init( buffer_size, parser->allocator );
    }

    code_generator->name_to_type.init( parser->allocator, 16 );
    code_generator->path_buffer.init( 512 * 10, parser->allocator );
}

void code_generator_terminate( CodeGenerator* code_generator ) {
    for ( uint32_t i = 0; i < code_generator->buffer_count; i++ ) {
        code_generator->string_buffers[i].shutdown();
    }

    code_generator->name_to_type.shutdown();
    code_generator->path_buffer.shutdown();
}

//
// Generate single files for each shader stage.
void code_generator_output_shader_files( CodeGenerator* code_generator, const char* path ) {

    code_generator->string_buffers[0].clear();
    code_generator->string_buffers[1].clear();
    code_generator->string_buffers[2].clear();

    // For each pass and for each pass generate permutation file.
    const uint32_t pass_count = (uint32_t)code_generator->parser->shader.passes.size();
    for ( uint32_t i = 0; i < pass_count; i++ ) {

        // Create one file for each code fragment
        const Pass& pass = code_generator->parser->shader.passes[i];

        for ( size_t s = 0; s < pass.shader_stages.size(); ++s ) {
            output_shader_stage( code_generator, path, pass.shader_stages[s] );
        }
    }
}

// Additional data to be added to output shaders.
// Vertex, Fragment, Geometry, Compute, Hull, Domain, Count
static const char*              s_shader_file_extension[Stage::Count + 1] = { ".vert", ".frag", ".geom", ".comp", ".tesc", ".tese", ".h" };
static const char*              s_shader_compiler_stage[Stage::Count + 1] = { "vert", "frag", "geom", "comp", "tesc", "tese", ".h" };
static const char*              s_shader_stage_defines[Stage::Count + 1] = { "#define VERTEX\n", "#define FRAGMENT\n", "#define GEOMETRY\n", "#define COMPUTE\n", "#define HULL\n", "#define DOMAIN\n", "\n" };

static void generate_glsl_and_defaults( const Shader& shader, StringBuffer& out_buffer, StringBuffer& out_defaults, const DataBuffer& data_buffer ) {
    if ( !shader.properties.size() ) {
        uint32_t zero_size = 0;
        out_defaults.append_m( &zero_size, sizeof(uint32_t) );
        return;
    }

    // Add the local constants into the code.
    out_buffer.append( "\n\t\tlayout (std140, binding=7) uniform MaterialConstants {\n\n" );

    // For GPU the struct must be 16 bytes aligned. Track alignment
    uint32_t gpu_struct_alignment = 0;

    // In the defaults, write the type, size in '4 bytes' blocks, then data.
    hydra::gfx::ResourceType::Enum resource_type = hydra::gfx::ResourceType::Constants;
    out_defaults.append_m( &resource_type, sizeof( hydra::gfx::ResourceType::Enum ) );

    // Reserve space for later writing the correct value.
    char* buffer_size_memory = out_defaults.reserve( sizeof( uint32_t ) );

    const size_t num_properties = shader.properties.size();
    for ( size_t i = 0; i < num_properties; i++ ) {
        hfx::Property* property = shader.properties[i];

        switch ( property->type ) {
            case PropertyType::Float:
            {
                out_buffer.append( "\t\t\tfloat\t\t\t\t\t" );
                out_buffer.append( property->name );
                out_buffer.append( ";\n" );

                // Get default value and write it into default buffer
                if ( property->data_index != 0xffffffff ) {
                    float value = 0.0f;
                    data_buffer_get( data_buffer, property->data_index, value );

                    out_defaults.append_m( &value, sizeof( float ) );
                }
                // Update offset
                property->offset_in_bytes = gpu_struct_alignment * 4;

                ++gpu_struct_alignment;
                break;
            }

            case PropertyType::Int:
            {
                break;
            }

            case PropertyType::Range:
            {
                break;
            }

            case PropertyType::Color:
            {
                break;
            }

            case PropertyType::Vector:
            {
                break;
            }
        }
    }

    uint32_t tail_padding_size = 4 - (gpu_struct_alignment % 4);
    out_buffer.append_f( "\t\t\tfloat\t\t\t\t\tpad_tail[%u];\n\n", tail_padding_size );
    out_buffer.append( "\t\t} local_constants;\n\n" );

    for ( uint32_t v = 0; v < tail_padding_size; ++v ) {
        float value = 0.0f;
        out_defaults.append_m( &value, sizeof( float ) );
    }

    // Write the constant buffer size in bytes.
    uint32_t constants_buffer_size = (gpu_struct_alignment + tail_padding_size) * sizeof( float );
    memcpy( buffer_size_memory, &constants_buffer_size, sizeof( uint32_t ) );
}

//
static void reflection_update_automatic_binding( hfx::ResourceBinding* out_bindings, u32 binding, cstring namespace_name, cstring name, hydra::gfx::ResourceType::Enum type ) {
    // Skip empty out
    if ( !out_bindings ) {
        return;
    }
    if ( binding < 32 ) {
        hfx::ResourceBinding& resource_binding = out_bindings[ binding ];
        if ( resource_binding.count != 0 && resource_binding.start != binding ) {
            hprint( "Resource clash in %s for resource %s binding %u with previous resource %s binding %u.\n", namespace_name, name, resource_binding.start, resource_binding.name, binding );
        }
        resource_binding.type = type;
        resource_binding.count = 1;
        resource_binding.start = ( u16 )binding;
        strcpy_s( resource_binding.name, 32, name );
    } else {
        hprint( "Cannot add binding %s with index %u to automatic resource generation.\n", name, binding );
    }
}

//
//
static void append_reflection_data( nlohmann::json& parsed_json, const char* namespace_name, StringBuffer* reflection_buffer,
                                    hydra::FlatHashMap<u64, cstring> name_to_type, StringBuffer& buffer, hfx::ResourceBinding* out_bindings,
                                    hydra::Allocator* allocator ) {
    
    
    if ( reflection_buffer ) {
        using json = nlohmann::json;

        reflection_buffer->append_f( "\t\tnamespace %s {\n\n", namespace_name );
        json types = parsed_json[ "types" ];

        std::string name_str;

        // Track which types are written as structs, as sometimes reflection
        // contains duplicates.
        hydra::FlatHashMap<u64, u8> written_types;
        written_types.init( allocator, 16 );

        for ( json::iterator it = types.begin(); it != types.end(); ++it ) {
            json definition = it.value();

            // Write struct name
            definition[ "name" ].get_to( name_str );

            cstring name_interned = (cstring)buffer.append_use( it.key().c_str() );
            cstring type_interned = (cstring)buffer.append_use( name_str.c_str() );

            u64 name_hash = hydra::hash_calculate( name_interned );
            u64 type_hash = hydra::hash_calculate( type_interned );

            // ALWAYS include this, as the name is used by following types to retrive it.
            // Just the code for the struct does not need to be written.
            name_to_type.insert( name_hash, type_interned );

            if ( written_types.get( type_hash ) ) {
                // Manually remove duplicates
                hprint( "Removing duplicate type %s in reflection json for %s.\n", name_interned, namespace_name );
                continue;
            }
            written_types.insert( type_hash, 1 );
            

            reflection_buffer->append_f( "\t\t\tstruct %s {\n", name_str.c_str() );

            json members = definition[ "members" ];

            u32 member_memory_offset = 0;
            u32 padding_added = 0;

            // Iterate all struct members
            for ( u32 i = 0; i < members.size(); ++i ) {

                json m = members[ i ];
                json type = m[ "type" ];
                json name = m[ "name" ];
                json offset = m[ "offset" ];    // memory offset

                // Check struct member offset and add padding if needed.
                type.get_to( name_str );
                u32 member_size = 0;
                if ( name_str.compare( "mat4" ) == 0 ) {
                    member_size = 64;
                }
                else if ( name_str.compare( "vec4" ) == 0 ) {
                    member_size = 16;
                }
                else if ( name_str.compare( "vec3" ) == 0 ) {
                    member_size = 12;
                }
                else if ( name_str.compare( "vec2" ) == 0 ) {
                    member_size = 8;
                }
                else if ( name_str.compare( "float" ) == 0 ) {
                    member_size = 4;
                }
                else if ( name_str.compare( "uint" ) == 0 ) {
                    member_size = 4;
                }
                else if ( name_str.compare( "int" ) == 0 ) {
                    member_size = 4;
                }
                else if ( name_str.compare( "uvec4" ) == 0 ) {
                    member_size = 16;
                }
                else if ( name_str.compare( "ivec4" ) == 0 ) {
                    member_size = 16;
                }
                else {
                    // Try to get the struct name
                    u64 member_typename_hash = hydra::hash_calculate( name_str.c_str() );
                    cstring type_name = name_to_type.get( member_typename_hash );
                    if ( type_name ) {
                        name_str = type_name;
                    }
                    else {
                        hprint( "Error parsing type %s\n", name_str.c_str() );
                    }
                }

                // Add padding
                if ( offset.is_number() ) {
                    u32 current_member_offset;
                    offset.get_to( current_member_offset );

                    // TODO:
                    //hprint( "TODO: improve array padding\n" );

                    if ( member_memory_offset < current_member_offset ) {
                        while ( member_memory_offset < current_member_offset ) {

                            reflection_buffer->append_f( "\t\t\t\tuint32_t\t\t\t\tpad%d;\n", padding_added++ );
                            ++member_memory_offset;
                        }
                    }
                    else {
                        member_memory_offset += member_size;
                    }
                }

                // Write type
                reflection_buffer->append_f( "\t\t\t\t%s", name_str.c_str() );
                // Write name
                name.get_to( name_str );
                reflection_buffer->append_f( "\t\t\t\t\t%s;\n", name_str.c_str() );
            }

            reflection_buffer->append_f( "\t\t\t};\n\n" );
        }

        // Write resource indices
        json ubos = parsed_json[ "ubos" ];
        for ( u32 i = 0; i < ubos.size(); ++i ) {
            json ubo = ubos[ i ];
            u32 set = ubo.value( "set", 0 );
            u32 binding = ubo.value( "binding", 0 );

            ubo[ "name" ].get_to( name_str );
            reflection_buffer->append_f( "\t\t\tstatic const uint32_t binding_cb_%s = %u; // Set %u, binding %u\n", name_str.c_str(), binding, set, binding );

            reflection_update_automatic_binding( out_bindings, binding, namespace_name, name_str.c_str(), hydra::gfx::ResourceType::Constants );
        }

        json textures = parsed_json[ "textures" ];
        for ( u32 i = 0; i < textures.size(); ++i ) {
            json texture = textures[ i ];
            u32 set = texture.value( "set", 0 );
            u32 binding = texture.value( "binding", 0 );
            texture[ "name" ].get_to( name_str );
            reflection_buffer->append_f( "\t\t\tstatic const uint32_t binding_tex_%s = %u; // Set %u, binding %u\n", name_str.c_str(), binding, set, binding );

            reflection_update_automatic_binding( out_bindings, binding, namespace_name, name_str.c_str(), hydra::gfx::ResourceType::Texture );
        }

        json ssbos = parsed_json[ "ssbos" ];
        for ( u32 i = 0; i < ssbos.size(); ++i ) {
            json ssbo = ssbos[ i ];
            u32 set = ssbo.value( "set", 0 );
            u32 binding = ssbo.value( "binding", 0 );
            ssbo[ "name" ].get_to( name_str );
            reflection_buffer->append_f( "\t\t\tstatic const uint32_t binding_sb_%s = %u; // Set %u, binding %u\n", name_str.c_str(), binding, set, binding );

            reflection_update_automatic_binding( out_bindings, binding, namespace_name, name_str.c_str(), hydra::gfx::ResourceType::StructuredBuffer );
        }

        reflection_buffer->append_f( "\n\t\t} // namespace %s\n\n", namespace_name );

        written_types.shutdown();
    }
}

static void append_include_code( const CodeFragment::Include& include, cstring path, const Parser* parser, Stage& stage, StringBuffer& filename_buffer, StringBuffer& code_buffer ) {
    
    if ( include.stage_mask != stage && include.stage_mask != Stage::Count ) {
        return;
    }

    if ( include.file_or_local == 1 ) {
        const CodeFragment* included_code_fragment = find_code_fragment( parser, include.filename );
        if ( included_code_fragment ) {
            code_buffer.append( included_code_fragment->code );
        }
        else {
            HYDRA_LOG( "Cannot find HFX shader include\n" );
        }
    }
    else {
        // Open and read include files
        filename_buffer.clear();
            
        if ( path ) {
            filename_buffer.append( path );
            filename_buffer.append( "\\" );
        }

        filename_buffer.append( include.filename );
        char* include_code = hydra::file_read_text( filename_buffer.data, parser->allocator, nullptr );
        if ( include_code ) {
            code_buffer.append_f( "%s\n", include_code );

            hfree( include_code, parser->allocator );
        }
        else {
            HYDRA_LOG( "Cannot find include file %s\n", filename_buffer.data );
        }
    }

    code_buffer.append( "\n" );
}


//
// Finalize shader code.
static void finalize_shader_code( const char* path, const CodeGenerator* code_generator, const Pass::ShaderStage& shader_stage,
                                  const StringBuffer& constants_buffer, StringBuffer& filename_buffer, StringBuffer& code_buffer ) {

    const Parser* parser = code_generator->parser;
    // Cache current buffer size, use it to compose the shader.
    //uint32_t cached_buffer_size = code_buffer.current_size;

    Stage stage = shader_stage.stage;
    const CodeFragment* code_fragment = shader_stage.code;

    // Append glsl version
    code_buffer.append_f( "%s\n", "#version 450" );

    // Add the per stage define.
    code_buffer.append( s_shader_stage_defines[ stage ] );
    code_buffer.append( "\n\t\t" );

    // Append local constants
    code_buffer.append( constants_buffer );

    // Add the code straight from the HFX file.
    code_buffer.append( "\n\t\t" );

    Lexer lexer;
    lexer_init( &lexer, code_fragment->code.text, nullptr );
    char* current_line_start = lexer.position;
    lexer_next_line( &lexer );
    char* next_line_start = lexer.position;

    char buffer[ 10000 ];

    // Get first include to search
    const CodeFragment::Include* include = code_fragment->includes.size() ? &code_fragment->includes[ 0 ] : nullptr;
    u32 include_index = include ? 0 : u32_max;
    u32 include_relative_line = include ? include->declaration_line - code_fragment->starting_file_line : u32_max;

    // Perform line by line code write
    while ( lexer.position - code_fragment->code.text < (i32)code_fragment->code.length ) {

        if ( lexer.line == include_relative_line ) {
            // Inject include code
            append_include_code( *include, path, parser, stage, filename_buffer, code_buffer );
            // Advance to next include if present
            ++include_index;
            if ( include_index < code_fragment->includes.size() ) {
                include = &code_fragment->includes[ include_index ];
                include_relative_line = include->declaration_line - code_fragment->starting_file_line;
            } else {
                include_relative_line = u32_max;
                include = nullptr;
            }
        }

        // Copy current line
        memcpy( buffer, current_line_start, next_line_start - current_line_start );
        // Append
        code_buffer.append_m( buffer, next_line_start - current_line_start );
        // Next line
        current_line_start = next_line_start;
        lexer_next_line( &lexer );
        next_line_start = lexer.position;
    }
}


//
// Compile shader given the finalized code.
// Returns the filename to the compiled shader if no errors occurred, null otherwise.
static char* compile_shader( cstring path, const CodeGenerator* code_generator, const Pass::ShaderStage& shader_stage,
                             StringBuffer& filename_buffer, StringBuffer& code_buffer, u32 cached_buffer_size ) {


    filename_buffer.clear();

    Stage stage = shader_stage.stage;

    // Calculate output filename.
    // Used as input for external compilation step.
    char* intermediate_filename = filename_buffer.append_use( code_generator->parser->shader.name );
    char* intermediate_shadername = filename_buffer.append_use( shader_stage.code->name );
    char* temp_filename = filename_buffer.append_use_f( "%s\\%s_%s_hfx.%s", code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[ stage ] );
    // Write current shader to file.
    FILE* temp_shader_file = fopen( temp_filename, "w" );
    fwrite( &code_buffer.data[ cached_buffer_size ], code_buffer.current_size - cached_buffer_size, 1, temp_shader_file );
    fclose( temp_shader_file );

    // Rewind code buffer by simply pointing to the memory before all the changes.
    code_buffer.current_size = cached_buffer_size;

    uint32_t compile_options = code_generator->options;

    //char* final_shader = nullptr;
    bool compilation_succeeded = true;
    char* final_shader_filename = nullptr;

    hprint( ">>>>>>> Compiling shader\n" );

    // Execute compilation process.
    if ( ( compile_options & CompileOptions_SpirV ) == CompileOptions_SpirV ) {
        // Convert to SpirV binary blob.
        char* glsl_compiler_path = filename_buffer.append_use_f( "%sglslangValidator.exe", code_generator->shader_binaries_path );
        final_shader_filename = filename_buffer.append_use_f( "%s.spv", temp_filename );
        const char* gl_vertex_id_define = "gl_VertexID=gl_VertexIndex";
        char* arguments = filename_buffer.append_use_f( "glslangValidator.exe %s -V -o %s -S %s --D %s", temp_filename, final_shader_filename, s_shader_compiler_stage[ stage ], gl_vertex_id_define );
        compilation_succeeded = hydra::process_execute( ".", glsl_compiler_path, arguments, "ERROR" );

        // Load newly generated file - binary.
        //final_shader = hydra::file_read_binary( final_shader_filename, code_generator->parser->allocator, &final_shader_size );
    } else {
        // Convert to SpirV binary blob.
        char* glsl_compiler_path = filename_buffer.append_use_f( "%sglslangValidator.exe", code_generator->shader_binaries_path );
        const char* gl_vertex_id_define = "gl_VertexIndex=gl_VertexID";
        char* arguments = filename_buffer.append_use_f( "glslangValidator.exe %s --aml -G -o %s\\shader.spv -S %s --D %s", temp_filename, code_generator->parser->destination_path, s_shader_compiler_stage[ stage ], gl_vertex_id_define );
        compilation_succeeded = hydra::process_execute( ".", glsl_compiler_path, arguments, "ERROR" );

        // spirv - opt.exe - O ComputeTest.spv - o ComputeTest_opt.spv
        // Convert back to glsl
        char* spirv_cross_path = filename_buffer.append_use_f( "%sspirv-cross.exe", code_generator->shader_binaries_path );
        arguments = filename_buffer.append_use_f( "spirv-cross.exe --version 450 --no-es %s\\shader.spv --output %s\\%s_%s.%s", code_generator->parser->destination_path,
                                                  code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[ stage ] );
        hydra::process_execute( ".", spirv_cross_path, arguments );

        final_shader_filename = filename_buffer.append_use_f( "%s\\%s_%s.%s", code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[ stage ] );
        // Use binary version because in the append it will be memcopied.
        //final_shader = hydra::file_read_binary( final_shader_filename, code_generator->parser->allocator, &final_shader_size );
    }

    if ( !compilation_succeeded ) {
        // Print the shader
        char* shader_code = hydra::file_read_text( temp_filename, code_generator->parser->allocator, nullptr );
        const char* process_output = hydra::process_get_output();

        const char* error_string = strstr( process_output, "ERROR" );
        // Error format is: ERROR: filename:(line)
        error_string = strstr( error_string, ":" );
        ++error_string;
        error_string = strstr( error_string, ":" );
        ++error_string;
        i32
            error_line = atoi( error_string );

        const i32 k_output_error_lines = 10;
        i32 min_line = hydra::max( 0, error_line - k_output_error_lines );
        // Search line
        char* shader_error_line = shader_code;

        Lexer lexer;
        lexer_init( &lexer, shader_error_line, nullptr );
        lexer_goto_line( &lexer, min_line );

        shader_error_line = lexer.position;

        lexer_next_line( &lexer );
        char* shader_next_error_line = lexer.position;

        int32_t shader_line_size = ( i32 )( shader_next_error_line - shader_error_line );
        // Output lines before the error line.
        // Need to limit the output to just one line at the time.
        for ( size_t i = 0; i < k_output_error_lines; i++ ) {
            char* shader_line_text = filename_buffer.append_use_substring( shader_error_line, 0, shader_line_size );

            hprint( "%s%s", ( i == k_output_error_lines - 1 ) ? "\nERROR LINE:\n" : "", shader_line_text );

            // Advance one line
            shader_error_line = shader_next_error_line;
            lexer_next_line( &lexer );
            shader_next_error_line = lexer.position;

            shader_line_size = ( i32 )( shader_next_error_line - shader_error_line );
        }

        hprint( "\n>>>>>>> Compilation ERROR!\n\n" );
        //hprint( "FULL CODE %s\n", shader_code);

        // NOTE: return!
        return nullptr;
    } else {
        hprint( ">>>>>>> Compilation successful!\n\n" );
    }

    // Delete intermediate files if needed
    const bool keep_intermediate = ( compile_options & CompileOptions_Output_Files ) == CompileOptions_Output_Files;
    if ( !keep_intermediate ) {
        hydra::file_delete( temp_filename );
    }

    return final_shader_filename;
}

//
// Finalize and append code to a code string buffer.
// For embedded code (into binary HFX), prepend the stage and null terminate.
static bool append_finalized_shader_code( const char* path, const CodeGenerator* code_generator, const Pass::ShaderStage& shader_stage,
                                          StringBuffer& filename_buffer, StringBuffer& code_buffer, const StringBuffer& constants_buffer,
                                          StringBuffer* reflection_buffer, const StringRef* pass_name ) {

    const Parser* parser = code_generator->parser;
    // Cache current buffer size, use it to compose the shader.
    uint32_t cached_buffer_size = code_buffer.current_size;

    Stage stage = shader_stage.stage;
    const CodeFragment* code_fragment = shader_stage.code;

    // Append glsl version
    code_buffer.append_f( "%s\n", "#version 450" );

    // Add the per stage define.
    code_buffer.append( s_shader_stage_defines[ stage ] );
    code_buffer.append( "\n\t\t" );

    // Append local constants
    code_buffer.append( constants_buffer );

    // Add the code straight from the HFX file.
    code_buffer.append( "\n\t\t" );

    Lexer lexer;
    lexer_init( &lexer, code_fragment->code.text, nullptr );
    char* current_line_start = lexer.position;
    lexer_next_line( &lexer );
    char* next_line_start = lexer.position;

    char buffer[ 10000 ];

    // Get first include to search
    const CodeFragment::Include* include = code_fragment->includes.size() ? &code_fragment->includes[ 0 ] : nullptr;
    u32 include_index = include ? 0 : u32_max;
    u32 include_relative_line = include ? include->declaration_line - code_fragment->starting_file_line : u32_max;

    // Perform line by line code write
    while ( lexer.position - code_fragment->code.text < (i32)code_fragment->code.length ) {

        if ( lexer.line == include_relative_line ) {
            // Inject include code
            append_include_code( *include, path, parser, stage, filename_buffer, code_buffer );
            // Advance to next include if present
            ++include_index;
            if ( include_index < code_fragment->includes.size() ) {
                include = &code_fragment->includes[ include_index ];
                include_relative_line = include->declaration_line - code_fragment->starting_file_line;
            }
            else {
                include_relative_line = u32_max;
                include = nullptr;
            }
        }
    
        // Copy current line
        memcpy( buffer, current_line_start, next_line_start - current_line_start );
        // Append
        code_buffer.append_m( buffer, next_line_start - current_line_start );
        // Next line
        current_line_start = next_line_start;
        lexer_next_line( &lexer );
        next_line_start = lexer.position;
    }
    

    filename_buffer.clear();

    // Calculate output filename.
    // Used as input for external compilation step.
    char* intermediate_filename = filename_buffer.append_use( code_generator->parser->shader.name );
    char* intermediate_shadername = filename_buffer.append_use( shader_stage.code->name );
    char* temp_filename = filename_buffer.append_use_f( "%s\\%s_%s_hfx.%s", code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[stage] );
    // Write current shader to file.
    FILE* temp_shader_file = fopen( temp_filename, "w" );
    fwrite( &code_buffer.data[cached_buffer_size], code_buffer.current_size - cached_buffer_size, 1, temp_shader_file );
    fclose( temp_shader_file );

    // Rewind code buffer by simply pointing to the memory before all the changes.
    code_buffer.current_size = cached_buffer_size;

    uint32_t compile_options = code_generator->options;
    
    size_t final_shader_size;
    char* final_shader = nullptr;
    bool compilation_succeeded = true;
    char* final_shader_filename = nullptr;

    hprint( ">>>>>>> Compiling shader\n" );

    // Execute compilation process.
    if ( (compile_options & CompileOptions_SpirV) == CompileOptions_SpirV ) {
        // Convert to SpirV binary blob.
        char* glsl_compiler_path = filename_buffer.append_use_f( "%sglslangValidator.exe", code_generator->shader_binaries_path );
        final_shader_filename = filename_buffer.append_use_f( "%s.spv", temp_filename );
        const char* gl_vertex_id_define = "gl_VertexID=gl_VertexIndex";
        char* arguments = filename_buffer.append_use_f( "glslangValidator.exe %s -V -o %s -S %s --D %s", temp_filename, final_shader_filename, s_shader_compiler_stage[stage], gl_vertex_id_define );
        compilation_succeeded = hydra::process_execute( ".", glsl_compiler_path, arguments, "ERROR" );
        
        // Load newly generated file - binary.
        final_shader = hydra::file_read_binary( final_shader_filename, code_generator->parser->allocator, &final_shader_size );
    }
    else {
        // Convert to SpirV binary blob.
        char* glsl_compiler_path = filename_buffer.append_use_f( "%sglslangValidator.exe", code_generator->shader_binaries_path );
        const char* gl_vertex_id_define = "gl_VertexIndex=gl_VertexID";
        char* arguments = filename_buffer.append_use_f( "glslangValidator.exe %s --aml -G -o %s\\shader.spv -S %s --D %s", temp_filename, code_generator->parser->destination_path, s_shader_compiler_stage[stage], gl_vertex_id_define );
        compilation_succeeded = hydra::process_execute( ".", glsl_compiler_path, arguments, "ERROR" );

        // spirv - opt.exe - O ComputeTest.spv - o ComputeTest_opt.spv
        // Convert back to glsl
        char* spirv_cross_path = filename_buffer.append_use_f( "%sspirv-cross.exe", code_generator->shader_binaries_path );
        arguments = filename_buffer.append_use_f( "spirv-cross.exe --version 450 --no-es %s\\shader.spv --output %s\\%s_%s.%s", code_generator->parser->destination_path,
                                                code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[stage] );
        hydra::process_execute( ".", spirv_cross_path, arguments );

        final_shader_filename = filename_buffer.append_use_f( "%s\\%s_%s.%s", code_generator->parser->destination_path, intermediate_filename, intermediate_shadername, s_shader_compiler_stage[stage] );
        // Use binary version because in the append it will be memcopied.
        final_shader = hydra::file_read_binary( final_shader_filename, code_generator->parser->allocator, &final_shader_size );
    }

    if ( !compilation_succeeded ) {
        // Print the shader
        char* shader_code = hydra::file_read_text( temp_filename, code_generator->parser->allocator, nullptr );
        const char* process_output = hydra::process_get_output();

        const char* error_string = strstr( process_output, "ERROR" );
        // Error format is: ERROR: filename:(line)
        error_string = strstr( error_string, ":" );
        ++error_string;
        error_string = strstr( error_string, ":" );
        ++error_string;
        i32
         error_line = atoi( error_string );

        const i32 k_output_error_lines = 10;
        i32 min_line = hydra::max( 0, error_line - k_output_error_lines );
        // Search line
        char* shader_error_line = shader_code;

        Lexer error_lexer;
        lexer_init( &error_lexer, shader_error_line, nullptr );
        lexer_goto_line( &error_lexer, min_line );

        shader_error_line = error_lexer.position;

        lexer_next_line( &error_lexer );
        char* shader_next_error_line = error_lexer.position;

        int32_t shader_line_size = (i32)(shader_next_error_line - shader_error_line);
        // Output lines before the error line.
        // Need to limit the output to just one line at the time.
        for ( size_t i = 0; i < k_output_error_lines; i++ ) {
            char* shader_line_text = filename_buffer.append_use_substring( shader_error_line, 0, shader_line_size );

            hprint( "%s%s", ( i == k_output_error_lines - 1 ) ? "\nERROR LINE:\n" : "", shader_line_text );

            // Advance one line
            shader_error_line = shader_next_error_line; 
            lexer_next_line( &error_lexer );
            shader_next_error_line = error_lexer.position;
            
            shader_line_size = (i32)(shader_next_error_line - shader_error_line);
        }

        hprint( "\n>>>>>>> Compilation ERROR!\n\n" );
        //hprint( "FULL CODE %s\n", shader_code);

        // NOTE: return!
        return false;
    }
    else {
        hprint( ">>>>>>> Compilation successful!\n\n" );
    }

    // From here code compilation is successful.
    const bool generate_reflection_data = ( ( compile_options & CompileOptions_Reflection_CPP ) == CompileOptions_Reflection_CPP) ||
                                          ( ( compile_options & CompileOptions_Reflection_Reload ) == CompileOptions_Reflection_Reload );

    char* reflection_filename = nullptr;
    if ( generate_reflection_data ) {
        char* spirv_cross_path = filename_buffer.append_use_f( "%sspirv-cross.exe", code_generator->shader_binaries_path );
        reflection_filename = filename_buffer.append_use_f( "%s.json", final_shader_filename );
        char* arguments = filename_buffer.append_use_f( "spirv-cross.exe %s --reflect --output %s", final_shader_filename, reflection_filename );
        hydra::process_execute( ".", spirv_cross_path, arguments );

        char* pass_name_C = filename_buffer.append_use( *pass_name );
        char* namespace_name = filename_buffer.append_use_f("%s_%s", pass_name_C, s_shader_compiler_stage[stage] );

        // Open generated reflection json file.
        using json = nlohmann::json;
        char* reflection_json_memory = hydra::file_read_text( reflection_filename, code_generator->parser->allocator, nullptr );
        json reflection_json = json::parse( reflection_json_memory );

        append_reflection_data( reflection_json, namespace_name, reflection_buffer, code_generator->name_to_type, filename_buffer, nullptr, nullptr );

        hydra::file_delete( reflection_filename );
    }
    
    const bool embedded = (compile_options & CompileOptions_Embedded) == CompileOptions_Embedded;
    // Append header
    if ( embedded ) {
        ShaderEffectFile::ChunkHeader header;
        header.code_size = (uint32_t)final_shader_size;
        header.shader_stage = (int8_t)stage;
        code_buffer.append_m( &header, sizeof( ShaderEffectFile::ChunkHeader ) );
    }

    code_buffer.append_m( (void*)final_shader, final_shader_size );
    hfree( final_shader, code_generator->parser->allocator );

    // Delete intermediate files if needed
    const bool keep_intermediate = ( compile_options & CompileOptions_Output_Files ) == CompileOptions_Output_Files;
    if ( !keep_intermediate ) {
        hydra::file_delete( temp_filename );
        hydra::file_delete( final_shader_filename );

        if ( reflection_filename ) {
            hydra::file_delete( reflection_filename );
        }
    }

    if ( embedded ) {
        int8_t null_termination = 0;
        code_buffer.append_m( &null_termination, 1 );
    }

    return true;
}

//
//
void output_shader_stage( CodeGenerator* code_generator, const char* path, const Pass::ShaderStage& stage ) {
    // Create file
    FILE* output_file;

    StringBuffer& filename_buffer = code_generator->string_buffers[0];
    filename_buffer.clear();

    if ( path )
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
    StringBuffer& constants_defaults_buffer = code_generator->string_buffers[3];
    constants_buffer.clear();
    constants_defaults_buffer.clear();

    generate_glsl_and_defaults( code_generator->parser->shader, constants_buffer, constants_defaults_buffer, *code_generator->parser->lexer->data_buffer );

    append_finalized_shader_code( path, code_generator, stage, filename_buffer, code_buffer, constants_buffer, nullptr, nullptr );

    // Write content to file.
    fwrite( code_buffer.data, code_buffer.current_size, 1, output_file );

    fclose( output_file );
}

//
//
static void update_shader_chunk_list( uint32_t* current_shader_offset, uint32_t pass_header_size, StringBuffer& offset_buffer, StringBuffer& code_buffer ) {

    ShaderEffectFile::ShaderChunk chunk = { *current_shader_offset, code_buffer.current_size - *current_shader_offset };
    offset_buffer.append_m( &chunk, sizeof( ShaderEffectFile::ShaderChunk ) );

    *current_shader_offset = code_buffer.current_size + pass_header_size;
}

//
//
static void write_automatic_resources_layout( const hfx::Pass& pass, StringBuffer& pass_buffer, uint32_t& pass_offset ) {

    using namespace hydra::gfx;

    // Add the local constant buffer obtained from all the properties in the layout.
    hydra::gfx::ResourceLayoutCreation::Binding binding = { hydra::gfx::ResourceType::Constants, 0, 1, "MaterialConstants" };
    char* num_resources_data = pass_buffer.reserve( sizeof( uint8_t ) );

    uint8_t num_resources = 1;  // Local constants added
    pass_buffer.append_m( (void*)&binding, sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) );
    pass_offset += sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) + sizeof( uint8_t );

    for ( size_t s = 0; s < pass.shader_stages.size(); ++s ) {
        const Pass::ShaderStage shader_stage = pass.shader_stages[s];

        for ( size_t p = 0; p < shader_stage.code->resources.size(); p++ ) {
            const hfx::CodeFragment::Resource& resource = shader_stage.code->resources[p];

            switch ( resource.type ) {
                case ResourceType::Texture:
                {
                    StringRef::copy_to( resource.name, binding.name, 32 );
                    binding.type = hydra::gfx::ResourceType::Texture;

                    pass_buffer.append_m( (void*)&binding, sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) );
                    pass_offset += sizeof( hydra::gfx::ResourceLayoutCreation::Binding );
                    ++num_resources;

                    break;
                }

                case ResourceType::ImageRW:
                {
                    StringRef::copy_to( resource.name, binding.name, 32 );
                    binding.type = hydra::gfx::ResourceType::ImageRW;

                    pass_buffer.append_m( (void*)&binding, sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) );
                    pass_offset += sizeof( hydra::gfx::ResourceLayoutCreation::Binding );
                    num_resources;

                    break;
                }
            }
        }
    }

    // Write num resources
    memcpy( num_resources_data, &num_resources, sizeof( uint8_t ) );
}

//
//
static void write_resources_layout( const hfx::Pass& pass, StringBuffer& pass_buffer, uint32_t& pass_offset ) {

    using namespace hydra::gfx;

    for ( size_t r = 0; r < pass.resource_lists.size(); ++r ) {
        const ResourceList* resource_list = pass.resource_lists[r];

        const uint8_t resources_count = (uint8_t)resource_list->resources.size();
        pass_buffer.append_m( (void*)&resources_count, sizeof(uint8_t) );
        pass_buffer.append_m( (void*)resource_list->resources.data(), sizeof( ResourceBinding ) * resources_count );
        pass_offset += sizeof( ResourceBinding ) * resources_count + sizeof( uint8_t );
    }
}

//
//
static void write_vertex_input( const hfx::Pass& pass, StringBuffer& pass_buffer ) {
    if ( !pass.vertex_layout )
        return;

    pass_buffer.append_m( (void*)&pass.vertex_layout->attributes[0], sizeof( hydra::gfx::VertexAttribute ) * pass.vertex_layout->attributes.size() );
    pass_buffer.append_m( (void*)&pass.vertex_layout->streams[0], sizeof( hydra::gfx::VertexStream ) * pass.vertex_layout->streams.size() );
}

//
//
static void write_render_states( const hfx::Pass& pass, StringBuffer& pass_buffer ) {
    if ( !pass.render_state )
        return;

    using namespace hydra::gfx;
    pass_buffer.append_m( (void*)&pass.render_state->rasterization, sizeof( RasterizationCreation ) + sizeof( DepthStencilCreation ) + sizeof( BlendStateCreation ) );
}

//
//
static void write_default_values( StringBuffer& constants_defaults_buffer, StringBuffer& out_buffer, const Shader& shader ) {

    // Count number of resources
    char* num_resources_data = out_buffer.reserve( sizeof( uint32_t ) );
    uint32_t num_resources = 1; // LocalConstant buffer

    out_buffer.append( constants_defaults_buffer );

    // TODO: For each property that is not a number (basically textures)
    //const std::vector<Property*>& properties = shader.properties;
    //for ( size_t i = 0; i < shader.properties.size(); i++ ) {
    //    hfx::Property* property = shader.properties[i];

    //    switch ( property->type ) {
    //    }
    //}

    // Update the count with the correct number
    memcpy( num_resources_data, &num_resources, sizeof( uint32_t ) );
}

//
//
static void write_properties( StringBuffer& out_buffer, const Shader& shader, const DataBuffer& data_buffer ) {

    ShaderEffectFile::MaterialProperty material_property;

    uint32_t num_properties = (uint32_t)shader.properties.size();
    out_buffer.append_m( &num_properties, sizeof( uint32_t ) );

    for ( size_t i = 0; i < shader.properties.size(); i++ ) {
        hfx::Property* property = shader.properties[i];

        material_property.type = property->type;
        StringRef::copy_to( property->name, material_property.name, 64 );
        material_property.offset = (uint16_t)property->offset_in_bytes;

        char* material_property_write_data = out_buffer.reserve( sizeof( ShaderEffectFile::MaterialProperty ) );

        // TODO: is this needed anymore ?
        // Values are now gathered from the default lists using per property offsets.
        //switch ( property->type ) {
        //    case Property::Float:
        //    {
        //        break;
        //    }

        //    default:
        //    {
        //        // TODO
        //        break;
        //    }
        //}

        memcpy( material_property_write_data, &material_property, sizeof( ShaderEffectFile::MaterialProperty ) );
    }
}


//
//
static bool is_resources_layout_automatic( const Shader& shader, const Pass& pass ) {
    return pass.resource_lists.size() == 0;
}

static void code_generator_generate_embedded_file_v2( CodeGenerator* code_generator, const char* output_filename ) {

    // Alias for string buffers used in the process.
    StringBuffer& filename_buffer = code_generator->string_buffers[ 0 ];
    StringBuffer& shader_code_buffer = code_generator->string_buffers[ 1 ];
    StringBuffer& pass_offset_buffer = code_generator->string_buffers[ 2 ];
    //StringBuffer& shader_chunk_list_buffer = code_generator->string_buffers[ 3 ];
    StringBuffer& pass_buffer = code_generator->string_buffers[ 4 ];
    StringBuffer& constants_buffer = code_generator->string_buffers[ 5 ];
    StringBuffer& constants_defaults_buffer = code_generator->string_buffers[ 6 ];
    StringBuffer& input_path_buffer = code_generator->string_buffers[ 7 ];
    StringBuffer& reflection_buffer = code_generator->string_buffers[ 8 ];

    pass_offset_buffer.clear();
    pass_buffer.clear();
    constants_buffer.clear();
    constants_defaults_buffer.clear();
    input_path_buffer.clear();
    reflection_buffer.clear();

    u32 compile_options = code_generator->options;
    const bool keep_intermediate = ( compile_options & CompileOptions_Output_Files ) == CompileOptions_Output_Files;

    // Calculate input path
    const char* input_path = code_generator->parser->source_path;

    hydra::BlobSerializer blob;
    blob.is_reading = false;
    ShaderEffectBlueprint* hfx_blueprint = blob.write_and_prepare<ShaderEffectBlueprint>( code_generator->parser->allocator, 0, 1024 * 1024 );
    
    // Copy binary header magic
    memcpy( hfx_blueprint->binary_header_magic, code_generator->binary_header_magic, 32 );
    blob.allocate_and_set( hfx_blueprint->name, code_generator->parser->shader.name.text, ( u32 )code_generator->parser->shader.name.length );

    // Output files only if compilation has succeded.
    bool compilation_succeeded = true;

    const uint32_t pass_count = ( uint32_t )code_generator->parser->shader.passes.size();
    blob.allocate_and_set( hfx_blueprint->passes, pass_count );

    const bool generate_reflection_data = ( ( compile_options & CompileOptions_Reflection_CPP ) == CompileOptions_Reflection_CPP ) ||
                                          ( ( compile_options & CompileOptions_Reflection_Reload ) == CompileOptions_Reflection_Reload );
    code_generator->generate_reflection_data = generate_reflection_data;

    char* reflection_filename = nullptr;

    if ( generate_reflection_data ) {
        const char* shader_name = filename_buffer.append_use( code_generator->parser->shader.name );
        reflection_buffer.append_f( "namespace %s {\n", shader_name );
        reflection_buffer.append_f( "\n\tstatic hydra::gfx::ResourceListCreation tables[ %u ];\n\n", pass_count );

        // Add pass index at the beginning
        for ( uint32_t i = 0; i < pass_count; i++ ) {
            const Pass& pass = code_generator->parser->shader.passes[ i ];

            reflection_buffer.append_f( "\tstatic const uint32_t\t\tpass_" );
            reflection_buffer.append( pass.name );
            reflection_buffer.append_f( " = %u;\n", i );
        }

        reflection_buffer.append_f( "\tstatic const uint32_t\t\tpass_count = %u;\n\n", pass_count );
    }

    hfx::ResourceBinding pass_bindings[ 32 ];

    // For each pass
    for ( uint32_t i = 0; i < pass_count; i++ ) {

        const Pass& pass = code_generator->parser->shader.passes[ i ];
        const uint32_t pass_shader_stages = ( uint32_t )pass.shader_stages.size();

        ShaderPassBlueprint& pass_blueprint = hfx_blueprint->passes[ i ];
        blob.allocate_and_set( pass_blueprint.shaders, pass_shader_stages );

        // Copy names
        StringRef::copy_to( pass.name, pass_blueprint.name, 32 );
        StringRef::copy_to( pass.stage_name, pass_blueprint.stage_name, 32 );

        // Add per pass namespace
        char* pass_name_C = filename_buffer.append_use( pass.name );
        reflection_buffer.append_f( "\tnamespace %s {\n", pass_name_C );

        pass_blueprint.compute_dispatch = pass.compute_dispatch;
        pass_blueprint.is_spirv = ( ( code_generator->options & CompileOptions_SpirV ) == CompileOptions_SpirV ) ? 1 : 0;

        shader_code_buffer.clear();

        // Handle automatic layout generation per pass
        const bool automatic_layout = is_resources_layout_automatic( code_generator->parser->shader, pass );
        if ( automatic_layout ) {
            memset( pass_bindings, 0, 32 * sizeof( hfx::ResourceBinding ) );
        }

        // For each shader stage
        for ( u32 s = 0; s < pass_shader_stages; ++s ) {
            const Pass::ShaderStage shader_stage = pass.shader_stages[ s ];

            shader_code_buffer.clear();
            finalize_shader_code( input_path, code_generator, shader_stage, constants_buffer, filename_buffer, shader_code_buffer );

            char* compiled_filename = compile_shader( input_path, code_generator, shader_stage, filename_buffer, shader_code_buffer, 0 );
            compilation_succeeded = compilation_succeeded && ( compiled_filename != nullptr );

            if ( !compilation_succeeded ) {
                break;
            }
            // From here code compilation is successful.
            
            if ( generate_reflection_data ) {
                char* spirv_cross_path = filename_buffer.append_use_f( "%sspirv-cross.exe", code_generator->shader_binaries_path );
                reflection_filename = filename_buffer.append_use_f( "%s.json", compiled_filename );
                char* arguments = filename_buffer.append_use_f( "spirv-cross.exe %s --reflect --output %s", compiled_filename, reflection_filename );
                hydra::process_execute( ".", spirv_cross_path, arguments );

                // Open generated reflection json file.
                using json = nlohmann::json;
                char* reflection_json_memory = hydra::file_read_text( reflection_filename, code_generator->parser->allocator, nullptr );
                json reflection_json = json::parse( reflection_json_memory );

                // Search binding points from reflection data
                append_reflection_data( reflection_json, s_shader_compiler_stage[ shader_stage.stage ], &reflection_buffer, code_generator->name_to_type,
                                        filename_buffer, pass_bindings, code_generator->parser->allocator );

                if ( ( compile_options & CompileOptions_Output_Files ) != CompileOptions_Output_Files )
                    hydra::file_delete( reflection_filename );
            }

            // Read output file and write it into the blob.
            hydra::FileReadResult file = hydra::file_read_binary( compiled_filename, code_generator->parser->allocator );

            ShaderCodeBlueprint& shader_blueprint = pass_blueprint.shaders[ s ];
            shader_blueprint.stage = (u8)shader_stage.stage;
            blob.allocate_and_set( shader_blueprint.code, (u32)file.size, file.data );

            hfree( file.data, code_generator->parser->allocator );

            if ( !keep_intermediate ) {
                hydra::file_delete( compiled_filename );
            }
        }

        // Render state
        if ( pass.render_state ) {
            
            blob.allocate_and_set<RenderStateBlueprint>( pass_blueprint.render_state, ( void* )&pass.render_state->rasterization );
        }
        else {
            pass_blueprint.render_state.offset = 0;
        }

        // Vertex input
        if ( pass.vertex_layout ) {

            blob.allocate_and_set<hydra::gfx::VertexAttribute>( pass_blueprint.vertex_attributes, ( u32 )pass.vertex_layout->attributes.size(), ( void* )&pass.vertex_layout->attributes[ 0 ] );
            blob.allocate_and_set<hydra::gfx::VertexStream>( pass_blueprint.vertex_streams, (u32)pass.vertex_layout->streams.size(), ( void* )&pass.vertex_layout->streams[ 0 ] );
        }
        else {
            pass_blueprint.vertex_attributes.size = 0;
            pass_blueprint.vertex_streams.size = 0;
        }

        // Create resource list to reflection map
        hydra::FlatHashMap<u64, u32> name_to_binding;
        name_to_binding.init( code_generator->parser->allocator, 16 );
        name_to_binding.set_default_value( u32_max );

        for ( u32 rb = 0; rb < 32; ++rb ) {
            if ( pass_bindings[ rb ].count != 0 ) {
                name_to_binding.insert( hydra::hash_calculate( pass_bindings[ rb ].name ), pass_bindings[ rb ].start );
            }
        }

        const u32 num_layouts = (u32)pass.resource_lists.size();
        blob.allocate_and_set<ResourceLayoutBlueprint>( pass_blueprint.resource_layouts, num_layouts + automatic_layout ? 1 : 0 );
        for ( u32 l = 0; l < num_layouts; ++l ) {
            ResourceList* resource_list = (ResourceList*)pass.resource_lists[ l ];

            const u8 num_resources = ( u8 )resource_list->resources.size();
            // Patch binding points and output layout indices
            for ( u32 r = 0; r < num_resources; ++ r) {
                ResourceBinding& rb = resource_list->resources[ r ];
                u16 binding_point = (u16)name_to_binding.get( hydra::hash_calculate( rb.name ) );
                rb.start = binding_point;

                reflection_buffer.append_f( "\t\tstatic const uint32_t layout_%s = %u;\n", rb.name, r );
            }

            ResourceLayoutBlueprint& resource_layout_blueprint = pass_blueprint.resource_layouts[ l ];
            blob.allocate_and_set<ResourceBinding>( resource_layout_blueprint.bindings, num_resources, (void*)resource_list->resources.data() );
        }

        char* pass_name_c = filename_buffer.append_use( pass.name );

        // Output Table to quickly setup resources
        if ( generate_reflection_data ) {
            reflection_buffer.append_f( "\n\t\tstruct Table {\n\t\t\tTable& reset() {\n\t\t\t\trlc = &tables[ pass_%s ];\n\t\t\t\trlc->reset();\n\t\t\t\treturn *this;\n\t\t\t}\n", pass_name_c );
            
            for ( u32 l = 0; l < num_layouts; ++l ) {
                ResourceList* resource_list = ( ResourceList* )pass.resource_lists[ l ];

                const u8 num_resources = ( u8 )resource_list->resources.size();
                // Patch binding points and output layout indices
                for ( u32 r = 0; r < num_resources; ++r ) {
                    ResourceBinding& rb = resource_list->resources[ r ];

                    switch ( rb.type ) {
                        case hydra::gfx::ResourceType::Constants:
                        case hydra::gfx::ResourceType::StructuredBuffer:
                        {
                            reflection_buffer.append_f( "\t\t\tTable& set_%s( hydra::gfx::Buffer* buffer ) {\n", rb.name );
                            reflection_buffer.append_f( "\t\t\t\trlc->buffer( buffer->handle, layout_%s );\n\t\t\t\treturn *this;\n\t\t\t}\n", rb.name );
                            break;
                        }

                        case hydra::gfx::ResourceType::Texture:
                        {
                            reflection_buffer.append_f( "\t\t\tTable& set_%s( hydra::gfx::Texture* texture ) {\n", rb.name );
                            reflection_buffer.append_f( "\t\t\t\trlc->texture( texture->handle, layout_%s );\n\t\t\t\treturn *this;\n\t\t\t}\n", rb.name );
                            break;
                        }
                    }
                }
            }

            reflection_buffer.append_f( "\n\t\t\thydra::gfx::ResourceListCreation* rlc;\n\t\t}; // struct Table\n\n" );
            reflection_buffer.append_f( "\n\t\tstatic Table& table() { static Table s_table; return s_table; }\n\n" );
        }
        
        // Optionally if properties are present but no layout is specified for them, add the final resource layout.        
        if ( automatic_layout ) {
            ResourceList automatic_resource_list;

            for ( u32 rb = 0; rb < 32; ++rb ) {
                if ( pass_bindings[ rb ].count != 0 ) {
                    hfx::ResourceBinding& binding = pass_bindings[ rb ];
                    automatic_resource_list.resources.push_back( binding );

                    // Save the layout binding into the reflection buffer
                    reflection_buffer.append_f( "\t\tstatic const uint32_t layout_%s = %u;\n", binding.name, (u32)automatic_resource_list.resources.size() - 1);
                }
            }

            ResourceLayoutBlueprint& resource_layout_blueprint = pass_blueprint.resource_layouts[ num_layouts ];
            blob.allocate_and_set<ResourceBinding>( resource_layout_blueprint.bindings, (u32)automatic_resource_list.resources.size(), ( void* )automatic_resource_list.resources.data() );
        }

        reflection_buffer.append_f( "\t} // pass %s\n\n", pass_name_c );
    }


    // Output to HFX and generated c++ file if compilation is good
    if ( compilation_succeeded ) {
        filename_buffer.clear();
        char* output_name = filename_buffer.append_use_f( "%s", output_filename );
        hydra::file_write_binary( output_name, blob.blob_memory, blob.allocated_offset );

        if ( generate_reflection_data ) {
            if ( !hydra::directory_exists( code_generator->cpp_generated_folder ) ) {
                hprint( "Directory %s does not exists! Creating it.\n", code_generator->cpp_generated_folder );

                if ( !hydra::directory_create( code_generator->cpp_generated_folder ) ) {
                    hprint( "Error creating directory %s! Cannot output generated shader generated file. Quitting.\n", code_generator->cpp_generated_folder );
                    return;
                }
            }

            char* generated_output_name = filename_buffer.reserve( 512 );
            strcpy( generated_output_name, output_filename );
            hydra::file_name_from_path( generated_output_name );

            const char* filename_string = filename_buffer.append_use_f( "%s//%s.h", code_generator->cpp_generated_folder, generated_output_name );
            FILE* reflection_file = fopen( filename_string, "w+" );
            if ( reflection_file ) {
                const char* shader_name = filename_buffer.append_use( code_generator->parser->shader.name );
                reflection_buffer.append_f( "\n} // shader %s\n", shader_name );

                fwrite( reflection_buffer.data, reflection_buffer.current_size, 1, reflection_file );
                fclose( reflection_file );
            } else {
                hprint( "Could not create file generated shader file %s.\n", filename_string );
            }
        }
    }    

    blob.shutdown();

#if 0
    {
        ShaderEffectFile hfx_originial;
        shader_effect_init( hfx_originial, output_filename, code_generator->parser->allocator );

        hydra::gfx::PipelineCreation pc;
        shader_effect_get_pipeline( hfx_originial, 0, pc );

        hydra::gfx::ShaderStateCreation::Stage& s = pc.shaders.stages[ 0 ];

        cstring old_data = s.code;
        u32 off = hfx_blueprint->passes[ 0 ].shaders[ 0 ].start;
        char* new_data = blob.memory + off;

        hprint( "DAK\n" );
    }
#endif
}
//
//
void code_generator_generate_embedded_file( CodeGenerator* code_generator, const char* output_filename ) {

    // -------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // Shader Effect File Format
    // -------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // | Header     | Pass Offset List | Pass Section 0                                                                                                                   | Pass Section 1
    // -------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // |            |                  |                  Pass Header                     |                  Pass Data
    // -------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // |            |                  | Shaders count | Res Count | Res List Offset | name | (Render States | Vertex Input)* | Shader Chunk List | Shader Code | Res List
    // -------------------------------------------------------------------------------------------------------------------------------------------------------------------

    // Pass Section:
    // |                  Pass Header                     |                 Pass Data
    // ---------------------------------------------------------------------------------------------------------------------------------
    // Shaders Count | Res Count | Res List Offset | name | (Render States | Vertex Input)* | Shader Chunk List | Shader Code | Res List
    // ---------------------------------------------------------------------------------------------------------------------------------

    // Alias for string buffers used in the process.
    StringBuffer& filename_buffer = code_generator->string_buffers[ 0 ];
    StringBuffer& shader_code_buffer = code_generator->string_buffers[1];
    StringBuffer& pass_offset_buffer = code_generator->string_buffers[2];
    StringBuffer& shader_chunk_list_buffer = code_generator->string_buffers[3];
    StringBuffer& pass_buffer = code_generator->string_buffers[4];
    StringBuffer& constants_buffer = code_generator->string_buffers[5];
    StringBuffer& constants_defaults_buffer = code_generator->string_buffers[6];
    StringBuffer& input_path_buffer = code_generator->string_buffers[ 7 ];
    StringBuffer& reflection_buffer = code_generator->string_buffers[ 8 ];

    pass_offset_buffer.clear();
    pass_buffer.clear();
    constants_buffer.clear();
    constants_defaults_buffer.clear();
    input_path_buffer.clear();
    reflection_buffer.clear();

    // Calculate input path
    const char* input_path = code_generator->parser->source_path;

    // Open reflection file if needed
    FILE* reflection_file = nullptr;

    const bool generate_reflection_data = ( ( code_generator->options & CompileOptions_Reflection_CPP ) == CompileOptions_Reflection_CPP) ||
                                          ( ( code_generator->options & CompileOptions_Reflection_Reload ) == CompileOptions_Reflection_Reload );

    if ( generate_reflection_data ) {
        
        /*const char* filename_string = filename_buffer.append_use_f( "%s.h", output_filename );
        reflection_file = fopen( filename_string, "w+" );

        
        fprintf( reflection_file, "namespace %s {\n", shader_name );

        filename_buffer.clear();*/
        const char* shader_name = filename_buffer.append_use( code_generator->parser->shader.name );
        reflection_buffer.append_f( "namespace %s {\n", shader_name );
    }

    //
    // 1. Generate common GLSL and default values. ////////////////////////////////////////////
    //

    // Generate the constants code to inject into the GLSL shader.
    // For now this will be the numerical properties with padding, in the future either that
    // or the manual layout.
    generate_glsl_and_defaults( code_generator->parser->shader, constants_buffer, constants_defaults_buffer, *code_generator->parser->lexer->data_buffer );


    //
    // 2. Build Pass Sections and save them into StringBuffers. ///////////////////////////////
    //

    const uint32_t pass_count = (uint32_t)code_generator->parser->shader.passes.size();

    // Pass sections offset starts after header and list of passes offsets.
    uint32_t pass_section_offset = sizeof( ShaderEffectFile::Header ) + sizeof( uint32_t ) * pass_count;

    ShaderEffectFile::PassHeader pass_header;

    bool compilation_succeeded = true;

    for ( uint32_t i = 0; i < pass_count; i++ ) {

        pass_offset_buffer.append_m( &pass_section_offset, sizeof( uint32_t ) );

        const Pass& pass = code_generator->parser->shader.passes[i];
        const uint32_t pass_shader_stages = (uint32_t)pass.shader_stages.size();

        // ----------------------------------------------
        // Pass Data
        // ----------------------------------------------
        // (Render States | Vertex Input)* | Shader Chunk List | Shader Code | Res List    (* optionals)
        // ----------------------------------------------
        // ShaderChunk = Shader Offset + Count

        const size_t vertex_input_size = pass.vertex_layout ? pass.vertex_layout->attributes.size() * sizeof( hydra::gfx::VertexAttribute ) + pass.vertex_layout->streams.size() * sizeof( hydra::gfx::VertexStream ) : 0;
        const size_t shader_list_offset = vertex_input_size + (pass.render_state ? sizeof( hydra::gfx::RasterizationCreation ) + sizeof( hydra::gfx::DepthStencilCreation ) + sizeof( hydra::gfx::BlendStateCreation ) : 0);
        //
        // 2.1 For current pass calculate shader code offsets, relative to the pass section start.

        const uint32_t start_shader_code_offset = (uint32_t)shader_list_offset + (pass_shader_stages * sizeof( ShaderEffectFile::ShaderChunk )) + sizeof( ShaderEffectFile::PassHeader );
        uint32_t current_shader_code_offset = start_shader_code_offset;

        shader_chunk_list_buffer.clear();
        shader_code_buffer.clear();

        const bool automatic_layout = is_resources_layout_automatic( code_generator->parser->shader, pass );
        uint32_t total_resources_layout = 0, local_resources = 0;

        //
        // 2.2 For each shader stage (vertex, fragment, compute...), finalize code and save offsets.

        // Free the types map, used by reflection
        
        // TODO: map
        //string_hash_free( code_generator->name_to_type );

        for ( size_t s = 0; s < pass_shader_stages; ++s ) {
            const Pass::ShaderStage shader_stage = pass.shader_stages[s];

            compilation_succeeded = compilation_succeeded && append_finalized_shader_code( input_path, code_generator, shader_stage, filename_buffer, shader_code_buffer, constants_buffer, &reflection_buffer, &pass.name );

            if ( !compilation_succeeded ) {
                break;
            }

            update_shader_chunk_list( &current_shader_code_offset, start_shader_code_offset, shader_chunk_list_buffer, shader_code_buffer );

            // Manually count resources for automatic layout.
            // This needs to be done on a per pass level.
            if ( automatic_layout ) {
                for ( size_t p = 0; p < shader_stage.code->resources.size(); p++ ) {
                    const hfx::CodeFragment::Resource& resource = shader_stage.code->resources[p];

                    switch ( resource.type ) {
                        case hydra::gfx::ResourceType::ImageRW:
                        case hydra::gfx::ResourceType::Texture:
                        case hydra::gfx::ResourceType::Constants:
                        {
                            ++local_resources;
                            break;
                        }
                    }
                }
            }
        }

        // Update pass offset to the resource list sub-section
        pass_section_offset += shader_code_buffer.current_size + start_shader_code_offset;

        // Add local constant buffer in the count only if automatic layout is needed.
        if ( automatic_layout ) {
            ++local_resources;
            ++total_resources_layout;
        }
        // Add also the resource list declared
        total_resources_layout += (uint32_t)pass.resource_lists.size();

        // Fill Pass Header
        StringRef::copy_to( pass.name, pass_header.name, 32 );
        StringRef::copy_to( pass.stage_name, pass_header.stage_name, 32 );
        pass_header.num_shader_chunks = (uint8_t)pass_shader_stages;
        pass_header.num_resource_layouts = (uint8_t)total_resources_layout;
        pass_header.resource_table_offset = shader_code_buffer.current_size + start_shader_code_offset;
        pass_header.has_resource_state = pass.render_state ? 1 : 0;
        pass_header.shader_list_offset = (uint16_t)shader_list_offset;
        pass_header.num_vertex_attributes = pass.vertex_layout ? (uint8_t)pass.vertex_layout->attributes.size() : 0;
        pass_header.num_vertex_streams = pass.vertex_layout ? (uint8_t)pass.vertex_layout->streams.size() : 0;
        pass_header.is_spirv = ( ( code_generator->options & CompileOptions_SpirV ) == CompileOptions_SpirV ) ? 1 : 0;
        pass_header.compute_dispatch = pass.compute_dispatch;

        pass_buffer.append_m( (void*)&pass_header, sizeof( ShaderEffectFile::PassHeader ) );

        write_render_states( pass, pass_buffer );
        write_vertex_input( pass, pass_buffer );
        
        pass_buffer.append( shader_chunk_list_buffer );
        pass_buffer.append( shader_code_buffer );

        //
        // 2.3. Write resources layout, either automatic and manually specified. ///////////////////////////
        //      Pass section offset must be updated for the next pass offset to be correct.

        // 2.3.1: First add all the declared resources in order of declaration.
        write_resources_layout( pass, pass_buffer, pass_section_offset );

        // 2.3.2: Optionally if properties are present but no layout is specified for them, add the final resource layout.
        if ( automatic_layout ) {
            write_automatic_resources_layout( pass, pass_buffer, pass_section_offset );
        }
    }

    //
    // 3. Write default local constant values, to be used when creating the effect. ///////////
    //

    // After all pass sections there is the default resources section.
    StringBuffer& resources_buffer = code_generator->string_buffers[7];
    resources_buffer.clear();

    write_default_values( constants_defaults_buffer, resources_buffer, code_generator->parser->shader );

    // Fill the file header
    ShaderEffectFile::Header file_header;
    memcpy( file_header.binary_header_magic, code_generator->binary_header_magic, 32 );
    file_header.num_passes = pass_count;
    file_header.resource_defaults_offset = sizeof( ShaderEffectFile::Header ) + pass_offset_buffer.current_size + pass_buffer.current_size;
    file_header.properties_offset = file_header.resource_defaults_offset + resources_buffer.current_size;
    StringRef::copy_to( code_generator->parser->shader.name, file_header.name, 32 );


    //
    // 4. Actually write the file /////////////////////////////////////////////////////////////
    //
    if ( compilation_succeeded ) {

        if ( generate_reflection_data ) {

            const char* filename_string = filename_buffer.append_use_f( "%s.h", output_filename );
            reflection_file = fopen( filename_string, "w+" );
            reflection_buffer.append( "} //\n" );

            fwrite( reflection_buffer.data, reflection_buffer.current_size, 1, reflection_file );
            fclose( reflection_file );
        }

        FILE* output_file;
        filename_buffer.clear();
        const char* filename_string = filename_buffer.append_use( output_filename );
        fopen_s( &output_file, filename_string, "wb" );

        if ( !output_file ) {
            perror( output_filename );
            printf( "Error opening file %s. Aborting. \n", output_filename );
            return;
        }


        // 4.1. Write the header
        fwrite( &file_header, sizeof( ShaderEffectFile::Header ), 1, output_file );
        // 4.2. Write the pass memory offsets
        fwrite( pass_offset_buffer.data, pass_offset_buffer.current_size, 1, output_file );
        // 4.3. Write the pass sections
        fwrite( pass_buffer.data, pass_buffer.current_size, 1, output_file );
        // 4.4. Write the resource defaults
        fwrite( resources_buffer.data, resources_buffer.current_size, 1, output_file );

        // 4.5. Write properties in string buffer.
        resources_buffer.clear();
        write_properties( resources_buffer, code_generator->parser->shader, *code_generator->parser->lexer->data_buffer );

        //
        // 5. Write properties to file. ///////////////////////////////////////////////////////////
        //
        fwrite( resources_buffer.data, resources_buffer.current_size, 1, output_file );

        fclose( output_file );
    }
    else {
        hprint( "Error compiling shader %s. No binary generated.\n", input_path );
    }
}

//
//
void code_generator_generate_shader_cpp_header( CodeGenerator* code_generator, const char* path ) {
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

    constants_ui_method.append( "\tvoid reflectMembers() {\n" );

    buffer_class.append( "struct LocalConstantsBuffer {\n\n\thydra::gfx::BufferHandle\tbuffer;\n" );
    buffer_class.append( "\tLocalConstants\t\t\t\t\tconstants;\n\tLocalConstantsUI\t\t\t\tconstantsUI;\n\n" );
    buffer_class.append( "\tvoid create( hydra::gfx::Device& device ) {\n\t\tusing namespace hydra;\n\n" );
    buffer_class.append( "\t\tgraphics::BufferCreation constants_creation = { graphics::BufferType::Constant, graphics::ResourceUsageType::Dynamic, sizeof( LocalConstants ), &constants, \"LocalConstants\" };\n" );
    buffer_class.append( "\t\tbuffer = device.create_buffer( constants_creation );\n\t}\n\n" );
    buffer_class.append( "\tvoid destroy( hydra::gfx::Device& device ) {\n\t\tdevice.destroy_buffer( buffer );\n\t}\n\n" );
    buffer_class.append( "\tvoid updateUI( hydra::gfx::Device& device ) {\n\t\t// Draw UI\n\t\tconstantsUI.reflectUI();\n\t\t// Update constants from UI\n" );
    buffer_class.append( "\t\thydra::gfx::MapBufferParameters map_parameters = { buffer.handle, 0, 0 };\n" );
    buffer_class.append( "\t\tLocalConstants* buffer_data = (LocalConstants*)device.map_buffer( map_parameters );\n\t\tif (buffer_data) {\n" );

    // For GPU the struct must be 16 bytes aligned. Track alignment
    uint32_t gpu_struct_alignment = 0;

    const DataBuffer& data_buffer = *code_generator->parser->lexer->data_buffer;
    // For each property write code
    for ( size_t i = 0; i < shader.properties.size(); i++ ) {
        hfx::Property* property = shader.properties[i];

        switch ( property->type ) {
            case PropertyType::Float:
            {
                constants_ui.append( "\tfloat\t\t\t\t\t" );
                constants_ui.append( property->name );

                cpu_constants.append( "\tfloat\t\t\t\t\t" );
                cpu_constants.append( property->name );

                if ( property->data_index != 0xffffffff ) {
                    float value = 0.0f;
                    data_buffer_get( data_buffer, property->data_index, value );
                    constants_ui.append_f( "\t\t\t\t= %ff", value );
                    cpu_constants.append_f( "\t\t\t\t= %ff", value );
                }

                constants_ui.append( ";\n" );

                cpu_constants.append( ";\n" );

                constants_ui_method.append( "\t\tImGui::InputScalar( \"" );
                constants_ui_method.append( property->ui_name );
                constants_ui_method.append( "\", ImGuiDataType_Float, &" );
                constants_ui_method.append( property->name );
                constants_ui_method.append( ");\n" );

                // buffer_data->scale = constantsUI.scale;
                buffer_class.append( "\t\t\tbuffer_data->" );
                buffer_class.append( property->name );
                buffer_class.append( " = constantsUI." );
                buffer_class.append( property->name );
                buffer_class.append( ";\n" );

                ++gpu_struct_alignment;

                break;
            }

            case PropertyType::Int:
            {
                break;
            }

            case PropertyType::Range:
            {
                break;
            }

            case PropertyType::Color:
            {
                break;
            }

            case PropertyType::Vector:
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
    cpu_constants.append_f( "\tfloat\t\t\t\t\tpad_tail[%u];\n\n", tail_padding_size );

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

// HFX interface ////////////////////////////////////////////////////////////////

static const size_t                 k_hfx_random_seed = 0xfeba666ddea21a46;

//
//
bool hfx_compile( const char* input_filename, const char* output_filename, u32 options, cstring cpp_generated_folder, bool force_rebuild ) {

    hydra::MallocAllocator heap_allocator;

    char* text = hydra::file_read_text( input_filename, &heap_allocator, nullptr );
    if ( !text ) {
        HYDRA_LOG( "Error compiling file %s: file not found.\n", input_filename );
        return false;
    }

#if defined(HY_STB)
    set_rand_seed( k_hfx_random_seed );
    size_t source_file_hash = hash_string( text, k_hfx_random_seed );
#else
    size_t source_file_hash = 0;
#endif // HY_STB

    hydra::FileTime file_time = hydra::file_last_write_time( input_filename );

    // Check if the binary was generated from the same file.
    // If so do not compile.
    if ( !force_rebuild && hydra::file_exists( output_filename ) ) {

#if defined HFX_V2
        static const u32 binary_header_size = 32 + sizeof( hydra::BlobHeader );
        char binary_header_magic[ binary_header_size ];

        hydra::FileHandle file;
        hydra::file_open( output_filename, "rb", &file );
        
        fread( binary_header_magic, binary_header_size, 1, file );
        hydra::file_close( file );

        // Get the file time from the header
        hydra::FileTime saved_filetime;
        memcpy( &saved_filetime, binary_header_magic + sizeof(hydra::BlobHeader), sizeof( hydra::FileTime ) );

        if ( file_time.dwHighDateTime == saved_filetime.dwHighDateTime &&
             file_time.dwLowDateTime == saved_filetime.dwLowDateTime ) {

            hfree( text, &heap_allocator );
            // TODO memory (not anymore) Allocator still has allocations from hfx_memory.

            return true;
        }
#else
        ShaderEffectFile::Header file_header;

        hydra::FileHandle file;
        hydra::file_open( output_filename, "rb", &file );
        fread( &file_header, sizeof( ShaderEffectFile::Header ), 1, file );
        hydra::file_close( file );

        // Get the file time from the header
        hydra::FileTime saved_filetime;
        memcpy( &saved_filetime, file_header.binary_header_magic, sizeof( hydra::FileTime ) );

        if ( file_time.dwHighDateTime == saved_filetime.dwHighDateTime &&
             file_time.dwLowDateTime == saved_filetime.dwLowDateTime ) {

            hfree( text, &heap_allocator );
            // TODO memory (not anymore) Allocator still has allocations from hfx_memory.

            return true;
        }
#endif // HFX_V2            
    }

    Lexer lexer;
    DataBuffer data_buffer;

    char input_path[512];
    strcpy( input_path, input_filename );
    hydra::file_directory_from_path( input_path );

    char output_path[512];
    strcpy( output_path, output_filename );
    hydra::file_directory_from_path( output_path );

    if ( !hydra::directory_exists( output_path ) ) {
        hprint( "Output directory does not exists, creating it.\n", output_path );
        if ( !hydra::directory_create( output_path ) ) {
            hprint( "Problems creating output path %s. Quitting.\n", output_path );
            return false;
        }
    }

    data_buffer_init( &data_buffer, 256, 2048 );
    lexer_init( &lexer, text, &data_buffer );

#if 0
    hprint( "S %s\n", text );
#endif 

    Parser parser;
    parser_init( &parser, &lexer, &heap_allocator, input_path, input_filename, output_path );
    parser_generate_ast( &parser );

    CodeGenerator code_generator;
    code_generator_init( &code_generator, &parser, 256 * 1024, 9 );

    // Cache source file time
    memcpy( code_generator.binary_header_magic, &file_time, sizeof( hydra::FileTime ) );
    // Init header magic
    memcpy( &code_generator.binary_header_magic[sizeof( hydra::FileTime )], &source_file_hash, sizeof( size_t ) );

    // Prepare environment for compilation.
    hydra::StringBuffer& filename_buffer = code_generator.string_buffers[0];
    filename_buffer.clear();

    // Calculate path of glsl compiler
    // if (options ...)
    char* vulkan_env = filename_buffer.reserve( 512 );
    hydra::environment_variable_get( "%VULKAN_SDK%", vulkan_env, 512 );

    // Cache paths
    code_generator.path_buffer.clear();
    code_generator.shader_binaries_path = code_generator.path_buffer.append_use_f( "%s\\Bin\\", vulkan_env );
    code_generator.cpp_generated_folder = cpp_generated_folder;
    code_generator.source_folder_path = code_generator.path_buffer.append_use_f( "%s", input_path );
    code_generator.destination_folder_path = code_generator.path_buffer.append_use_f( "%s", output_path );

    // Clear buffer to be used inside compilation.
    filename_buffer.clear();

    // Cache options
    code_generator.options = options;

    // If compiling with Vulkan, always force SpirV output. This is optional for GLSL.
    if ( (code_generator.options & CompileOptions_Vulkan) == CompileOptions_Vulkan ) {
        code_generator.options |= CompileOptions_SpirV;
    }

    if ( (options & CompileOptions_Embedded) == CompileOptions_Embedded ) {
        //hfx::code_generator_generate_embedded_file( &code_generator, output_filename );
        // Test new hfx binary
        hfx::code_generator_generate_embedded_file_v2( &code_generator, output_filename );
    }
    else {
        hfx::code_generator_output_shader_files( &code_generator, output_filename );
    }
    
    hfx::parser_terminate( &parser );
    hfx::code_generator_terminate( &code_generator );
    hfree( text, &heap_allocator );

    // Optional: init the shader effect if present
    // TODO memory: this is causing memory leaks.
    /*if ( out_shader_effect_file ) {
        char* hfx_memory = hydra::file_read_binary( output_filename, heap_allocator, nullptr );
        shader_effect_init( *out_shader_effect_file, hfx_memory );
    }*/

    // TODO memory (not anymore): allocator still have allocations when compiling for the first time.

    return true;
}

//
// Inspect and print informations about HFX binary file.
void hfx_inspect( const char* binary_filename ) {

#if defined(HFX_V2)

    hy_assertm( false, "Not implemented!" );

#else
    hydra::HeapAllocator heap_allocator;
    heap_allocator.init( 1 * 1024 * 1024 );

    char* text = hydra::file_read_binary( binary_filename, &heap_allocator, nullptr );
    if ( !text ) {
        HYDRA_LOG( "Error compiling file %s: file not found.\n", binary_filename );
        heap_allocator.shutdown();
        return;
    }

    ShaderEffectFile hfx_file;
    shader_effect_init( hfx_file, text );

    using namespace hydra::gfx;

    HYDRA_LOG( "//////////      HFX Inspection\n" );
    HYDRA_LOG( "// Name: %s\n", hfx_file.header->name );
    HYDRA_LOG( "// Passes: %u\n//\n", hfx_file.header->num_passes );

    const uint32_t num_passes = hfx_file.header->num_passes;
    for ( uint32_t i = 0; i < num_passes; i++ ) {
        ShaderEffectFile::PassHeader* pass = shader_effect_get_pass( hfx_file.memory, i );

        HYDRA_LOG( "//// Pass %u %s\n////\n", i, pass->name );
        HYDRA_LOG( "// Stage name %s\n", pass->stage_name );
        HYDRA_LOG( "// Resource Layouts %u\n", pass->num_resource_layouts );
        HYDRA_LOG( "// Shader Chunks %u\n", pass->num_shader_chunks );
        HYDRA_LOG( "// Vertex Attributes %u\n", pass->num_vertex_attributes );
        HYDRA_LOG( "// Vertex Streams %u\n", pass->num_vertex_streams );
        HYDRA_LOG( "// Resource Table Offset %u\n", pass->resource_table_offset );
        HYDRA_LOG( "// Shader List Offset %u\n", pass->shader_list_offset );

        PipelineCreation pipeline;
        shader_effect_pass_get_pipeline( pass, pipeline );

        HYDRA_LOG( "////// Shader %s\n", pipeline.shaders.name );

        for ( uint32_t j = 0; j < pipeline.shaders.stages_count; j++ ) {
            const ShaderStateCreation::Stage& shader_stage = pipeline.shaders.stages[j];

            HYDRA_LOG( "//Stage %s code:\n%s\n", ShaderStage::ToString( shader_stage.type ), shader_stage.code );
        }

        HYDRA_LOG( "////// Resource List Layouts %u\n", pipeline.num_active_layouts );

        for ( uint32_t j = 0; j < pipeline.num_active_layouts; j++ ) {
            uint8_t num_bindings;
            const auto bindings = hfx::shader_effect_pass_get_layout_bindings( pass, j, num_bindings );

            HYDRA_LOG( "// Layout %u\n", j );

            for ( uint32_t b = 0; b < num_bindings; b++ ) {
                const auto binding = bindings[b];
                HYDRA_LOG( "//// Binding %s, type %s\n", binding.name, ResourceType::ToString( binding.type ) );
            }
        }

        HYDRA_LOG( "//\n////// Blend States (active %u)\n", pipeline.blend_state.active_states );
        for ( uint32_t j = 0; j < pipeline.blend_state.active_states; j++ ) {
            const BlendState& blend_state = pipeline.blend_state.blend_states[j];
            HYDRA_LOG( "// Enabled %u\n", blend_state.blend_enabled );
        }

        // DepthStencil
        // Rasterization
        // Vertex
    }

    HYDRA_LOG( "//////////      END HFX Inspection\n" );

    hfree( text, &heap_allocator );
    heap_allocator.shutdown();

#endif // HFX_V2
}

//
//
void hfx_inspect_imgui( ShaderEffectFile& bhfx_file ) {


#if defined(HFX_V2)

    hy_assertm( false, "Not implemented!" );

#else

#if defined (HYDRA_IMGUI)

    if (ImGui::BeginChild( "hfx_inspect" )) {

        using namespace hydra::gfx;

        ImGui::Text( "HFX: %s", bhfx_file.header->name );

        if ( ImGui::TreeNode( "Passes" ) ) {

            const uint32_t num_passes = bhfx_file.header->num_passes;
            for ( uint32_t i = 0; i < num_passes; i++ ) {
                ShaderEffectFile::PassHeader* pass = shader_effect_get_pass( bhfx_file.memory, i );

                // Expand pass
                if ( ImGui::TreeNode( pass->name ) ) {

                    PipelineCreation pipeline;
                    shader_effect_pass_get_pipeline( pass, pipeline );

                    ImGui::Separator();
                    // Expand shaders
                    for ( uint32_t j = 0; j < pipeline.shaders.stages_count; j++ ) {
                        const ShaderStateCreation::Stage& shader_stage = pipeline.shaders.stages[j];
                        if ( ImGui::TreeNode( ShaderStage::ToString( shader_stage.type ) ) ) {

                            ImGui::InputTextMultiline( "##source", (char*)shader_stage.code, shader_stage.code_size, ImVec2( -FLT_MIN, ImGui::GetTextLineHeight() * 16 ), ImGuiInputTextFlags_ReadOnly );
                            ImGui::TreePop();

                        }
                    }

                    ImGui::Separator();

                    // Expand render states/pipeline
                    if ( ImGui::TreeNode( "Input Assembly" ) ) {

                        for ( uint32_t j = 0; j < pipeline.vertex_input.num_vertex_streams; j++ ) {
                            const auto vertex_stream = pipeline.vertex_input.vertex_streams[j];
                            ImGui::Text( "Binding %u, stride %u, rate %s", vertex_stream.binding, vertex_stream.stride, VertexInputRate::ToString( vertex_stream.input_rate ) );
                        }

                        for ( uint32_t j = 0; j < pipeline.vertex_input.num_vertex_attributes; j++ ) {
                            const auto vertex_attribute = pipeline.vertex_input.vertex_attributes[j];
                            ImGui::Text( "Binding %u, format %s, location %u, offset %u", vertex_attribute.binding, VertexComponentFormat::ToString(vertex_attribute.format),
                                         vertex_attribute.location, vertex_attribute.offset );
                        }

                        ImGui::TreePop();
                    }

                    // Expand 
                    if ( ImGui::TreeNode( "Depth Stencil" ) ) {

                        const auto depth_stencil = pipeline.depth_stencil;
                        ImGui::Text( "Enable %u, write %u, test %s", depth_stencil.depth_enable, depth_stencil.depth_write_enable, ComparisonFunction::ToString(depth_stencil.depth_comparison) );
                        ImGui::Text( "Stencil enable %u", depth_stencil.stencil_enable );

                        ImGui::Text( "   Front - compare %s, compare_mask %u, write_mask %u, ref %u, fail %s, pass %s, depth_fail %s", ComparisonFunction::ToString( depth_stencil.front.compare ),
                                     depth_stencil.front.compare_mask, depth_stencil.front.write_mask, depth_stencil.front.reference,
                                     StencilOperation::ToString( depth_stencil.front.fail ), StencilOperation::ToString( depth_stencil.front.pass ),
                                     StencilOperation::ToString( depth_stencil.front.depth_fail ) );

                        ImGui::Text( "   Back - compare %s, compare_mask %u, write_mask %u, ref %u, fail %s, pass %s, depth_fail %s", ComparisonFunction::ToString( depth_stencil.back.compare ),
                                     depth_stencil.back.compare_mask, depth_stencil.back.write_mask, depth_stencil.back.reference,
                                     StencilOperation::ToString( depth_stencil.back.fail ), StencilOperation::ToString( depth_stencil.back.pass ),
                                     StencilOperation::ToString( depth_stencil.back.depth_fail ) );

                        ImGui::TreePop();
                    }

                    if ( ImGui::TreeNode( "Blending" ) ) {

                        const auto blending = pipeline.blend_state;
                        ImGui::Text( "Active blends %u", blending.active_states );
                        for ( uint32_t j = 0; j < blending.active_states; j++ ) {
                            const BlendState& state = blending.blend_states[j];
                            ImGui::Text( "   Enable %u, separate blend %u, SrcColor %s, DstColor %s, ColorOp %s, SrcAlpha %s, DstAlpha %s, DstOp %s", state.blend_enabled, state.separate_blend, Blend::ToString(state.source_color), Blend::ToString(state.destination_color),
                                         BlendOperation::ToString( state.color_operation ), Blend::ToString( state.source_alpha ), Blend::ToString( state.destination_alpha ),
                                         BlendOperation::ToString( state.alpha_operation ) );
                        }

                        ImGui::TreePop();
                    }

                    // Expand resource lists
                    if ( ImGui::TreeNode( "Resource List Layouts" ) ) {
                        
                        static const char* s_layout_names[] = { "0", "1", "2", "3" };
                        for ( uint32_t j = 0; j < pipeline.num_active_layouts; j++ ) {
                            if ( ImGui::TreeNode( s_layout_names[j] ) ) {
                                uint8_t num_bindings;
                                const auto bindings = hfx::shader_effect_pass_get_layout_bindings( pass, j, num_bindings );

                                for ( uint32_t b = 0; b < num_bindings; b++ ) {
                                    const auto binding = bindings[b];
                                    ImGui::Text(" Binding %s, type %s\n", binding.name, ResourceType::ToString( binding.type ) );
                                }

                                ImGui::TreePop();
                            }
                        }
                        

                        ImGui::TreePop();
                    }

                    // Expand debug informations
                    if ( ImGui::TreeNode( "Debug" ) ) {
                        ImGui::Text( "Resource Layouts %u\n", pass->num_resource_layouts );
                        ImGui::Text( "Shader Chunks %u\n", pass->num_shader_chunks );
                        ImGui::Text( "Vertex Attributes %u\n", pass->num_vertex_attributes );
                        ImGui::Text( "Vertex Streams %u\n", pass->num_vertex_streams );
                        ImGui::Text( "Resource Table Offset %u\n", pass->resource_table_offset );
                        ImGui::Text( "Shader List Offset %u\n", pass->shader_list_offset );
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::EndChild();
    }

#endif // HYDRA_IMGUI

#endif // HFX_V2
}


#endif // HFX_COMPILER

#if !defined (HFX_V2)
// ShaderEffectFile /////////////////////////////////////////////////////////////

//
//
void shader_effect_init( ShaderEffectFile& file, const char* full_filename, hydra::Allocator* allocator ) {
    char* memory = hydra::file_read_binary( full_filename, allocator, nullptr );
    shader_effect_init( file, memory );
}

void shader_effect_init( ShaderEffectFile& file, char* memory ) {
    file.memory = memory;
    file.header = (hfx::ShaderEffectFile::Header*)file.memory;

    char* default_resources_data = file.memory + file.header->resource_defaults_offset;

    //uint32_t num_resources = *(uint32_t*)(default_resources_data);
    default_resources_data += sizeof( uint32_t );

    // Read local constants defaults
    //hydra::gfx::ResourceType::Enum resource_type = *(hydra::gfx::ResourceType::Enum*)(default_resources_data);
    default_resources_data += sizeof( hydra::gfx::ResourceType::Enum );

    file.local_constants_size = *(uint32_t*)(default_resources_data);
    file.local_constants_default_data = default_resources_data + sizeof( uint32_t );

    // Cache property access
    file.num_properties = (uint16_t)*(uint32_t*)(file.memory + file.header->properties_offset);
    file.properties_data = (file.memory + file.header->properties_offset) + sizeof( uint32_t );
}

void shader_effect_shutdown( ShaderEffectFile& file, hydra::Allocator* allocator ) {
    
    hfree( file.memory, allocator );
}

//
//
ShaderEffectFile::PassHeader* shader_effect_get_pass( char* hfx_memory, uint32_t index ) {
    const uint32_t pass_offset = *(uint32_t*)(hfx_memory + sizeof( ShaderEffectFile::Header ) + (index * sizeof( uint32_t )));
    return (ShaderEffectFile::PassHeader*)(hfx_memory + pass_offset);
}

//
//
ShaderEffectFile::MaterialProperty* shader_effect_get_property( char* properties_data, uint32_t index ) {
    return (ShaderEffectFile::MaterialProperty*)(properties_data + index * sizeof( ShaderEffectFile::MaterialProperty ));
}

//
// Helper method to create shader stages
static void shader_effect_pass_get_shader_creation( ShaderEffectFile::PassHeader* pass_header, uint32_t index, hydra::gfx::ShaderStateCreation::Stage* shader_creation ) {

    //uint32_t shader_count = pass_header->num_shader_chunks;
    char* pass_memory = (char*)pass_header;
    char* shader_offset_list_start = pass_memory + sizeof( ShaderEffectFile::PassHeader ) + pass_header->shader_list_offset;
    const uint32_t shader_offset = *(uint32_t*)(shader_offset_list_start + (index * sizeof( ShaderEffectFile::ShaderChunk )));
    char* shader_chunk_start = pass_memory + shader_offset;

    hfx::ShaderEffectFile::ChunkHeader* shader_chunk_header = (hfx::ShaderEffectFile::ChunkHeader*)(shader_chunk_start);
    shader_creation->type = (hydra::gfx::ShaderStage::Enum)shader_chunk_header->shader_stage;
    shader_creation->code_size = shader_chunk_header->code_size;
    shader_creation->code = (const char*)(shader_chunk_start + sizeof( hfx::ShaderEffectFile::ChunkHeader ));
}

//
// Local method to retrieve vertex input informations.
//
static void get_vertex_input( ShaderEffectFile::PassHeader* pass_header, hydra::gfx::VertexInputCreation& vertex_input ) {
    
    const uint32_t attribute_count = pass_header->num_vertex_attributes;
    char* pass_memory = (char*)pass_header;
    const uint32_t vertex_input_offset = pass_header->has_resource_state ? sizeof( hydra::gfx::RasterizationCreation ) + sizeof( hydra::gfx::DepthStencilCreation ) + sizeof( hydra::gfx::BlendStateCreation ) : 0;
    char* vertex_input_start = pass_memory + sizeof( ShaderEffectFile::PassHeader ) + vertex_input_offset;
    
    vertex_input.num_vertex_attributes = attribute_count;
    if ( attribute_count ) {

        //vertex_input.vertex_attributes = (const hydra::gfx::VertexAttribute*)malloc( sizeof( const hydra::gfx::VertexAttribute ) * attribute_count );
        memcpy( (void*)vertex_input.vertex_attributes, vertex_input_start, sizeof( const hydra::gfx::VertexAttribute ) * attribute_count );

        vertex_input_start += attribute_count * sizeof( hydra::gfx::VertexAttribute );
        //vertex_input.vertex_streams = (const hydra::gfx::VertexStream*)malloc( sizeof( const hydra::gfx::VertexStream ) * pass_header->num_vertex_streams );
        memcpy( (void*)vertex_input.vertex_streams, vertex_input_start, sizeof( const hydra::gfx::VertexStream ) * pass_header->num_vertex_streams );
        vertex_input.num_vertex_streams = pass_header->num_vertex_streams;
    }
    else {
        vertex_input.num_vertex_streams = 0;
    }
}

//
// Get the pipeline creation for the specific pass.
//
void shader_effect_get_pipeline( ShaderEffectFile& hfx, uint32_t pass_index, hydra::gfx::PipelineCreation& out_pipeline ) {
    ShaderEffectFile::PassHeader* pass_header = shader_effect_get_pass( hfx.memory, pass_index );

    shader_effect_pass_get_pipeline( pass_header, out_pipeline );
    out_pipeline.name = hfx.header->name;
}

//
// Get the specific list layout creation for the pass.
//
void shader_effect_get_resource_list_layout( ShaderEffectFile& hfx, uint32_t pass_index, uint32_t layout_index, hydra::gfx::ResourceLayoutCreation& out_list ) {
    ShaderEffectFile::PassHeader* pass_header = shader_effect_get_pass( hfx.memory, pass_index );

    uint8_t num_bindings;
    const hydra::gfx::ResourceLayoutCreation::Binding* bindings = hfx::shader_effect_pass_get_layout_bindings( pass_header, layout_index, num_bindings );
    for ( size_t i = 0; i < num_bindings; i++ ) {
        out_list.add_binding( bindings[ i ] );
    }

    out_list.set_name( hfx.header->name );
}

void shader_effect_get_resource_layout( ShaderEffectFile& hfx, u32 pass_index, u32 layout_index, hydra::gfx::ResourceLayoutCreation& out_list, NameToIndex* out_map ) {
    ShaderEffectFile::PassHeader* pass_header = shader_effect_get_pass( hfx.memory, pass_index );

    char buffer[ 64 ];
    uint8_t num_bindings;
    const hydra::gfx::ResourceLayoutCreation::Binding* bindings = hfx::shader_effect_pass_get_layout_bindings( pass_header, layout_index, num_bindings );
    for ( u16 i = 0; i < num_bindings; i++ ) {

        const hydra::gfx::ResourceLayoutCreation::Binding& binding = bindings[ i ];
        out_list.add_binding( binding );

        // Add entry to hash map
        sprintf( buffer, "%s_%s", pass_header->name, binding.name );
        out_map->insert( buffer, i );
        //string_hash_put( out_map, buffer, i );
    }

    out_list.set_name( hfx.header->name );
}

u32 shader_effect_get_pass_index( ShaderEffectFile& hfx, const char* name ) {
    // TODO: improve!
    const u32 passes = hfx.header->num_passes;
    for ( u32 p = 0; p < passes; ++p ) {

        hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( hfx.memory, p );
        if ( strcmp( name, pass_header->name ) == 0 ) {
            return p;
        }
    }
    return u32_max;
}

const char* shader_effect_get_pass_name( ShaderEffectFile& hfx, u32 index ) {
    const u32 passes = hfx.header->num_passes;
    if ( index < passes ) {
        const hfx::ShaderEffectFile::PassHeader* pass_header = hfx::shader_effect_get_pass( hfx.memory, index );
        return pass_header->name;
    }

    return nullptr;
}

//
// Fill the pipeline with more informations possible found in the HFX file.
//
void shader_effect_pass_get_pipeline( ShaderEffectFile::PassHeader* pass_header, hydra::gfx::PipelineCreation& out_pipeline ) {
    // get_shader_creation
    // get_vertex_input
    uint32_t shader_count = pass_header->num_shader_chunks;
    hydra::gfx::ShaderStateCreation& creation = out_pipeline.shaders;

    for ( uint16_t i = 0; i < shader_count; i++ ) {
        hfx::shader_effect_pass_get_shader_creation( pass_header, i, &creation.stages[i] );
    }

    creation.name = pass_header->name;
    creation.stages_count = shader_count;
    creation.spv_input = pass_header->is_spirv;

    hfx::get_vertex_input( pass_header, out_pipeline.vertex_input );

    if ( pass_header->has_resource_state ) {
        char* pass_memory = (char*)pass_header;
        char* render_state_memory = pass_memory + sizeof( ShaderEffectFile::PassHeader );
        memcpy( &out_pipeline.rasterization, render_state_memory, sizeof( hydra::gfx::RasterizationCreation ) + sizeof( hydra::gfx::DepthStencilCreation ) + sizeof( hydra::gfx::BlendStateCreation ) );
    }

    out_pipeline.num_active_layouts = pass_header->num_resource_layouts;
    out_pipeline.name = pass_header->name;
}

//
//
const hydra::gfx::ResourceLayoutCreation::Binding* shader_effect_pass_get_layout_bindings( ShaderEffectFile::PassHeader* pass_header, uint32_t layout_index, uint8_t& out_num_bindings ) {
    char* pass_memory = (char*)(pass_header) + pass_header->resource_table_offset;

    // Scan through all the resouce layouts.
    while ( layout_index-- ) {
        uint8_t num_bindings = (uint8_t)*pass_memory;
        pass_memory += (sizeof( uint8_t ) + num_bindings * sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) );
    }

    // Retrieve bindings count
    out_num_bindings = (uint8_t)*pass_memory;
    // Returns the bindings.
    return (const hydra::gfx::ResourceLayoutCreation::Binding*)(pass_memory + sizeof( uint8_t ));
}

#endif // HFX_V2

// ShaderPassBlueprint ////////////////////////////////////////////////////
void ShaderPassBlueprint::fill_pipeline( hydra::gfx::PipelineCreation& out_pipeline ) {

    // Shader
    hydra::gfx::ShaderStateCreation& shader_creation = out_pipeline.shaders;
    const u32 shader_count = shaders.size;

    shader_creation.reset().set_spv_input( is_spirv ).set_name( name );
    for ( u32 s = 0; s < shader_count; ++s ) {
        hfx::ShaderCodeBlueprint& shader_code = shaders[ s ];
        shader_creation.add_stage( ( cstring )shader_code.code.data.get(), shader_code.code.size, ( hydra::gfx::ShaderStage::Enum )shader_code.stage );
    }

    // Vertex input
    if ( vertex_streams.size ) {

        hydra::gfx::VertexInputCreation& vertex_input = out_pipeline.vertex_input;

        vertex_input.num_vertex_attributes = vertex_attributes.size;
        vertex_input.num_vertex_streams = vertex_streams.size;

        memcpy( ( void* )vertex_input.vertex_attributes, vertex_attributes.get(), sizeof( const hydra::gfx::VertexAttribute ) * vertex_attributes.size );
        memcpy( ( void* )vertex_input.vertex_streams, vertex_streams.get(), sizeof( const hydra::gfx::VertexStream ) * vertex_streams.size );
    }

    // Render States
    if ( render_state.is_not_null() ) {
        memcpy( &out_pipeline.rasterization, render_state.get(), sizeof( hydra::gfx::RasterizationCreation ) + sizeof( hydra::gfx::DepthStencilCreation ) + sizeof( hydra::gfx::BlendStateCreation ) );
    }

    out_pipeline.name = name;
    out_pipeline.num_active_layouts = resource_layouts.size;
}

void ShaderPassBlueprint::fill_resource_layout( hydra::gfx::ResourceLayoutCreation& creation, u32 index ) {

    creation.reset();

    hfx::ResourceLayoutBlueprint& rlb = resource_layouts[ index ];
    creation.num_bindings = rlb.bindings.size;

    memcpy( creation.bindings, rlb.bindings.get(), sizeof( hydra::gfx::ResourceLayoutCreation::Binding ) * creation.num_bindings );
}

} // namespace hfx