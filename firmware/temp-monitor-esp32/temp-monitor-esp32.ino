/**
 * Temp Monitor — Waveshare ESP32-S3-RLCD-4.2
 * Phase 6: Firebase Realtime Database upload (no LAN web server)
 */

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "board.h"
#include "config.h"
#include "display_epaper.h"
#include "firebase_client.h"
#include "input_buttons.h"
#include "record_store.h"
#include "sd_logger.h"
#include "time_keeper.h"
#include "ui_lvgl.h"
#include "wifi_manager.h"

namespace {
float lastTempC = 0.0f;
float lastHumidity = 0.0f;
bool hasSensor = false;
int8_t lastBatteryPct = -1;
bool ntpSynced = false;
bool timeSyncAttempted = false;
char statusLine[48] = "Starting";
time_t lastStoredSlot = 0;

constexpr uint32_t WIFI_DISCONNECT_PORTAL_MS = 12000;

void formatClockLine(char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  const time_t now = time(nullptr);
  struct tm timeInfo {};
  if (timeKeeperIsValid() && localtime_r(&now, &timeInfo)) {
    snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d", timeInfo.tm_year + 1900,
             timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min,
             timeInfo.tm_sec);
  } else {
    snprintf(out, outLen, "---- -- -- --:--:--");
  }
}

FirebaseMetaState buildFirebaseMeta() {
  FirebaseMetaState state;
  state.tempC = lastTempC;
  state.humidity = lastHumidity;
  state.hasSensor = hasSensor;
  state.batteryPct = lastBatteryPct;
  state.sdReady = sdLoggerReady();
  state.recordCount = recordStoreCount();
  state.recordMax = recordStoreMax();
  static char clockLine[32];
  formatClockLine(clockLine, sizeof(clockLine));
  state.clockLine = clockLine;
  return state;
}

void pushFirebaseMeta(bool force) {
  if (!wifiManagerIsConnected() || wifiManagerPortalActive()) return;
  if (!firebaseClientConfigured()) return;
  firebaseClientPushMeta(buildFirebaseMeta(), force);
}

void refreshScreen() {
  if (!displayReady()) return;
  uiUpdate(wifiManagerIsConnected(), timeKeeperIsValid(), lastTempC, lastHumidity, hasSensor, lastBatteryPct,
           recordStoreCount(), recordStoreMax(), sdLoggerReady(), statusLine);
  displayRefreshFull();
}

void serviceTick() {
  displayTick();
  if (!displayReady()) return;
  uiUpdate(wifiManagerIsConnected(), timeKeeperIsValid(), lastTempC, lastHumidity, hasSensor, lastBatteryPct,
           recordStoreCount(), recordStoreMax(), sdLoggerReady(), statusLine);
  displayRefreshFull();
}

void readBattery() {
  uint8_t pct = 0;
  if (batteryRead(&pct)) {
    lastBatteryPct = static_cast<int8_t>(pct);
  }
}

void readSensor() {
  float temp = 0.0f;
  float hum = 0.0f;
  if (sensorRead(&temp, &hum)) {
    lastTempC = temp;
    lastHumidity = hum;
    hasSensor = true;
    Serial.printf("Sensor: %.1f C, %.1f %%RH\n", temp, hum);
  } else {
    hasSensor = sensorReadyNow();
    Serial.println("Sensor read failed");
  }
}

void maybeStoreSample() {
  if (!hasSensor || !timeKeeperIsValid()) return;

  const time_t now = time(nullptr);
  const time_t slot = now - (now % SAMPLE_INTERVAL_SEC);
  if (slot == lastStoredSlot) return;

  if (recordStoreAppend(static_cast<uint32_t>(slot), lastTempC, lastHumidity)) {
    if (wifiManagerIsConnected() && firebaseClientConfigured()) {
      firebaseClientPushReading(static_cast<uint32_t>(slot), lastTempC, lastHumidity);
      pushFirebaseMeta(true);
    }
  }
  lastStoredSlot = slot;
  timeKeeperPersist();
  uiRefreshChart();
}

bool tryNtpSync() {
  if (!wifiManagerIsConnected()) return false;

  if (!wifiManagerWaitForNetwork(8000, serviceTick)) {
    strncpy(statusLine, "No DNS", sizeof(statusLine) - 1);
    statusLine[sizeof(statusLine) - 1] = '\0';
    refreshScreen();
    return false;
  }

  configTime(TIMEZONE_OFFSET_SEC, 0, NTP_SERVER_1, NTP_SERVER_2);
  const uint32_t start = millis();
  while (!timeKeeperIsValid() && millis() - start < 15000) {
    wifiManagerLoop(serviceTick);
    delay(250);
  }

  ntpSynced = timeKeeperIsValid();
  if (ntpSynced) {
    struct tm timeInfo {};
    const time_t now = time(nullptr);
    localtime_r(&now, &timeInfo);
    Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n", timeInfo.tm_year + 1900,
                  timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min,
                  timeInfo.tm_sec);
    timeKeeperPersist();
    strncpy(statusLine, "NTP synced", sizeof(statusLine) - 1);
    maybeStoreSample();
    pushFirebaseMeta(true);
    uiRefreshChart();
  } else {
    Serial.println("NTP sync timeout");
    strncpy(statusLine, "NTP failed", sizeof(statusLine) - 1);
  }
  statusLine[sizeof(statusLine) - 1] = '\0';
  refreshScreen();
  return ntpSynced;
}

