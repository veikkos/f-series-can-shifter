#pragma once

#include <stdint.h>

enum INDICATOR {
    I_OFF = 0,
    I_LEFT,
    I_RIGHT,
    I_HAZZARD
};

enum GEAR {
    PARK = -1,
    REVERSE = 0,
    NEUTRAL = 1,
    DRIVE = 2
};

enum GEAR_MANUAL {
    NONE = 0, M1, M2, M3, M4, M5, M6, M7, M8
};

enum GEAR_MODE {
    NORMAL,
    SPORT
};

enum IGNITION_STATE {
    IG_OFF = 0,
    IG_ACCESSORY = 1,
    IG_ON = 2,
    IG_STARTER = 3
};

enum GwsCanId : uint32_t {
    GWS_ID_POSITION  = 0x197, // lever position report (RX)
    GWS_ID_BACKLIGHT = 0x202, // backlight control (TX)
    GWS_ID_DISPLAY   = 0x3FD, // gear-indicator update (TX)
};

enum GwsLeverPosition : uint8_t {
    LEVER_CENTRE_MIDDLE = 0x0E,
    LEVER_UP            = 0x1E, // towards front of car
    LEVER_UP_TWO        = 0x2E,
    LEVER_DOWN          = 0x3E, // towards back of car
    LEVER_DOWN_TWO      = 0x4E,
    LEVER_CENTRE_SIDE   = 0x7E,
    LEVER_SIDE_UP       = 0x5E,
    LEVER_SIDE_DOWN     = 0x6E,
};

static const uint8_t GWS_PARK_BUTTON_PRESSED = 0xD5;

enum GwsDisplay : uint8_t {
    DISPLAY_BLANK    = 0x00,
    DISPLAY_PARK     = 0x20,
    DISPLAY_REVERSE  = 0x40,
    DISPLAY_NEUTRAL  = 0x60,
    DISPLAY_DRIVE    = 0x80, // D
    DISPLAY_DRIVE_MS = 0x81, // D, can move to M/S; GWS shows M/S when the lever is at the side
    DISPLAY_FLASH    = 0x08, // OR with the gear to flash it
};

static const uint8_t GWS_COUNTER_INVALID = 0x0F;

static const uint8_t BACKLIGHT_FULL = 0xFF;
static const uint8_t BACKLIGHT_OFF  = 0x00;

enum GamepadButton : uint8_t {
    BTN_GEAR_REVERSE = 0,
    BTN_GEAR_DRIVE   = 1,
    BTN_MODE_SPORT   = 2,
    BTN_PADDLE_UP    = 3,
    BTN_PADDLE_DOWN  = 4,
    BTN_GEAR_PARK    = 5,
    BTN_MODE_MANUAL  = 6,
};

// Gear the lever has selected.
enum GwsGear : int8_t {
    GWS_REVERSE      = -1,
    GWS_NEUTRAL      = 0,
    GWS_DRIVE        = 1,
    GWS_TRANSITIONAL = 2, // momentary state while toggling auto/manual
    GWS_PARK         = 3,
};

struct SInput {
    IGNITION_STATE ignition = IG_ON;
    INDICATOR indicator_state = I_OFF;

    uint16_t time_year = 2010;
    uint8_t time_month = 1;
    uint8_t time_day = 1;
    uint8_t time_hour = 12;
    uint8_t time_minute = 0;
    uint8_t time_second = 0;

    uint16_t rpm = 0;
    uint16_t speed = 0;
    GEAR currentGear = PARK;
    GEAR_MANUAL explicitGear = NONE;
    GEAR_MODE mode = NORMAL;
    uint16_t fuel = 1000;
    uint16_t fuel_injection = 0;
    uint8_t water_temp = 0;
    uint8_t oil_temp = 0;
    uint16_t custom_light = 0;

    bool custom_light_on = false;
    bool light_shift = false;
    bool light_highbeam = false;
    bool light_lowbeam = true;
    bool light_fog = false;
    bool light_tc_active = false;
    bool light_esc_active = false;
    bool light_tc_disabled = false;
    bool light_esc_disabled = false;
    bool light_beacon = false;
    bool oil_warn = false;
    bool battery_warn = false;
    bool abs_warn = false;
    bool engine_temp_yellow = false;
    bool engine_temp_red = false;
    bool check_engine = false;
    bool clutch_temp = false;
    bool brake_temp = false;
    bool radiator_warn = false;
    bool engine_running = true;
    int16_t ambient_temp = 200;
    bool yellow_triangle = false;
    bool red_triangle = false;
    bool gear_issue = false;
    bool exclamation_mark = false;
    bool adblue_low = false;
    bool checkered_flag = false;
    bool limit_yellow = false;
    bool limit_red = false;

    struct {
        bool fl_deflated = false;
        bool fr_deflated = false;
        bool rl_deflated = false;
        bool rr_deflated = false;
        bool all_deflated = false;
    } tires;

    struct {
        bool fl_open = false;
        bool fr_open = false;
        bool rl_open = false;
        bool rr_open = false;
        bool tailgate_open = false;
    } doors;

    bool handbrake = false;

    struct {
        bool enabled = false;
        uint16_t speed = 0;

        struct {
            uint8_t distance = 0; // 0: no ACC, 1-4: distances from short to far
            bool yellow_car_static = false;
            bool foot_on_brake = false;
            bool red_car_blinking = false;
        } acc;
    } cruise;
};
