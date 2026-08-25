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

// TF/microSD — Waveshare 06_SD_Card example (SDMMC 1-bit)
constexpr uint8_t SD_MMC_CLK = 38;
constexpr uint8_t SD_MMC_CMD = 21;
constexpr uint8_t SD_MMC_D0 = 39;

// Phase 2: 3-day history @ 5-minute intervals
constexpr uint32_t SAMPLE_INTERVAL_SEC = 300;  // 5 min
constexpr uint16_t RECORDS_PER_DAY = 288;      // 24 * 60 / 5
constexpr uint16_t RECORD_DAYS = 3;
constexpr uint16_t RECORD_MAX = RECORDS_PER_DAY * RECORD_DAYS;  // 864
constexpr uint16_t CHART_POINTS = 144;         // 12 hours @ 5 min

// Chart Y-axis (°C)
constexpr int16_t CHART_Y_MIN = 20;
constexpr int16_t CHART_Y_MAX = 45;

constexpr const char* WIFI_PORTAL_SSID = "TempMon-Setup";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_SEC = 20;

// NTP (UTC+8 Hong Kong)
constexpr int32_t TIMEZONE_OFFSET_SEC = 8 * 3600;
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
