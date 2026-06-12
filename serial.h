#pragma once

#include <stdint.h>

#define pc Serial

void serialBegin();
void serialPoll();

// Whether a valid telemetry frame arrived within the last GAME_DATA_TIMEOUT_MS
bool serialGameFresh(uint32_t now);
