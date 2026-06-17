#include "gws_lever.h"
#include "types.h"

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

// The M/S side gate covers its centre rest and both paddle detents; every other
// position is the auto gate. Deriving the gate from the absolute position means
// a fast move that skips the CENTRE_SIDE rest frame is still seen as a gate
// change, rather than being missed because it wasn't the exact rest transition.
static bool inSideGate(uint8_t position) {
    return position == LEVER_CENTRE_SIDE ||
           position == LEVER_SIDE_UP ||
           position == LEVER_SIDE_DOWN;
}

// Notches to step when the lever moves between positions: the gear follows the
// lever deeper into a gate (one per detent) and ignores the spring back toward
// centre. Robust to the reader repeating or skipping detents.
static int gateStep(uint8_t from, uint8_t to) {
    int down = downDepth(to) - downDepth(from);
    if (down > 0) return down;  // deeper toward drive
    int up = upDepth(to) - upDepth(from);
    if (up > 0) return -up;     // deeper toward reverse
    return 0;
}

bool gwsLeverDecode(const uint8_t* data, LeverEvents* events) {
    static uint8_t currentPosition = LEVER_CENTRE_MIDDLE;
    static int currentCounter = -1;

    uint8_t readerCounter = data[1];
    if (readerCounter == currentCounter) {
        return false;
    }

    uint8_t position = data[2];

    bool wasSide = inSideGate(currentPosition);
    bool nowSide = inSideGate(position);
    events->leftManualGate = wasSide && !nowSide;
    events->enteredManualGate = !wasSide && nowSide;

    events->stepsTowardDrive = gateStep(currentPosition, position);

    events->paddleUpHeld = position == LEVER_SIDE_UP;
    events->paddleDownHeld = position == LEVER_SIDE_DOWN;

    events->parkButtonPressed = data[3] == GWS_PARK_BUTTON_PRESSED;

    currentPosition = position;
    currentCounter = readerCounter;
    return true;
}
