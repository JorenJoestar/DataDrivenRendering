#pragma once

//
//  Hydra Input - v0.04
//
//  Input devices and actions implementation.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2021/01/07    18.16
//
//
// Revision history //////////////////////
//
//      0.04 (2021/01/19): + Added repeat, min and max deadzones to input bindings.
//      0.03 (2021/01/19): + Added init method to action map to preallocate action array. Typical array resize pointer invalidation.
//      0.02 (2021/01/15): + Added vector 1d to differenitate between axis and button compositing.
//      0.01 (2021/01/07): + First implementation.


#include "hydra_lib.h"

namespace hydra {

static const uint32_t               k_max_gamepads = 4;

//
//
enum GamepadAxis {
    GAMEPAD_AXIS_LEFTX = 0,
    GAMEPAD_AXIS_LEFTY,
    GAMEPAD_AXIS_RIGHTX,
    GAMEPAD_AXIS_RIGHTY,
    GAMEPAD_AXIS_TRIGGERLEFT,
    GAMEPAD_AXIS_TRIGGERRIGHT,
    GAMEPAD_AXIS_COUNT
}; // enum GamepadAxis

//
//
enum GamepadButtons {
    GAMEPAD_BUTTON_A = 0,
    GAMEPAD_BUTTON_B,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_LEFTSTICK,
    GAMEPAD_BUTTON_RIGHTSTICK,
    GAMEPAD_BUTTON_LEFTSHOULDER,
    GAMEPAD_BUTTON_RIGHTSHOULDER,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_COUNT
}; // enum GamepadButtons

//
//
enum MouseButtons {
    MOUSE_BUTTONS_NONE = -1,
    MOUSE_BUTTONS_LEFT = 0,
    MOUSE_BUTTONS_RIGHT,
    MOUSE_BUTTONS_MIDDLE,
    MOUSE_BUTTONS_COUNT

}; // enum MouseButtons

//
//
enum Keys {
    KEY_UNKNOWN = 0,
    KEY_A = 4,
    KEY_B = 5,
    KEY_C = 6,
    KEY_D = 7,
    KEY_E = 8,
    KEY_F = 9,
    KEY_G = 10,
    KEY_H = 11,
    KEY_I = 12,
    KEY_J = 13,
    KEY_K = 14,
    KEY_L = 15,
    KEY_M = 16,
    KEY_N = 17,
    KEY_O = 18,
    KEY_P = 19,
    KEY_Q = 20,
    KEY_R = 21,
    KEY_S = 22,
    KEY_T = 23,
    KEY_U = 24,
    KEY_V = 25,
    KEY_W = 26,
    KEY_X = 27,
    KEY_Y = 28,
    KEY_Z = 29,

    KEY_1 = 30,
    KEY_2 = 31,
    KEY_3 = 32,
    KEY_4 = 33,
    KEY_5 = 34,
    KEY_6 = 35,
    KEY_7 = 36,
    KEY_8 = 37,
    KEY_9 = 38,
    KEY_0 = 39,

    KEY_RETURN = 40,
    KEY_ESCAPE = 41,
    KEY_BACKSPACE = 42,
    KEY_TAB = 43,
    KEY_SPACE = 44,

    KEY_MINUS = 45,
    KEY_EQUALS = 46,
    KEY_LEFTBRACKET = 47,
    KEY_RIGHTBRACKET = 48,
    KEY_BACKSLASH = 49,
    KEY_NONUSHASH = 50,
    KEY_SEMICOLON = 51,
    KEY_APOSTROPHE = 52,
    KEY_GRAVE = 53,
    KEY_COMMA = 54,
    KEY_PERIOD = 55,
    KEY_SLASH = 56,

    KEY_CAPSLOCK = 57,

    KEY_F1 = 58,
    KEY_F2 = 59,
    KEY_F3 = 60,
    KEY_F4 = 61,
    KEY_F5 = 62,
    KEY_F6 = 63,
    KEY_F7 = 64,
    KEY_F8 = 65,
    KEY_F9 = 66,
    KEY_F10 = 67,
    KEY_F11 = 68,
    KEY_F12 = 69,

