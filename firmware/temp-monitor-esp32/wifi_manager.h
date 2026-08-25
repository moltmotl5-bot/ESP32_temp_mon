#pragma once

#include <stdint.h>

using WifiUiTickFn = void (*)();

bool wifiManagerBegin(WifiUiTickFn tick);
void wifiManagerLoop(WifiUiTickFn tick);

bool wifiManagerIsConnected();
bool wifiManagerPortalActive();

void wifiManagerStartPortal(WifiUiTickFn tick);
void wifiManagerResetPortal(WifiUiTickFn tick);

bool wifiManagerWaitForNetwork(uint32_t timeoutMs, WifiUiTickFn tick);
bool wifiManagerHasSavedCredentials();

const char* wifiManagerPortalSsid();
