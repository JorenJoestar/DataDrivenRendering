#pragma once

#include "kernel/platform.hpp"
#include "kernel/primitive_types.hpp"
#include "kernel/service.hpp"

namespace hydra {

    typedef void                        ( *PrintCallback )( const char* );  // Additional callback for printing

    struct LogService : public Service {

        hy_declare_service( LogService );

        void                            print_format( cstring format, ... );

        void                            set_callback( PrintCallback callback );

        PrintCallback                   print_callback = nullptr;

        static constexpr cstring        k_name = "hydra_log_service";
    };

    #define hprint(format, ...)          hydra::LogService::instance()->print_format(format, __VA_ARGS__);

} // namespace hydra
