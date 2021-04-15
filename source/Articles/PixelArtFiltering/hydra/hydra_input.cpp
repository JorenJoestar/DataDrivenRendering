//
//  Hydra Input - v0.04

#include "hydra_input.h"

#include "imgui\imgui.h"

#define INPUT_BACKEND_SDL

#if defined (INPUT_BACKEND_SDL)
#include <SDL.h>
#endif // INPUT_BACKEND_SDL

namespace hydra {

//
//
struct InputBackend {

    void            init( Gamepad* gamepads, u32 num_gamepads );
    void            shutdown();

    void            get_mouse_state( InputVector2& position, u8* buttons, u32 num_buttons );

    void            on_event( void* event_, u8* keys, u32 num_keys, Gamepad* gamepads, u32 num_gamepads );

}; // struct InputBackendSDL

#if defined (INPUT_BACKEND_SDL)
static bool init_gamepad( int32_t index, Gamepad& gamepad ) {
    SDL_Joystick* joy = SDL_JoystickOpen( index );

    // Set memory to 0
    memset( &gamepad, 0, sizeof( Gamepad ) );

    if ( joy ) {
        hydra::print_format( "Opened Joystick 0\n" );
        hydra::print_format( "Name: %s\n", SDL_JoystickNameForIndex( index ) );
        hydra::print_format( "Number of Axes: %d\n", SDL_JoystickNumAxes( joy ) );
        hydra::print_format( "Number of Buttons: %d\n", SDL_JoystickNumButtons( joy ) );

        gamepad.index = index;
        gamepad.name = SDL_JoystickNameForIndex( index );
        gamepad.handle = joy;
        gamepad.id = SDL_JoystickInstanceID( joy );

        return true;

    } else {
        hydra::print_format( "Couldn't open Joystick %u\n", index );
        gamepad.index = u32_max;

        return false;
    }
}

static void terminate_gamepad( Gamepad& gamepad ) {

    SDL_JoystickClose( (SDL_Joystick*)gamepad.handle );
    gamepad.index = u32_max;
    gamepad.name = 0;
    gamepad.handle = 0;
    gamepad.id = u32_max;
}

// InputBackendSDL //////////////////////////////////////////////////////////////
void InputBackend::init( Gamepad* gamepads, u32 num_gamepads ) {

    SDL_JoystickEventState( SDL_ENABLE );

    const i32 num_joystics = SDL_NumJoysticks();
    if ( num_joystics > 0 ) {
        hydra::print_format( "Detected joysticks!" );

        for ( i32 i = 0; i < num_joystics; i++ ) {
            init_gamepad( i, gamepads[ i ] );
        }
    }
}

void InputBackend::shutdown() {

    SDL_JoystickEventState( SDL_DISABLE );
}

static u32 to_sdl_mouse_button( MouseButtons button ) {
    switch ( button ) {
        case MOUSE_BUTTONS_LEFT:
            return SDL_BUTTON_LEFT;
        case MOUSE_BUTTONS_MIDDLE:
            return SDL_BUTTON_MIDDLE;
        case MOUSE_BUTTONS_RIGHT:
            return SDL_BUTTON_RIGHT;
    }
    
    return u32_max;
}

void InputBackend::get_mouse_state( InputVector2& position, u8* buttons, u32 num_buttons ) {
    i32 mouse_x, mouse_y;
    u32 mouse_buttons = SDL_GetMouseState( &mouse_x, &mouse_y );

    for ( u32 i = 0; i < num_buttons; ++i ) {
        u32 sdl_button = to_sdl_mouse_button( ( MouseButtons )i );
        buttons[ i ] = mouse_buttons & SDL_BUTTON( sdl_button );
    }

    position.x = ( f32 )mouse_x;
    position.y = ( f32 )mouse_y;
}

void InputBackend::on_event( void* event_, u8* keys, u32 num_keys, Gamepad* gamepads, u32 num_gamepads ) {
    SDL_Event& event = *( SDL_Event* )event_;
    switch ( event.type ) {

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            i32 key = event.key.keysym.scancode;
            if ( key >= 0 && key < (i32)num_keys )
                keys[ key ] = ( event.type == SDL_KEYDOWN );

            break;
        }

        case SDL_JOYDEVICEADDED:
        {
            hydra::print_format( "Gamepad Added\n" );
            int32_t joystick_index = event.jdevice.which;

            init_gamepad( joystick_index, gamepads[ joystick_index ] );

            break;
        }

        case SDL_JOYDEVICEREMOVED:
        {
            hydra::print_format( "Gamepad Removed\n" );
            int32_t joystick_instance_id = event.jdevice.which;
            // Search for the correct gamepad
            for ( size_t i = 0; i < k_max_gamepads; i++ ) {
                if ( gamepads[ i ].id == joystick_instance_id ) {
                    terminate_gamepad( gamepads[ i ] );
                    break;
                }
            }

            break;
        }

        case SDL_JOYAXISMOTION:
        {
#if defined (INPUT_DEBUG_OUTPUT)
            hydra::print_format( "Axis %u - %f\n", event.jaxis.axis, event.jaxis.value / 32768.0f );
#endif // INPUT_DEBUG_OUTPUT

            for ( size_t i = 0; i < k_max_gamepads; i++ ) {
                if ( gamepads[ i ].id == event.jaxis.which ) {
                    gamepads[ i ].axis[ event.jaxis.axis ] = event.jaxis.value / 32768.0f;
                    break;
                }
            }
            break;
        }

        case SDL_JOYBALLMOTION:
        {
#if defined (INPUT_DEBUG_OUTPUT)
            hydra::print_format( "Ball\n" );
#endif // INPUT_DEBUG_OUTPUT

            for ( size_t i = 0; i < k_max_gamepads; i++ ) {
                if ( gamepads[ i ].id == event.jball.which ) {
                    break;
                }
            }
            break;
        }

        case SDL_JOYHATMOTION:
        {
#if defined (INPUT_DEBUG_OUTPUT)
            hydra::print_format( "Hat\n" );
#endif // INPUT_DEBUG_OUTPUT

            /*for ( size_t i = 0; i < k_max_gamepads; i++ ) {
                if ( gamepads[ i ].id == event.jhat.which ) {
                    gamepads[ i ].hats[ event.jhat.hat ] = event.jhat.value;
                    break;
                }
            }*/
            break;
        }

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        {
#if defined (INPUT_DEBUG_OUTPUT)
            hydra::print_format( "Button\n" );
#endif // INPUT_DEBUG_OUTPUT

            for ( size_t i = 0; i < k_max_gamepads; i++ ) {
                if ( gamepads[ i ].id == event.jbutton.which ) {
                    gamepads[ i ].buttons[ event.jbutton.button ] = event.jbutton.state == SDL_PRESSED ? 1 : 0;
                    break;
                }
            }
            break;
        }
    }
}

