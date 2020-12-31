#include "star_map_application.h"

#include "hydra/hydra_lexer.h"
#include "hydra/hydra_graphics.h"
#include "hydra/hydra_shaderfx.h"
#include "imgui/imgui.h"

#include "cglm/struct/mat4.h"
#include "cglm/struct/cam.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/quat.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//#define STAR_OUTPUT_ENTRIES

namespace Constellations {

    static const char* s_names_strings[] = {
        "Andromeda","Antila","Apus","Aquarius","Aquila","Ara","Aries","Auriga","Bootes","Caelum","Camelopardis","Cancer","Canes_Venatici","Canis_Major","Canis_Minor", "Capricornus", "Carina","Cassiopeia","Centaurus","Cepheus","Cetus","Chamaeleon","Circinus","Columba","Coma_Berenices","Corona_Australis","Corona_Borealis","Corvus","Crater","Crux","Cygnus","Delphinus","Dorado","Draco","Equuleus","Eridanus","Fornax","Gemini","Grus","Hercules","Horologium","Hydra","Hydrus","Indus","Lacerta","Leo","Leo_Minor","Lepus","Libra","Lupus","Lynx","Lyra","Mensa","Microscopium","Monoceros","Musca","Norma","Octans","Ophiuchus","Orion","Pavo","Pegasus","Perseus","Phoenix","Pictor","Pisces","Pisces_Austrinus","Puppis","Pyxis","Reticulum","Sagitta","Sagittarius","Scorpius","Sculptor","Scutum","Serpens_Caput","Serpens_Cauda","Sextans","Taurus","Telescopium","Triangulum","Triangulum_Australe","Tucana","Ursa_Major","Ursa_Minor","Vela","Virgo","Volans","Vulpecula",
    };

    static const char* s_abbreviations_strings[] = {
        "AND", "ANT", "APS", "AQR", "AQL", "ARA", "ARI", "AUR", "BOO", "CAE", "CAM", "CNC", "CVN", "CMA", "CMI", "CAP", "CAR", "CAS", "CEN", "CEP", "CET", "CHA", "CIR", "COL", "COM", "CRA", "CRB", "CRV", "CRT", "CRU", "CYG", "DEL", "DOR", "DRA", "EQU", "ERI", "FOR", "GEM", "GRU", "HER", "HOR", "HYA", "HYI", "IND", "LAC", "LEO", "LMI", "LEP", "LIB", "LUP", "LYN", "LYR", "MEN", "MIC", "MON", "MUS", "NOR", "OCT", "OPH", "ORI", "PAV", "PEG", "PER", "PHE", "PIC", "PSC", "PSA", "PUP", "PYX", "RET", "SGE", "SGR", "SCO", "SCL", "SCT", "SER", "SER", "SEX", "TAU", "TEL", "TRI", "TRA", "TUC", "UMA", "UMI", "VEL", "VIR", "VOL", "VUL",
    };

    const char* to_string( Abbreviations abbreviation ) {
        return s_abbreviations_strings[abbreviation];
    }

    const char* to_string( Names name ) {
        return s_names_strings[name];
    }


    void init( Constellations* constellations ) {
        string_hash_init_arena( constellations->names_dictionary );

        array_set_length( constellations->entries, Abbreviations::Count_Abbr );

        for ( int32_t i = 0; i < Abbreviations::Count_Abbr; ++i ) {
            string_hash_put( constellations->names_dictionary, s_abbreviations_strings[ i ], i );

            constellations->entries[ i ] = { 0, 0 };
        }

#if defined CHECK_CONSTELLATIONS_NAME
        for ( size_t i = 0; i < Constellations::Abbreviations::Count_Abbr; i++ ) {
            hydra::print_format( "%s, %s\n", Constellations::s_abbreviations_strings[ i ], Constellations::s_names_strings[ i ] );
        }
#endif // CHECK_CONSTELLATIONS_NAME
    }

    int32_t get_index( Constellations* constellations, const char* abbreviation ) {
        return string_hash_get( constellations->names_dictionary, abbreviation );
    }
} // namespace Constellations


// Star color calculations

struct Range {
    uint32_t        min;
    uint32_t        max;
};

// Morgan-Keenan classification
// https://starparty.com/topics/astronomy/stars/the-morgan-keenan-system/

// Letters are for star categories.
// Numbers (0..9) are for further subdivision: 0 hottest, 9 colder.

static const uint32_t           k_max_star_types = 'z' - 'a';

// Temperature ranges (in Kelvin) of the different MK spectral types.
static const Range              k_star_temperature_ranges[ k_max_star_types ] = {
    // A0-A9            B                   C                 D                 E           F               G
    { 7300, 10000 }, { 10000, 30000 }, { 2400, 3200 }, { 100000, 1000000 }, { 0, 0 }, { 6000, 7300 }, { 5300, 6000 }, { 0, 0 }, { 0, 0 },
    //  J          K                    L           M                           O           P           Q        R          S               T
    { 0, 0 }, { 3800, 5300 }, { 1300, 2100 }, { 2500, 3800 }, { 0, 0 }, { 30000, 40000 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 2400, 3500 }, { 600, 1300 },
    // U         V          W              X            Y
    { 0, 0 }, { 0, 0 }, { 25000, 40000 }, { 0, 0 }, { 0, 600 }
};

