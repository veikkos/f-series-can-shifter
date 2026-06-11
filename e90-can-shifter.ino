#include "config.h"
#include <Arduino.h>
#include "types.h"
#include "serial.h"
#include "can_adapter.h"
#include "gamepad.h"

SInput s_input;
SGws s_gws;

// Gamepad sync attempts before forcing the lever state to match the game
static const uint8_t SYNC_ATTEMPT_LIMIT = 3;

// CRC8, polynomial 0x1D, init 0x00, final xor 0x70
static uint8_t crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x1D) : (uint8_t)(crc << 1);
        }
    }
    return crc ^ 0x70;
}

// The game's current gear as a lever gear
static GwsGear gameGear() {
    switch (s_input.currentGear) {
        case PARK:    return GWS_PARK;
        case REVERSE: return GWS_REVERSE;
        case DRIVE:   return GWS_DRIVE;
        default:      return GWS_NEUTRAL; // NEUTRAL
    }
}

// The lever's gear, with the transient TRANSITIONAL state counted as DRIVE
static GwsGear leverGear() {
    return s_gws.gear == GWS_TRANSITIONAL ? GWS_DRIVE : s_gws.gear;
}

// Gamepad button that engages the given gear in the game
static GamepadButton gearButton(GwsGear gear) {
    switch (gear) {
        case GWS_PARK:    return BTN_GEAR_PARK;
        case GWS_REVERSE: return BTN_GEAR_REVERSE;
        case GWS_DRIVE:   return BTN_GEAR_DRIVE;
        default:          return BTN_GEAR_NEUTRAL;
    }
}

// Move the gear along the P-R-N-D gate: positive steps toward drive, negative
// toward reverse. Park is only reachable via the park button, so up-tips never
// re-enter it.
static GwsGear stepGear(GwsGear gear, int steps) {
    static const GwsGear ladder[] = { GWS_PARK, GWS_REVERSE, GWS_NEUTRAL, GWS_DRIVE };
    if (gear == GWS_PARK && steps < 0) {
        return GWS_PARK;
    }
    GwsGear current = (gear == GWS_TRANSITIONAL) ? GWS_DRIVE : gear;
    int idx = 1;
    for (int i = 0; i < 4; i++) {
        if (ladder[i] == current) idx = i;
    }
    idx += steps;
    if (idx < 1) idx = 1; // never tip into park
    if (idx > 3) idx = 3;
    return ladder[idx];
}

// How far a lever position reaches into the up (toward reverse) gate, 0 if not
static int upDepth(uint8_t position) {
    if (position == LEVER_UP)     return 1;
    if (position == LEVER_UP_TWO) return 2;
    return 0;
}

// How far a lever position reaches into the down (toward drive) gate, 0 if not
static int downDepth(uint8_t position) {
    if (position == LEVER_DOWN)     return 1;
    if (position == LEVER_DOWN_TWO) return 2;
    return 0;
}

static bool gameShifterManual() {
    return s_input.explicitGear != NONE;
}

void sendBacklight() {
    static uint8_t frame[2] = {BACKLIGHT_OFF, 0x00};
    bool lit = s_input.light_lowbeam || s_input.light_highbeam;
    frame[0] = lit ? BACKLIGHT_FULL : BACKLIGHT_OFF;
    canSend(GWS_ID_BACKLIGHT, frame, sizeof(frame));
}