#else

// STUB implementation

void InputBackend::init( Gamepad* gamepads, u32 num_gamepads ) {
}

void InputBackend::shutdown() {
}

void InputBackend::on_event( void* event_, Gamepad* gamepads, u32 num_gamepads ) {
}

void InputBackend::get_mouse_state( InputVector2& position, u8* buttons, u32 num_buttons ) {
    position.x = FLT_MAX;
    position.y = FLT_MAX;
}

#endif // INPUT_BACKEND_SDL

// InputSystem //////////////////////////////////////////////////////////////////
static InputBackend s_input_backend;

void InputSystem::init() {
    string_buffer.init( 1000, hydra::memory_get_system_allocator() );
    array_init( input_action_maps );

    // Init gamepads handles
    for ( size_t i = 0; i < k_max_gamepads; i++ ) {
        gamepads[ i ].handle = nullptr;
    }
    memset( keys, 0, KEY_COUNT );
    memset( previous_keys, 0, KEY_COUNT );
    memset( mouse_button, 0, MOUSE_BUTTONS_COUNT );
    memset( previous_mouse_button, 0, MOUSE_BUTTONS_COUNT );

    s_input_backend.init( gamepads, k_max_gamepads );
}

void InputSystem::terminate() {

    s_input_backend.shutdown();

    string_buffer.terminate();
}

