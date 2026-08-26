#pragma once

#include <stdint.h>

struct WebDashboardState {
  float tempC = 0.0f;
  float humidity = 0.0f;
  bool hasSensor = false;
  int8_t batteryPct = -1;
  bool wifiConnected = false;
  bool timeValid = false;
  bool sdReady = false;
  uint16_t recordCount = 0;
  uint16_t recordMax = 0;
};

void webServerBegin();
void webServerStop();
void webServerLoop();
void webServerUpdateState(const WebDashboardState& state);
bool webServerActive();