//
// Returns temperature based on MK classification in 2 chars.
static uint32_t Morgan_Keenan_to_temperature( char spectral_type, char sub_type ) {
    const Range& temperature_range = k_star_temperature_ranges[ spectral_type - 'A' ];
    const uint32_t range_step = ( temperature_range.max - temperature_range.min ) / 9;

    const uint32_t sub_index = '9' - sub_type;
    return temperature_range.min + sub_index * range_step;
}

// Taken from bbr_color.txt
// Parse just the rgb colors min-max ranges of the spectral types
struct RGB {
    float   r;
    float   g;
    float   b;
};

// Goes from 1000 to 40000 in increases of 100 Kelvin.
static const uint32_t           k_max_rgb_temperatures = ( 40000 / 100 ) - ( 1000 / 100 );
static RGB                      k_rgb_temperatures[ k_max_rgb_temperatures ];

//
static void temperature_to_color( uint32_t temperature, float& r, float& g, float& b ) {
    int32_t index = ( temperature / 100 ) - 10; // 1000 / 100 = 10
    index = glm_clamp( index, 0, k_max_rgb_temperatures );

    const RGB& rgb = k_rgb_temperatures[ index ];
    r = rgb.r;
    g = rgb.g;
    b = rgb.b;
}

static void Morgan_Keenan_to_color( char spectral_type, char sub_type, float& r, float& g, float& b ) {
    uint32_t temperature_kelvin = Morgan_Keenan_to_temperature( spectral_type, sub_type );

    temperature_to_color( temperature_kelvin, r, g, b );
}

//
// Julian Date
// Julian dates are in DAYS (and fractions)
// JulianCalendar calendar = new JulianCalendar();
// Saturday, A.D. 2017 Jan 28	00:00:00.0	2457781.5
//    DateTime today = DateTime.Today;
//    DateTime dateInJulian = calendar.ToDateTime(today.Year, today.Month, today.Day, 0, 0, 0, 0);
// String ddd = calendar.ToString();
// float JD = 2457781.5f;
// T is in Julian centuries
// float T = ( JD - 2451545.0f ) / 36525.0f;

//
// From https://en.wikipedia.org/wiki/Julian_day
//
// Gregorian Calendar Date to Julian Day Number conversion

// This is the reference Julian Date used in current astronomy.
static const int32_t j2000 = 2451545;

//
// Julian Day Number calculations.
// https://en.wikipedia.org/wiki/Julian_day
// https://aa.quae.nl/en/reken/juliaansedag.html
// https://core2.gsfc.nasa.gov/time/julian.txt
// http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html
static int32_t calculate_julian_day_number( int32_t year, int32_t month, int32_t day ) {

    // Formula coming from Wikipedia.
    int32_t a = ( month - 14 ) / 12;
    int32_t jdn =  ( 1461 * (year + 4800 + a)) / 4 +
                    ( 367 * ( month - 2 - 12 *  a ) ) / 12 - 
                    ( 3 * ( ( year + 4900 + a ) / 100 ) ) / 4 +
                    day - 32075;

    // Other formula found online:
    /*int m, y, leap_days;
    a = ( ( 14 - month ) / 12 );
    m = ( month - 3 ) + ( 12 * a );
    y = year + 4800 - a;
    leap_days = ( y / 4 ) - ( y / 100 ) + ( y / 400 );
    int32_t jdn2 = day + ( ( ( 153 * m ) + 2 ) / 5 ) + ( 365 * y ) + leap_days - 32045;*/

    return jdn;
}

//
// Julian Date
//
static double calculate_julian_date( int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second ) {
    int32_t jdn = calculate_julian_day_number( year, month, day );

    double jd = jdn + (( hour - 12.0 ) / 24.0) + (minute / 1440.0) + (second / 86400.0);
    return jd;
}

//
// Julian centuries since January 1, 2000, used to rotate the stars.
//
static double calculate_julian_century_date( int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second ) {
    double jd = calculate_julian_date( year, month, day, hour, minute, second );
    return ( jd - j2000 ) / 36525.0;
}

//
// Convert to euclidean coordinates
//
static void convert_to_euclidean( float right_ascension, float declination, float radial_distance, float& out_x, float& out_y, float& out_z ) {
    const float cosd = cosf( declination );

    out_x = radial_distance * sinf( right_ascension ) * cosd;
    out_y = radial_distance * cosf( right_ascension ) * cosd;
    out_z = radial_distance * sinf( declination );
}

// Rendering //////////////////////////////////////
static hfx::ShaderEffectFile        shader_effect_file;


struct LinVertex {
    vec3s                           position;
    uint32_t                        color;

    void                            set( float x, float y, float z, uint32_t color );
}; // struct LinVertex 

struct LinesGPULocalConstants {
    mat4s                           view_projection;
    mat4s                           projection;

    vec4s                           resolution;

    float                           line_width;
    float                           alpha_mask_scale;
    float                           pad[ 2 ];
};

struct StarsGPUConstants {

    mat4s                       view_projection_matrix;
    mat4s                       star_rotation_matrix;

    vec4s                       camera_up;
    vec4s                       camera_right;

    float                       min_radius;
    float                       glare_scale;
    float                       star_radius_scale;
    float                       distance_scale;

}; // struct StarsGPUConstants


static const uint32_t k_max_lines = 50000;

static LinVertex s_line_buffer[ k_max_lines ];

