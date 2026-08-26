#pragma once

#include <stdint.h>

class String;

void sdLoggerInit();
bool sdLoggerReady();
bool sdLoggerAppend(uint32_t timestamp, float tempC, float humPct);
bool sdLoggerBuildReadingsJson(String& outJson);
