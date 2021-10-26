#include "bit.hpp"

#include "log.hpp"

#include <immintrin.h>
#include <intrin0.h>

namespace hydra {

    
u32 trailing_zeros_u32( u32 x ) {
    /*unsigned long result = 0;  // NOLINT(runtime/int)
    _BitScanForward( &result, x );
    return result;*/
    return _tzcnt_u32( x );
}

u32 leading_zeroes_u32( u32 x ) {
    /*unsigned long result = 0;  // NOLINT(runtime/int)
    _BitScanReverse( &result, x );
    return result;*/
    return __lzcnt( x );
}

u32 leading_zeroes_u32_msvc( u32 x ) {
    unsigned long result = 0;  // NOLINT(runtime/int)
    if ( _BitScanReverse( &result, x ) ) {
        return 31 - result;
    }
    return 32;
}

u64 trailing_zeros_u64( u64 x ) {
    return _tzcnt_u64( x );
}

u32 round_up_to_power_of_2( u32 v ) {

    u32 nv = 1 << ( 32 - hydra::leading_zeroes_u32( v ) );
#if 0
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
#endif
    return nv;
}
void print_binary( u64 n ) {

    hprint( "0b" );
    for ( u32 i = 0; i < 64; ++i ) {
        u64 bit = (n >> ( 64 - i - 1 )) & 0x1;
        hprint( "%llu", bit );
    }
    hprint( " " );
}

void print_binary( u32 n ) {

    hprint( "0b" );
    for ( u32 i = 0; i < 32; ++i ) {
        u32 bit = ( n >> ( 32 - i - 1 ) ) & 0x1;
        hprint( "%u", bit );
    }
    hprint( " " );
}

} // namespace hydra