void sendGear() {
    static uint8_t frame[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static uint8_t counter = 0;

    frame[1] = counter;

    if (s_gws.gear == GWS_PARK) {
        frame[2] = DISPLAY_PARK;
    } else if (s_gws.gear == GWS_REVERSE) {
        frame[2] = DISPLAY_REVERSE;
    } else if (s_gws.gear == GWS_NEUTRAL) {
        frame[2] = DISPLAY_NEUTRAL;
    } else if (s_gws.gear == GWS_DRIVE) {
        frame[2] = DISPLAY_DRIVE_MS;
    } else if (s_gws.gear == GWS_TRANSITIONAL) {
        frame[2] = DISPLAY_DRIVE;
    }

    // Flash the indicator until the game engages the requested gear
    if (leverGear() != gameGear()) {
        frame[2] |= DISPLAY_FLASH;
    }

    frame[0] = crc8(&frame[1], 4);

    canSend(GWS_ID_DISPLAY, frame, sizeof(frame));

    counter++;
    if (counter == GWS_COUNTER_INVALID) {
        counter = 0;
    }
}

void sendCanBus() {
    static uint32_t previous = 0;
    uint32_t current = millis();

    if (current - previous >= 100) {
        sendBacklight();
        sendGear();
        previous = current;
    }
}

void handleGwsPosition(const uint8_t* data) {
    static uint8_t currentPosition = LEVER_CENTRE_SIDE;
    static int currentCounter = -1;

    uint8_t readerCounter = data[1];
    if (readerCounter == currentCounter) {
        return;
    }

    uint8_t position = data[2];

    if (currentPosition == LEVER_CENTRE_SIDE && position == LEVER_CENTRE_MIDDLE) {
        s_gws.mode_attempts = 0;
        s_gws.shifter_manual = false;
    }

    if (currentPosition == LEVER_CENTRE_MIDDLE && position == LEVER_CENTRE_SIDE) {
        s_gws.mode_attempts = 0;
        s_gws.shifter_manual = true;
    }

    // Tipping deeper into a gate steps the gear: one notch = one step, two = two
    int downStep = downDepth(position) - downDepth(currentPosition);
    int upStep   = upDepth(position)   - upDepth(currentPosition);
    if (downStep > 0) {
        s_gws.gear = stepGear(s_gws.gear, downStep);
        s_gws.gear_attempts = 0;
    } else if (upStep > 0) {
        s_gws.gear = stepGear(s_gws.gear, -upStep);
        s_gws.gear_attempts = 0;
    }

    // Sequential paddles in the manual gate
    if (position == LEVER_SIDE_UP && currentPosition != LEVER_SIDE_UP) {
        gamepadPress(BTN_PADDLE_UP);
        gamepadRelease(BTN_PADDLE_UP);
    }
    if (position == LEVER_SIDE_DOWN && currentPosition != LEVER_SIDE_DOWN) {
        gamepadPress(BTN_PADDLE_DOWN);
        gamepadRelease(BTN_PADDLE_DOWN);
    }

    if ((data[3] == GWS_PARK_BUTTON_PRESSED) && (s_gws.gear != GWS_PARK)) {
        s_gws.gear_attempts = 0;
        s_gws.gear = GWS_PARK;
    }

    currentPosition = position;
    currentCounter = readerCounter;
}

// Drive the game towards the lever's selected gear / mode via gamepad buttons
void sendJoystick() {
    static uint32_t lastModeAttempt = 0;
    static uint32_t lastGearAttempt = 0;
    uint32_t current = millis();

    if (s_gws.shifter_manual != gameShifterManual()) {
        if (s_gws.mode_attempts == SYNC_ATTEMPT_LIMIT) {
            if (!CONFIGURATION_MODE) {
                s_gws.gear = GWS_TRANSITIONAL;
                s_gws.mode_attempts = 0;
            }
        } else if (current - lastModeAttempt >= 1000) {
            gamepadPress(BTN_MODE_TOGGLE);
            gamepadRelease(BTN_MODE_TOGGLE);
            s_gws.mode_attempts++;
            lastModeAttempt = current;
        }
    }

    if (leverGear() != gameGear()) {
        if (s_gws.gear_attempts == SYNC_ATTEMPT_LIMIT) {
            s_gws.gear = gameGear();
            s_gws.gear_attempts = 0;
        } else if (current - lastGearAttempt >= 1000) {
            GamepadButton button = gearButton(leverGear());

            gamepadRelease(BTN_GEAR_REVERSE);
            gamepadRelease(BTN_GEAR_NEUTRAL);
            gamepadRelease(BTN_GEAR_DRIVE);
            gamepadRelease(BTN_GEAR_PARK);

            gamepadPress(button);
            if (leverGear() == GWS_NEUTRAL) {
                gamepadRelease(button); // neutral is a momentary tap
            }

            s_gws.gear_attempts++;
            lastGearAttempt = current;
        }
    }
}

static const CanHandlerEntry handler_table[] = {
    { GWS_ID_POSITION, handleGwsPosition }
};
static const size_t handler_count = sizeof(handler_table) / sizeof(handler_table[0]);

void setup() {
    serialBegin();
    canBegin();
    gamepadBegin();
}

void loop() {
    sendCanBus();
    sendJoystick();
    serialPoll();
    canPoll(handler_table, handler_count);
}
