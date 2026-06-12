#include "shifter.h"
#include "gamepad.h"

// How long the game gets to engage a lever request before the game's own
// state is adopted back
static const uint32_t SYNC_GIVE_UP_MS = 3000;

struct SyncRequest {
    bool pending = false; // a lever-initiated change is waiting for the game
    uint32_t openedMs = 0;
};

static void openRequest(SyncRequest& r, uint32_t now) {
    r.pending = true;
    r.openedMs = now;
}

// Whether a mismatching game state should be adopted this tick. A match
// closes the pending request; on adoption the caller closes it
static bool adoptTick(SyncRequest& r, bool matches, uint32_t now) {
    if (matches) {
        r.pending = false;
        return false;
    }
    return !r.pending || now - r.openedMs >= SYNC_GIVE_UP_MS;
}

// Lever-side state the game is given to engage
static struct {
    GwsGear gear = GWS_NEUTRAL;
    bool manual = false;
    bool connected = false; // game telemetry was fresh on the last tick
    SyncRequest gearRequest;
    SyncRequest modeRequest;
} s;

// Step the gear along the R-N-D gate: positive steps toward drive, negative
// toward reverse. Park sits at the reverse end of the gate and is engaged only
// via the park button, so the clamp keeps tips from ever landing back in park
static GwsGear stepGear(GwsGear gear, int steps) {
    static const GwsGear ladder[] = { GWS_REVERSE, GWS_NEUTRAL, GWS_DRIVE };
    GwsGear current = (gear == GWS_PARK) ? GWS_REVERSE : gear;
    int idx = 0; // REVERSE
    for (int i = 0; i < 3; i++) {
        if (ladder[i] == current) idx = i;
    }
    idx += steps;
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    return ladder[idx];
}

// Press or release a button to track a held state, touching it only on change
static void holdButton(GamepadButton button, bool hold) {
    static uint8_t held = 0; // bitmask of buttons currently held
    uint8_t bit = 1 << button;
    if (hold == ((held & bit) != 0)) {
        return;
    }
    if (hold) {
        gamepadPress(button);
        held |= bit;
    } else {
        gamepadRelease(button);
        held &= ~bit;
    }
}

// Gamepad button that holds the given gear engaged, false for neutral which
// holds none
static bool gearButton(GwsGear gear, GamepadButton* button) {
    switch (gear) {
        case GWS_PARK:    *button = BTN_GEAR_PARK;    return true;
        case GWS_REVERSE: *button = BTN_GEAR_REVERSE; return true;
        case GWS_DRIVE:   *button = BTN_GEAR_DRIVE;   return true;
        default:          return false; // neutral
    }
}

// Hold the selected gear's button, no button held means neutral. Releases
// come first so two gears are never held at once
static void pressGearButton(GwsGear gear) {
    if (gear != GWS_REVERSE) holdButton(BTN_GEAR_REVERSE, false);
    if (gear != GWS_DRIVE)   holdButton(BTN_GEAR_DRIVE, false);
    if (gear != GWS_PARK)    holdButton(BTN_GEAR_PARK, false);

    GamepadButton button;
    if (gearButton(gear, &button)) {
        holdButton(button, true);
    }
}

void shifterApplyLever(const LeverEvents& events, uint32_t now) {
    // The lever always drives the buttons directly, connected or not; the
    // requests only decide when the shifter snaps back to the game's state
    if (events.enteredManualGate || events.leftManualGate) {
        s.manual = events.enteredManualGate;
        holdButton(BTN_MODE_MANUAL, s.manual);
        openRequest(s.modeRequest, now);
    }

    // Tipping deeper into a gate steps the gear: one notch = one step, two = two
    if (events.stepsTowardDrive != 0) {
        GwsGear stepped = stepGear(shifterTargetGear(), events.stepsTowardDrive);
        if (stepped != s.gear) {
            s.gear = stepped;
            pressGearButton(stepped);
            openRequest(s.gearRequest, now);
        }
    }

    // Sequential paddles follow the lever in the manual gate: held while the
    // lever is held in a detent, released when it springs back to centre
    holdButton(BTN_PADDLE_UP, events.paddleUpHeld);
    holdButton(BTN_PADDLE_DOWN, events.paddleDownHeld);

    if (events.parkButtonPressed && s.gear != GWS_PARK) {
        s.gear = GWS_PARK;
        pressGearButton(GWS_PARK);
        openRequest(s.gearRequest, now);
    }
}

void shifterTick(uint32_t now, GwsGear gameGear, bool gameManual, bool gameFresh) {
    // With no game talking there is nothing to adopt; the lever is a plain
    // button box
    if (!gameFresh) {
        s.connected = false;
        return;
    }

    // On reconnect a request made while disconnected gets a fresh grace
    // period against the live game
    if (!s.connected) {
        s.connected = true;
        s.gearRequest.openedMs = now;
        s.modeRequest.openedMs = now;
    }

    if (adoptTick(s.modeRequest, s.manual == gameManual, now)) {
        if (s.modeRequest.pending) {
            // The lever's mode request went unanswered, the gear is uncertain
            s.gear = GWS_TRANSITIONAL;
        }
        s.manual = gameManual;
        holdButton(BTN_MODE_MANUAL, gameManual);
        s.modeRequest.pending = false;
    }

    if (adoptTick(s.gearRequest, shifterTargetGear() == gameGear, now)) {
        s.gear = gameGear;
        pressGearButton(gameGear);
        s.gearRequest.pending = false;
    }
}

bool shifterConnected() {
    return s.connected;
}

GwsGear shifterGear() {
    return s.gear;
}

GwsGear shifterTargetGear() {
    return s.gear == GWS_TRANSITIONAL ? GWS_DRIVE : s.gear;
}