// StarMapApplication /////////////////////////////
void StarMapApplication::app_init() {

#if defined STAR_OUTPUT_ENTRIES
    // Check mk type star temperature ranges
    for ( size_t i = 0; i < k_max_star_types; i++ ) {
        hydra::print_format( " %c %d %d\n", 'a' + i, k_star_temperature_ranges[ i ].min, k_star_temperature_ranges[ i ].max );
    }
#endif // STAR_OUTPUT_ENTRIES

    // j2000 = 2451545.0;
    
    uint32_t jdn2000 = calculate_julian_day_number( 2000, 1, 1 );
    float jd2000 = calculate_julian_date( 2000, 1, 1, 0, 0, 0 );

    // Saturday, A.D. 2017 Jan 28	00:00:00.0	2457781.5
    // Date test:
    static const float k_julian_date_test = 2457781.5f;
    float julian_date_calculated = calculate_julian_date( 2017, 1, 28, 0, 0, 0 );

    hydra::MemoryAllocator* allocator = hydra::memory_get_system_allocator();

    ////////////////////////////////
    // Init astrolonomical data
    char* star_catalogue_data = hydra::file_read_into_memory( "..\\data\\articles\\StarRendering\\bsc5.bin", nullptr, false, *allocator );

    BrighStarCatalogueHeader* header = (BrighStarCatalogueHeader*)star_catalogue_data;

    star_count = abs( header->star_count );

    star_catalogue = (BrightStarEntry*)allocator->allocate( star_count * sizeof( BrightStarEntry ), 1 );
    memcpy( star_catalogue, (star_catalogue_data + sizeof( BrighStarCatalogueHeader )), star_count * sizeof( BrightStarEntry ) );

    allocator->free_( star_catalogue_data );

#if defined STAR_OUTPUT_ENTRIES
    for ( uint32_t i = 0; i < star_count; i++ ) {
        const BrightStarEntry& star_entry_data = star_catalogue[i];
        hydra::print_format( "%4.0f, %f\n", star_entry_data.catalogue_number, star_entry_data.visual_magnitude / 100.0f );
    }
#endif // STAR_OUTPUT_ENTRIES

    Constellations::init( &constellations );

    // Read constellation file
    char* constellation_data = hydra::file_read_into_memory( "..\\data\\articles\\StarRendering\\constellations_lines.txt", nullptr, false, *allocator );

    DataBuffer data_buffer;
    data_buffer_init( &data_buffer, 10000, 10000 * 4 );

    Lexer lexer;
    lexer_init( &lexer, constellation_data, &data_buffer );

    // First parse: calculate offsets and total size of indices array.
    uint32_t data_size = 0;
    bool parsing = true;

    while ( parsing ) {

        Token token;
        lexer_next_token( &lexer, token );

        switch ( token.type ) {

            case Token::Token_Hash:
            {
                // Skip the line
                lexer_goto_next_line( &lexer );

                break;
            }

            case Token::Token_Identifier:
            {
                // Read name
                char name[ 4 ];
                name[ 0 ] = toupper( token.text.text[ 0 ] );
                name[ 1 ] = toupper( token.text.text[ 1 ] );
                name[ 2 ] = toupper( token.text.text[ 2 ] );
                name[ 3 ] = 0;

                int32_t constellation_index = Constellations::get_index( &constellations, name );

                // Read segment count
                lexer_expect_token( &lexer, token, Token::Token_Number );

                uint32_t count = atoi( token.text.text );
                constellations.entries[ constellation_index ].count += count - 1; // This is segments count
                data_size += count - 1; // Segments count

                // Just advance the token to the next line.
                for ( uint32_t i = 0; i < count; ++i ) {
                    lexer_next_token( &lexer, token );
                }

                break;
            }

            case Token::Type::Token_EndOfStream:
            {
                parsing = false;
                break;
            }
        }
    }

    // Prepare constellation indices data
    constellations.data = ( int32_t* )allocator->allocate( sizeof( int32_t ) * data_size * 2, 1 );
    // We have all the per constellation segment counts, calculate the offsets.

    Constellations::ConstellationEntry& first_constellation = constellations.entries[ 0 ];
    uint32_t current_offset = first_constellation.count;

    for ( int32_t i = 1; i < Constellations::Abbreviations::Count_Abbr; ++i ) {
        Constellations::ConstellationEntry& constellation = constellations.entries[ i ];
        constellation.offset = current_offset * 2;  // 2 entries for each segment

        current_offset += constellation.count;
    }

    // Second parse: fill in constellation data
    lexer_init( &lexer, constellation_data, &data_buffer );

    parsing = true;

    // Cache parsed offsets for constellations that have multiple lines (like CAM - Camelopardis).
    uint32_t parse_offsets[ Constellations::Count_Abbr ];
    memset( parse_offsets, 0, sizeof(uint32_t) * Constellations::Count_Abbr );

    while ( parsing ) {

        Token token;
        lexer_next_token( &lexer, token );

        switch ( token.type ) {

            case Token::Token_Hash:
            {
                // Skip the line
                lexer_goto_next_line( &lexer );

                break;
            }

            case Token::Token_Identifier:
            {
                // Read name
                char name[ 4 ];
                name[ 0 ] = toupper( token.text.text[ 0 ] );
                name[ 1 ] = toupper( token.text.text[ 1 ] );
                name[ 2 ] = toupper( token.text.text[ 2 ] );
                name[ 3 ] = 0;

                const int32_t constellation_index = Constellations::get_index( &constellations, name );

                // Read segment count
                lexer_expect_token( &lexer, token, Token::Token_Number );
                const uint32_t count = atoi( token.text.text );
                // Read first star index.
                lexer_expect_token( &lexer, token, Token::Token_Number );
                uint32_t current_star_index = atoi( token.text.text );

                const Constellations::ConstellationEntry& constellation = constellations.entries[ constellation_index ];

                for ( uint32_t i = 1; i < count; ++i ) {
                    lexer_next_token( &lexer, token );
                    const uint32_t next_star_index = atoi( token.text.text );

                    const uint32_t data_index = ( i - 1 ) * 2 + parse_offsets[constellation_index];
                    // Add segment with star indices
                    constellations.data[ constellation.offset + data_index] = current_star_index;
                    constellations.data[ constellation.offset + data_index + 1] = next_star_index;

                    current_star_index = next_star_index;
                }

                parse_offsets[ constellation_index ] = ( count - 1 ) * 2;

                break;
            }

            case Token::Type::Token_EndOfStream:
            {
                parsing = false;
                break;
            }
        }
    }

    allocator->free_( constellation_data );

    // Parse BlackBody color linked to temperature
    char* bbr_data = hydra::file_read_into_memory( "..\\data\\articles\\StarRendering\\bbr_color.txt", nullptr, false, *allocator );
    lexer_init( &lexer, bbr_data, &data_buffer );

    parsing = true;
    uint32_t rgb_entry_index = 0;

    while ( parsing ) {

        Token token;
        lexer_next_token( &lexer, token );

        switch ( token.type ) {

            case Token::Token_Hash:
            {
                // Skip the line
                lexer_goto_next_line( &lexer );

                break;
            }

            case Token::Token_Number:
            {
                // Skip first line - temperatures are duplicated with different formats.
                lexer_goto_next_line( &lexer );

                // Parse a line
                // 1000 K   2deg  0.6499 0.3474  2.472e+06    1.0000 0.0337 0.0000  255  51   0  #ff3300
                // 1000 K  10deg  0.6472 0.3506  2.525e+06    1.0000 0.0401 0.0000  255  56   0  #ff3800
                const uint32_t temperature = atoi( token.text.text );
                // Advance until we hit the RGB values
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                // Special case: parser does not know how to parse 2.472e+06 - it is 3 elements instead of 1.
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );
                lexer_next_token( &lexer, token );

                int32_t index = ( temperature / 100 ) - 10; // 1000 / 100 = 10
                index = glm_clamp( index, 0, k_max_rgb_temperatures );

                RGB& rgb = k_rgb_temperatures[ index ];
                data_buffer_get_current( *lexer.data_buffer, rgb.r );

                lexer_next_token( &lexer, token );
                data_buffer_get_current( *lexer.data_buffer, rgb.g );

                lexer_next_token( &lexer, token );
                data_buffer_get_current( *lexer.data_buffer, rgb.b );

                lexer_goto_next_line( &lexer );

                break;
            }

            case Token::Type::Token_EndOfStream:
            {
                parsing = false;
                break;
            }
        }
    }

    allocator->free_( bbr_data );

