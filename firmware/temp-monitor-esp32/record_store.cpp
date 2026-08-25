#include "record_store.h"

#include "config.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace {
Preferences prefs;
TempRecord records[RECORD_MAX];
uint16_t head = 0;
uint16_t count = 0;
bool loaded = false;

constexpr const char* NVS_NS = "temp-mon";
constexpr const char* KEY_HEAD = "head";
constexpr const char* KEY_COUNT = "count";
constexpr const char* KEY_DATA = "data";

void persistLocked() {
  prefs.putUShort(KEY_HEAD, head);
  prefs.putUShort(KEY_COUNT, count);
  prefs.putBytes(KEY_DATA, records, sizeof(records));
}

void sanitizeMeta() {
  if (head >= RECORD_MAX) head = 0;
  if (count > RECORD_MAX) count = RECORD_MAX;
}
}  // namespace

void recordStoreInit() {
  if (loaded) return;

  prefs.begin(NVS_NS, false);
  head = prefs.getUShort(KEY_HEAD, 0);
  count = prefs.getUShort(KEY_COUNT, 0);
  sanitizeMeta();

  const size_t blobLen = prefs.getBytesLength(KEY_DATA);
  if (blobLen == sizeof(records)) {
    prefs.getBytes(KEY_DATA, records, sizeof(records));
  } else {
    memset(records, 0, sizeof(records));
    if (blobLen != 0) {
      Serial.printf("Record store: blob size mismatch (%u), reset\n", static_cast<unsigned>(blobLen));
      head = 0;
      count = 0;
      persistLocked();
    }
  }
  prefs.end();

  loaded = true;
  Serial.printf("Record store: %u/%u entries (head=%u)\n", count, RECORD_MAX, head);
}

bool recordStoreAppend(uint32_t timestamp, float tempC, float humPct) {
  if (!loaded) recordStoreInit();

  records[head].timestamp = timestamp;
  records[head].temperature = tempC;
  records[head].humidity = humPct;

  head = static_cast<uint16_t>((head + 1) % RECORD_MAX);
  if (count < RECORD_MAX) {
    count++;
  }

  prefs.begin(NVS_NS, false);
  persistLocked();
  prefs.end();

  Serial.printf("Record saved: ts=%lu %.1f C %.1f %% (%u/%u)\n", static_cast<unsigned long>(timestamp),
                tempC, humPct, count, RECORD_MAX);
  return true;
}

uint16_t recordStoreCount() {
  if (!loaded) recordStoreInit();
  return count;
}

uint16_t recordStoreMax() { return RECORD_MAX; }

uint16_t recordStoreCopyRecent(uint16_t maxCount, TempRecord* out) {
  if (!loaded) recordStoreInit();
  if (!out || count == 0 || maxCount == 0) return 0;

  const uint16_t n = maxCount < count ? maxCount : count;
  for (uint16_t i = 0; i < n; ++i) {
    const int idx =
        (static_cast<int>(head) - static_cast<int>(n) + static_cast<int>(i) + RECORD_MAX) % RECORD_MAX;
    out[i] = records[idx];
  }
  return n;
}

void recordStoreClear() {
  if (!loaded) recordStoreInit();

  memset(records, 0, sizeof(records));
  head = 0;
  count = 0;

  prefs.begin(NVS_NS, false);
  persistLocked();
  prefs.end();

  Serial.println("Record store cleared");
}
