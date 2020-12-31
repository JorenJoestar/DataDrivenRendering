#pragma once

#include "hydra/hydra_application.h"
#include "hydra/hydra_lib.h"
#include "hydra/hydra_rendering.h"

// 
// - Parse stars and constellations
// - Render fake stars
// - Add camera
// - Add julian time and sidereal time and coordinate systems
// - Render proper stars
// - Render constellations
// - Add proper motion to stars?

// Constellations data
// File containing list of constellation by Right Ascension and Declination.
// http://cdsarc.u-strasbg.fr/viz-bin/Cat?VI/49
// https://github.com/hemel-waarnemen-com/Constellation-lines

namespace Constellations {

    enum Abbreviations {
        AND, ANT, APS, AQR, AQL, ARA, ARI, AUR, BOO, CAE, CAM, CNC, CVN, CMA, CMI, CAP, CAR, CAS, CEN,
        CEP, CET, CHA, CIR, COL, COM, CRA, CRB, CRV, CRT, CRU, CYG, DEL, DOR, DRA, EQU, ERI, FOR, GEM,
        GRU, HER, HOR, HYA, HYI, IND, LAC, LEO, LMI, LEP, LIB, LUP, LYN, LYR, MEN, MIC, MON, MUS, NOR,
        OCT, OPH, ORI, PAV, PEG, PER, PHE, PIC, PSC, PSA, PUP, PYX, RET, SGE, SGR, SCO, SCL, SCT, SER1,
        SER2, SEX, TAU, TEL, TRI, TRA, TUC, UMA, UMI, VEL, VIR, VOL, VUL, Count_Abbr
    }; // enum Abbreviations

    enum Names {
        Andromeda, Antila, Apus, Aquarius, Aquila, Ara, Aries, Auriga, Bootes, Caelum, Camelopardis, 
        Cancer, Canes_Venatici, Canis_Major, Canis_Minor, Carina, Cassiopeia, Centaurus, Cepheus, Cetus, 
        Chamaeleon, Circinus, Columba, Coma_Berenices, Corona_Australis, Corona_Borealis, Corvus, Crater, 
        Crux, Cygnus, Delphinus, Dorado, Draco, Equuleus, Eridanus, Fornax, Gemini, Grus, Hercules, 
        Horologium, Hydra, Hydrus, Indus, Lacerta, Leo, Leo_Minor, Lepus, Libra, Lupus, Lynx, Lyra, 
        Mensa, Microscopium, Monoceros, Musca, Norma, Octans, Ophiuchus, Orion, Pavo, Pegasus, Perseus, 
        Phoenix, Pictor, Pisces, Pisces_Austrinus, Puppis, Pyxis, Reticulum, Sagitta, Sagittarius, 
        Scorpius, Sculptor, Scutum, Serpens_Caput, Serpens_Cauda, Sextans, Taurus, Telescopium, 
        Triangulum, Triangulum_Australe, Tucana, Ursa_Major, Ursa_Minor, Vela, Virgo, Volans, Vulpecula,
        Count_Names
    }; // enum Names

    const char*         to_string( Abbreviations abbreviation );
    const char*         to_string( Names name );

    struct AbbreviationsDictionary {
        char*           key;
        int32_t         value;
    }; // struct AbbreviationsDictionary

    //
    // Entry relative to global constellation data.
    struct ConstellationEntry {
        uint16_t        offset;
        uint16_t        count;
    }; // struct ConstellationEntry


    struct Constellations {

        string_hash( AbbreviationsDictionary )  names_dictionary    = nullptr;
        array( ConstellationEntry )             entries             = nullptr;
        int32_t*                                data                = nullptr;   // Monolithic entry data - all contiguous.

    }; // struct Constellations


