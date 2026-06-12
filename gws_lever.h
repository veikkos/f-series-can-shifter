#pragma once

#include <stdint.h>

// Events decoded from one GWS lever position frame
struct LeverEvents {
    int stepsTowardDrive = 0; // one per detent, negative toward reverse
    bool enteredManualGate = false;
    bool leftManualGate = false;
    bool paddleUp = false;
    bool paddleDown = false;
    bool parkButtonPressed = false;
};

// Decode a lever position frame into events, false for a repeated frame
bool gwsLeverDecode(const uint8_t* data, LeverEvents* events);
