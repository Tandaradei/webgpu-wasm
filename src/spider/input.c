#include "input.h"

void _spInputResetKeyStates(REF(_SPInputState state)) {
    // Clear down and up bits
    for(uint32_t i = 0; i < _SP_INPUT_KEY_COUNT; i++) {
        state->key_states[i] &= ~(SPKeyState_Down | SPKeyState_Up);
    }
}

void _spInputSetKeyState(REF(_SPInputState state), SPKey key, SPKeyState key_state) {
    SP_ASSERT(key < _SP_INPUT_KEY_COUNT);
    state->key_states[key] = key_state;
}

void _spInputUpdateKeyState(REF(_SPInputState state), SPKey key, SPKeyState key_state) {
    SP_ASSERT(key < _SP_INPUT_KEY_COUNT);
    state->key_states[key] |= key_state;
}

SPKeyState _spInputGetKeyState(CONST_REF(_SPInputState state), SPKey key) {
    SP_ASSERT(key < _SP_INPUT_KEY_COUNT);
    return state->key_states[key];
}

SPKey _spInputGetKeyForString(const char* key_string) {
    SP_ASSERT(key_string && strlen(key_string) > 0);
    static const SPKey digits[10] = {
        SPKey_0, SPKey_1, SPKey_2, SPKey_3, SPKey_4,
        SPKey_5, SPKey_6, SPKey_7, SPKey_8, SPKey_9
    };
    static const SPKey letters[26] = {
        SPKey_A, SPKey_B, SPKey_C, SPKey_D, SPKey_E,
        SPKey_F, SPKey_G, SPKey_H, SPKey_I, SPKey_J,
        SPKey_K, SPKey_L, SPKey_M, SPKey_N, SPKey_O,
        SPKey_P, SPKey_Q, SPKey_R, SPKey_S, SPKey_T,
        SPKey_U, SPKey_V, SPKey_W, SPKey_X, SPKey_Y,
        SPKey_Z
    };
    // // letters
    if(!strncmp(key_string, "Key", 3)) {
        char key_char = key_string[3];
        if(key_char >= 'A' && key_char <= 'Z') {
            return letters[key_char - 'A'];
        }
    }
    // digits
    if(!strncmp(key_string, "Digit", 5)) {
        char key_char = key_string[5];
        if(key_char >= '0' && key_char <= '9') {
            return digits[key_char - '0'];
        }
    }
    if(!strcmp(key_string, "Space")) return SPKey_Space;
    if(!strcmp(key_string, "Tab")) return SPKey_Tab;
    if(!strcmp(key_string, "PageUp")) return SPKey_PageUp;
    if(!strcmp(key_string, "PageDown")) return SPKey_PageDown;
    if(!strcmp(key_string, "Home")) return SPKey_Home;
    if(!strcmp(key_string, "End")) return SPKey_End;
    if(!strcmp(key_string, "Delete")) return SPKey_Delete;
    if(!strcmp(key_string, "Backspace")) return SPKey_Backspace;
    if(!strcmp(key_string, "Enter")) return SPKey_Enter;
    if(!strcmp(key_string, "Escape")) return SPKey_Escape;
    if(!strcmp(key_string, "ControlLeft")) return SPKey_ControlLeft;
    if(!strcmp(key_string, "AltLeft")) return SPKey_AltLeft;
    if(!strcmp(key_string, "ShiftLeft")) return SPKey_ShiftLeft;
    if(!strcmp(key_string, "ArrowLeft")) return SPKey_ArrowLeft;
    if(!strcmp(key_string, "ArrowRight")) return SPKey_ArrowRight;
    if(!strcmp(key_string, "ArrowUp")) return SPKey_ArrowUp;
    if(!strcmp(key_string, "ArrowDown")) return SPKey_ArrowDown;
    return SPKey_None;
}

const char* _spInputGetStringForKey(const SPKey key) {
    SP_ASSERT(key < _SP_INPUT_KEY_COUNT);
    static const char* strings[] = {
        "None",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "Space", "Tab",
        "PageUp", "PageDown", "Home", "End",
        "Delete", "Backspace", "Enter", "Escape",
        "ControlLeft", "AltLeft", "ShiftLeft",
        "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"
    };
    return strings[key];
}

void _spInputResetMouseButtonStates(REF(_SPInputState state)) {
    for(uint32_t i = 0; i < _SP_INPUT_MOUSE_BUTTON_COUNT; i++) {
        state->mouse_button_states[i] &= ~(SPMouseButtonState_Down | SPMouseButtonState_Up);
    }
}

void _spInputSetMouseButtonState(REF(_SPInputState state), SPMouseButton button, SPMouseButtonState button_state) {
    SP_ASSERT(button < _SP_INPUT_MOUSE_BUTTON_COUNT);
    state->mouse_button_states[button] = button_state;
}

void _spInputUpdateMouseButtonState(REF(_SPInputState state), SPMouseButton button, SPMouseButtonState button_state) {
    SP_ASSERT(button < _SP_INPUT_MOUSE_BUTTON_COUNT);
    state->mouse_button_states[button] |= button_state;
}

SPMouseButtonState _spInputGetMouseButtonState(CONST_REF(_SPInputState state), SPMouseButton button) {
    SP_ASSERT(button < _SP_INPUT_MOUSE_BUTTON_COUNT);
    return state->mouse_button_states[button];
}

SPMouseButton _spInputGetMouseButtonForId(uint32_t id) {
    // Normally id 0 means left button, but we have None on id 0
    id = id + 1;
    static const SPMouseButton buttons[] = {
        SPMouseButton_None,
        SPMouseButton_Left,
        SPMouseButton_Middle,
        SPMouseButton_Right
    };
    if(id < _SP_INPUT_MOUSE_BUTTON_COUNT) {
        return buttons[id];
    }
    return SPMouseButton_None;

}

const char* _spInputGetStringForMouseButton(const SPMouseButton button) {
    SP_ASSERT(button < _SP_INPUT_MOUSE_BUTTON_COUNT);
    static const char* strings[] = {
        "None",
        "Left", "Middle", "Right"
    };
    return strings[button];
}