#pragma once

#include "kernel/log.hpp"

namespace hydra {

    #define hy_assert( condition )      if (!(condition)) { hprint(HY_FILELINE("FALSE\n")); HY_DEBUG_BREAK }
    #define hy_assertm( condition, message, ... ) if (!(condition)) { hprint(HY_FILELINE(HY_CONCAT(message, "\n")), __VA_ARGS__); HY_DEBUG_BREAK }

} // namespace hydra
