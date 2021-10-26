#pragma once

#include "kernel/primitive_types.hpp"

namespace hydra {

    bool                            process_execute( cstring working_directory, cstring process_fullpath, cstring arguments, cstring search_error_string = "" );
    cstring                         process_get_output();

} // namespace hydra
