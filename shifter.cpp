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
    bool physicalManual = false; // lever is physically in the M/S side gate
    bool modeMismatch = false;   // physical gate disagrees with the game's mode
    SyncRequest gearRequest;
    SyncRequest modeRequest;
} s;

// From Park the lever sits at the centre with Neutral on either side, so the
// first detent in either direction lands on Neutral and only the second reaches
// Reverse or Drive (R-N-P-N-D). That first detent is spent as a Neutral buffer
static GwsGear stepGear(GwsGear gear, int steps) {
    static const GwsGear ladder[] = { GWS_REVERSE, GWS_NEUTRAL, GWS_DRIVE };
    int idx = 1; // NEUTRAL: also Park's centre rest
    if (gear == GWS_PARK) {
        if (steps > 0) steps -= 1;      // first down detent buffers to Neutral
        else if (steps < 0) steps += 1; // first up detent buffers to Neutral
    } else {
        for (int i = 0; i < 3; i++) {
            if (ladder[i] == gear) idx = i;
        }
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

// Drive the gamepad to reflect the current selection
static void applyButtons() {
    GamepadButton target;
    bool hasTarget = s.manual ? (target = BTN_MODE_MANUAL, true)
                              : gearButton(s.gear, &target);

    static const GamepadButton all[] = {
        BTN_GEAR_REVERSE, BTN_GEAR_DRIVE, BTN_GEAR_PARK, BTN_MODE_MANUAL };
    for (GamepadButton b : all) {
        if (!hasTarget || b != target) holdButton(b, false);
    }
    if (hasTarget) holdButton(target, true);
}

void shifterApplyLever(const LeverEvents& events, uint32_t now) {
    // The lever always drives the buttons directly, connected or not; the
    // requests only decide when the shifter snaps back to the game's state
    if (events.enteredManualGate || events.leftManualGate) {
        s.manual = events.enteredManualGate;
        s.physicalManual = events.enteredManualGate;
        applyButtons();
        openRequest(s.modeRequest, now);
    }

    // Tipping deeper into a gate steps the gear: one notch = one step, two = two
    if (events.stepsTowardDrive != 0) {
        GwsGear stepped = stepGear(shifterTargetGear(), events.stepsTowardDrive);
        if (stepped != s.gear) {
            s.gear = stepped;
            applyButtons();
            openRequest(s.gearRequest, now);
        }
    }

    // Sequential paddles follow the lever in the manual gate: held while the
    // lever is held in a detent, released when it springs back to centre
    holdButton(BTN_PADDLE_UP, events.paddleUpHeld);
    holdButton(BTN_PADDLE_DOWN, events.paddleDownHeld);

    if (events.parkButtonPressed && s.gear != GWS_PARK) {
        s.gear = GWS_PARK;
        applyButtons();
        openRequest(s.gearRequest, now);
    }
}

void shifterTick(uint32_t now, GwsGear gameGear, bool gameManual, bool gameFresh) {
    // With no game talking there is nothing to adopt; the lever is a plain
    // button box
    if (!gameFresh) {
        s.connected = false;
        s.modeMismatch = false;
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
        applyButtons();
        s.modeRequest.pending = false;
    }

    if (adoptTick(s.gearRequest, shifterTargetGear() == gameGear, now)) {
        s.gear = gameGear;
        applyButtons();
        s.gearRequest.pending = false;
    }

    // The physical gate and the game's mode can only be realigned by moving the
    // lever between the side and centre gates, so flag the disagreement for the
    // indicator to blink until the lever is cycled
    s.modeMismatch = s.physicalManual != gameManual;

    // TRANSITIONAL is only a stand-in for Drive while the physical gate and the
    // game's mode disagree, forcing a plain flashing D instead of M/S. Once they
    // line up again collapse it back to a real Drive; otherwise it sticks (the
    // game sitting in Drive keeps the gear request matched, so it is never
    // adopted away) and the side gate can never light M/S again
    if (s.gear == GWS_TRANSITIONAL && !s.modeMismatch) {
        s.gear = GWS_DRIVE;
        applyButtons();
    }
}

bool shifterConnected() {
    return s.connected;
}

bool shifterModeMismatch() {
    return s.modeMismatch;
}

GwsGear shifterGear() {
    return s.gear;
}

GwsGear shifterTargetGear() {
    return s.gear == GWS_TRANSITIONAL ? GWS_DRIVE : s.gear;
}
