#include "board.h"

#include "config.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
bool sensorReady = false;
bool batteryReady = false;
float batteryFilteredPct = -1.0f;

constexpr uint16_t SHTC3_PRODUCT_CODE = 0x0807;
constexpr uint16_t SHTC3_PRODUCT_MASK = 0x083F;

bool isShtc3ProductId(uint16_t id) { return (id & SHTC3_PRODUCT_MASK) == SHTC3_PRODUCT_CODE; }

bool writeCommand(uint16_t cmd) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write(static_cast<uint8_t>(cmd >> 8));
  Wire.write(static_cast<uint8_t>(cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

bool probeSensor(uint16_t* idOut = nullptr) {
  if (!writeCommand(0x3517)) return false;  // wake
  delay(2);
  if (!writeCommand(0xEFC8)) return false;  // read ID
  if (Wire.requestFrom(SHTC3_ADDR, static_cast<uint8_t>(3)) != 3) return false;
  const uint8_t idMsb = Wire.read();
  const uint8_t idLsb = Wire.read();
  Wire.read();  // CRC
  writeCommand(0xB098);  // sleep
  const uint16_t id = static_cast<uint16_t>((idMsb << 8) | idLsb);
  if (idOut) *idOut = id;
  // SHTC3 IDs vary per unit (e.g. 0x0887); only product-code bits are fixed.
  return isShtc3ProductId(id);
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
  Wire.setClock(100000);
  delay(50);  // allow SHTC3 to stabilize after power hold

  sensorReady = false;
  for (uint8_t attempt = 0; attempt < 5; ++attempt) {
    uint16_t id = 0;
    if (probeSensor(&id)) {
      sensorReady = true;
      Wire.setClock(400000);
      Serial.printf("SHTC3 probe OK (id=0x%04X)\n", id);
      return true;
    }
    Serial.printf("SHTC3 probe retry %u (id=0x%04X)\n", attempt + 1, id);
    delay(100);
  }

  Wire.setClock(400000);
  Serial.println("SHTC3 probe FAILED");
  return false;
}

bool sensorRead(float* tempC, float* humidityPct) {
  if (!tempC || !humidityPct) return false;

  if (!sensorReady && !sensorInit()) return false;

  if (!writeCommand(0x3517)) return false;  // wake
  delay(2);
  if (!writeCommand(0x7CA2)) return false;  // measure T first, normal mode (same as BUS-ETA)
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

bool sensorReadyNow() { return sensorReady; }
