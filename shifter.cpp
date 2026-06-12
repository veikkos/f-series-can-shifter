#include "shifter.h"
#include "gamepad.h"

// How long the game gets to engage a lever request before the game's own
// state is adopted back
static const uint32_t SYNC_GIVE_UP_MS = 3000;

// A lever request: the selection is already held on the gamepad buttons, so
// there is nothing to retry — the game just gets a grace period to engage it
// before a mismatch means the game won. A mismatch with no request pending
// means the game changed the state on its own (car reset, in-game paddles)
// and is adopted at once
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

static void tapButton(GamepadButton button) {
    gamepadPress(button);
    gamepadRelease(button);
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

// Hold the mode button while manual/sport is selected, released means automatic
static void pressModeButton(bool manual) {
    static bool held = false;
    if (manual == held) {
        return;
    }
    if (manual) {
        gamepadPress(BTN_MODE_MANUAL);
    } else {
        gamepadRelease(BTN_MODE_MANUAL);
    }
    held = manual;
}

static void pressGearButton(GwsGear gear) {
    static GwsGear held = GWS_NEUTRAL;
    if (gear == held) {
        return;
    }
    GamepadButton button;
    if (gearButton(held, &button)) {
        gamepadRelease(button);
    }
    if (gearButton(gear, &button)) {
        gamepadPress(button);
    }
    held = gear;
}

void shifterApplyLever(const LeverEvents& events, uint32_t now) {
    // The lever always drives the buttons directly, connected or not; the
    // requests only decide when the shifter snaps back to the game's state
    if (events.enteredManualGate || events.leftManualGate) {
        s.manual = events.enteredManualGate;
        pressModeButton(s.manual);
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

    // Sequential paddles in the manual gate
    if (events.paddleUp) {
        tapButton(BTN_PADDLE_UP);
    }
    if (events.paddleDown) {
        tapButton(BTN_PADDLE_DOWN);
    }

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
        pressModeButton(gameManual);
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
