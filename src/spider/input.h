#ifndef SPIDER_INPUT_H_
#define SPIDER_INPUT_H_

#include <stdbool.h>
#include "impl.h"

typedef enum SPKeyState {
    SPKeyState_None = 0, // Key is not pressed
    SPKeyState_Pressed = 1, // Key is pressed
    SPKeyState_Down = 2, // Key was pressed this frame
    SPKeyState_Up = 4, // Key was released this frame
} SPKeyState;

typedef enum SPMouseButtonState {
    SPMouseButtonState_None = 0, // MouseButton is not pressed
    SPMouseButtonState_Pressed = 1, // MouseButton is pressed
    SPMouseButtonState_Down = 2, // MouseButton was pressed this frame
    SPMouseButtonState_Up = 4, // MouseButton was released this frame
} SPMouseButtonState;

typedef enum SPKey {
    // 1 - 1 total
    SPKey_None,
    // 26 - 27 total
    SPKey_A,
    SPKey_B,
    SPKey_C,
    SPKey_D,
    SPKey_E,
    SPKey_F,
    SPKey_G,
    SPKey_H,
    SPKey_I,
    SPKey_J,
    SPKey_K,
    SPKey_L,
    SPKey_M,
    SPKey_N,
    SPKey_O,
    SPKey_P,
    SPKey_Q,
    SPKey_R,
    SPKey_S,
    SPKey_T,
    SPKey_U,
    SPKey_V,
    SPKey_W,
    SPKey_X,
    SPKey_Y,
    SPKey_Z,
    // 10 - 37 total
    SPKey_0,
    SPKey_1,
    SPKey_2,
    SPKey_3,
    SPKey_4,
    SPKey_5,
    SPKey_6,
    SPKey_7,
    SPKey_8,
    SPKey_9,
    // 2 - 39 total
    SPKey_Space,
    SPKey_Tab,
    // 4 - 43 total
    SPKey_PageUp,
    SPKey_PageDown,
    SPKey_Home,
    SPKey_End,
    // 4 - 50 total
    SPKey_Delete,
    SPKey_Backspace,
    SPKey_Enter,
    SPKey_Escape,
    // 3 - 50 total
    SPKey_ControlLeft,
    SPKey_AltLeft,
    SPKey_ShiftLeft,
    // 4 - 54 total
    SPKey_ArrowLeft,
    SPKey_ArrowRight,
    SPKey_ArrowUp,
    SPKey_ArrowDown,
    // Not used for key count calculation
    SPKey_Force32 = 0xFFFFFFFF
} SPKey;

#define _SP_INPUT_KEY_COUNT 54

typedef enum SPMouseButton {
    SPMouseButton_None,
    SPMouseButton_Left,
    SPMouseButton_Middle,
    SPMouseButton_Right,

    SPMouseButton_Force32 = 0xFFFFFFFF
} SPMouseButton;

#define _SP_INPUT_MOUSE_BUTTON_COUNT 4

typedef enum SPInputModifiers {
    SPInputModifiers_None,
    SPInputModifiers_Control = 1,
    SPInputModifiers_Alt = 2,
    SPInputModifiers_Shift = 4,
    SPInputModifiers_Meta = 8
} SPInputModifiers;

typedef struct _SPInputState {
    SPKeyState key_states[_SP_INPUT_KEY_COUNT];
    SPMouseButtonState mouse_button_states[_SP_INPUT_MOUSE_BUTTON_COUNT];
    SPInputModifiers modifiers;
    struct {
        uint32_t x;
        uint32_t y;
    } mouse_position;
    char utf8_code[32];
} _SPInputState;

void _spInputResetKeyStates(REF(_SPInputState state));
void _spInputSetKeyState(REF(_SPInputState state), SPKey key, SPKeyState key_state);
void _spInputUpdateKeyState(REF(_SPInputState state), SPKey key, SPKeyState key_state);
SPKeyState _spInputGetKeyState(CONST_REF(_SPInputState state), SPKey key);
SPKey _spInputGetKeyForString(const char* key_string);
const char* _spInputGetStringForKey(const SPKey key);

void _spInputResetMouseButtonStates(REF(_SPInputState state));
void _spInputSetMouseButtonState(REF(_SPInputState state), SPMouseButton button, SPMouseButtonState button_state);
void _spInputUpdateMouseButtonState(REF(_SPInputState state), SPMouseButton button, SPMouseButtonState button_state);
SPMouseButtonState _spInputGetMouseButtonState(CONST_REF(_SPInputState state), SPMouseButton button);
SPMouseButton _spInputGetMouseButtonForId(uint32_t id);
const char* _spInputGetStringForMouseButton(const SPMouseButton key);

void _spInputSetMousePosition(REF(_SPInputState state), uint32_t x, uint32_t y);
uint32_t _spInputGetMousePositionX(CONST_REF(_SPInputState state));
uint32_t _spInputGetMousePositionY(CONST_REF(_SPInputState state));


#endif // SPIDER_INPUT_H_