static constexpr f32 k_mouse_drag_min_distance = 4.f;

bool InputSystem::is_key_down( Keys key ) {
    return keys[ key ] && has_focus;
}

bool InputSystem::is_key_just_pressed( Keys key, bool repeat ) {
    return keys[ key ] && !previous_keys[ key ] && has_focus;
}

bool InputSystem::is_mouse_down( MouseButtons button ) {
    return mouse_button[ button ];
}

bool InputSystem::is_mouse_clicked( MouseButtons button ) {
    return mouse_button[ button ] && !previous_mouse_button[ button ];
}

bool InputSystem::is_mouse_released( MouseButtons button ) {
    return !mouse_button[ button ];
}

bool InputSystem::is_mouse_dragging( MouseButtons button ) {
    if ( !mouse_button[ button ] )
        return false;

    return mouse_drag_distance[ button ] > k_mouse_drag_min_distance;
}

void InputSystem::on_event( void* event_) {
    s_input_backend.on_event( event_, keys, KEY_COUNT, gamepads, k_max_gamepads );
}

void InputSystem::add( InputActionMap& action_map ) {
    action_map.active = true;
    array_push( input_action_maps, action_map );
}

void InputSystem::new_frame() {
    // Cache previous frame keys.
    // Resetting previous frame breaks key pressing - there can be more frames between key presses even if continuously pressed.
    for ( u32 i = 0; i < KEY_COUNT; ++i ) {
        previous_keys[ i ] = keys[ i ];
        //keys[ i ] = 0;
    }

    for ( u32 i = 0; i < MOUSE_BUTTONS_COUNT; ++i ) {
        previous_mouse_button[ i ] = mouse_button[ i ];
    }
}

