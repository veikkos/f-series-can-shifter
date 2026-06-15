#include <emscripten.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "config.h"
#include "types.h"
#include "serial.h"
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

// --- GWS self-return ------------------------------------------------------
// The lever belongs in the M/S side gate only while the indicator is the
// M/S-capable Drive (DISPLAY_DRIVE_MS). Any other indication — P, R, N, or a
// plain transitional D — means it should be at centre, so the real selector
// drives its own lever back. Model that motor travel here too.
static const uint32_t LEVER_RETURN_MS = 400; // modelled motor travel time
static bool g_returning = false;
static uint32_t g_returnStartMs = 0;

static void gwsSelfReturn() {
    uint8_t code = wasmCanDisplayByte() & ~DISPLAY_FLASH;
    bool wantsCentre = g_restPos == LEVER_CENTRE_SIDE && code != DISPLAY_DRIVE_MS;
    if (!wantsCentre) {
        g_returning = false;
        return;
    }
    if (!g_returning) {
        g_returning = true;
        g_returnStartMs = g_now;
    } else if (g_now - g_returnStartMs >= LEVER_RETURN_MS) {
        g_restPos = LEVER_CENTRE_MIDDLE; // lever clicks into Drive
        g_heldPaddle = 0;
        g_returning = false;
    }
}

// --- Game state selection -------------------------------------------------
static int g_gearSel = 0;     // 0 Park, 1 Reverse, 2 Neutral, 3 Drive(auto), 4 Drive(manual)
static bool g_sport = false;
static bool g_lights = true;  // low beam -> backlight on
static int g_manualGear = 1;  // 1..NUMBER_OF_GEARS
static bool g_connected = true;

// --- Binary telemetry frame ----------------------------------------------
// The simulator speaks the genuine 35-byte serial protocol so serial_binary.cpp
// decodes it exactly as it would on real hardware. Only the fields that drive
// the shifter (gear, gear extension, low beam) are modelled; the rest are left
// at sane idle values.
#define FRAME_LENGTH 35

static void putU16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

// Build the selected game state into a real binary frame and feed it to the
// firmware's serial port. Gear and the gear-extension byte are chosen so the
// parser in serial_binary.cpp reconstructs the intended currentGear / explicit
// manual gear / mode (see its gear logic).
static void feedGame() {
    uint8_t gear = NEUTRAL;   // byte 11: 0 = R, 1 = N, 2+ = forward gears
    char gearExt = 'N';       // byte 26: P/A/M/S/N
    switch (g_gearSel) {
        case 0: gearExt = 'P'; break;                         // Park
        case 1: gear = REVERSE; gearExt = 'N'; break;         // Reverse
        case 2: gear = NEUTRAL; gearExt = 'N'; break;         // Neutral
        case 3: gear = 2; gearExt = 'A'; break;               // Drive (automatic)
        default: {                                            // Drive (manual)
            int mg = g_manualGear;
            if (mg < 1) mg = 1;
            if (mg > NUMBER_OF_GEARS) mg = NUMBER_OF_GEARS;
            gear = (uint8_t)(mg + 1); // parser derives explicit gear = gear - 1
            gearExt = g_sport ? 'S' : 'M';
            break;
        }
    }

    uint32_t showlights = 0;
    if (g_lights) showlights |= (1UL << 12); // low beam -> backlight on

    uint8_t f[FRAME_LENGTH];
    memset(f, 0, sizeof(f));

    f[0] = 'S';
    // Timestamp (byte 1..6) — arbitrary but valid
    f[1] = 25; f[2] = 1; f[3] = 1; f[4] = 12; f[5] = 0; f[6] = 0;
    putU16(&f[7], 0);            // rpm
    putU16(&f[9], 0);            // speed
    f[11] = gear;
    f[12] = 0;                   // water temp
    f[13] = 0;                   // oil temp
    putU16(&f[14], 1000);        // fuel (100.0 %)
    putU16(&f[16], (uint16_t)(showlights & 0xFFFF));
    f[18] = (uint8_t)((showlights >> 16) & 0xFF);
    f[19] = (uint8_t)((showlights >> 24) & 0xFF);
    f[20] = 0;                   // showlights ext
    putU16(&f[21], 0);           // fuel injection
    putU16(&f[23], 0);           // custom light
    f[25] = 0;                   // custom light on
    f[26] = (uint8_t)gearExt;
    putU16(&f[27], 0);           // cruise speed
    f[29] = 0;                   // cruise status
    f[30] = IG_ON;               // ignition
    f[31] = 1;                   // engine running
    putU16(&f[32], 200);         // ambient temp (20.0 C)

    uint8_t checksum = 0;
    for (int i = 1; i < FRAME_LENGTH - 1; i++) {
        checksum = (uint8_t)((checksum + f[i]) & 0xFF);
    }
    f[FRAME_LENGTH - 1] = checksum;

    wasmSerialFeed(f, FRAME_LENGTH);
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void sim_init() {
    g_now = 0;
    g_restPos = LEVER_CENTRE_MIDDLE;
    g_heldPaddle = 0;
    g_parkOneShot = false;
    g_transHead = g_transTail = 0;
    g_counter = 0;
    g_returning = false;
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

    // Decode the freshly fed telemetry before loop() so shifterTick() acts on
    // the current game state this tick. The firmware's loop() runs serialPoll()
    // *after* shifterTick(), so a frame is normally only acted on the following
    // iteration. On real hardware loop() spins thousands of times per telemetry
    // frame, so that lag is invisible; here it would be a full visible tick,
    // and with the responding game closing the loop it sets up a Park<->Neutral
    // oscillation at startup. Parsing first models the prior fast iteration that
    // would already have decoded the frame. loop()'s own serialPoll() then just
    // finds an empty buffer.
    serialPoll();

    loop();

    // After the firmware has updated the indicator, let the modelled selector
    // pull its lever back out of the side gate if a plain D is being shown
    gwsSelfReturn();
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
EMSCRIPTEN_KEEPALIVE int sim_gear() { return (int)shifterGear(); }
EMSCRIPTEN_KEEPALIVE int sim_connected() { return shifterConnected() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sim_mismatch() { return shifterModeMismatch() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sim_lever_gate() { return g_restPos == LEVER_CENTRE_SIDE ? 1 : 0; }

} // extern "C"