#if defined STAR_OUTPUT_ENTRIES
    for ( size_t i = 0; i < Constellations::Count_Abbr; i++ ) {

        hydra::print_format( "Constellation %s \n", Constellations::to_string((Constellations::Abbreviations)i) );

        const Constellations::ConstellationEntry& constellation = constellations.entries[ i ];
        for ( size_t i = 0; i < constellation.count; i++ ) {
            uint32_t star_index = constellations.data[ constellation.offset + i * 2 ];
            uint32_t star_index_2 = constellations.data[ constellation.offset + i * 2 + 1 ];

            hydra::print_format( " %d %d - ", star_index, star_index_2 );
        }

        hydra::print_format( "\n" );
    }
#endif // STAR_OUTPUT_ENTRIES

    ////////////////////////////////
    // Init rendering
    
    load_render_resources( *gpu_device, true );

    camera.init_perpective( 0.1f, 1000.f, 60.f, gpu_device->swapchain_width * 1.f / gpu_device->swapchain_height );
    camera_input.init();
    camera_movement_update.init( 20.0f, 20.0f );

    camera.direction = { 0,0,1 };

    // Init positioning

    // http://www.geomidpoint.com/example.html
    // Longitude in radians (east positive)
    // Latitude in radians (north positive)
    //
    // New York lat	40° 42' 51.6708" N	40.7143528	0.710599509
    // New York lon    74° 0' 21.5022" W	-74.0059731	-1.291647896
    //float nyc_latitude_radians = glm_rad( 40.7143528 );
    //float nyc_longitude_radians = glm_rad( -74.0059731 );

    // Rome lat 41.8919300 degrees
    // Rome lon 12.5113300 degrees
    //float rome_latitude_radians = glm_rad( 41.8919300 );
    //float rome_longitude_radians = glm_rad( 12.5113300 );

    latitude = 41.8919300;
    longitude = 12.5113300;
}

void StarMapApplication::app_terminate() {

    gpu_device->destroy_pipeline( star_rendering_pipeline );
    gpu_device->destroy_buffer( star_cb );
    gpu_device->destroy_buffer( star_instance_buffer );
    gpu_device->destroy_resource_list( star_resource_list );
    gpu_device->destroy_resource_list_layout( star_resource_list_layout );

    gpu_device->destroy_pipeline( lines_rendering_pipeline );
    gpu_device->destroy_resource_list( lines_resource_list );
    gpu_device->destroy_resource_list_layout( lines_resource_list_layout );
    gpu_device->destroy_buffer( lines_vb );
    gpu_device->destroy_buffer( lines_cb );

    gpu_device->destroy_texture( capsule_texture );

    hydra::memory_get_system_allocator()->free_( star_catalogue );
}