    void                init( Constellations* constellations );
    int32_t             get_index( Constellations* constellations, const char* abbreviation );

} // namespace Constellations
/*
        Constellations abbreviations

       | Abbrev. | Constellation Name | | Abbrev. | Constellation Name |
       |_________|____________________|_|_________|____________________|
       | AND     | Andromeda          | | LEO     | Leo                |
       | ANT     | Antila             | | LMI     | Leo Minor          |
       | APS     | Apus               | | LEP     | Lepus              |
       | AQR     | Aquarius           | | LIB     | Libra              |
       | AQL     | Aquila             | | LUP     | Lupus              |
       | ARA     | Ara                | | LYN     | Lynx               |
       | ARI     | Aries              | | LYR     | Lyra               |
       | AUR     | Auriga             | | MEN     | Mensa              |
       | BOO     | Bootes             | | MIC     | Microscopium       |
       | CAE     | Caelum             | | MON     | Monoceros          |
       | CAM     | Camelopardis       | | MUS     | Musca              |
       | CNC     | Cancer             | | NOR     | Norma              |
       | CVN     | Canes Venatici     | | OCT     | Octans             |
       | CMA     | Canis Major        | | OPH     | Ophiuchus          |
       | CMI     | Canis Minor        | | ORI     | Orion              |
       | CAP     | Capricornus        | | PAV     | Pavo               |
       | CAR     | Carina             | | PEG     | Pegasus            |
       | CAS     | Cassiopeia         | | PER     | Perseus            |
       | CEN     | Centaurus          | | PHE     | Phoenix            |
       | CEP     | Cepheus            | | PIC     | Pictor             |
       | CET     | Cetus              | | PSC     | Pisces             |
       | CHA     | Chamaeleon         | | PSA     | Pisces Austrinus   |
       | CIR     | Circinus           | | PUP     | Puppis             |
       | COL     | Columba            | | PYX     | Pyxis              |
       | COM     | Coma Berenices     | | RET     | Reticulum          |
       | CRA     | Corona Australis   | | SGE     | Sagitta            |
       | CRB     | Corona Borealis    | | SGR     | Sagittarius        |
       | CRV     | Corvus             | | SCO     | Scorpius           |
       | CRT     | Crater             | | SCL     | Sculptor           |
       | CRU     | Crux               | | SCT     | Scutum             |
       | CYG     | Cygnus             | | SER1    | Serpens Caput      |
       | DEL     | Delphinus          | | SER2    | Serpens Cauda      |
       | DOR     | Dorado             | | SEX     | Sextans            |
       | DRA     | Draco              | | TAU     | Taurus             |
       | EQU     | Equuleus           | | TEL     | Telescopium        |
       | ERI     | Eridanus           | | TRI     | Triangulum         |
       | FOR     | Fornax             | | TRA     | Triangulum Australe|
       | GEM     | Gemini             | | TUC     | Tucana             |
       | GRU     | Grus               | | UMA     | Ursa Major         |
       | HER     | Hercules           | | UMI     | Ursa Minor         |
       | HOR     | Horologium         | | VEL     | Vela               |
       | HYA     | Hydra              | | VIR     | Virgo              |
       | HYI     | Hydrus             | | VOL     | Volans             |
       | IND     | Indus              | | VUL     | Vulpecula          |
       | LAC     | Lacerta            | |         |                    |
       |_________|____________________|_|_________|____________________|
*/

/*
* File description: plain text with constellation data.
*
* Column descriptions
*   Latin constellation abbreviation (%3s);
*   number of stars to draw lines between = number of numbers following on this line (%2d);
*   (rest of the columns) BSC numbers for those stars (1-9110) (%4d each).
*/

// Yale Star Catalogue Header
/*
* http://tdc-www.harvard.edu/catalogs/bsc5.header.html
*
The first 28 bytes of both BSC5 and BSC5ra contain the following information:
Integer*4 STAR0=0	Subtract from star number to get sequence number
Integer*4 STAR1=1	First star number in file
Integer*4 STARN=9110  	Number of stars in file
Integer*4 STNUM=1	0 if no star i.d. numbers are present
            1 if star i.d. numbers are in catalog file
            2 if star i.d. numbers are  in file
Logical*4 MPROP=1	1 if proper motion is included
            0 if no proper motion is included
Integer*4 NMAG=-1	Number of magnitudes present (-1=J2000 instead of B1950)
Integer*4 NBENT=32	Number of bytes per star entry
*/