void handleButton(ButtonEvent event) {
  const bool offline = !wifiManagerIsConnected() || wifiManagerPortalActive();

  if (offline) {
    switch (event) {
      case ButtonEvent::KeyLong:
      case ButtonEvent::KeyShort:
        wifiManagerStartPortal(serviceTick);
        return;
      case ButtonEvent::BootLong:
        wifiManagerResetPortal(serviceTick);
        return;
      default:
        break;
    }
  }

  switch (event) {
    case ButtonEvent::KeyShort:
      readSensor();
      maybeStoreSample();
      pushFirebaseMeta(true);
      uiRefreshChart();
      refreshScreen();
      break;
    case ButtonEvent::KeyLong:
      timeSyncAttempted = false;
      tryNtpSync();
      break;
    case ButtonEvent::BootShort:
      if (uiToggleChartYAxis()) {
        strncpy(statusLine, "Chart Y: auto", sizeof(statusLine) - 1);
      } else {
        strncpy(statusLine, "Chart Y: 20-45C", sizeof(statusLine) - 1);
      }
      statusLine[sizeof(statusLine) - 1] = '\0';
      refreshScreen();
      break;
    case ButtonEvent::BootLong:
      recordStoreClear();
      lastStoredSlot = 0;
      uiRefreshChart();
      strncpy(statusLine, "History cleared", sizeof(statusLine) - 1);
      statusLine[sizeof(statusLine) - 1] = '\0';
      refreshScreen();
      break;
    default:
      break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialStart = millis();
  while (!Serial && (millis() - serialStart < 5000)) {
    delay(10);
  }
  delay(200);
  Serial.println();
  Serial.println("=== Temp Monitor Phase 6 boot ===");

  boardPowerInit();
  delay(100);

  inputInit();
  recordStoreInit();
  timeKeeperInit();
  sdLoggerInit();

  hasSensor = sensorInit();
  Serial.printf("SHTC3: %s\n", hasSensor ? "OK" : "FAIL");

  if (!displayInit()) {
    Serial.println("Display init failed — rebuild with PSRAM=opi (see firmware/build.sh)");
  } else {
    uiInit();
    uiRefreshChart();
    Serial.println("Display OK");
  }

  batteryInit();
  readBattery();
  Serial.printf("Battery: %d%%\n", lastBatteryPct);

  if (hasSensor) {
    readSensor();
  }

  if (firebaseClientConfigured()) {
    Serial.println("Firebase: configured");
  } else {
    Serial.println("Firebase: disabled (copy secrets.h.example to secrets.h)");
  }

  strncpy(statusLine, "Connecting", sizeof(statusLine) - 1);
  statusLine[sizeof(statusLine) - 1] = '\0';
  refreshScreen();

  wifiManagerBegin(serviceTick);
  Serial.println("Setup complete");
}

void loop() {
  wifiManagerLoop(serviceTick);
  displayTick();

  static uint32_t lastBatteryMs = 0;
  if (millis() - lastBatteryMs >= BATTERY_READ_MS) {
    lastBatteryMs = millis();
    readBattery();
  }

  static uint32_t lastSensorMs = 0;
  if (millis() - lastSensorMs >= SENSOR_READ_MS) {
    lastSensorMs = millis();
    readSensor();
    maybeStoreSample();
    pushFirebaseMeta(false);
    uiRefreshChart();
    refreshScreen();
  }

  static uint32_t lastClockMs = 0;
  if (millis() - lastClockMs >= UI_CLOCK_MS) {
    lastClockMs = millis();
    maybeStoreSample();
    timeKeeperTick();
    if (displayReady()) {
      uiUpdate(wifiManagerIsConnected(), timeKeeperIsValid(), lastTempC, lastHumidity, hasSensor,
               lastBatteryPct, recordStoreCount(), recordStoreMax(), sdLoggerReady(), statusLine);
      displayRefreshFull();
    }
  }

  if (wifiManagerIsConnected() && !wifiManagerPortalActive() && !timeSyncAttempted) {
    timeSyncAttempted = true;
    tryNtpSync();
  }

  static uint32_t disconnectedSinceMs = 0;
  if (!wifiManagerPortalActive()) {
    if (!wifiManagerIsConnected()) {
      if (disconnectedSinceMs == 0) {
        disconnectedSinceMs = millis();
        timeSyncAttempted = false;
        if (timeKeeperIsValid()) {
          strncpy(statusLine, "Offline logging", sizeof(statusLine) - 1);
        } else {
          strncpy(statusLine, "No WiFi", sizeof(statusLine) - 1);
        }
        statusLine[sizeof(statusLine) - 1] = '\0';
      } else if (millis() - disconnectedSinceMs >= WIFI_DISCONNECT_PORTAL_MS) {
        disconnectedSinceMs = 0;
        wifiManagerStartPortal(serviceTick);
      }
    } else {
      if (disconnectedSinceMs != 0) {
        timeSyncAttempted = false;
        pushFirebaseMeta(true);
      }
      disconnectedSinceMs = 0;
    }
  } else {
    disconnectedSinceMs = 0;
    strncpy(statusLine, "WiFi setup", sizeof(statusLine) - 1);
    statusLine[sizeof(statusLine) - 1] = '\0';
  }

  if (const ButtonEvent event = inputPoll(); event != ButtonEvent::None) {
    handleButton(event);
  }

  static uint32_t lastSensorRetryMs = 0;
  if (!hasSensor && millis() - lastSensorRetryMs >= 30000) {
    lastSensorRetryMs = millis();
    if (sensorInit()) {
      readSensor();
      refreshScreen();
    }
  }

  delay(5);
}
