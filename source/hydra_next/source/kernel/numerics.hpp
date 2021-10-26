#pragma once

#include "kernel/primitive_types.hpp"

namespace hydra {

    // Math utils /////////////////////////////////////////////////////////////////////////////////
    // Conversions from float/double to int/uint
    //
    // Define this macro to check if converted value can be contained in the destination int/uint.
    #define HYDRA_MATH_OVERFLOW

    template <typename T>
    T                               max( const T& a, const T& b ) { return a > b ? a : b; }

    template <typename T>
    T                               min( const T& a, const T& b ) { return a < b ? a : b; }

    template <typename T>
    T                               clamp( const T& v, const T& a, const T& b ) { return v < a ? a : (v > b ? b : v); }

    u32                             ceilu32( f32 value );
    u32                             ceilu32( f64 value );
    u16                             ceilu16( f32 value );
    u16                             ceilu16( f64 value );
    i32                             ceili32( f32 value );
    i32                             ceili32( f64 value );
    i16                             ceili16( f32 value );
    i16                             ceili16( f64 value );

    u32                             flooru32( f32 value );
    u32                             flooru32( f64 value );
    u16                             flooru16( f32 value );
    u16                             flooru16( f64 value );
    i32                             floori32( f32 value );
    i32                             floori32( f64 value );
    i16                             floori16( f32 value );
    i16                             floori16( f64 value );

    u32                             roundu32( f32 value );
    u32                             roundu32( f64 value );
    u16                             roundu16( f32 value );
    u16                             roundu16( f64 value );
    i32                             roundi32( f32 value );
    i32                             roundi32( f64 value );
    i16                             roundi16( f32 value );
    i16                             roundi16( f64 value );
} // namespace hydra
