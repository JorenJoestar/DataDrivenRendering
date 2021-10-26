#pragma once

#include "kernel/primitive_types.hpp"

namespace hydra {

    //
    // Calculate aligned memory size.
    sizet       memory_align( sizet size, sizet alignment );



    // Implementation /////////////////////////////////////////////////////

    inline sizet memory_align( sizet size, sizet alignment ) {
        const sizet alignment_mask = alignment - 1;
        return ( size + alignment_mask ) & ~alignment_mask;
    }

} // namespace hydra