void InputSystem::update( f32 delta ) {

    // Update Mouse ////////////////////////////////////////
    previous_mouse_position = mouse_position;
    // Update current mouse state
    s_input_backend.get_mouse_state( mouse_position, mouse_button, MOUSE_BUTTONS_COUNT );

    for ( u32 i = 0; i < MOUSE_BUTTONS_COUNT; ++i ) {
        // Just clicked. Save position
        if ( is_mouse_clicked( ( MouseButtons )i ) ) {
            mouse_clicked_position[ i ] = mouse_position;
        }
        else if ( is_mouse_down( ( MouseButtons )i ) ) {
            f32 delta_x = mouse_position.x - mouse_clicked_position[ i ].x;
            f32 delta_y = mouse_position.y - mouse_clicked_position[ i ].y;
            mouse_drag_distance[ i ] = sqrtf( (delta_x * delta_x) + (delta_y * delta_y) );
        }
    }

    
    // Update all Input Actions ////////////////////////////
    // TODO: flat all arrays
    // Scan each action map
    for ( size_t i = 0; i < array_size( input_action_maps ); i++ ) {
        InputActionMap& action_map = input_action_maps[ i ];
        if ( !action_map.active ) {
            continue;
        }

        // Scan each action of the map
        for ( size_t j = 0; j < action_map.num_actions; j++ ) {
            InputAction& input_action = action_map.actions[ j ];

            // First get all the button or composite parts. Composite input will be calculated after.
            for ( size_t k = 0; k < array_size( input_action.bindings ); k++ ) {
                InputBinding& input_binding = input_action.bindings[ k ];
                // Skip composite bindings. Their value will be calculated after.
                if ( input_binding.is_composite )
                    continue;

                input_binding.value = false;

                switch ( input_binding.device ) {
                    case InputBinding::DEVICE_KEYBOARD:
                    {
                        bool key_value = input_binding.repeat ? is_key_down( (Keys)input_binding.button ) : is_key_just_pressed( (Keys)input_binding.button, false );
                        input_binding.value = key_value ? 1.0f : 0.0f;
                        break;
                    }

                    case InputBinding::DEVICE_GAMEPAD:
                    {
                        Gamepad& gamepad = gamepads[ 0 ];
                        if ( gamepad.handle == nullptr ) {
                            break;
                        }

                        const float min_deadzone = input_binding.min_deadzone;
                        const float max_deadzone = input_binding.max_deadzone;

                        switch ( input_binding.device_part ) {
                            case InputBinding::GAMEPAD_AXIS:
                            {
                                input_binding.value = gamepad.axis[ input_binding.button ];
                                input_binding.value = abs( input_binding.value ) < min_deadzone ? 0.0f : input_binding.value;
                                input_binding.value = abs( input_binding.value ) > max_deadzone ? ( input_binding.value < 0 ? -1.0f : 1.0f ) : input_binding.value;

                                break;
                            }
                            case InputBinding::GAMEPAD_BUTTONS:
                            {
                                input_binding.value = gamepad.buttons[ input_binding.button ];
                                break;
                            }
                            /*case InputBinding::GAMEPAD_HAT:
                            {
                                input_binding.value = gamepad.hats[ input_binding.button ];
                                break;
                            }*/
                        }

                        break;
                    }
                }
            }

            // Calculate/syntethize composite input values into input action
            input_action.value = { 0,0 };

            for ( size_t k = 0; k < array_size( input_action.bindings ); k++ ) {
                InputBinding& input_binding = input_action.bindings[ k ];

                if ( input_binding.is_part_of_composite )
                    continue;

                switch ( input_binding.type ) {
                    case InputBinding::BUTTON:
                    {
                        input_action.value.x = max( input_action.value.x, input_binding.value ? 1.0f : 0.0f );
                        break;
                    }

                    case InputBinding::AXIS_1D:
                    {
                        input_action.value.x = input_binding.value != 0.f ? input_binding.value : input_action.value.x;
                        break;
                    }

                    case InputBinding::AXIS_2D:
                    {
                        // Retrieve following 2 bindings
                        InputBinding& input_binding_x = input_action.bindings[ ++k ];
                        InputBinding& input_binding_y = input_action.bindings[ ++k ];

                        input_action.value.x = input_binding_x.value != 0.0f ? input_binding_x.value : input_action.value.x;
                        input_action.value.y = input_binding_y.value != 0.0f ? input_binding_y.value : input_action.value.y;

                        break;
                    }

                    case InputBinding::VECTOR_1D:
                    {
                        // Retrieve following 2 bindings
                        InputBinding& input_binding_pos = input_action.bindings[ ++k ];
                        InputBinding& input_binding_neg = input_action.bindings[ ++k ];

                        input_action.value.x = input_binding_pos.value ? input_binding_pos.value : input_binding_neg.value ? -input_binding_neg.value : input_action.value.x;
                        break;
                    }

                    case InputBinding::VECTOR_2D:
                    {
                        // Retrieve following 4 bindings
                        InputBinding& input_binding_up = input_action.bindings[ ++k ];
                        InputBinding& input_binding_down = input_action.bindings[ ++k ];
                        InputBinding& input_binding_left = input_action.bindings[ ++k ];
                        InputBinding& input_binding_right = input_action.bindings[ ++k ];

                        input_action.value.x = input_binding_right.value ? 1.0f : input_binding_left.value ? -1.0f : input_action.value.x;
                        input_action.value.y = input_binding_up.value ? 1.0f : input_binding_down.value ? -1.0f : input_action.value.y;
                        break;
                    }
                }
            }
        }
    }
}

// InputAction /////////////////////////////////////////////////////////
static InputBinding::Device device_from_part( InputBinding::DevicePart part ) {
    switch ( part ) {
        case InputBinding::MOUSE:
        {
            return InputBinding::DEVICE_MOUSE;
        }

        case InputBinding::GAMEPAD_AXIS:
        case InputBinding::GAMEPAD_BUTTONS:
        //case InputBinding::GAMEPAD_HAT:
        {
            return InputBinding::DEVICE_GAMEPAD;
        }

        case InputBinding::KEYBOARD:
        default:
        {
            return InputBinding::DEVICE_KEYBOARD;
        }
    }
}

void InputAction::init() {

    array_init( bindings );
}

void InputAction::add_button( InputBinding::DevicePart device_part, uint16_t button, bool repeat ) {
    const InputBinding binding{ InputBinding::BUTTON, device_from_part( device_part ), device_part, button, 0.f, false, false, repeat };
    array_push( bindings, binding );
}

