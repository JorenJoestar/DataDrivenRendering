#pragma once

#include "kernel/primitive_types.hpp"

namespace hydra {

    
    // Common methods /////////////////////////////////////////////////////
    u32             leading_zeroes_u32( u32 x );
    u32             leading_zeroes_u32_msvc( u32 x );
    u32             trailing_zeros_u32( u32 x );
    u64             trailing_zeros_u64( u64 x );

    u32             round_up_to_power_of_2( u32 v );

    void            print_binary( u64 n );
    void            print_binary( u32 n );

    // class BitMask //////////////////////////////////////////////////////

    // An abstraction over a bitmask. It provides an easy way to iterate through the
    // indexes of the set bits of a bitmask.  When Shift=0 (platforms with SSE),
    // this is a true bitmask.  On non-SSE, platforms the arithematic used to
    // emulate the SSE behavior works in bytes (Shift=3) and leaves each bytes as
    // either 0x00 or 0x80.
    //
    // For example:
    //   for (int i : BitMask<uint32_t, 16>(0x5)) -> yields 0, 2
    //   for (int i : BitMask<uint64_t, 8, 3>(0x0000000080800000)) -> yields 2, 3
    template <class T, int SignificantBits, int Shift = 0>
    class BitMask {
        //static_assert( std::is_unsigned<T>::value, "" );
        //static_assert( Shift == 0 || Shift == 3, "" );

    public:
     // These are useful for unit tests (gunit).
        using value_type = int;
        using iterator = BitMask;
        using const_iterator = BitMask;

        explicit BitMask( T mask ) : mask_( mask ) {
        }
        BitMask& operator++() {
            mask_ &= ( mask_ - 1 );
            return *this;
        }
        explicit operator bool() const {
            return mask_ != 0;
        }
        int operator*() const {
            return LowestBitSet();
        }
        uint32_t LowestBitSet() const {
            return trailing_zeros_u32( mask_ ) >> Shift;
        }
        uint32_t HighestBitSet() const {
            return static_cast< uint32_t >( ( bit_width( mask_ ) - 1 ) >> Shift );
        }

        BitMask begin() const {
            return *this;
        }
        BitMask end() const {
            return BitMask( 0 );
        }

        uint32_t TrailingZeros() const {
            return trailing_zeros_u32( mask_ );// >> Shift;
        }

        uint32_t LeadingZeros() const {
            return leading_zeroes_u32( mask_ );// >> Shift;
        }

    private:
        friend bool operator==( const BitMask& a, const BitMask& b ) {
            return a.mask_ == b.mask_;
        }
        friend bool operator!=( const BitMask& a, const BitMask& b ) {
            return a.mask_ != b.mask_;
        }

        T mask_;
    }; // class BitMask

} // namespace hydra 