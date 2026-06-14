#include <emscripten.h>
#include <stdint.h>
#include <stddef.h>
#include "config.h"
#include "types.h"
#include "shifter.h"
#include "sim_internal.h"

// JS <-> firmware bridge. Models the physical GWS lever (so user actions turn
// into real GWS_ID_POSITION frames the firmware decodes), injects mock game
// state, owns the virtual clock, and exposes the captured outputs to the GUI.
// The gear logic itself is the unmodified firmware: e90-can-shifter loop(),
// shifter.cpp and gws_lever.cpp.

extern void setup();
extern void loop();

// --- Virtual clock --------------------------------------------------------
static unsigned long g_now = 0;
unsigned long millis() { return g_now; }
void delay(unsigned long) {}

// --- Physical lever model -------------------------------------------------
static uint8_t g_restPos = LEVER_CENTRE_MIDDLE; // gate rest: middle (auto) or side (M/S)
static uint8_t g_heldPaddle = 0;                // LEVER_SIDE_UP/DOWN while a paddle is held
static bool g_parkOneShot = false;
static uint8_t g_counter = 0;

#define TRANSIENT_LEN 32
static uint8_t g_trans[TRANSIENT_LEN];
static int g_transHead = 0;
static int g_transTail = 0;

static void pushTransient(uint8_t pos) {
    int next = (g_transTail + 1) % TRANSIENT_LEN;
    if (next == g_transHead) return; // full
    g_trans[g_transTail] = pos;
    g_transTail = next;
}

// A tip is a momentary deflection into a detent, then the spring back to rest
static void tip(uint8_t deflected) {
    pushTransient(deflected);
    pushTransient(g_restPos);
}

// One lever position frame per tick, like the real GWS reporting continuously
static void emitLeverFrame() {
    uint8_t pos;
    if (g_transHead != g_transTail) {
        pos = g_trans[g_transHead];
        g_transHead = (g_transHead + 1) % TRANSIENT_LEN;
    } else if (g_heldPaddle) {
        pos = g_heldPaddle;
    } else {
        pos = g_restPos;
    }

    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    g_counter = (uint8_t)(g_counter + 1); // always differs from the previous frame
    data[1] = g_counter;
    data[2] = pos;
    data[3] = g_parkOneShot ? GWS_PARK_BUTTON_PRESSED : 0x00;
    g_parkOneShot = false;
    wasmCanQueuePosition(data);
}

// --- Game state selection -------------------------------------------------
static int g_gearSel = 0;     // 0 Park, 1 Reverse, 2 Neutral, 3 Drive(auto), 4 Drive(manual)
static bool g_sport = false;
static bool g_lights = true;  // low beam -> backlight on
static int g_manualGear = 1;  // 1..NUMBER_OF_GEARS
static bool g_connected = true;

// Push the selected game state straight into s_input via the serial backend
static void feedGame() {
    int currentGear = DRIVE;
    int explicitGear = NONE;
    int mode = NORMAL;
    switch (g_gearSel) {
        case 0: currentGear = PARK; break;
        case 1: currentGear = REVERSE; break;
        case 2: currentGear = NEUTRAL; break;
        case 3: currentGear = DRIVE; break;
        default: {
            currentGear = DRIVE;
            int mg = g_manualGear;
            if (mg < 1) mg = 1;
            if (mg > NUMBER_OF_GEARS) mg = NUMBER_OF_GEARS;
            explicitGear = mg; // GEAR_MANUAL: M1 == 1 .. M8 == 8
            mode = g_sport ? SPORT : NORMAL;
            break;
        }
    }
    wasmGameSet(currentGear, explicitGear, mode, g_lights ? 1 : 0, g_now);
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void sim_init() {
    g_now = 0;
    g_restPos = LEVER_CENTRE_MIDDLE;
    g_heldPaddle = 0;
    g_parkOneShot = false;
    g_transHead = g_transTail = 0;
    g_counter = 0;
    setup();
}

EMSCRIPTEN_KEEPALIVE void sim_tick(double now_ms) {
    g_now = (unsigned long)now_ms;

    emitLeverFrame();

    // Feed game state while connected; stopping lets the firmware's freshness
    // window lapse into config mode after 5 s
    if (g_connected) {
        feedGame();
    }

    loop();
}

EMSCRIPTEN_KEEPALIVE void sim_tip_drive(int detents) {
    if (g_restPos != LEVER_CENTRE_MIDDLE) return; // tips live in the auto gate
    tip(detents >= 2 ? LEVER_DOWN_TWO : LEVER_DOWN);
}

EMSCRIPTEN_KEEPALIVE void sim_tip_reverse(int detents) {
    if (g_restPos != LEVER_CENTRE_MIDDLE) return;
    tip(detents >= 2 ? LEVER_UP_TWO : LEVER_UP);
}

EMSCRIPTEN_KEEPALIVE void sim_enter_ms() {
    g_restPos = LEVER_CENTRE_SIDE;
    g_heldPaddle = 0;
}

EMSCRIPTEN_KEEPALIVE void sim_leave_ms() {
    g_restPos = LEVER_CENTRE_MIDDLE;
    g_heldPaddle = 0;
}

EMSCRIPTEN_KEEPALIVE void sim_paddle_up(int held) {
    if (g_restPos != LEVER_CENTRE_SIDE) return;
    g_heldPaddle = held ? LEVER_SIDE_UP : 0;
}

EMSCRIPTEN_KEEPALIVE void sim_paddle_down(int held) {
    if (g_restPos != LEVER_CENTRE_SIDE) return;
    g_heldPaddle = held ? LEVER_SIDE_DOWN : 0;
}

EMSCRIPTEN_KEEPALIVE void sim_park() {
    g_parkOneShot = true;
}

EMSCRIPTEN_KEEPALIVE void sim_set_gear(int sel) { g_gearSel = sel; }
EMSCRIPTEN_KEEPALIVE void sim_set_sport(int on) { g_sport = on != 0; }
EMSCRIPTEN_KEEPALIVE void sim_set_lights(int on) { g_lights = on != 0; }
EMSCRIPTEN_KEEPALIVE void sim_set_manual_gear(int n) { g_manualGear = n; }
EMSCRIPTEN_KEEPALIVE void sim_set_connected(int on) { g_connected = on != 0; }

EMSCRIPTEN_KEEPALIVE int sim_display_byte() { return wasmCanDisplayByte(); }
EMSCRIPTEN_KEEPALIVE int sim_backlight() { return wasmCanBacklightByte(); }
EMSCRIPTEN_KEEPALIVE int sim_buttons() { return wasmGamepadMask(); }
EMSCRIPTEN_KEEPALIVE int sim_connected() { return shifterConnected() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sim_mismatch() { return shifterModeMismatch() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sim_lever_gate() { return g_restPos == LEVER_CENTRE_SIDE ? 1 : 0; }

} // extern "C"
