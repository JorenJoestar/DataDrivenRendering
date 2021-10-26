#include "numerics.hpp"

#include <cmath>

#include "kernel/log.hpp"

namespace hydra {
#if defined (HYDRA_MATH_OVERFLOW)

//
// Use an integer 64 to see if the value converted is overflowing.
//
#define hy_math_convert(val, max, type, func)\
    i64 value_container = (i64)func(value);\
    if (abs(value_container) > max)\
        hprint( "Overflow converting values %llu, %llu\n", value_container, max );\
    const type v = (type)value_container;


#define hy_math_func_f32(max, type, func)\
    type func##type( f32 value ) {\
        hy_math_convert( value, max, type, func##f );\
        return v;\
    }\

#define hy_math_func_f64(max, type, func)\
    type func##type( f64 value ) {\
        hy_math_convert( value, max, type, func );\
        return v;\
    }\

#else
#define hy_math_convert(val, max, type, func)\
        (type)func(value);

#define hy_math_func_f32(max, type, func)\
    type func##type( f32 value ) {\
        return hy_math_convert( value, max, type, func##f );\
    }\

#define hy_math_func_f64(max, type, func)\
    type func##type( f64 value ) {\
        return hy_math_convert( value, max, type, func );\
    }\

#endif // HYDRA_MATH_CHECK

//
// Avoid double typeing functions for float and double
//
#define hy_math_func_f32_f64(max, type, func)\
    hy_math_func_f32( max, type, func )\
    hy_math_func_f64( max, type, func )\

// Function declarations //////////////////////////////////////////////////////////////////////////

// Ceil
hy_math_func_f32_f64( UINT32_MAX, u32, ceil )
hy_math_func_f32_f64( UINT16_MAX, u16, ceil )
hy_math_func_f32_f64( INT32_MAX, i32, ceil )
hy_math_func_f32_f64( INT16_MAX, i16, ceil )

// Floor
hy_math_func_f32_f64( UINT32_MAX, u32, floor )
hy_math_func_f32_f64( UINT16_MAX, u16, floor )
hy_math_func_f32_f64( INT32_MAX, i32, floor )
hy_math_func_f32_f64( INT16_MAX, i16, floor )

// Round
hy_math_func_f32_f64( UINT32_MAX, u32, round )
hy_math_func_f32_f64( UINT16_MAX, u16, round )
hy_math_func_f32_f64( INT32_MAX, i32, round )
hy_math_func_f32_f64( INT16_MAX, i16, round )

} // namespace hydra

