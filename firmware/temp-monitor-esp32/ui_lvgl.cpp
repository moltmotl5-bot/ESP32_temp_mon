#include "ui_lvgl.h"

#include "config.h"
#include "record_store.h"
#include "wifi_manager.h"

#include <stdio.h>
#include <time.h>

namespace {
lv_obj_t* dateLabel = nullptr;
lv_obj_t* timeLabel = nullptr;
lv_obj_t* batteryLabel = nullptr;
lv_obj_t* tempLabel = nullptr;
lv_obj_t* humLabel = nullptr;
lv_obj_t* statusLabel = nullptr;
lv_obj_t* footerLabel = nullptr;
lv_obj_t* chartPanel = nullptr;
lv_obj_t* tempChart = nullptr;
lv_chart_series_t* tempSeries = nullptr;
lv_obj_t* chartModeLabel = nullptr;
lv_obj_t* yAxisLabels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

bool chartYAuto = false;
lv_coord_t chartYValues[CHART_POINTS];

constexpr int CHART_LEFT = 34;
constexpr int CHART_TOP = 24;
constexpr int CHART_W = SCREEN_W - 20 - CHART_LEFT;
constexpr int CHART_H = 108;
constexpr int Y_TICK_COUNT = 5;
constexpr int Y_TICKS_FIXED[Y_TICK_COUNT] = {45, 40, 35, 30, 20};

const lv_font_t* fontAscii() { return &lv_font_montserrat_14; }

const char* batterySymbol(int8_t batteryPct) {
  if (batteryPct < 0) return LV_SYMBOL_BATTERY_EMPTY;
  if (batteryPct >= 80) return LV_SYMBOL_BATTERY_FULL;
  if (batteryPct >= 60) return LV_SYMBOL_BATTERY_3;
  if (batteryPct >= 40) return LV_SYMBOL_BATTERY_2;
  if (batteryPct >= 15) return LV_SYMBOL_BATTERY_1;
  return LV_SYMBOL_BATTERY_EMPTY;
}

void setTextColor(lv_obj_t* label, bool whiteText) {
  lv_obj_set_style_text_color(label, whiteText ? lv_color_white() : lv_color_black(), 0);
}

void stylePanel(lv_obj_t* obj, bool inverted) {
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_border_width(obj, inverted ? 3 : 2, 0);
  lv_obj_set_style_border_color(obj, lv_color_black(), 0);
  lv_obj_set_style_bg_color(obj, inverted ? lv_color_black() : lv_color_white(), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

lv_obj_t* makeAsciiLabel(lv_obj_t* parent, int x, int y, int w) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, w);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, fontAscii(), 0);
  setTextColor(label, false);
  return label;
}

void updateClockLabels(bool ntpSynced) {
  if (!dateLabel || !timeLabel) return;

  const time_t now = time(nullptr);
  struct tm timeInfo {};
  if (ntpSynced && now >= 1700000000 && localtime_r(&now, &timeInfo)) {
    lv_label_set_text_fmt(dateLabel, "%04d-%02d-%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1,
                          timeInfo.tm_mday);
    lv_label_set_text_fmt(timeLabel, "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min,
                          timeInfo.tm_sec);
  } else {
    lv_label_set_text(dateLabel, "---- -- --");
    lv_label_set_text(timeLabel, "--:--:--");
  }
}

void updateYAxisLabels(lv_coord_t yMin, lv_coord_t yMax) {
  for (int i = 0; i < Y_TICK_COUNT; ++i) {
    if (!yAxisLabels[i]) continue;

    int tickValue = 0;
    if (chartYAuto) {
      const float ratio = static_cast<float>(Y_TICK_COUNT - 1 - i) / static_cast<float>(Y_TICK_COUNT - 1);
      tickValue = static_cast<int>(yMin + ratio * (yMax - yMin) + 0.5f);
    } else {
      tickValue = Y_TICKS_FIXED[i];
    }

    lv_label_set_text_fmt(yAxisLabels[i], "%d", tickValue);
    const int y = CHART_TOP + (i * CHART_H) / (Y_TICK_COUNT - 1) - 6;
    lv_obj_set_pos(yAxisLabels[i], 4, y);
  }
}

