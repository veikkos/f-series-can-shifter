#pragma once

#include <stdint.h>
#include "types.h"
#include "gws_lever.h"

// Lever-side gear state machine that drives the game to match the lever via
// gamepad buttons

// Apply decoded lever events: lever moves drive the gamepad buttons directly
// and open a request the game gets a grace period to engage
void shifterApplyLever(const LeverEvents& events, uint32_t now);

// Adopt the game's state when a lever request goes unanswered past the grace
// period, or immediately when the game changed the state on its own (car
// reset, in-game paddles). While the game is stale (gameFresh false) nothing
// is adopted and the lever acts as a plain button box; reconnecting gives a
// pending lever request a fresh grace period
void shifterTick(uint32_t now, GwsGear gameGear, bool gameManual, bool gameFresh,
                 GEAR_MANUAL gameManualGear);

// Whether game telemetry was fresh on the last tick
bool shifterConnected();

// Whether the lever's physical gate disagrees with the game's mode, only
// resolvable by moving the lever between the side and centre gates
bool shifterModeMismatch();

// The lever's gear for the gear indicator display
GwsGear shifterGear();

// The gear the game is driven toward, with TRANSITIONAL counted as DRIVE
GwsGear shifterTargetGear();
