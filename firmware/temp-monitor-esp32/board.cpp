#include "board.h"

#include "config.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
bool sensorReady = false;
bool batteryReady = false;
float batteryFilteredPct = -1.0f;

bool writeCommand(uint16_t cmd) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write(static_cast<uint8_t>(cmd >> 8));
  Wire.write(static_cast<uint8_t>(cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

bool probeSensor() {
  if (!writeCommand(0x3517)) return false;  // wake
  delay(1);
  if (!writeCommand(0xEFC8)) return false;  // read ID
  if (Wire.requestFrom(SHTC3_ADDR, static_cast<uint8_t>(3)) != 3) return false;
  const uint8_t idMsb = Wire.read();
  const uint8_t idLsb = Wire.read();
  Wire.read();  // CRC
  writeCommand(0xB098);  // sleep
  return idMsb == 0x08 && idLsb == 0x07;
}
}  // namespace

void boardPowerInit() {
  pinMode(PIN_POWER_HOLD, OUTPUT);
  digitalWrite(PIN_POWER_HOLD, HIGH);
  delay(10);
}

bool batteryInit() {
  pinMode(PIN_BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
  batteryReady = true;
  return true;
}

bool batteryRead(uint8_t* pctOut) {
  if (!batteryReady || !pctOut) return false;

  uint32_t totalMv = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    totalMv += analogReadMilliVolts(PIN_BATTERY_ADC);
    delay(1);
  }
  const float voltage =
      (static_cast<float>(totalMv) / 8.0f) * BATTERY_VOLTAGE_DIVIDER / 1000.0f;

  float pct = (voltage - BATTERY_VOLTAGE_EMPTY) /
              (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100.0f;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;

  if (batteryFilteredPct < 0.0f) {
    batteryFilteredPct = pct;
  } else {
    batteryFilteredPct = batteryFilteredPct * 0.7f + pct * 0.3f;
  }

  *pctOut = static_cast<uint8_t>(batteryFilteredPct + 0.5f);

  static uint8_t logCount = 0;
  static uint32_t lastLogMs = 0;
  const uint32_t now = millis();
  if (logCount < 5 || now - lastLogMs >= 10000) {
    logCount++;
    lastLogMs = now;
    Serial.printf("Battery: raw=%lumV cell=%.2fV -> %u%%\n", totalMv / 8, voltage, *pctOut);
  }

  return true;
}

bool sensorInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  sensorReady = probeSensor();
  return sensorReady;
}

bool sensorRead(float* tempC, float* humidityPct) {
  if (!sensorReady || !tempC || !humidityPct) return false;

  if (!writeCommand(0x3517)) return false;  // wake
  delay(1);
  if (!writeCommand(0x7CA2)) return false;  // measure T first, normal mode
  delay(15);

  if (Wire.requestFrom(SHTC3_ADDR, static_cast<uint8_t>(6)) != 6) {
    writeCommand(0xB098);
    return false;
  }

  const uint16_t rawTemp = static_cast<uint16_t>((Wire.read() << 8) | Wire.read());
  Wire.read();  // temp CRC
  const uint16_t rawHum = static_cast<uint16_t>((Wire.read() << 8) | Wire.read());
  Wire.read();  // humidity CRC

  writeCommand(0xB098);  // sleep

  *tempC = -45.0f + 175.0f * (static_cast<float>(rawTemp) / 65535.0f);
  *humidityPct = 100.0f * (static_cast<float>(rawHum) / 65535.0f);
  return true;
}
