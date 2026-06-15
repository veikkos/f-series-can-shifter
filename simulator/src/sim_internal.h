#pragma once

// Internal bridge <-> wasm-backend interface. These are not part of the
// firmware; they let bridge.cpp inject simulated lever/game input and read the
// captured outputs.

#include <stdint.h>
#include <stddef.h>

// can_adapter_wasm.cpp
void wasmCanQueuePosition(const uint8_t* data); // queue a GWS lever frame (RX)
uint8_t wasmCanDisplayByte();                   // last GWS_ID_DISPLAY frame[2]
uint8_t wasmCanBacklightByte();                 // last GWS_ID_BACKLIGHT frame[0]

// host_serial_wasm.cpp — host serial transport. The bridge enqueues real binary
// telemetry frames here; the firmware (serial_binary.cpp) reads and decodes
// them through Serial.available()/read().
void wasmSerialFeed(const uint8_t* data, size_t len);

// gamepad_wasm.cpp
uint8_t wasmGamepadMask(); // 1 bit per GamepadButton, set while held