    KEY_PRINTSCREEN = 70,
    KEY_SCROLLLOCK = 71,
    KEY_PAUSE = 72,
    KEY_INSERT = 73,
    KEY_HOME = 74,
    KEY_PAGEUP = 75,
    KEY_DELETE = 76,
    KEY_END = 77,
    KEY_PAGEDOWN = 78,
    KEY_RIGHT = 79,
    KEY_LEFT = 80,
    KEY_DOWN = 81,
    KEY_UP = 82,

    KEY_NUMLOCKCLEAR = 83,
    KEY_KP_DIVIDE = 84,
    KEY_KP_MULTIPLY = 85,
    KEY_KP_MINUS = 86,
    KEY_KP_PLUS = 87,
    KEY_KP_ENTER = 88,
    KEY_KP_1 = 89,
    KEY_KP_2 = 90,
    KEY_KP_3 = 91,
    KEY_KP_4 = 92,
    KEY_KP_5 = 93,
    KEY_KP_6 = 94,
    KEY_KP_7 = 95,
    KEY_KP_8 = 96,
    KEY_KP_9 = 97,
    KEY_KP_0 = 98,
    KEY_KP_PERIOD = 99,

    KEY_NONUSBACKSLASH = 100, 
    KEY_APPLICATION = 101,
    KEY_POWER = 102,
    KEY_KP_EQUALS = 103,
    KEY_F13 = 104,
    KEY_F14 = 105,
    KEY_F15 = 106,
    KEY_F16 = 107,
    KEY_F17 = 108,
    KEY_F18 = 109,
    KEY_F19 = 110,
    KEY_F20 = 111,
    KEY_F21 = 112,
    KEY_F22 = 113,
    KEY_F23 = 114,
    KEY_F24 = 115,
    KEY_EXE = 116,
    KEY_HELP = 117,
    KEY_MENU = 118,
    KEY_SELECT = 119,
    KEY_STOP = 120,
    KEY_AGAIN = 121,
    KEY_UNDO = 122,
    KEY_CUT = 123,
    KEY_COPY = 124,
    KEY_PASTE = 125,
    KEY_FIND = 126,
    KEY_MUTE = 127,
    KEY_VOLUMEUP = 128,
    KEY_VOLUMEDOWN = 129,

    KEY_KP_COMMA = 133,
    KEY_KP_EQUALSAS400 = 134,

    KEY_INTERNATIONAL1 = 135,
    KEY_INTERNATIONAL2 = 136,
    KEY_INTERNATIONAL3 = 137, /**< Yen */
    KEY_INTERNATIONAL4 = 138,
    KEY_INTERNATIONAL5 = 139,
    KEY_INTERNATIONAL6 = 140,
    KEY_INTERNATIONAL7 = 141,
    KEY_INTERNATIONAL8 = 142,
    KEY_INTERNATIONAL9 = 143,
    KEY_LANG1 = 144, /**< Hangul/English toggle */
    KEY_LANG2 = 145, /**< Hanja conversion */
    KEY_LANG3 = 146, /**< Katakana */
    KEY_LANG4 = 147, /**< Hiragana */
    KEY_LANG5 = 148, /**< Zenkaku/Hankaku */
    KEY_LANG6 = 149, /**< reserved */
    KEY_LANG7 = 150, /**< reserved */
    KEY_LANG8 = 151, /**< reserved */
    KEY_LANG9 = 152, /**< reserved */

    KEY_ALTERASE = 153, /**< Erase-Eaze */
    KEY_SYSREQ = 154,
    KEY_CANCEL = 155,
    KEY_CLEAR = 156,
    KEY_PRIOR = 157,
    KEY_RETURN2 = 158,
    KEY_SEPARATOR = 159,
    KEY_OUT = 160,
    KEY_OPER = 161,
    KEY_CLEARAGAIN = 162,
    KEY_CRSEL = 163,
    KEY_EXSEL = 164,