void StarMapApplication::app_update( hydra::ApplicationUpdate& update ) {

    update_camera( camera );

    float longitude_radians = glm_rad( longitude );
    float latitude_radians = glm_rad( latitude );

    // Calculate rotation matrix based on time, latitude and longitude
    // T is time in julian century, as used in the paper.
    double T = calculate_julian_century_date( year, month, day, hour, minute, second );
    double local_mean_sidereal_time = 4.894961f + 230121.675315f * T + longitude_radians;

    // Exploration of different rotations
    static bool ry_invert = false;

    versors rotation_y = ry_invert ? glms_quatv( latitude_radians - GLM_PI_2f, { 1, 0, 0 } ) : glms_quatv( latitude_radians - GLM_PI_2f, { 0, 1, 0 } );
    versors rotation_z = glms_quatv( -local_mean_sidereal_time, { 0, 0, 1 } );

    static bool rotation_order_invert = false;

    versors final_rotation = rotation_order_invert ? glms_quat_mul( rotation_y, rotation_z ) : glms_quat_mul( rotation_z, rotation_y );
    if ( apply_precession ) {
        versors precession_rotation_z = glms_quatv( 0.01118f, { 0, 0, 1 } );
        versors precession = glms_quat_mul( glms_quat_mul( precession_rotation_z, glms_quatv( -0.00972, {1, 0, 0} ) ), precession_rotation_z );

        final_rotation = glms_quat_mul( final_rotation, precession );
    }

    mat4s star_rotation_matrix = glms_quat_mat4( final_rotation );

    static bool apply_scale = false;
    const float distance_scale = apply_scale ? star_distance_scale : 1.0f;

    if ( apply_scale )
        star_rotation_matrix = glms_mat4_mul( glms_scale_make( { -distance_scale, distance_scale, distance_scale } ), star_rotation_matrix );

    // 
    hydra::graphics::MapBufferParameters cb_map = { star_cb, 0, 0 };
    StarsGPUConstants* cb_data = ( StarsGPUConstants* )gpu_device->map_buffer( cb_map );
    if ( cb_data ) {

        // Calculate view projection matrix
        memcpy( cb_data->view_projection_matrix.raw, &camera.view_projection.m00, 64 );
        memcpy( cb_data->star_rotation_matrix.raw, &star_rotation_matrix.m00, 64 );
        memcpy( cb_data->camera_up.raw, camera.up.raw, 12 );
        memcpy( cb_data->camera_right.raw, camera.right.raw, 12 );

        cb_data->min_radius = 4.0f * tanf( glm_rad( camera.field_of_view_y ) * 0.5f ) / gpu_device->swapchain_height;
        cb_data->glare_scale = glare_scale;
        cb_data->star_radius_scale = star_radius_scale;
        cb_data->distance_scale = star_distance_scale;

        gpu_device->unmap_buffer( cb_map );
    }

    uint64_t sort_key = 0;

    using namespace hydra::graphics;
    // Reuse main command buffer.
    CommandBuffer* gpu_commands = update.gpu_commands;
    gpu_commands->clear( sort_key, 0, 0, 0, 1 );
    gpu_commands->clear_depth_stencil( sort_key++, 1.0f, 0 );

    // Draw the stars! ////////////////////////////
    gpu_commands->bind_pass( sort_key++, update.gpu_device->get_swapchain_pass() );
    gpu_commands->set_scissor( sort_key++, nullptr );
    gpu_commands->set_viewport( sort_key++, nullptr );

    gpu_commands->bind_vertex_buffer( sort_key++, star_instance_buffer, 0, 0 );
    gpu_commands->bind_pipeline( sort_key++, star_rendering_pipeline );
    gpu_commands->bind_resource_list( sort_key++, &star_resource_list, 1, nullptr, 0 );

    uint32_t star_count_to_render = star_count;
    // Render only the choosen constellation
    if ( show_all_constellations ) {

        for ( size_t i = 0; i < Constellations::Count_Abbr; i++ ) {

            const Constellations::ConstellationEntry& ce = constellations.entries[ i ];

            // Add constellation rendering
            draw_constellation_lines( ce, star_rotation_matrix );
        }
    }
    else if ( chosen_constellation != Constellations::Abbreviations::Count_Abbr ) {

        const int32_t constellation_index = Constellations::get_index( &constellations, Constellations::to_string( chosen_constellation ) );
        const Constellations::ConstellationEntry& ce = constellations.entries[ constellation_index ];

        // Add constellation rendering
        draw_constellation_lines( ce, star_rotation_matrix );
    }

    gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, 6, 0, star_count_to_render );

    // Draw view orientation axis.
    // Calculate a decentered world position for the starting point.
    vec3s world_position = glms_vec3_add( camera.position, glms_vec3_scale( camera.direction, -1.5f ) );
    world_position = glms_vec3_add( world_position, glms_vec3_scale( camera.right, -1.333f ) );
    world_position = glms_vec3_add( world_position, glms_vec3_scale( camera.up, -0.8f ) );

    // Draw 3 lines in world space in the camera view.
    static float axis_length = 0.1f;
    line( world_position, glms_vec3_add( world_position, { axis_length,0,0 } ), hydra::graphics::ColorUint::red, hydra::graphics::ColorUint::red );
    line( world_position, glms_vec3_add( world_position, { 0,axis_length,0 } ), hydra::graphics::ColorUint::green, hydra::graphics::ColorUint::green );
    line( world_position, glms_vec3_add( world_position, { 0,0,-axis_length } ), hydra::graphics::ColorUint::blue, hydra::graphics::ColorUint::blue );


    // Draw constellation lines
    if ( current_line ) {
        MapBufferParameters cb_map = { lines_cb, 0, 0 };
        LinesGPULocalConstants* cb_data = ( LinesGPULocalConstants* )gpu_device->map_buffer( cb_map );
        if ( cb_data ) {
            cb_data->view_projection = camera.view_projection;
            cb_data->projection = camera.projection;
            cb_data->resolution = { gpu_device->swapchain_width * 1.0f, gpu_device->swapchain_height * 1.0f, 1.0f / gpu_device->swapchain_width, 1.0f / gpu_device->swapchain_height };
            cb_data->line_width = constellation_lines_width;
            cb_data->alpha_mask_scale = constellation_alpha_mask_scale;

            gpu_device->unmap_buffer( cb_map );
        }

        const uint32_t mapping_size = sizeof( LinVertex ) * current_line;
        MapBufferParameters map_parameters_vb = { lines_vb, 0, mapping_size };
        LinVertex* vtx_dst = ( LinVertex* )gpu_device->map_buffer( map_parameters_vb );

        if ( vtx_dst ) {
            memcpy( vtx_dst, &s_line_buffer[ 0 ], mapping_size );

            gpu_device->unmap_buffer( map_parameters_vb );
        }

        gpu_commands->bind_pipeline( sort_key++, lines_rendering_pipeline );
        gpu_commands->bind_vertex_buffer( sort_key++, lines_vb, 0, 0 );
        gpu_commands->bind_resource_list( sort_key++, &lines_resource_list, 1, nullptr, 0 );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( sort_key++, TopologyType::Triangle, 0, num_vertices, 0, current_line / 2 );

        current_line = 0;
    }

    // GUI
    if ( ImGui::Begin( "Star Map", 0 ) ) {
        static int current_item = 0;

        if ( ImGui::Combo( "", &current_item, Constellations::s_names_strings, Constellations::Count_Abbr ) ) {
            chosen_constellation = ( Constellations::Abbreviations )current_item;
            update_constellation_gpu_data( current_item );
        }

        ImGui::SliderFloat( "Glare Scale", &glare_scale, 0.0f, 10.0f );
        ImGui::SameLine();
        if ( ImGui::Button( "Reset Glare Scale" ) ) {
            glare_scale = 1.0f;
        }

        ImGui::SliderFloat( "Star Radius Scale", &star_radius_scale, 0.0f, 10.0f );
        ImGui::SameLine();
        if ( ImGui::Button( "Reset Scale" ) ) {
            star_radius_scale = 1.0f;
        }
        ImGui::SliderFloat( "StarMap Distance Scale", &star_distance_scale, 0.0f, 100.0f );

        ImGui::SliderFloat( "Constellation Alpha", &constellation_lines_alpha, 0.0f, 1.0f );
        ImGui::SliderFloat( "Constellation Line Width", &constellation_lines_width, 0.0f, 10.0f );
        ImGui::SliderFloat( "Constellation Alpha Mask Scale", &constellation_alpha_mask_scale, 0.0f, 10.0f );
        ImGui::Checkbox( "Show All Constellations", &show_all_constellations );
        //ImGui::Checkbox( "Invert RY Rotation", &ry_invert );
        //ImGui::Checkbox( "Rotation Order Inverse", &rotation_order_invert );
        //ImGui::Checkbox( "Apply X scale", &apply_scale );

        ImGui::Separator();
        ImGui::LabelText( "", "LSMT %f, T %f", local_mean_sidereal_time, T );
        int ymd[ 3 ]{ year, month, day };
        ImGui::SliderInt3( "Year,Month,Day", ymd, 0, 3000 );
        year = ymd[ 0 ];
        month = glm_clamp( ymd[ 1 ], 1, 12 );
        day = glm_clamp(ymd[ 2 ], 1, 31);

        int time[ 3 ]{ hour, minute, second };
        ImGui::SliderInt3( "Hour,Min,Sec", time, 0, 60 );
        hour = glm_clamp( time[ 0 ], 0, 23 );
        minute = glm_clamp( time[ 1 ], 0, 59 );
        second = glm_clamp( time[ 2 ], 0, 59 );

        ImGui::SliderFloat( "Latitude", &latitude, 0, 359 );
        ImGui::SliderFloat( "Longitude", &longitude, 0, 359 );

        ImGui::Separator();

        ImGui::LabelText( "", "Camera Direction %1.3f, %1.3f, %1.3f", camera.direction.x, camera.direction.y, camera.direction.z );

        static vec3s direction;
        ImGui::LabelText( "", "Star Direction %1.3f, %1.3f, %1.3f", direction.x, direction.y, direction.z );

        if ( ImGui::Button( "Reset Camera" ) ) {
            camera.init_perpective( 0.1f, 1000.f, 60.f, gpu_device->swapchain_width * 1.f / gpu_device->swapchain_height );
        }

        /*if ( ImGui::Button( "Orient towards constellation" ) ) {
            chosen_constellation = Constellations::Abbreviations::UMI;

            if ( chosen_constellation != Constellations::Abbreviations::Count_Abbr ) {
                const int32_t constellation_index = Constellations::get_index( &constellations, Constellations::to_string( chosen_constellation ) );
                auto ce = constellations.entries[ constellation_index ];

                uint32_t star_index = constellations.data[ ce.offset ];
                const BrightStarEntry& bright_star = star_catalogue[ star_index - 1 ];

                convert_to_euclidean( bright_star.right_ascension, bright_star.declination, 1.0f, direction.x, direction.y, direction.z );
                direction = glms_mat4_mulv3( star_rotation_matrix, direction, 1.0f );

                Camera::yaw_pitch_from_direction( direction, camera_input.target_yaw, camera_input.target_pitch );
            }    
        }*/
    }
    ImGui::End();

    if ( ImGui::Begin( "Rendering Debug", 0 ) ) {
        if ( ImGui::Button( "Shader Rebuild" ) ) {
            load_render_resources( *update.gpu_device, false );
        }

        hfx::hfx_inspect_imgui( shader_effect_file );
    }
    ImGui::End();
}

