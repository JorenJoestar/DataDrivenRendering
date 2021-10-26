// Hydra Lib - v0.01


#include <stdlib.h>

#include "data_structures.hpp"

struct Canide {
    float c, d, e;
};

///////////////////////////////////////
int main( int, char** ) {

    hydra::HeapAllocator ha;

    {
        hydra::Array<Canide> canide;
        canide.init( &ha, 1 );

        hy_assert( false );

        for ( u32 i = 0; i < 11; i++ ) {
            Canide a{ i * 1.0f,3,4 };
            canide.push( a );
        }
    }

    hprint( "Grande!\n" );

    return 0;
}

