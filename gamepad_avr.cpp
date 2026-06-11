#include "config.h"

#if defined(GWS_BOARD_AVR)

#include <Joystick.h>
#include "gamepad.h"

static Joystick_ joystick(
    0x03, JOYSTICK_TYPE_GAMEPAD, 32, 0,
    false, false, false, false, false, false,
    false, false, false, false, false
);

void gamepadBegin() {
    joystick.begin();
}

void gamepadPress(uint8_t button) {
    joystick.pressButton(button);
}

void gamepadRelease(uint8_t button) {
    joystick.releaseButton(button);
}

#endif
