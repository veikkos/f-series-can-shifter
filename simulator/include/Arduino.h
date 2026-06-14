#pragma once

// Minimal Arduino-core shim for the WebAssembly simulator build. Only the
// symbols the reused firmware sources actually reference are provided, so the
// real .ino, shifter and gws_lever compile unchanged under emcc.

#include <stdint.h>
#include <stddef.h>

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef uint8_t byte;

// Virtual clock advanced by the simulator each tick (see bridge.cpp).
unsigned long millis();
void delay(unsigned long ms);
