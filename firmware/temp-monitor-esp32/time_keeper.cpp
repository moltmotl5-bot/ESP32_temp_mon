#include "time_keeper.h"

#include <Arduino.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

namespace {
Preferences prefs;

constexpr const char* NVS_NS = "temp-mon";
constexpr const char* KEY_EPOCH = "epoch";
constexpr uint32_t EPOCH_MIN = 1700000000;

void applyEpoch(time_t epoch) {
  if (epoch < static_cast<time_t>(EPOCH_MIN)) return;

  struct timeval tv {};
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}
}  // namespace

void timeKeeperInit() {
  prefs.begin(NVS_NS, true);
  const uint32_t saved = prefs.getUInt(KEY_EPOCH, 0);
  prefs.end();

  if (saved >= EPOCH_MIN) {
    applyEpoch(static_cast<time_t>(saved));
    Serial.printf("Time restored from NVS: %lu\n", static_cast<unsigned long>(saved));
  }
}

bool timeKeeperIsValid() {
  return time(nullptr) >= static_cast<time_t>(EPOCH_MIN);
}

void timeKeeperPersist() {
  const time_t now = time(nullptr);
  if (now < static_cast<time_t>(EPOCH_MIN)) return;

  prefs.begin(NVS_NS, false);
  prefs.putUInt(KEY_EPOCH, static_cast<uint32_t>(now));
  prefs.end();
}

void timeKeeperTick() {
  static uint32_t lastPersistMs = 0;
  if (!timeKeeperIsValid()) return;
  if (millis() - lastPersistMs < 60000) return;

  lastPersistMs = millis();
  timeKeeperPersist();
}
