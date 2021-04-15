//
//  Hydra Animation - v0.04

#include "hydra_animation.h"

#include "cglm\struct\vec2.h"
#include "cglm\util.h"

namespace hydra {

void AnimationSystem::init() {
    array_init( animation_datas );
}

void AnimationSystem::shutdown() {
    array_free( animation_datas );
}

static void set_time( AnimationState& state, const AnimationData& data, f32 time ) {
    state.current_time = time;

    f32 duration = f32( data.num_frames ) / data.fps;
    u32 frame = flooru32(data.num_frames * ( time / duration ));

    if ( data.is_looping )
        frame %= data.num_frames;
    else
        frame = min( frame, data.num_frames - 1);

    u32 frame_x = frame % data.frames_columns;
    u32 frame_y = frame / data.frames_columns;

    // Horizontal only scroll. Change U0 and U1 only.
    state.uv0 = glms_vec2_add( data.uv_position, vec2s{ data.uv_size.x * frame_x, data.uv_size.y * frame_y } );
    state.uv1 = glms_vec2_add( state.uv0, data.uv_size );
}

void AnimationSystem::start_animation( AnimationState& animation, AnimationHandle handle, bool restart ) {
    if ( handle != animation.handle || restart ) {
        set_time( animation, animation_datas[ handle ], 0.f );
        animation.handle = handle;
    }
}

void AnimationSystem::update_animation( AnimationState& animation, f32 delta_time ) {
    set_time( animation, animation_datas[ animation.handle ], animation.current_time + delta_time );
}

f32 AnimationSystem::get_duration( const AnimationState& animation ) const {
    const AnimationData& data = animation_datas[ animation.handle ];
    return f32( data.num_frames ) / data.fps;
}

bool AnimationSystem::is_finished( const AnimationState& animation ) const {
    const AnimationData& data = animation_datas[ animation.handle ];
    const f32 duration = f32( data.num_frames ) / data.fps;
    return data.is_looping ? false : animation.current_time >= duration;
}

AnimationHandle AnimationSystem::create_animation( const AnimationCreation& creation ) {
    AnimationHandle handle = ( u32 )array_size( animation_datas );

    AnimationData new_data;

    new_data.uv_position = { creation.start_pixel.x / creation.texture_size.x, creation.start_pixel.y / creation.texture_size.y };
    new_data.uv_size = { creation.frame_size.x / creation.texture_size.x, creation.frame_size.y / creation.texture_size.y };
    new_data.num_frames = creation.num_frames;
    new_data.frames_columns = creation.columns;
    new_data.fps = creation.fps;
    new_data.is_looping = creation.looping;

    array_push( animation_datas, new_data );

    return handle;
}

void AnimationSystem::destroy_animation( AnimationHandle handle ) {

    // TODO: should use a ResourcePool!
}


// Utils ////////////////////////////////////////////////////////////////////////
Direction8::Enum hydra::Direction8::from_axis( f32 x, f32 y ) {
    const f32 angle = atan2f( y, x );
    const u32 octant = roundu32( 8 * angle / ( GLM_PI * 2.f ) + 8 ) % 8;
    return Direction8::Enum(octant);
}

Direction4::Enum hydra::Direction4::from_axis( f32 x, f32 y ) {
    const f32 angle = atan2f( y, x );
    const u32 quadrant = roundu32( 4 * angle / (GLM_PI * 2.f) + 4 ) % 4;
    return Direction4::Enum( quadrant );
}

// AnimationCreation ////////////////////////////////////////////////////////////
AnimationCreation& AnimationCreation::reset() {
    return *this;
}

AnimationCreation& AnimationCreation::set_texture_size( const vec2s& size ) {
    texture_size = size;
    return *this;
}

AnimationCreation& AnimationCreation::set_origin( const vec2s& origin ) {
    start_pixel = origin;
    return *this;
}

AnimationCreation& AnimationCreation::set_size( const vec2s& size ) {
    frame_size = size;
    return *this;
}

AnimationCreation& AnimationCreation::set_animation( u32 num_frames_, u32 columns_, u32 fps_, bool looping_ ) {
    num_frames = num_frames_;
    columns = columns_;
    fps = fps_;
    looping = looping_;
    return *this;
}

} // namespace hydra
