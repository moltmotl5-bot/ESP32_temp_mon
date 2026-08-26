#include "firebase_client.h"

#include "config.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <stdio.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.h.example"
#warning "Copy secrets.h.example to secrets.h and set FIREBASE_DEVICE_KEY"
#endif

extern "C" {
#include "esp_crt_bundle.h"
}

namespace {
constexpr uint32_t META_PUSH_INTERVAL_MS = 30000;
uint32_t lastMetaPushMs = 0;

bool configured() {
  return FIREBASE_DEVICE_KEY[0] != '\0' &&
         strcmp(FIREBASE_DEVICE_KEY, "REPLACE_WITH_YOUR_DEVICE_WRITE_SECRET") != 0;
}

bool putJson(const char* pathSuffix, const char* jsonBody) {
  if (!configured() || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClientSecure client;
  client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end);

  HTTPClient http;
  char url[192];
  snprintf(url, sizeof(url), "https://%s%s", FIREBASE_DB_HOST, pathSuffix);
  if (!http.begin(client, url)) {
    Serial.println("Firebase: http.begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  const int code = http.PUT(jsonBody);
  const size_t respLen = http.getSize();
  if (code < 200 || code >= 300) {
    Serial.printf("Firebase: PUT %s failed (%d)\n", pathSuffix, code);
    if (respLen > 0 && respLen < 256) {
      Serial.println(http.getString());
    }
    http.end();
    return false;
  }

  http.end();
  return true;
}
}  // namespace

bool firebaseClientConfigured() { return configured(); }

bool firebaseClientPushReading(uint32_t timestamp, float tempC, float humPct) {
  char path[128];
  snprintf(path, sizeof(path), "/devices/%s/readings/%lu.json", FIREBASE_DEVICE_ID,
           static_cast<unsigned long>(timestamp));

  char body[160];
  snprintf(body, sizeof(body), "{\"temp\":%.1f,\"hum\":%.1f,\"_deviceKey\":\"%s\"}", tempC, humPct,
           FIREBASE_DEVICE_KEY);

  if (!putJson(path, body)) {
    return false;
  }

  Serial.printf("Firebase: reading ts=%lu\n", static_cast<unsigned long>(timestamp));
  return true;
}

bool firebaseClientPushMeta(const FirebaseMetaState& state, bool force) {
  if (!force && millis() - lastMetaPushMs < META_PUSH_INTERVAL_MS) {
    return false;
  }

  char path[96];
  snprintf(path, sizeof(path), "/devices/%s/meta.json", FIREBASE_DEVICE_ID);

  const time_t now = time(nullptr);
  char body[320];
  snprintf(body, sizeof(body),
           "{\"temp\":%.1f,\"hum\":%.1f,\"battery\":%d,\"clock\":\"%s\",\"sd\":%s,"
           "\"records\":%u,\"recordsMax\":%u,\"updatedAt\":%lu,\"_deviceKey\":\"%s\"}",
           state.tempC, state.humidity, static_cast<int>(state.batteryPct), state.clockLine,
           state.sdReady ? "true" : "false", state.recordCount, state.recordMax,
           static_cast<unsigned long>(now >= 0 ? now : 0), FIREBASE_DEVICE_KEY);

  if (!putJson(path, body)) {
    return false;
  }

  lastMetaPushMs = millis();
  Serial.println("Firebase: meta updated");
  return true;
}

void firebaseClientLoop() {
  (void)0;
}
