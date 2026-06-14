#include "config.h"

#if defined(GWS_BOARD_WASM)

#include <stdint.h>
#include "types.h"
#include "serial.h"
#include "sim_internal.h"

// Host serial backend for the simulator. Instead of decoding the binary
// telemetry protocol, the bridge writes the game's state straight into the
// fields the firmware actually reads (gear, manual/explicit gear, mode, low
// beam). Freshness is tracked the same way serial_binary.cpp does it, so the
// firmware still drops to config mode when the game stops feeding.

extern SInput s_input;

static uint32_t g_lastFrameMs = 0;
static bool g_frameReceived = false;

void serialBegin() {}

void serialPoll() {} // game state is injected directly via wasmGameSet

bool serialGameFresh(uint32_t now) {
    return g_frameReceived && (now - g_lastFrameMs < GAME_DATA_TIMEOUT_MS);
}

void wasmGameSet(int currentGear, int explicitGear, int mode, int lowbeam, uint32_t now) {
    s_input.currentGear = (GEAR)currentGear;
    s_input.explicitGear = (GEAR_MANUAL)explicitGear;
    s_input.mode = (GEAR_MODE)mode;
    s_input.light_lowbeam = lowbeam != 0;
    g_lastFrameMs = now;
    g_frameReceived = true;
}

#endif // GWS_BOARD_WASM