void InputAction::add_vector_1d( InputBinding::DevicePart device_part_pos, uint16_t button_pos, InputBinding::DevicePart device_part_neg,
                                 uint16_t button_neg, bool repeat ) {
    const InputBinding binding{ InputBinding::VECTOR_1D, device_from_part( device_part_pos ), device_part_pos, button_pos, 0.f, true, false, repeat };
    const InputBinding binding_positive{ InputBinding::VECTOR_1D, device_from_part( device_part_pos ), device_part_pos, button_pos, 0.f, false, true, repeat };
    const InputBinding binding_negative{ InputBinding::VECTOR_1D, device_from_part( device_part_neg ), device_part_neg, button_neg, 0.f, false, true, repeat };

    array_push( bindings, binding );
    array_push( bindings, binding_positive );
    array_push( bindings, binding_negative );
}

void InputAction::add_vector_2d( InputBinding::DevicePart device_part_up, uint16_t button_up, InputBinding::DevicePart device_part_down, uint16_t button_down,
                                 InputBinding::DevicePart device_part_left, uint16_t button_left, InputBinding::DevicePart device_part_right, uint16_t button_right,
                                 bool repeat ) {
    const InputBinding binding{ InputBinding::VECTOR_2D, device_from_part( device_part_up ), device_part_up, button_up, 0.f, true, false, repeat };
    const InputBinding binding_up{ InputBinding::VECTOR_2D, device_from_part( device_part_up ), device_part_up, button_up, 0.f, false, true, repeat };
    const InputBinding binding_down{ InputBinding::VECTOR_2D, device_from_part( device_part_down ), device_part_down, button_down, 0.f, false, true, repeat };
    const InputBinding binding_left{ InputBinding::VECTOR_2D, device_from_part( device_part_left ), device_part_left, button_left, 0.f, false, true, repeat };
    const InputBinding binding_right{ InputBinding::VECTOR_2D, device_from_part( device_part_right ), device_part_right, button_right, 0.f, false, true, repeat };

    array_push( bindings, binding );
    array_push( bindings, binding_up );
    array_push( bindings, binding_down );
    array_push( bindings, binding_left );
    array_push( bindings, binding_right );
}

void InputAction::add_axis_1d( InputBinding::DevicePart device_part, uint16_t axis, float min_deadzone, float max_deadzone ) {
    const InputBinding binding{ InputBinding::AXIS_1D, device_from_part( device_part ), device_part, axis, 0.f, false, false, false, min_deadzone, max_deadzone };

    array_push( bindings, binding );
}

void InputAction::add_axis_2d( InputBinding::DevicePart device_part, uint16_t x_axis, uint16_t y_axis, float min_deadzone, float max_deadzone ) {
    const InputBinding binding{ InputBinding::AXIS_2D, device_from_part( device_part ), device_part, x_axis, 0.f, true, false, false, min_deadzone, max_deadzone };
    const InputBinding binding_x{ InputBinding::AXIS_2D, device_from_part( device_part ), device_part, x_axis, 0.f, false, true, false, min_deadzone, max_deadzone };
    const InputBinding binding_y{ InputBinding::AXIS_2D, device_from_part( device_part ), device_part, y_axis, 0.f, false, true, false, min_deadzone, max_deadzone };

    array_push( bindings, binding );
    array_push( bindings, binding_x );
    array_push( bindings, binding_y );
}

bool InputAction::triggered() const {
    return value.x != 0.0f;
}

float InputAction::read_value_1d() const {
    return value.x;
}

InputVector2 InputAction::read_value_2d() const {
    return value;
}

// InputActionMap ///////////////////////////////////////////////////////////////
void InputActionMap::init( u32 max_actions ) {
    array_init( actions );
    array_set_length( actions, max_actions );

    num_actions = 0;
}

void InputActionMap::shutdown() {
    array_free( actions );
}

InputAction& InputActionMap::add_action() {
    if ( num_actions == ( u32 )array_size( actions ) )
        assert( false );

    actions[ num_actions ].init();
    return actions[ num_actions++ ];
}

} // namespace hydra