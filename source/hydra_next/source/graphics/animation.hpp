#pragma once

#pragma once

//
//  Hydra Animation - v0.07
//
//  Animation Systems
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2021/01/07, 18.18
//
//
// Revision history //////////////////////
//
//      0.07 (2021/06/23): + Changed animation state to use offset,size insted of uv0,uv1.
//      0.06 (2021/06/21): + Added inversion as new looping mode.
//      0.05 (2021/06/16): + Updated to hydra next.
//      0.04 (2021/01/19): + Added duration and ending queries for animation.
//      0.03 (2021/01/16): + Added support for grid based animations. + Improved animation creation.
//      0.02 (2021/01/09): + Added restart flag to start_animation.
//      0.01 (2021/01/08): + Initial file writing.

#include "kernel/primitive_types.hpp"
#include "kernel/array.hpp"

#include "cglm/types-struct.h"

namespace hydra {

typedef u32     AnimationHandle;

//
//
struct AnimationCreation {

    vec2s                   texture_size;
    vec2s                   start_pixel;
    vec2s                   frame_size;

    u16                     num_frames;
    u16                     columns;
    u8                      fps;

    bool                    looping;
    bool                    invert;

    AnimationCreation&      reset();
    AnimationCreation&      set_texture_size( const vec2s& size );
    AnimationCreation&      set_origin( const vec2s& origin );
    AnimationCreation&      set_size( const vec2s& size );
    AnimationCreation&      set_animation( u32 num_frames, u32 columns, u32 fps, bool looping, bool invert );

}; // struct AnimationCreation

//
//
struct AnimationData {

    vec2s       uv_offset;
    vec2s       uv_size;

    // Total number of frames
    u16         num_frames;
    // Columns for grid animations.
    u16         frames_columns;

    u8          fps;
    bool        is_looping;
    bool        is_inverted;    // Invert animation for ping-pong between frames

    cstring     name;

}; // struct AnimationData

//
//
struct AnimationState {
    AnimationHandle handle;
    f32         current_time;

    bool        inverted;

    vec2s       uv_offset;
    vec2s       uv_size;
}; // struct Animation


//
//
struct AnimationSystem {

    void                    init( Allocator* allocator );
    void                    shutdown();

    // Start animation only if it is new or explicitly restarting
    void                    start_animation( AnimationState& animation, AnimationHandle handle, bool restart );
    void                    update_animation( AnimationState& animation, f32 delta_time );

    f32                     get_duration( const AnimationState& animation ) const;
    bool                    is_finished( const AnimationState& animation ) const;

    AnimationHandle         create_animation( const AnimationCreation& creation );
    void                    destroy_animation( AnimationHandle handle );

    Array<AnimationData>    animation_datas;
    Allocator*              allocator;

}; // struct AnimationSystem


// Utils ////////////////////////////////////////////////////////////////////////
namespace Direction8 {
    enum Enum {
        Right, TopRight, Top, TopLeft, Left, BottomLeft, Bottom, BottomRight, Count
    };

    Direction8::Enum        from_axis( f32 x, f32 y );
}

namespace Direction4 {
    enum Enum {
        Right, Top, Left, Bottom, Count
    };

    Direction4::Enum        from_axis( f32 x, f32 y );
}

} // namespace hydra