void StarMapApplication::update_constellation_gpu_data( uint32_t constellation_index ) {

    hydra::graphics::MapBufferParameters map_parameters{ star_instance_buffer, 0, 0 };
    float* star_positions = ( float* )gpu_device->map_buffer( map_parameters );
    if ( star_positions ) {

        // 2 Behaviours:
        // 1. No constellation choosen, show all stars
        if ( constellation_index == Constellations::Count_Abbr || true ) {
            for ( size_t i = 0; i < star_count; i++ ) {
                float* star_position = star_positions + ( i * 8 );

                const BrightStarEntry& bright_star = star_catalogue[ i ];
                // Position
                const float radial_distance = 1.0f;
                convert_to_euclidean( bright_star.right_ascension, bright_star.declination, radial_distance, star_position[ 0 ], star_position[ 1 ], star_position[ 2 ] );
                
                // Magnitude
                star_position[ 3 ] = ( bright_star.visual_magnitude / 100.f ) + 0.4f;

                // Color
                Morgan_Keenan_to_color( bright_star.spectral_type[ 0 ], bright_star.spectral_type[ 1 ], star_position[ 4 ], star_position[ 5 ], star_position[ 6 ] );
            }
        }
        else { // 2. Constellation chosen

            const int32_t constellation_index = Constellations::get_index( &constellations, Constellations::to_string( chosen_constellation ) );
            auto ce = constellations.entries[ constellation_index ];

            for ( size_t j = 0; j < ce.count; j++ ) {
                uint32_t star_index = constellations.data[ ce.offset + j * 2 ];
                hydra::print_format( "Star %d %d\n", j, star_index );
            }

            const Constellations::ConstellationEntry& constellation = constellations.entries[ constellation_index ];

            for ( size_t i = 0; i < ce.count; i++ ) {
                float* star_position = star_positions + ( i * 8 );

                uint32_t star_index = constellations.data[ ce.offset + i * 2 ];
                const BrightStarEntry& bright_star = star_catalogue[ star_index - 1 ];

                // Position
                const float radial_distance = 1.0f;
                convert_to_euclidean( bright_star.right_ascension, bright_star.declination, radial_distance, star_position[ 0 ], star_position[ 1 ], star_position[ 2 ] );

                // Magnitude
                star_position[ 3 ] = ( bright_star.visual_magnitude / 100.f ) + 0.4f;

                // Color
                Morgan_Keenan_to_color( bright_star.spectral_type[ 0 ], bright_star.spectral_type[ 1 ], star_position[ 4 ], star_position[ 5 ], star_position[ 6 ] );
            }
        }
        
        gpu_device->unmap_buffer( map_parameters );
    }
}

