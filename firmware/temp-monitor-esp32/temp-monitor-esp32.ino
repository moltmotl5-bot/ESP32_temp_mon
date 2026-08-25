/**
 * Temp Monitor — Waveshare ESP32-S3-RLCD-4.2
 * Phase 4: SD card long-term CSV logging (alongside NVS)
 */

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "board.h"
#include "config.h"
#include "display_epaper.h"
#include "input_buttons.h"
#include "record_store.h"
#include "sd_logger.h"
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
constexpr uint32_t NTP_EPOCH_MIN = 1700000000;

bool timeIsValid() {
  const time_t now = time(nullptr);
  return now >= static_cast<time_t>(NTP_EPOCH_MIN);
}

void refreshScreen() {
  if (!displayReady()) return;
  uiUpdate(wifiManagerIsConnected(), timeIsValid(), lastTempC, lastHumidity, hasSensor, lastBatteryPct,
           recordStoreCount(), recordStoreMax(), sdLoggerReady(), statusLine);
  displayRefreshFull();
}

void serviceTick() {
  displayTick();
  if (!displayReady()) return;
  uiUpdate(wifiManagerIsConnected(), timeIsValid(), lastTempC, lastHumidity, hasSensor, lastBatteryPct,
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
  if (!hasSensor || !timeIsValid()) return;

  const time_t now = time(nullptr);
  const time_t slot = now - (now % SAMPLE_INTERVAL_SEC);
  if (slot == lastStoredSlot) return;

  recordStoreAppend(static_cast<uint32_t>(slot), lastTempC, lastHumidity);
  lastStoredSlot = slot;
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
  while (time(nullptr) < NTP_EPOCH_MIN && millis() - start < 15000) {
    wifiManagerLoop(serviceTick);
    delay(250);
  }

  ntpSynced = timeIsValid();
  if (ntpSynced) {
    struct tm timeInfo {};
    const time_t now = time(nullptr);
    localtime_r(&now, &timeInfo);
    Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n", timeInfo.tm_year + 1900,
                  timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min,
                  timeInfo.tm_sec);
    strncpy(statusLine, "NTP synced", sizeof(statusLine) - 1);
    maybeStoreSample();
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
  Serial.println("=== Temp Monitor Phase 4 boot ===");

  boardPowerInit();
  delay(100);

  inputInit();
  recordStoreInit();
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
    uiRefreshChart();
    refreshScreen();
  }

  static uint32_t lastClockMs = 0;
  if (millis() - lastClockMs >= UI_CLOCK_MS) {
    lastClockMs = millis();
    maybeStoreSample();
    if (displayReady()) {
      uiUpdate(wifiManagerIsConnected(), timeIsValid(), lastTempC, lastHumidity, hasSensor,
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
        if (timeIsValid()) {
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
