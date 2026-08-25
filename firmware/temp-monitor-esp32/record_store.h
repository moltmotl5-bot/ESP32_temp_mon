#pragma once

#include <stdint.h>

struct TempRecord {
  uint32_t timestamp;   // Unix epoch (seconds), aligned to 5-min slot
  float temperature;    // °C
  float humidity;       // % RH
};

void recordStoreInit();
bool recordStoreAppend(uint32_t timestamp, float tempC, float humPct);
uint16_t recordStoreCount();
uint16_t recordStoreMax();
uint16_t recordStoreCopyRecent(uint16_t maxCount, TempRecord* out);
bool recordStoreLatestTimestamp(uint32_t* outTs);
void recordStoreClear();
