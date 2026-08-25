#include "wifi_manager.h"

#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace {
WiFiManager wm;
bool portalActive = false;
bool portalRequested = false;
bool resetBeforePortal = false;

enum class Phase { Init, StaConnect, Portal, Online };
Phase phase = Phase::Init;
uint32_t phaseStartedMs = 0;

constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;

void configurePortalDefaults() {
  wm.setConfigPortalBlocking(false);
  wm.setCaptivePortalEnable(true);
  wm.setTitle("Temp Monitor");
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);
  wm.setConfigPortalTimeout(0);
  wm.setBreakAfterConfig(true);
  wm.setRestorePersistent(true);
  wm.setEnableConfigPortal(true);
}

void stopPortalIfActive() {
  if (wm.getConfigPortalActive()) {
    wm.stopConfigPortal();
  }
}

void onStaConnected() {
  stopPortalIfActive();
  portalActive = false;
  portalRequested = false;
  phase = Phase::Online;
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
  Serial.print("WiFi ready: ");
  Serial.println(WiFi.SSID());
}

void openPortal(bool resetSettings) {
  if (resetSettings) {
    wm.resetSettings();
  }

  stopPortalIfActive();
  WiFi.disconnect(true, true);
  delay(200);

  configurePortalDefaults();
  portalActive = true;
  portalRequested = false;
  phase = Phase::Portal;
  phaseStartedMs = millis();

  WiFi.mode(WIFI_AP_STA);
  if (!wm.startConfigPortal(WIFI_PORTAL_SSID)) {
    Serial.println("WiFiManager portal failed to start");
  }

  Serial.print("WiFi portal active: ");
  Serial.println(WIFI_PORTAL_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void beginStaConnect() {
  configurePortalDefaults();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (!wm.getWiFiIsSaved()) {
    Serial.println("No saved WiFi — opening setup portal");
    openPortal(false);
    return;
  }

  const String ssid = wm.getWiFiSSID();
  const String pass = wm.getWiFiPass();
  Serial.print("Connecting to saved WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());
  phase = Phase::StaConnect;
  phaseStartedMs = millis();
}

void servicePortal(WifiUiTickFn tick) {
  wm.process();
  portalActive = wm.getConfigPortalActive() || phase == Phase::Portal;
  if (tick) tick();

  if (phase == Phase::Portal && WiFi.status() == WL_CONNECTED && !wm.getConfigPortalActive()) {
    onStaConnected();
  }
}
}  // namespace

bool wifiManagerBegin(WifiUiTickFn tick) {
  (void)tick;
  beginStaConnect();
  return true;
}

void wifiManagerLoop(WifiUiTickFn tick) {
  if (portalRequested) {
    openPortal(resetBeforePortal);
    portalRequested = false;
  }

  switch (phase) {
    case Phase::Init:
      break;

    case Phase::StaConnect:
      if (tick) tick();
      if (WiFi.status() == WL_CONNECTED) {
        onStaConnected();
        break;
      }
      if (millis() - phaseStartedMs >= CONNECT_TIMEOUT_MS) {
        Serial.println("Saved WiFi connect timeout — opening portal");
        openPortal(false);
      }
      break;

    case Phase::Portal:
      servicePortal(tick);
      break;

    case Phase::Online:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost — reconnecting");
        phase = Phase::StaConnect;
        phaseStartedMs = millis();
        WiFi.reconnect();
      }
      break;
  }

  if (wm.getConfigPortalActive()) {
    portalActive = true;
    if (phase != Phase::Portal) {
      phase = Phase::Portal;
    }
  }
}

bool wifiManagerIsConnected() { return WiFi.status() == WL_CONNECTED; }

bool wifiManagerPortalActive() { return portalActive || wm.getConfigPortalActive(); }

bool wifiManagerHasSavedCredentials() {
  configurePortalDefaults();
  return wm.getWiFiIsSaved();
}

void wifiManagerStartPortal(WifiUiTickFn tick) {
  (void)tick;
  resetBeforePortal = false;
  portalRequested = true;
}

void wifiManagerResetPortal(WifiUiTickFn tick) {
  (void)tick;
  resetBeforePortal = true;
  portalRequested = true;
}

bool wifiManagerWaitForNetwork(uint32_t timeoutMs, WifiUiTickFn tick) {
  const uint32_t start = millis();
  IPAddress resolved;

  while (millis() - start < timeoutMs) {
    wifiManagerLoop(tick);

    if (WiFi.status() != WL_CONNECTED) {
      delay(50);
      continue;
    }

    if (WiFi.hostByName(NTP_SERVER_1, resolved) || WiFi.hostByName(NTP_SERVER_2, resolved)) {
      return true;
    }

    delay(250);
  }

  return false;
}

const char* wifiManagerPortalSsid() { return WIFI_PORTAL_SSID; }
