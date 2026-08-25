#pragma once

#include <stdint.h>

// Waveshare ESP32-S3-RLCD-4.2 (ST7305 400×300 reflective LCD)
// https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice
constexpr uint8_t LCD_CS = 40;
constexpr uint8_t LCD_DC = 5;
constexpr uint8_t LCD_RST = 41;
constexpr uint8_t LCD_SCK = 11;
constexpr uint8_t LCD_MOSI = 12;

constexpr uint16_t SCREEN_W = 400;
constexpr uint16_t SCREEN_H = 300;

// Onboard I2C (SHTC3 @ 0x70)
constexpr uint8_t I2C_SDA = 13;
constexpr uint8_t I2C_SCL = 14;
constexpr uint8_t SHTC3_ADDR = 0x70;

// User buttons (active LOW)
constexpr uint8_t BTN_BOOT = 0;
constexpr uint8_t BTN_KEY = 18;

// Battery power hold — must stay HIGH when running on battery
constexpr uint8_t PIN_POWER_HOLD = 17;

constexpr uint8_t PIN_BATTERY_ADC = 4;
constexpr float BATTERY_VOLTAGE_EMPTY = 3.0f;
constexpr float BATTERY_VOLTAGE_FULL = 4.2f;
constexpr float BATTERY_VOLTAGE_DIVIDER = 3.0f;

constexpr uint32_t LONG_PRESS_MS = 800;
constexpr uint32_t SENSOR_READ_MS = 60000;
constexpr uint32_t BATTERY_READ_MS = 2000;
constexpr uint32_t UI_CLOCK_MS = 1000;

constexpr const char* WIFI_PORTAL_SSID = "TempMon-Setup";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_SEC = 20;

// NTP (UTC+8 Hong Kong)
constexpr int32_t TIMEZONE_OFFSET_SEC = 8 * 3600;
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
