#pragma once

#include "kernel/primitive_types.hpp"

namespace hydra {

    struct Service {

        virtual void                        init( void* configuration ) { }
        virtual void                        shutdown() { }

    }; // struct Service

    #define hy_declare_service(Type)        static Type* instance();

} // namespace hydra
