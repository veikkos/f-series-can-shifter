#pragma once

#include <stdint.h>

// USB-HID gamepad output. Button indices are 0-based.
void gamepadBegin();
void gamepadPress(uint8_t button);
void gamepadRelease(uint8_t button);
