#include "config.h"

#if defined(USE_CAN_WASM)

#include <stdint.h>
#include <stddef.h>
#include "can_adapter.h"
#include "types.h"
#include "sim_internal.h"

// Host CAN backend for the simulator. Outgoing frames from the firmware
// (the gear-indicator display and the backlight) are captured so the GUI can
// render the lever, and a queue of simulated lever-position frames is drained
// into the firmware's frame handlers, exactly as a real bus would.

static uint8_t g_displayByte = DISPLAY_BLANK; // last GWS_ID_DISPLAY frame[2]
static uint8_t g_backlightByte = BACKLIGHT_OFF; // last GWS_ID_BACKLIGHT frame[0]

#define WASM_CAN_QUEUE_LEN 64
static uint8_t g_rx[WASM_CAN_QUEUE_LEN][8];
static size_t g_rxHead = 0;
static size_t g_rxTail = 0;

void wasmCanQueuePosition(const uint8_t* data) {
    size_t next = (g_rxTail + 1) % WASM_CAN_QUEUE_LEN;
    if (next == g_rxHead) {
        return; // queue full, drop the frame
    }
    for (int i = 0; i < 8; ++i) {
        g_rx[g_rxTail][i] = data[i];
    }
    g_rxTail = next;
}

uint8_t wasmCanDisplayByte() {
    return g_displayByte;
}

uint8_t wasmCanBacklightByte() {
    return g_backlightByte;
}

void canBegin() {}

void canSend(uint32_t id, const uint8_t* data, uint8_t len) {
    if (id == GWS_ID_DISPLAY && len >= 3) {
        g_displayByte = data[2];
    } else if (id == GWS_ID_BACKLIGHT && len >= 1) {
        g_backlightByte = data[0];
    }
}

void canPoll(const CanHandlerEntry* handlers, size_t count) {
    while (g_rxHead != g_rxTail) {
        const uint8_t* frame = g_rx[g_rxHead];
        for (size_t i = 0; i < count; ++i) {
            // Only position frames are produced, but match by id like real CAN
            if (handlers[i].id == GWS_ID_POSITION) {
                handlers[i].handler(frame);
                break;
            }
        }
        g_rxHead = (g_rxHead + 1) % WASM_CAN_QUEUE_LEN;
    }
}

#endif // USE_CAN_WASM
