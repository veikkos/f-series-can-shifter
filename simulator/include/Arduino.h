#pragma once

// Minimal Arduino-core shim for the WebAssembly simulator build. Only the
// symbols the reused firmware sources actually reference are provided, so the
// real .ino, shifter and gws_lever compile unchanged under emcc.

#include <stdint.h>
#include <stddef.h>
#include <stdio.h> // real Arduino.h pulls this in; firmware uses vsnprintf etc.

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

// Minimal Arduino Serial stand-in. The real firmware talks to the PC over this
// (see `#define pc Serial`); in the simulator the host transport in
// serial_wasm.cpp buffers the binary telemetry bytes the bridge feeds in, so
// serial_binary.cpp decodes the genuine protocol unchanged.
class HostSerial {
public:
    void begin(unsigned long baud);
    int available();
    int read();
    void print(const char* s);
};

extern HostSerial Serial;