void StarMapApplication::draw_constellation_lines( const Constellations::ConstellationEntry& constellation, const mat4s& star_rotation_matrix ) {
    for ( size_t i = 0; i < constellation.count; i++ ) {
        uint32_t star_index = constellations.data[ constellation.offset + i * 2 ];
        const BrightStarEntry& bright_star = star_catalogue[ star_index - 1 ];

        uint32_t star_index_2 = constellations.data[ constellation.offset + i * 2 + 1 ];
        const BrightStarEntry& bright_star_2 = star_catalogue[ star_index_2 - 1 ];

        vec3s star_position, star_position2;
        convert_to_euclidean( bright_star.right_ascension, bright_star.declination, 1, star_position.x, star_position.y, star_position.z );
        convert_to_euclidean( bright_star_2.right_ascension, bright_star_2.declination, 1, star_position2.x, star_position2.y, star_position2.z );

        // Rotate according to long, lat
        star_position = glms_mat4_mulv3( star_rotation_matrix, star_position, 0.0f );
        star_position2 = glms_mat4_mulv3( star_rotation_matrix, star_position2, 0.0f );

        line( star_position, star_position2, hydra::graphics::ColorUint::from_u8( 255, 255, 255, constellation_lines_alpha * 255 ), hydra::graphics::ColorUint::from_u8( 255, 255, 255, constellation_lines_alpha * 255 ) );
    }
}

