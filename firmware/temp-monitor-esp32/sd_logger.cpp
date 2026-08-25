#include "sd_logger.h"

#include "config.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <time.h>

namespace {
bool sdMounted = false;
bool headerWritten = false;

constexpr const char* LOG_PATH = "/temp_log.csv";
constexpr const char* CSV_HEADER = "timestamp,datetime,temp_c,humidity_pct\n";

bool ensureHeader() {
  if (headerWritten) return true;

  if (SD_MMC.exists(LOG_PATH)) {
    headerWritten = true;
    return true;
  }

  File file = SD_MMC.open(LOG_PATH, FILE_WRITE);
  if (!file) {
    Serial.println("SD logger: failed to create log file");
    return false;
  }

  const size_t written = file.print(CSV_HEADER);
  file.close();
  if (written != strlen(CSV_HEADER)) {
    Serial.println("SD logger: failed to write CSV header");
    return false;
  }

  headerWritten = true;
  Serial.println("SD logger: created temp_log.csv");
  return true;
}
}  // namespace

void sdLoggerInit() {
  if (sdMounted) return;

  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("SD logger: setPins failed");
    return;
  }

  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD logger: mount failed (no card?)");
    return;
  }

  sdMounted = true;
  Serial.printf("SD logger: mounted (%llu MB)\n",
                static_cast<unsigned long long>(SD_MMC.cardSize()) / (1024ULL * 1024ULL));

  if (!ensureHeader()) {
    sdMounted = false;
    SD_MMC.end();
  }
}

bool sdLoggerReady() { return sdMounted; }

bool sdLoggerAppend(uint32_t timestamp, float tempC, float humPct) {
  if (!sdMounted) return false;
  if (!ensureHeader()) return false;

  struct tm timeInfo {};
  const time_t ts = static_cast<time_t>(timestamp);
  if (!localtime_r(&ts, &timeInfo)) return false;

  char line[96];
  const int len = snprintf(line, sizeof(line), "%lu,%04d-%02d-%02d %02d:%02d:%02d,%.1f,%.1f\n",
                           static_cast<unsigned long>(timestamp), timeInfo.tm_year + 1900,
                           timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min,
                           timeInfo.tm_sec, tempC, humPct);
  if (len <= 0 || static_cast<size_t>(len) >= sizeof(line)) return false;

  File file = SD_MMC.open(LOG_PATH, FILE_APPEND);
  if (!file) {
    Serial.println("SD logger: open for append failed");
    sdMounted = false;
    headerWritten = false;
    return false;
  }

  const size_t written = file.write(reinterpret_cast<const uint8_t*>(line),
                                    static_cast<size_t>(len));
  file.close();

  if (written != static_cast<size_t>(len)) {
    Serial.println("SD logger: write incomplete");
    return false;
  }

  Serial.printf("SD log: ts=%lu %.1f C %.1f %%\n", static_cast<unsigned long>(timestamp), tempC,
                humPct);
  return true;
}
