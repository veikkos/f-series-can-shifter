#include "config.h"
#include <Arduino.h>
#include "types.h"
#include "serial.h"
#include "can_adapter.h"
#include "gamepad.h"
#include "gws_lever.h"
#include "shifter.h"

SInput s_input;

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

    GwsGear gear = shifterGear();
    if (gear == GWS_PARK) {
        frame[2] = DISPLAY_PARK;
    } else if (gear == GWS_REVERSE) {
        frame[2] = DISPLAY_REVERSE;
    } else if (gear == GWS_NEUTRAL) {
        frame[2] = DISPLAY_NEUTRAL;
    } else if (gear == GWS_DRIVE) {
        frame[2] = DISPLAY_DRIVE_MS;
    } else if (gear == GWS_TRANSITIONAL) {
        frame[2] = DISPLAY_DRIVE;
    }

    // Flash the indicator until the game engages the requested gear, with no
    // game talking there is nothing to wait for
    if (shifterConnected() && shifterTargetGear() != gameGear()) {
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
    LeverEvents events;
    if (gwsLeverDecode(data, &events)) {
        shifterApplyLever(events, millis());
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
    uint32_t now = millis();
    shifterTick(now, gameGear(), gameShifterManual(), serialGameFresh(now));
    serialPoll();
    canPoll(handler_table, handler_count);
}
