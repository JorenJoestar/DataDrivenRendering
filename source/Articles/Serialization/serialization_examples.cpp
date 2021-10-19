
#include "serialization_examples.hpp"

#include "blob.hpp"
#include "json.hpp"

#include <iostream>

#include <stdint.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>



// json utility ///////////////////////////////////////////////////////////
static bool read_json_with_exceptions( Allocator* allocator, cstring filename, nlohmann::json& parsed_json ) {

    using json = nlohmann::json;
    char* json_content = file_read_text( filename, allocator, nullptr );
    if ( !json_content ) {
        hprint( "Error opening file %s\n", filename );
        return false;
    }

    try {
        parsed_json = json::parse( json_content );
    } catch ( json::parse_error& error ) {
        hprint( "Error parsing %s: %s\n", filename, error.what() );
        return false;
    }

    return true;
}

// Resource Compilation ///////////////////////////////////////////////////

void compile_cutscene( Allocator* allocator, cstring source, cstring destination ) {

    using json = nlohmann::json;
    json parsed_json;
    if ( !read_json_with_exceptions( allocator, source, parsed_json ) ) {
        return;
    }

    // Using memory blob, but it is fixed sized still!
    sizet blob_size = sizeof( CutsceneBlueprint ) + ( 256 * 256 ); // Allocate extra memory for strings
    // Calculate total size of memory blob
    json entries = parsed_json[ "commands" ];
    const u32 num_entries = ( u32 )entries.size();
    blob_size += sizeof( CutsceneEntry ) * num_entries;

    MemoryBlob blob;
    blob.write<CutsceneBlueprint>( allocator, 0, blob_size, nullptr );

    // Reserve header
    CutsceneBlueprint* root = blob.allocate_static<CutsceneBlueprint>();
    blob.allocate_and_set( root->entries, num_entries );

    std::string name_string;

    for ( u32 i = 0; i < num_entries; ++i ) {
        json element = entries[ i ];
        element[ "type" ].get_to( name_string );
        CutsceneEntry& entry = root->entries[ i ];

        if ( name_string.compare( "dialogue" ) == 0 ) {
            element[ "text" ].get_to( name_string );
            char* memory = blob.allocate_static( name_string.size() + 1 );
            strcpy( memory, name_string.c_str() );
            memory[ name_string.size() ] = 0;

            entry.type = CutsceneCommandType::Dialogue;
            entry.data.set( memory, ( u32 )name_string.size() );
        } else if ( name_string.compare( "parallel" ) == 0 ) {
            u8 count = element.value( "count", 1 );

            u8* memory = ( u8* )blob.allocate_static( 1 );
            *memory = count;

            entry.type = CutsceneCommandType::Parallel;
            entry.data.set( ( char* )memory, 1 );
        } else if ( name_string.compare( "move_camera" ) == 0 ) {

            CutsceneMoveData* memory = blob.allocate_static<CutsceneMoveData>();
            memory->x = element.value( "x", 0.0f );
            memory->y = element.value( "y", 0.0f );;
            memory->speed = element.value( "speed", 0.0f );

            entry.type = CutsceneCommandType::MoveCamera;
            entry.data.set( ( char* )memory, sizeof( CutsceneMoveData ) );
        } else if ( name_string.compare( "fade" ) == 0 ) {

            CutsceneFadeData* fade = blob.allocate_static<CutsceneFadeData>();
            fade->start = element.value( "start", 0.0f );
            fade->end = element.value( "end", 0.0f );
            fade->duration = element.value( "duration", 1.0f );

            entry.type = CutsceneCommandType::Fade;
            entry.data.set( ( char* )fade, sizeof( CutsceneFadeData ) );
        } else if ( name_string.compare( "move_entity" ) == 0 ) {

            CutsceneMoveEntityData* data = blob.allocate_static<CutsceneMoveEntityData>();
            data->move_data.x = element.value( "x", 0.0f );
            data->move_data.y = element.value( "y", 0.0f );
            data->move_data.speed = element.value( "speed", 0.0f );

            bool instantaneous = element.value( "instant", false );
            data->move_data.speed = instantaneous ? 0.f : data->move_data.speed;

            element[ "entity_name" ].get_to( name_string );
            blob.allocate_and_set( data->entity_name, "%s", name_string.c_str() );
            
            entry.type = CutsceneCommandType::MoveEntity;
            entry.data.set( ( char* )data, sizeof( CutsceneMoveEntityData ) + name_string.size() + 1 );
        } else if ( name_string.compare( "change_atlas_entry" ) == 0 ) {

            // Cache current allocation to calculate final data size.
            u32 initial_allocated_offset = blob.allocated_offset;

            CutsceneChangeAtlasEntryData* data = blob.allocate_static<CutsceneChangeAtlasEntryData>();

            element[ "entity_name" ].get_to( name_string );
            blob.allocate_and_set( data->entity_name, name_string.c_str() );

            element[ "entry_name" ].get_to( name_string );
            blob.allocate_and_set( data->entry_name, name_string.c_str() );

            entry.type = CutsceneCommandType::ChangeAtlasEntry;
            entry.data.set( ( char* )data, blob.allocated_offset - initial_allocated_offset );
        }
    }

    file_write_binary( destination, blob.blob_memory, blob.allocated_offset );

    blob.shutdown();
}

