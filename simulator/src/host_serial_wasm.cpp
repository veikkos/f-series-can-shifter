#include "config.h"

#if defined(GWS_BOARD_WASM)

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <Arduino.h>
#include "sim_internal.h"

// Host serial transport for the simulator — the virtual UART, not a parser.
// This is just the "wire": a byte buffer that the bridge fills with real binary
// telemetry frames and that the firmware drains through Serial.available()/
// read(). The actual protocol
// decoding is done by the unmodified firmware in serial_binary.cpp, so the
// simulator exercises the genuine parser (checksum, gear + gear-extension
// byte, freshness timeout, ...).

HostSerial Serial;

#define RX_BUF_SIZE 256
static uint8_t g_rx[RX_BUF_SIZE];
static size_t g_head = 0;
static size_t g_tail = 0;

void HostSerial::begin(unsigned long /*baud*/) {
    g_head = g_tail = 0;
}

int HostSerial::available() {
    return (int)((g_tail - g_head + RX_BUF_SIZE) % RX_BUF_SIZE);
}

int HostSerial::read() {
    if (g_head == g_tail) return -1;
    uint8_t c = g_rx[g_head];
    g_head = (g_head + 1) % RX_BUF_SIZE;
    return c;
}

void HostSerial::print(const char* s) {
    // Surface firmware diagnostics (e.g. checksum mismatch) on the console
    fputs(s, stdout);
}

// Bridge -> firmware: enqueue raw telemetry bytes onto the virtual UART
void wasmSerialFeed(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        size_t next = (g_tail + 1) % RX_BUF_SIZE;
        if (next == g_head) break; // buffer full; drop the rest
        g_rx[g_tail] = data[i];
        g_tail = next;
    }
}

#endif // GWS_BOARD_WASM
