#pragma once

#include <stdint.h>
#include "types.h"
#include "gws_lever.h"

// Lever-side gear state machine that drives the game to match the lever via
// gamepad buttons

// Apply decoded lever events
void shifterApplyLever(const LeverEvents& events);

// Retry gamepad presses until the game matches the lever, or adopt the
// game's state after too many attempts. A stale game (gameFresh false) is
// never adopted
void shifterTick(uint32_t now, GwsGear gameGear, bool gameManual, bool gameFresh);

// The lever's gear for the gear indicator display
GwsGear shifterGear();

// The gear the game is driven toward, with TRANSITIONAL counted as DRIVE
GwsGear shifterTargetGear();