void StarMapApplication::load_render_resources( hydra::graphics::Device& gpu_device, bool startup ) {

    // Delete old resources
    if ( !startup ) {
        gpu_device.destroy_pipeline( star_rendering_pipeline );
        gpu_device.destroy_buffer( star_cb );
        gpu_device.destroy_buffer( star_instance_buffer );
        gpu_device.destroy_resource_list( star_resource_list );
        gpu_device.destroy_resource_list_layout( star_resource_list_layout );
        
        gpu_device.destroy_pipeline( lines_rendering_pipeline );
        gpu_device.destroy_resource_list( lines_resource_list );
        gpu_device.destroy_resource_list_layout( lines_resource_list_layout );
        gpu_device.destroy_buffer( lines_vb );
        gpu_device.destroy_buffer( lines_cb );
    }

    // Create startup only resources
    if ( startup ) {
        int w, h, comp;
        uint8_t* image_data = stbi_load( "..\\data\\articles\\StarRendering\\capsule_mask.png", &w, &h, &comp, 0 );
        hydra::graphics::TextureCreation texture_creation = { image_data, w, h, 1, 1, 0, hydra::graphics::TextureFormat::R8G8B8A8_UNORM, hydra::graphics::TextureType::Texture2D, "Capsule" };
        capsule_texture = gpu_device.create_texture( texture_creation );
    }

    hfx::hfx_compile( "..\\data\\articles\\StarRendering\\stars.hfx", "..\\bin\\stars.bhfx", hfx::CompileOptions_Vulkan | hfx::CompileOptions_Embedded, &shader_effect_file );

    {
        hydra::graphics::PipelineCreation render_pipeline;
        hfx::shader_effect_get_pipeline( shader_effect_file, 0, render_pipeline );

        hydra::graphics::ResourceListLayoutCreation rll_creation{};
        hfx::shader_effect_get_resource_list_layout( shader_effect_file, 0, 0, rll_creation );

        // TODO: num active layout is already set to the max, so using add_rll breaks.
        star_resource_list_layout = gpu_device.create_resource_list_layout( rll_creation );
        render_pipeline.resource_list_layout[ 0 ] = star_resource_list_layout;
        render_pipeline.render_pass = gpu_device.get_swapchain_pass();

        star_rendering_pipeline = gpu_device.create_pipeline( render_pipeline );

        hydra::graphics::BufferCreation cb_creation = { hydra::graphics::BufferType::Constant, hydra::graphics::ResourceUsageType::Dynamic, ( 16 + 16 + 12 ) * 4, nullptr, "CB_Stars" };
        star_cb = gpu_device.create_buffer( cb_creation );

        hydra::graphics::ResourceListCreation rl_creation{};
        rl_creation.set_layout( render_pipeline.resource_list_layout[ 0 ] ).add_resource( star_cb.handle ).set_name( "RL_Star" );
        star_resource_list = gpu_device.create_resource_list( rl_creation );

        // Create star instances buffer
        hydra::graphics::BufferCreation ib_creation = { hydra::graphics::BufferType::Vertex, hydra::graphics::ResourceUsageType::Dynamic, 32 * star_count, nullptr, "InstanceBuffer_Stars" };
        star_instance_buffer = gpu_device.create_buffer( ib_creation );
    }

    update_constellation_gpu_data( Constellations::Abbreviations::Count_Abbr );

    // Line rendering resource creation
    {
        hydra::graphics::PipelineCreation render_pipeline_lines;
        hfx::shader_effect_get_pipeline( shader_effect_file, 1, render_pipeline_lines );

        hydra::graphics::ResourceListLayoutCreation rll_creation_lines{};
        hfx::shader_effect_get_resource_list_layout( shader_effect_file, 1, 0, rll_creation_lines );

        // TODO: num active layout is already set to the max, so using add_rll breaks.
        lines_resource_list_layout = gpu_device.create_resource_list_layout( rll_creation_lines );
        render_pipeline_lines.resource_list_layout[ 0 ] = lines_resource_list_layout;
        render_pipeline_lines.render_pass = gpu_device.get_swapchain_pass();

        lines_rendering_pipeline = gpu_device.create_pipeline( render_pipeline_lines );

        hydra::graphics::BufferCreation vb_creation = { hydra::graphics::BufferType::Vertex, hydra::graphics::ResourceUsageType::Dynamic, sizeof( LinVertex ) * k_max_lines, nullptr, "VB_Lines" };
        lines_vb = gpu_device.create_buffer( vb_creation );

        hydra::graphics::BufferCreation cb_creation_l = { hydra::graphics::BufferType::Constant, hydra::graphics::ResourceUsageType::Dynamic, sizeof( LinesGPULocalConstants ), nullptr, "CB_Lines" };
        lines_cb = gpu_device.create_buffer( cb_creation_l );

        hydra::graphics::ResourceListCreation rl_creation_lines{};
        rl_creation_lines.set_layout( lines_resource_list_layout ).add_resource( lines_cb.handle ).add_resource( capsule_texture.handle ).set_name( "RL_Lines" );
        lines_resource_list = gpu_device.create_resource_list( rl_creation_lines );
    }
}

//
//
void StarMapApplication::line( const vec3s& from, const vec3s& to, uint32_t color0, uint32_t color1 ) {
    if ( current_line >= k_max_lines )
        return;

    s_line_buffer[ current_line++ ].set( from.x, from.y, from.z, color0 );
    s_line_buffer[ current_line++ ].set( to.x, to.y, to.z, color1 );
}


void LinVertex::set( float x, float y, float z, uint32_t color ) {
    position = { x, y, z };
    this->color = color;
}

// MAIN ///////////////////////////////////////////////////////////////////////////////////////////

int main( int argc, char** argv ) {

    StarMapApplication app;
    app.main_loop( { nullptr, 1280, 720, hydra::ApplicationRootTask_SDL, hydra::RenderingService_HighLevelRenderer, "Star Map" } );
    return 0;
}