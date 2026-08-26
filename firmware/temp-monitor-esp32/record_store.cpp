#include "record_store.h"

#include "config.h"
#include "sd_logger.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
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
constexpr const char* KEY_DATA_LEGACY = "data";

constexpr int CHUNK_COUNT = 6;
constexpr uint16_t CHUNK_RECORDS = RECORD_MAX / CHUNK_COUNT;  // 144 records = 1728 B
static_assert(CHUNK_RECORDS * CHUNK_COUNT == RECORD_MAX, "RECORD_MAX must divide into chunks");
constexpr size_t CHUNK_BYTES = CHUNK_RECORDS * sizeof(TempRecord);

constexpr const char* CHUNK_KEYS[CHUNK_COUNT] = {"dat0", "dat1", "dat2", "dat3", "dat4", "dat5"};

void sanitizeMeta() {
  if (head >= RECORD_MAX) head = 0;
  if (count > RECORD_MAX) count = RECORD_MAX;
}

void removeAllDataKeys() {
  prefs.remove(KEY_DATA_LEGACY);
  for (int i = 0; i < CHUNK_COUNT; ++i) {
    prefs.remove(CHUNK_KEYS[i]);
  }
}

bool chunkSizesValid() {
  for (int i = 0; i < CHUNK_COUNT; ++i) {
    if (prefs.getBytesLength(CHUNK_KEYS[i]) != CHUNK_BYTES) {
      return false;
    }
  }
  return true;
}

bool loadChunkedRecords() {
  if (!chunkSizesValid()) return false;

  for (int i = 0; i < CHUNK_COUNT; ++i) {
    const size_t offset = static_cast<size_t>(i) * CHUNK_BYTES;
    if (prefs.getBytes(CHUNK_KEYS[i], reinterpret_cast<uint8_t*>(records) + offset, CHUNK_BYTES) !=
        CHUNK_BYTES) {
      return false;
    }
  }
  return true;
}

bool loadLegacyRecords() {
  const size_t legacyLen = prefs.getBytesLength(KEY_DATA_LEGACY);
  if (legacyLen != sizeof(records)) return false;
  return prefs.getBytes(KEY_DATA_LEGACY, records, sizeof(records)) == sizeof(records);
}

bool saveChunkedRecordsOnce() {
  bool ok = true;
  for (int i = 0; i < CHUNK_COUNT; ++i) {
    const size_t offset = static_cast<size_t>(i) * CHUNK_BYTES;
    const size_t written =
        prefs.putBytes(CHUNK_KEYS[i], reinterpret_cast<const uint8_t*>(records) + offset, CHUNK_BYTES);
    if (written != CHUNK_BYTES) {
      Serial.printf("Record store: chunk %d write failed (%u/%u)\n", i,
                    static_cast<unsigned>(written), static_cast<unsigned>(CHUNK_BYTES));
      ok = false;
    }
  }
  return ok;
}

bool saveChunkedRecords() {
  removeAllDataKeys();
  if (saveChunkedRecordsOnce()) {
    return true;
  }

  Serial.println("Record store: retry after clearing data keys");
  removeAllDataKeys();
  return saveChunkedRecordsOnce();
}

void persistLocked() {
  prefs.putUShort(KEY_HEAD, head);
  prefs.putUShort(KEY_COUNT, count);
  if (!saveChunkedRecords()) {
    Serial.println("Record store: persist failed — NVS may be full; use BOOT long clear or erase NVS");
  }
}

void logLatestSample() {
  if (count == 0) return;
  const int idx = (static_cast<int>(head) - 1 + RECORD_MAX) % RECORD_MAX;
  Serial.printf("Record store sample: idx=%d ts=%lu T=%.1f H=%.1f\n", idx,
                static_cast<unsigned long>(records[idx].timestamp), records[idx].temperature,
                records[idx].humidity);
}
}  // namespace

void recordStoreInit() {
  if (loaded) return;

  prefs.begin(NVS_NS, false);
  head = prefs.getUShort(KEY_HEAD, 0);
  count = prefs.getUShort(KEY_COUNT, 0);
  sanitizeMeta();

  const bool hasLegacy = prefs.getBytesLength(KEY_DATA_LEGACY) > 0;
  const bool loadedData = loadChunkedRecords() || loadLegacyRecords();
  if (!loadedData) {
    memset(records, 0, sizeof(records));
    removeAllDataKeys();
    if (head != 0 || count != 0 || hasLegacy) {
      Serial.println("Record store: data blob missing, resetting meta");
      head = 0;
      count = 0;
      prefs.putUShort(KEY_HEAD, head);
      prefs.putUShort(KEY_COUNT, count);
    }
  } else if (hasLegacy && !chunkSizesValid()) {
    Serial.println("Record store: migrating legacy blob to chunked NVS");
    persistLocked();
  }

  prefs.end();

  loaded = true;
  Serial.printf("Record store: %u/%u entries (head=%u)\n", count, RECORD_MAX, head);
  logLatestSample();
}

bool recordStoreAppend(uint32_t timestamp, float tempC, float humPct) {
  if (!loaded) recordStoreInit();
  if (tempC <= 0.0f || isnan(tempC) || isnan(humPct)) {
    Serial.println("Record store: skip invalid sample");
    return false;
  }

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

  if (sdLoggerReady()) {
    sdLoggerAppend(timestamp, tempC, humPct);
  }

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

bool recordStoreLatestTimestamp(uint32_t* outTs) {
  if (!loaded) recordStoreInit();
  if (!outTs || count == 0) return false;

  const int idx = (static_cast<int>(head) - 1 + RECORD_MAX) % RECORD_MAX;
  *outTs = records[idx].timestamp;
  return records[idx].timestamp >= 1700000000UL;
}

void recordStoreClear() {
  if (!loaded) recordStoreInit();

  memset(records, 0, sizeof(records));
  head = 0;
  count = 0;

  prefs.begin(NVS_NS, false);
  removeAllDataKeys();
  prefs.putUShort(KEY_HEAD, head);
  prefs.putUShort(KEY_COUNT, count);
  saveChunkedRecords();
  prefs.end();

  Serial.println("Record store cleared");
}