void createHeader() {
  lv_obj_t* header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, SCREEN_W, 36);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  stylePanel(header, false);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "Temp Mon");
  lv_obj_set_style_text_font(title, fontAscii(), 0);
  setTextColor(title, false);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 6, 0);

  batteryLabel = lv_label_create(header);
  lv_label_set_text(batteryLabel, LV_SYMBOL_BATTERY_FULL);
  lv_obj_set_style_text_font(batteryLabel, fontAscii(), 0);
  setTextColor(batteryLabel, false);
  lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID, -4, 0);

  dateLabel = makeAsciiLabel(header, 88, 2, 110);
  timeLabel = makeAsciiLabel(header, 200, 2, 96);
  lv_obj_set_style_text_align(timeLabel, LV_TEXT_ALIGN_RIGHT, 0);
}

void createValueRow() {
  lv_obj_t* panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, SCREEN_W, 44);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 36);
  stylePanel(panel, false);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  tempLabel = makeAsciiLabel(panel, 16, 10, 170);
  lv_label_set_text(tempLabel, "T: --.- C");

  humLabel = makeAsciiLabel(panel, 210, 10, 180);
  lv_label_set_text(humLabel, "H: --.- %");
}

void createChartPanel() {
  chartPanel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(chartPanel, SCREEN_W, 152);
  lv_obj_align(chartPanel, LV_ALIGN_TOP_MID, 0, 80);
  stylePanel(chartPanel, false);
  lv_obj_clear_flag(chartPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = makeAsciiLabel(chartPanel, 8, 4, 120);
  lv_label_set_text(title, "12h Temp");

  chartModeLabel = makeAsciiLabel(chartPanel, 280, 4, 110);
  lv_label_set_text(chartModeLabel, "20-45C");
  lv_obj_set_style_text_align(chartModeLabel, LV_TEXT_ALIGN_RIGHT, 0);

  for (int i = 0; i < Y_TICK_COUNT; ++i) {
    yAxisLabels[i] = makeAsciiLabel(chartPanel, 4, CHART_TOP, 28);
    lv_label_set_text(yAxisLabels[i], "--");
    lv_obj_set_style_text_align(yAxisLabels[i], LV_TEXT_ALIGN_RIGHT, 0);
  }

  tempChart = lv_chart_create(chartPanel);
  lv_obj_set_size(tempChart, CHART_W, CHART_H);
  lv_obj_set_pos(tempChart, CHART_LEFT, CHART_TOP);
  lv_chart_set_type(tempChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(tempChart, CHART_POINTS);
  lv_chart_set_range(tempChart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN, CHART_Y_MAX);
  lv_chart_set_div_line_count(tempChart, 4, 6);
  lv_obj_set_style_size(tempChart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(tempChart, 2, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(tempChart, lv_color_white(), 0);
  lv_obj_set_style_border_color(tempChart, lv_color_black(), 0);
  lv_obj_set_style_border_width(tempChart, 1, 0);

  tempSeries = lv_chart_add_series(tempChart, lv_color_black(), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_ext_y_array(tempChart, tempSeries, chartYValues);

  lv_obj_t* leftMark = makeAsciiLabel(chartPanel, CHART_LEFT, 136, 60);
  lv_label_set_text(leftMark, "-12h");

  lv_obj_t* rightMark = makeAsciiLabel(chartPanel, SCREEN_W - 68, 136, 60);
  lv_label_set_text(rightMark, "now");
  lv_obj_set_style_text_align(rightMark, LV_TEXT_ALIGN_RIGHT, 0);
}

void createStatusBar() {
  statusLabel = makeAsciiLabel(lv_scr_act(), 6, 236, SCREEN_W - 12);
  footerLabel = makeAsciiLabel(lv_scr_act(), 0, 0, SCREEN_W - 8);
  lv_obj_align(footerLabel, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void fillChartYRange(float* yMinOut, float* yMaxOut, const TempRecord* recs, uint16_t count) {
  if (!chartYAuto) {
    if (yMinOut) *yMinOut = static_cast<float>(CHART_Y_MIN);
    if (yMaxOut) *yMaxOut = static_cast<float>(CHART_Y_MAX);
    return;
  }

  float minT = 999.0f;
  float maxT = -999.0f;

  if (count > 0) {
    for (uint16_t i = 0; i < count; ++i) {
      if (recs[i].temperature < minT) minT = recs[i].temperature;
      if (recs[i].temperature > maxT) maxT = recs[i].temperature;
    }
  }

  if (count == 0) {
    if (yMinOut) *yMinOut = static_cast<float>(CHART_Y_MIN);
    if (yMaxOut) *yMaxOut = static_cast<float>(CHART_Y_MAX);
    return;
  }

  minT -= 2.0f;
  maxT += 2.0f;
  if (minT < 0.0f) minT = 0.0f;
  if (maxT <= minT) maxT = minT + 5.0f;

  if (yMinOut) *yMinOut = minT;
  if (yMaxOut) *yMaxOut = maxT;
}
}  // namespace

void uiInit() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

  createHeader();
  createValueRow();
  createChartPanel();
  createStatusBar();

  for (uint16_t i = 0; i < CHART_POINTS; ++i) {
    chartYValues[i] = LV_CHART_POINT_NONE;
  }
  uiRefreshChart();
}

void uiRefreshChart() {
  if (!tempChart || !tempSeries) return;

  TempRecord recs[CHART_POINTS];
  const uint16_t count = recordStoreCopyRecent(CHART_POINTS, recs);
  const uint16_t pad = count < CHART_POINTS ? CHART_POINTS - count : 0;

  float yMin = static_cast<float>(CHART_Y_MIN);
  float yMax = static_cast<float>(CHART_Y_MAX);
  fillChartYRange(&yMin, &yMax, recs, count);

  const lv_coord_t yMinCoord = static_cast<lv_coord_t>(yMin);
  const lv_coord_t yMaxCoord = static_cast<lv_coord_t>(yMax);
  lv_chart_set_range(tempChart, LV_CHART_AXIS_PRIMARY_Y, yMinCoord, yMaxCoord);

  for (uint16_t i = 0; i < pad; ++i) {
    chartYValues[i] = LV_CHART_POINT_NONE;
  }
  for (uint16_t i = 0; i < count; ++i) {
    chartYValues[pad + i] = static_cast<lv_coord_t>(recs[i].temperature + 0.5f);
  }

  lv_chart_refresh(tempChart);
  updateYAxisLabels(yMinCoord, yMaxCoord);

  if (chartModeLabel) {
    if (chartYAuto) {
      lv_label_set_text_fmt(chartModeLabel, "auto %d-%dC", static_cast<int>(yMin),
                            static_cast<int>(yMax));
    } else {
      lv_label_set_text(chartModeLabel, "20-45C");
    }
  }
}

bool uiToggleChartYAxis() {
  chartYAuto = !chartYAuto;
  uiRefreshChart();
  return chartYAuto;
}

void uiUpdate(bool wifiConnected, bool ntpSynced, float tempC, float humidityPct, bool hasSensor,
              int8_t batteryPct, uint16_t recordCount, uint16_t recordMax, bool sdReady,
              const char* statusLine) {
  updateClockLabels(ntpSynced);

  if (batteryLabel) {
    lv_label_set_text(batteryLabel, batterySymbol(batteryPct));
  }

  if (tempLabel) {
    if (hasSensor) {
      lv_label_set_text_fmt(tempLabel, "T: %.1f C", tempC);
    } else {
      lv_label_set_text(tempLabel, "T: --.- C");
    }
  }

  if (humLabel) {
    if (hasSensor) {
      lv_label_set_text_fmt(humLabel, "H: %.1f %%", humidityPct);
    } else {
      lv_label_set_text(humLabel, "H: --.- %");
    }
  }

  if (statusLabel) {
    char line[128];
    snprintf(line, sizeof(line), "%s | WiFi:%s | NTP:%s | SD:%s | SHTC3:%s | Rec:%u/%u",
             statusLine ? statusLine : "", wifiConnected ? "OK" : "--", ntpSynced ? "OK" : "--",
             sdReady ? "OK" : "--", hasSensor ? "OK" : "ERR", recordCount, recordMax);
    lv_label_set_text(statusLabel, line);
  }

  if (footerLabel) {
    if (wifiManagerPortalActive() || !wifiConnected) {
      lv_label_set_text(footerLabel, "Join TempMon-Setup | KEY: setup | BOOT: reset");
    } else {
      lv_label_set_text(footerLabel, "KEY:read BOOT:auto-Y BOOTlong:clear");
    }
  }
}
