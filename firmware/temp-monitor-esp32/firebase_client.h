#pragma once

#include <stdint.h>

struct FirebaseMetaState {
  float tempC = 0.0f;
  float humidity = 0.0f;
  bool hasSensor = false;
  int8_t batteryPct = -1;
  bool sdReady = false;
  uint16_t recordCount = 0;
  uint16_t recordMax = 0;
  const char* clockLine = "---- -- -- --:--:--";
};

bool firebaseClientConfigured();
bool firebaseClientPushReading(uint32_t timestamp, float tempC, float humPct);
bool firebaseClientPushMeta(const FirebaseMetaState& state, bool force = false);
void firebaseClientLoop();
