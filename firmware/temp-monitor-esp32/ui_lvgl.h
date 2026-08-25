#pragma once

#include "lvgl_setup.h"

void uiInit();
void uiUpdate(bool wifiConnected, bool ntpSynced, float tempC, float humidityPct, bool hasSensor,
              int8_t batteryPct, uint16_t recordCount, uint16_t recordMax, const char* statusLine);