void inspect_cutscene( Allocator* allocator, cstring filename ) {

    sizet binary_size;
    char* cutscene_binary = file_read_binary( filename, allocator, &binary_size );
    if ( cutscene_binary ) {
        MemoryBlob blob;
        CutsceneBlueprint* cutscene = blob.read<CutsceneBlueprint>( allocator, CutsceneBlueprint::k_version, cutscene_binary, binary_size );

        hprint( "Inspecting cutscene %s\n" );

        u32 num_entries = cutscene->entries.size;
        for ( u32 i = 0; i < num_entries; ++i ) {
            CutsceneEntry& entry = cutscene->entries[ i ];

            switch ( entry.type ) {
                case CutsceneCommandType::Dialogue:
                {
                    cstring dialogue_text = (cstring)entry.data.get();
                    hprint( "\tEntry %u, dialogue %s\n", i, dialogue_text );
                    break;
                }

                case CutsceneCommandType::Fade:
                {
                    CutsceneFadeData* fade = ( CutsceneFadeData* )entry.data.get();
                    hprint( "\tEntry %u, fade, %f %f %f\n", i, fade->start, fade->end, fade->duration );
                    break;
                }

                case CutsceneCommandType::MoveCamera:
                {
                    CutsceneMoveData* move = (CutsceneMoveData*)entry.data.get();
                    hprint( "\tEntry %u, MoveCamera, %f, %f, %f\n", i, move->x, move->y, move->speed );
                    break;
                }

                case CutsceneCommandType::MoveEntity:
                {
                    CutsceneMoveEntityData* move = ( CutsceneMoveEntityData* )entry.data.get();
                    hprint( "\tEntry %u, MoveEntity, %s %f, %f, %f\n", i, move->entity_name.c_str(), move->move_data.x, move->move_data.y, move->move_data.speed );
                    break;
                }

                default:
                {
                    // Not implemented!
                    break;
                }
            }
        }

        hprint( "\n\n" );
    }
}

// Scene //////////////////////////////////////////////////////////////////

template<>
void MemoryBlob::serialize<RenderingBlueprint>( RenderingBlueprint* data ) {

    if ( serializer_version > 0 )
        serialize( &data->v1_padding );

    serialize( &data->is_atlas );
    serialized_offset += 3;
    serialize( &data->texture_name );
}

template<>
void MemoryBlob::serialize<EntityBlueprint>( EntityBlueprint* data ) {

    serialize( &data->name );

    hprint( "Found entity %s\n", data->name.c_str() );

    if ( serializer_version > 0 )
        serialize( &data->v1_padding );

    serialize( &data->rendering );

    if ( serializer_version > 1 ) {
        serialize( &data->position );
    } else {
        data->position = { 0.f, 0.f };
    }

    serialize( &data->offset_z );
}

template<>
void MemoryBlob::serialize<SceneBlueprint>( SceneBlueprint* data ) {

    if ( serializer_version > 0 )
        serialize( &data->name );
    else
        data->name.set_empty();

    serialize( &data->entities );
}


void compile_scene( Allocator* allocator, cstring source, cstring destination ) {

    using json = nlohmann::json;
    json parsed_json;
    if ( !read_json_with_exceptions( allocator, source, parsed_json ) ) {
        return;
    }

    // Using new memory blob
    sizet blob_size = sizeof( SceneBlueprint ) + ( 256 * 1024 ); // Allocate extra memory for strings
    // Calculate total size of memory blob
    json entries = parsed_json[ "entities" ];
    const u32 num_entries = ( u32 )entries.size();
    blob_size += sizeof( EntityBlueprint ) * num_entries;

    MemoryBlob blob;
    blob.is_reading = false;
    blob.write<SceneBlueprint>( allocator, SceneBlueprint::k_version, blob_size, nullptr );

    // Allocate root header
    SceneBlueprint* header = blob.allocate_static<SceneBlueprint>();
    blob.allocate_and_set( header->entities, num_entries );

    std::string name_string;

    // Write name
    parsed_json[ "name" ].get_to( name_string );
    blob.allocate_and_set( header->name, name_string.c_str() );

    // Iterate through all entities
    for ( u32 i = 0; i < num_entries; ++i ) {
        json element = entries[ i ];
        element[ "name" ].get_to( name_string );

        EntityBlueprint& entity = header->entities[ i ];
        blob.allocate_and_set( entity.name, name_string.c_str() );

        entity.position.x = element.value( "position_x", 0.0f );
        entity.position.y = element.value( "position_y", 0.0f );
        entity.offset_z = element.value( "offset_z", 0.0f );

        hprint( "Writing entity %s\n", entity.name.c_str() );

        json component = element[ "rendering" ];
        if ( component.is_object() ) {
            RenderingBlueprint* rendering = blob.allocate_static<RenderingBlueprint>();
            component[ "atlas_path" ].get_to( name_string );

            blob.allocate_and_set( rendering->texture_name, name_string.c_str() );
            rendering->is_atlas = 1;

            entity.rendering.set( ( char* )rendering );
        } else {
            entity.rendering.set( nullptr );
        }
    }

    file_write_binary( destination, blob.blob_memory, blob.allocated_offset );
}

void inspect_scene( Allocator* allocator, cstring filename ) {

    sizet binary_size;
    char* binary = file_read_binary( filename, allocator, &binary_size );
    if ( binary ) {
        MemoryBlob blob;
        SceneBlueprint* scene = blob.read<SceneBlueprint>( allocator, SceneBlueprint::k_version, binary, binary_size );

        hprint( "Inspecting scene %s\n", scene->name.c_str() );

        u32 num_entities = scene->entities.size;
        for ( u32 i = 0; i < num_entities; ++i) {
            EntityBlueprint& entity = scene->entities[ i ];

            hprint( "\tEntity %s\n", entity.name.c_str() );
            if ( entity.rendering.is_not_null() ) {
                RenderingBlueprint* rendering = entity.rendering.get();
                hprint( "\t\tRendering: texture name %s\n", rendering->texture_name.c_str() );
            }

            hprint( "\t\tPosition %f, %f\n", entity.position.x, entity.position.y );
            hprint( "\t\tOffset Z %f\n", entity.offset_z );
        }
    }
}