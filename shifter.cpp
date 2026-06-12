#include "shifter.h"
#include "gamepad.h"

// Gamepad sync attempts before adopting the game's state
static const uint8_t SYNC_ATTEMPT_LIMIT = 3;

// Pause between gamepad sync attempts
static const uint32_t SYNC_RETRY_MS = 1000;

// A sync machine drives one lever-selected setting, gear or mode, into the
// game by pressing a gamepad button until the game agrees or the attempts
// run out
enum SyncState : uint8_t {
    SYNC_IDLE,       // lever and game agree
    SYNC_REQUESTING, // pressing the gamepad button, counting attempts
};

enum SyncAction : uint8_t {
    SYNC_NONE,  // nothing to do this tick
    SYNC_PRESS, // press the gamepad button
    SYNC_ADOPT, // give up and take the game's state
};

struct SyncMachine {
    SyncState state = SYNC_IDLE;
    uint8_t attempts = 0;
    uint32_t lastAttemptMs = 0;
};

// A new lever target gets fresh attempts
static void syncNewTarget(SyncMachine& m) {
    m.attempts = 0;
}

// Advance a sync machine one tick and return what the caller should do
static SyncAction syncTick(SyncMachine& m, bool matches, uint32_t now) {
    switch (m.state) {
        case SYNC_IDLE:
            if (!matches) {
                m.state = SYNC_REQUESTING;
            }
            return SYNC_NONE;

        case SYNC_REQUESTING:
            if (matches) {
                m.state = SYNC_IDLE;
                return SYNC_NONE;
            }
            if (m.attempts >= SYNC_ATTEMPT_LIMIT) {
                return SYNC_ADOPT;
            }
            if (now - m.lastAttemptMs >= SYNC_RETRY_MS) {
                m.attempts++;
                m.lastAttemptMs = now;
                return SYNC_PRESS;
            }
            return SYNC_NONE;
    }
    return SYNC_NONE;
}

// Lever-side state the machines drive the game toward
static struct {
    GwsGear gear = GWS_NEUTRAL;
    bool manual = false;
    SyncMachine gearSync;
    SyncMachine modeSync;
} s;

// Step the gear along the R-N-D gate: positive steps toward drive, negative
// toward reverse. Park sits at the reverse end of the gate and is engaged only
// via the park button, so the clamp keeps tips from ever landing back in park.
static GwsGear stepGear(GwsGear gear, int steps) {
    static const GwsGear ladder[] = { GWS_REVERSE, GWS_NEUTRAL, GWS_DRIVE };
    GwsGear current = (gear == GWS_PARK)         ? GWS_REVERSE
                    : (gear == GWS_TRANSITIONAL) ? GWS_DRIVE
                    : gear;
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

// Hold the selected gear's button, no button held means neutral
static void pressGearButton(GwsGear gear) {
    gamepadRelease(BTN_GEAR_REVERSE);
    gamepadRelease(BTN_GEAR_DRIVE);
    gamepadRelease(BTN_GEAR_PARK);

    switch (gear) {
        case GWS_PARK:    gamepadPress(BTN_GEAR_PARK);    break;
        case GWS_REVERSE: gamepadPress(BTN_GEAR_REVERSE); break;
        case GWS_DRIVE:   gamepadPress(BTN_GEAR_DRIVE);   break;
        default:          break; // neutral: nothing held
    }
}

void shifterApplyLever(const LeverEvents& events) {
    if (events.leftManualGate) {
        s.manual = false;
        syncNewTarget(s.modeSync);
    }
    if (events.enteredManualGate) {
        s.manual = true;
        syncNewTarget(s.modeSync);
    }

    // Tipping deeper into a gate steps the gear: one notch = one step, two = two
    if (events.stepsTowardDrive != 0) {
        GwsGear stepped = stepGear(s.gear, events.stepsTowardDrive);
        if (stepped != s.gear) {
            s.gear = stepped;
            syncNewTarget(s.gearSync);
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
        syncNewTarget(s.gearSync);
    }
}

void shifterTick(uint32_t now, GwsGear gameGear, bool gameManual, bool gameFresh) {
    // A stale game state is never adopted, pressing just stops at the
    // attempt limit
    switch (syncTick(s.modeSync, s.manual == gameManual, now)) {
        case SYNC_PRESS:
            tapButton(BTN_MODE_TOGGLE);
            break;
        case SYNC_ADOPT:
            if (gameFresh) {
                s.gear = GWS_TRANSITIONAL;
                syncNewTarget(s.modeSync);
            }
            break;
        case SYNC_NONE:
            break;
    }

    switch (syncTick(s.gearSync, shifterTargetGear() == gameGear, now)) {
        case SYNC_PRESS:
            pressGearButton(shifterTargetGear());
            break;
        case SYNC_ADOPT:
            if (gameFresh) {
                s.gear = gameGear;
                syncNewTarget(s.gearSync);
            }
            break;
        case SYNC_NONE:
            break;
    }
}

GwsGear shifterGear() {
    return s.gear;
}

GwsGear shifterTargetGear() {
    return s.gear == GWS_TRANSITIONAL ? GWS_DRIVE : s.gear;
}
