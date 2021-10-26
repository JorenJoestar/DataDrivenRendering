#include "application.hpp"

namespace hydra {

void Application::run( const ApplicationConfiguration& configuration ) {

    create( configuration );
    main_loop();
    destroy();
}

} // namespace hydra