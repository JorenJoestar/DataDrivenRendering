#pragma once

#include "serialization_demo.hpp"
#include "blob.hpp"

//
// vec2s ///////////////////////////////////////////////////////////////////
struct vec2s {
    f32     x;
    f32     y;
}; // struct vec2s

// Forward declaration for template specialization.
template<>
void BlobSerializer::serialize<vec2s>( vec2s* data );


// Cutscene structs ///////////////////////////////////////////////////////

//
//
enum class CutsceneCommandType : u8 {
    MoveCamera = 0,
    MoveEntity,
    Dialogue,
    Parallel,           // Used to have multiple commands run in parallel.
    Fade,
    PlayAnimation,
    ChangeAtlasEntry,

    Count
}; // enum Type

//
//
struct CutsceneMoveData {

    f32                         x;
    f32                         y;
    f32                         speed;

}; // struct CutsceneMoveCameraData

//
//
struct CutsceneFadeData {

    f32                         start;
    f32                         end;
    f32                         duration;

}; // struct CutsceneFadeData

//
//
struct CutsceneMoveEntityData {

    CutsceneMoveData            move_data;
    RelativeString              entity_name;
}; // struct CutsceneMoveEntityData

//
//
struct CutsceneChangeAtlasEntryData {

    RelativeString       entry_name;
    RelativeString       entity_name;
}; // struct CutsceneChangeAtlasEntryData

//
//
struct CutsceneEntry {
    CutsceneCommandType         type;

    RelativeArray<u8>           data;

}; // struct CutsceneEntry
//
//
struct CutsceneBlueprint : public Blob {

    RelativeArray<CutsceneEntry> entries;

    static constexpr u32        k_version = 0;

}; // struct CutsceneBlueprint


void compile_cutscene( Allocator* allocator, cstring source, cstring destination );
void inspect_cutscene( Allocator* allocator, cstring filename );


// Scene //////////////////////////////////////////////////////////////////

//
//
struct RenderingBlueprint {
    u32                     v1_padding;     // Added to test different versions.
    u8                      is_atlas;       // Texture can be single or atlas.
    RelativeString          texture_name;

}; // struct RenderingBlueprint

//
//
struct EntityBlueprint {
    RelativeString          name;
    u32                     v1_padding;     // Added to test different versions.
    RelativePointer<RenderingBlueprint>  rendering;

    vec2s                   position;
    f32                     offset_z;

}; // struct EntityBlueprint

//
//
struct SceneBlueprint : public Blob {

    RelativeString                  name;
    RelativeArray<EntityBlueprint>  entities;

    static constexpr u32            k_version = 2;

}; // struct SceneBlueprint


void compile_scene( Allocator* allocator, cstring source, cstring destination );
void inspect_scene( Allocator* allocator, cstring filename );


// Saved game /////////////////////////////////////////////////////////////

