#pragma once

#include "config.h"

void boardPowerInit();
bool batteryInit();
bool batteryRead(uint8_t* pctOut);
bool sensorInit();
bool sensorRead(float* tempC, float* humidityPct);
