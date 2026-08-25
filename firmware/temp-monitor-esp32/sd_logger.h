#pragma once

#include <stdint.h>

void sdLoggerInit();
bool sdLoggerReady();
bool sdLoggerAppend(uint32_t timestamp, float tempC, float humPct);
