#include "config.h"

#if defined(GWS_BOARD_WASM)

#include <stdint.h>
#include "gamepad.h"
#include "sim_internal.h"

// Host gamepad backend: record the held buttons in a bitmask the GUI reads to
// light its joystick LEDs.

static uint8_t g_mask = 0;

uint8_t wasmGamepadMask() {
    return g_mask;
}

void gamepadBegin() {
    g_mask = 0;
}

void gamepadPress(uint8_t button) {
    g_mask |= (uint8_t)(1u << button);
}

void gamepadRelease(uint8_t button) {
    g_mask &= (uint8_t)~(1u << button);
}

#endif // GWS_BOARD_WASM
