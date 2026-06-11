#include "config.h"

#if defined(GWS_BOARD_TEENSY)

#include <Arduino.h>
#include "gamepad.h"

#if !defined(JOYSTICK_INTERFACE)
    #error "Teensy USB Type must include a joystick. Set Tools > USB Type to 'Serial + Keyboard + Mouse + Joystick'."
#endif

// The built-in Joystick object numbers buttons from 1

void gamepadBegin() {
    Joystick.useManualSend(false);
}

void gamepadPress(uint8_t button) {
    Joystick.button(button + 1, 1);
}

void gamepadRelease(uint8_t button) {
    Joystick.button(button + 1, 0);
}

#endif
