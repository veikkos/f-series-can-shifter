#include "config.h"
#include <Arduino.h>
#include "types.h"
#include "serial.h"
#include "can_adapter.h"
#include "gamepad.h"

SInput s_input;
SGws s_gws;

static int signum(int val) {
    return (0 < val) - (val < 0);
}

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

// Game gear from the latest telemetry, reduced to a sign: -1 R, 0 N, 1 D
static int gameGearSign() {
    switch (s_input.currentGear) {
        case REVERSE: return -1;
        case DRIVE:   return 1;
        default:      return 0; // NEUTRAL, PARK
    }
}

static bool gameShifterManual() {
    return s_input.explicitGear != NONE;
}

static bool gameInPark() {
    return s_input.currentGear == PARK;
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

    if (gameInPark() && s_gws.gear == GWS_NEUTRAL) {
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
    if (signum(s_gws.gear) != gameGearSign()) {
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
    static uint8_t currentCounter = 0;

    uint8_t readerCounter = data[1];
    if (readerCounter == currentCounter) {
        return;
    }

    uint8_t position = data[2];

    if (currentPosition == LEVER_CENTRE_SIDE && position == LEVER_CENTRE_MIDDLE) {
        s_gws.attempts = 0;
        s_gws.shifter_manual = false;
    }

    if (currentPosition == LEVER_CENTRE_MIDDLE && position == LEVER_CENTRE_SIDE) {
        s_gws.attempts = 0;
        s_gws.shifter_manual = true;
    }

    if (position != currentPosition && position != LEVER_CENTRE_MIDDLE && position != LEVER_CENTRE_SIDE) {
        switch (position) {
            case LEVER_UP:
            case LEVER_UP_TWO:
                if (s_gws.gear == GWS_NEUTRAL) {
                    s_gws.attempts = 0;
                    s_gws.gear = GWS_REVERSE;
                }
                if (signum(s_gws.gear) == 1) {
                    s_gws.attempts = 0;
                    s_gws.gear = GWS_NEUTRAL;
                }
                break;
            case LEVER_DOWN:
            case LEVER_DOWN_TWO:
                if (s_gws.gear == GWS_NEUTRAL) {
                    s_gws.attempts = 0;
                    s_gws.gear = GWS_DRIVE;
                }
                if (s_gws.gear == GWS_REVERSE) {
                    s_gws.attempts = 0;
                    s_gws.gear = GWS_NEUTRAL;
                }
                break;
            case LEVER_SIDE_UP:
                gamepadPress(BTN_PADDLE_UP);
                gamepadRelease(BTN_PADDLE_UP);
                break;
            case LEVER_SIDE_DOWN:
                gamepadPress(BTN_PADDLE_DOWN);
                gamepadRelease(BTN_PADDLE_DOWN);
                break;
            default:
                break;
        }
    }

    if ((data[3] == GWS_PARK_BUTTON_PRESSED) && (s_gws.gear != GWS_NEUTRAL)) {
        s_gws.attempts = 0;
        s_gws.gear = GWS_NEUTRAL;
    }

    currentPosition = position;
    currentCounter = readerCounter;
}

// Drive the game towards the lever's selected gear / mode via gamepad buttons
void sendJoystick() {
    static uint32_t lastAttempt = 0;
    uint32_t current = millis();

    if (s_gws.shifter_manual != gameShifterManual()) {
        if (s_gws.attempts == 3) {
            if (!CONFIGURATION_MODE) {
                s_gws.gear = GWS_TRANSITIONAL;
                s_gws.attempts = 0;
            }
        } else if (current - lastAttempt >= 1000) {
            gamepadPress(BTN_MODE_TOGGLE);
            gamepadRelease(BTN_MODE_TOGGLE);
            s_gws.attempts++;
            lastAttempt = current;
        }
    }

    if (signum(s_gws.gear) != gameGearSign()) {
        if (s_gws.attempts == 3) {
            s_gws.gear = (GwsGear)gameGearSign();
            s_gws.attempts = 0;
        } else if (current - lastAttempt >= 1000) {
            // s_gws.gear (-1/0/1) maps to the reverse/neutral/drive button.
            uint8_t gearToEngage = s_gws.gear + 1;

            gamepadRelease(BTN_GEAR_REVERSE);
            gamepadRelease(BTN_GEAR_NEUTRAL);
            gamepadRelease(BTN_GEAR_DRIVE);

            if (gearToEngage == BTN_GEAR_NEUTRAL) {
                gamepadPress(gearToEngage);
                gamepadRelease(gearToEngage);
            } else {
                gamepadPress(gearToEngage);
            }

            s_gws.attempts++;
            lastAttempt = current;
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