struct BrighStarCatalogueHeader {

    int32_t             base_sequence_index;
    int32_t             first_star_index;
    int32_t             star_count;
    int32_t             star_index_type;

    uint32_t            proper_motion_flag;

    int32_t             magnitude_type;
    int32_t             star_entry_size;

}; // struct BrighStarCatalogueHeader

/*
*
Each catalog entry in BSC5 and BSC5ra contains 32 bytes with the following information:
Real*4 XNO          Catalog number of star
Real*8 SRA0         B1950 Right Ascension (radians)
Real*8 SDEC0        B1950 Declination (radians)
Character*2 IS      Spectral type (2 characters)
Integer*2 MAG       V Magnitude * 100
Real*4 XRPM         R.A. proper motion (radians per year)
Real*4 XDPM         Dec. proper motion (radians per year)
*/

// Need to pack to properly read the entries, otherwise the struct is padded to 40 bytes instead of 32.
#pragma pack (1)
struct BrightStarEntry {

    float           catalogue_number;
    double          right_ascension;
    double          declination;
    char            spectral_type[2];

    int16_t         visual_magnitude;
    float           right_ascension_proper_motion;
    float           declination_proper_motion;

}; // struct BrightStarEntry
#pragma pack()  // restore default packing

struct StarMapApplication : public hydra::Application {

    void                            app_init() override;
    void                            app_terminate() override;

    void                            app_update( hydra::ApplicationUpdate& update ) override;

    void                            update_constellation_gpu_data( uint32_t constellation_index );
    void                            draw_constellation_lines( const Constellations::ConstellationEntry& constellation, const mat4s& star_rotation_matrix );

    void                            load_render_resources( hydra::graphics::Device& gpu_device, bool startup );

    void                            line( const vec3s& from, const vec3s& to, uint32_t color0, uint32_t color1 );   // Draw 3D lines

    BrightStarEntry*                star_catalogue;
    uint32_t                        star_count;

    Constellations::Constellations  constellations;
    Constellations::Abbreviations   chosen_constellation = Constellations::Abbreviations::Count_Abbr;

    hydra::graphics::TextureHandle  capsule_texture;
    hydra::graphics::PipelineHandle star_rendering_pipeline;
    hydra::graphics::ResourceListLayoutHandle star_resource_list_layout;
    hydra::graphics::ResourceListHandle star_resource_list;
    hydra::graphics::BufferHandle   star_cb;
    hydra::graphics::BufferHandle   star_instance_buffer;

    // Line rendering
    hydra::graphics::PipelineHandle lines_rendering_pipeline;
    hydra::graphics::ResourceListLayoutHandle lines_resource_list_layout;
    hydra::graphics::ResourceListHandle lines_resource_list;
    hydra::graphics::BufferHandle   lines_vb;
    hydra::graphics::BufferHandle   lines_cb;

    uint32_t                        current_line = 0;

    hydra::graphics::Camera         camera;

    // Controls
    float                           glare_scale     = 1.0f;
    float                           star_radius_scale  = 1.0f;
    float                           star_distance_scale = 1.0f; // How distant are the stars placed.

    float                           latitude        = 0;
    float                           longitude       = 0;

    int32_t                         year            = 2000;
    int32_t                         month           = 1;
    int32_t                         day             = 1;
    int32_t                         hour            = 0;
    int32_t                         minute          = 0;
    int32_t                         second          = 0;

    float                           constellation_lines_alpha = 0.1f;
    float                           constellation_lines_width = 1.0f;
    float                           constellation_alpha_mask_scale = 2.0f;

    bool                            apply_precession = true;
    bool                            show_all_constellations = true;

}; // struct StarMapApplication