    KEY_KP_00 = 176,
    KEY_KP_000 = 177,
    KEY_THOUSANDSSEPARATOR = 178,
    KEY_DECIMALSEPARATOR = 179,
    KEY_CURRENCYUNIT = 180,
    KEY_CURRENCYSUBUNIT = 181,
    KEY_KP_LEFTPAREN = 182,
    KEY_KP_RIGHTPAREN = 183,
    KEY_KP_LEFTBRACE = 184,
    KEY_KP_RIGHTBRACE = 185,
    KEY_KP_TAB = 186,
    KEY_KP_BACKSPACE = 187,
    KEY_KP_A = 188,
    KEY_KP_B = 189,
    KEY_KP_C = 190,
    KEY_KP_D = 191,
    KEY_KP_E = 192,
    KEY_KP_F = 193,
    KEY_KP_XOR = 194,
    KEY_KP_POWER = 195,
    KEY_KP_PERCENT = 196,
    KEY_KP_LESS = 197,
    KEY_KP_GREATER = 198,
    KEY_KP_AMPERSAND = 199,
    KEY_KP_DBLAMPERSAND = 200,
    KEY_KP_VERTICALBAR = 201,
    KEY_KP_DBLVERTICALBAR = 202,
    KEY_KP_COLON = 203,
    KEY_KP_HASH = 204,
    KEY_KP_SPACE = 205,
    KEY_KP_AT = 206,
    KEY_KP_EXCLAM = 207,
    KEY_KP_MEMSTORE = 208,
    KEY_KP_MEMRECALL = 209,
    KEY_KP_MEMCLEAR = 210,
    KEY_KP_MEMADD = 211,
    KEY_KP_MEMSUBTRACT = 212,
    KEY_KP_MEMMULTIPLY = 213,
    KEY_KP_MEMDIVIDE = 214,
    KEY_KP_PLUSMINUS = 215,
    KEY_KP_CLEAR = 216,
    KEY_KP_CLEARENTRY = 217,
    KEY_KP_BINARY = 218,
    KEY_KP_OCTAL = 219,
    KEY_KP_DECIMAL = 220,
    KEY_KP_HEXADECIMAL = 221,

    KEY_LCTRL = 224,
    KEY_LSHIFT = 225,
    KEY_LALT = 226, /**< alt, option */
    KEY_LGUI = 227, /**< windows, command (apple), meta */
    KEY_RCTRL = 228,
    KEY_RSHIFT = 229,
    KEY_RALT = 230, /**< alt gr, option */
    KEY_RGUI = 231, /**< windows, command (apple), meta */

    KEY_MODE = 257,    /**< I'm not sure if this is really not covered
                                 *   by any of the above, but since there's a
                                 *   special KMOD_MODE for it I'm adding it here
                                 */

    KEY_AUDIONEXT = 258,
    KEY_AUDIOPREV = 259,
    KEY_AUDIOSTOP = 260,
    KEY_AUDIOPLAY = 261,
    KEY_AUDIOMUTE = 262,
    KEY_MEDIASELECT = 263,
    KEY_WWW = 264,
    KEY_MAIL = 265,
    KEY_CALCULATOR = 266,
    KEY_COMPUTER = 267,
    KEY_AC_SEARCH = 268,
    KEY_AC_HOME = 269,
    KEY_AC_BACK = 270,
    KEY_AC_FORWARD = 271,
    KEY_AC_STOP = 272,
    KEY_AC_REFRESH = 273,
    KEY_AC_BOOKMARKS = 274,

    KEY_BRIGHTNESSDOWN = 275,
    KEY_BRIGHTNESSUP = 276,
    KEY_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                           switch, video mode switch */
    KEY_KBDILLUMTOGGLE = 278,
    KEY_KBDILLUMDOWN = 279,
    KEY_KBDILLUMUP = 280,
    KEY_EJECT = 281,
    KEY_SLEEP = 282,

    KEY_APP1 = 283,
    KEY_APP2 = 284,

    KEY_AUDIOREWIND = 285,
    KEY_AUDIOFASTFORWARD = 286,

