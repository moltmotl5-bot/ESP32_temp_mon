#include "sd_logger.h"

#include "config.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <WString.h>
#include <stdio.h>
#include <time.h>

namespace {
bool sdMounted = false;
bool headerWritten = false;

constexpr const char* LOG_PATH = "/temp_log.csv";
constexpr const char* CSV_HEADER = "timestamp,datetime,temp_c,humidity_pct\n";
constexpr size_t SD_TAIL_READ_BYTES = 480 * 1024;

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

bool parseCsvLine(const char* line, uint32_t* outTs, float* outTemp, float* outHum) {
  if (!line || !outTs || !outTemp || !outHum) return false;

  unsigned long ts = 0;
  float temp = 0.0f;
  float hum = 0.0f;
  if (sscanf(line, "%lu,%*[^,],%f,%f", &ts, &temp, &hum) != 3) {
    return false;
  }
  if (ts < 1700000000UL || temp <= 0.0f) {
    return false;
  }

  *outTs = static_cast<uint32_t>(ts);
  *outTemp = temp;
  *outHum = hum;
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

bool sdLoggerBuildReadingsJson(String& outJson) {
  if (!sdMounted || !SD_MMC.exists(LOG_PATH)) {
    return false;
  }

  File file = SD_MMC.open(LOG_PATH, FILE_READ);
  if (!file) {
    return false;
  }

  const size_t fileSize = file.size();
  if (fileSize <= strlen(CSV_HEADER)) {
    file.close();
    return false;
  }

  if (fileSize > SD_TAIL_READ_BYTES) {
    file.seek(fileSize - SD_TAIL_READ_BYTES);
    file.readStringUntil('\n');
  } else {
    file.readStringUntil('\n');
  }

  outJson = "{\"source\":\"sd\",\"points\":[";
  outJson.reserve(fileSize + 32U);

  bool first = true;
  bool hasData = false;
  char line[128];

  while (file.available()) {
    const int lineLen = file.readBytesUntil('\n', line, sizeof(line) - 1);
    if (lineLen <= 0) {
      continue;
    }
    line[lineLen] = '\0';

    uint32_t ts = 0;
    float temp = 0.0f;
    float hum = 0.0f;
    if (!parseCsvLine(line, &ts, &temp, &hum)) {
      continue;
    }

    if (!first) {
      outJson += ',';
    }
    first = false;
    hasData = true;

    char item[72];
    snprintf(item, sizeof(item), "{\"ts\":%lu,\"temp\":%.1f,\"hum\":%.1f}",
             static_cast<unsigned long>(ts), temp, hum);
    outJson += item;
  }

  file.close();
  if (!hasData) {
    outJson = "";
    return false;
  }

  outJson += "]}";
  Serial.printf("SD logger: exported %u bytes for web readings\n",
                static_cast<unsigned>(outJson.length()));
  return true;
}