    KEY_COUNT = 512
}; // enum Keys

//
//
struct InputVector2 {
    f32                             x;
    f32                             y;
};

//
//
struct Gamepad {
    f32                             axis[ GAMEPAD_AXIS_COUNT ];
    u8                              buttons[ GAMEPAD_BUTTON_COUNT ];

    void*                           handle;
    cstring                         name;

    u32                             index;
    i32                             id;
};

struct InputBinding {

    enum Device                     { DEVICE_KEYBOARD, DEVICE_MOUSE, DEVICE_GAMEPAD };
    enum DevicePart                 { KEYBOARD, MOUSE, GAMEPAD_AXIS, GAMEPAD_BUTTONS };
    enum Type                       { BUTTON, AXIS_1D, AXIS_2D, VECTOR_1D, VECTOR_2D, BUTTON_ONE_MOD, BUTTON_TWO_MOD };

    enum Axis1DPart                 { POSITIVE, NEGATIVE };
    enum Vector2DPart               { UP, DOWN, LEFT, RIGHT };

    Type                            type;
    Device                          device;
    DevicePart                      device_part;
    u16                             button;         // Stores the buttons either from GAMEPAD_BUTTONS_*, KEY_*, MOUSE_BUTTON_*.

    f32                             value;
    bool                            is_composite;
    bool                            is_part_of_composite;
    bool                            repeat;

    f32                             min_deadzone = 0.10f;
    f32                             max_deadzone = 0.95f;
}; // struct InputBinding


struct InputAction {

    void                            init();

    void                            add_button( InputBinding::DevicePart device, uint16_t button, bool repeat = false );
    void                            add_axis_1d( InputBinding::DevicePart device, uint16_t axis, float min_deadzone, float max_deadzone );
    void                            add_axis_2d( InputBinding::DevicePart device, uint16_t x_axis, uint16_t y_axis, float min_deadzone, float max_deadzone );
    void                            add_vector_1d( InputBinding::DevicePart device_pos, uint16_t button_pos, InputBinding::DevicePart device_neg, uint16_t button_neg, bool repeat = true );
    void                            add_vector_2d( InputBinding::DevicePart device_up, uint16_t button_up, InputBinding::DevicePart device_down, uint16_t button_down, InputBinding::DevicePart device_left, uint16_t button_left, InputBinding::DevicePart device_right, uint16_t button_right, bool repeat = true );

    bool                            triggered() const;
    f32                             read_value_1d() const;
    InputVector2                    read_value_2d() const;

    InputVector2                    value;

    array_type( InputBinding )      bindings;
    const char*                     name;


}; // struct InputAction


struct InputActionMap {

    void                            init( u32 max_actions );
    void                            shutdown();

    InputAction&                    add_action();

    array_type( InputAction )       actions;

    const char*                     name;
    u32                             num_actions;
    bool                            active;

}; // struct InputActionMap


struct InputSystem {

    void                            init();
    void                            terminate();

    bool                            is_key_down( Keys key );
    bool                            is_key_just_pressed( Keys key, bool repeat = false );

    bool                            is_mouse_down( MouseButtons button );
    bool                            is_mouse_clicked( MouseButtons button );
    bool                            is_mouse_released( MouseButtons button );
    bool                            is_mouse_dragging( MouseButtons button );

    void                            update( f32 delta );

    void                            new_frame();            // Called before message handling
    void                            on_event( void* input_event );

    void                            add( InputActionMap& action_map );

    hydra::StringBuffer             string_buffer;
    array_type( InputActionMap )    input_action_maps;

    Gamepad                         gamepads[k_max_gamepads];

    u8                              keys[ KEY_COUNT ];
    u8                              previous_keys[ KEY_COUNT ];

    InputVector2                    mouse_position;
    InputVector2                    previous_mouse_position;
    InputVector2                    mouse_clicked_position[ MOUSE_BUTTONS_COUNT ];
    u8                              mouse_button[ MOUSE_BUTTONS_COUNT ];
    u8                              previous_mouse_button[ MOUSE_BUTTONS_COUNT ];
    f32                             mouse_drag_distance[ MOUSE_BUTTONS_COUNT ];

    bool                            has_focus;

}; // struct InputSystem

} // namespace hydra