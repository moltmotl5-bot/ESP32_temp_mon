#include "ui_lvgl.h"

#include "config.h"
#include "font_source_han_sans_18.h"
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

const lv_font_t* fontUi() { return &font_source_han_sans_18; }

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

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, int w, bool whiteText) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, w);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, fontUi(), 0);
  setTextColor(label, whiteText);
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

void createHeader() {
  lv_obj_t* header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, SCREEN_W, 36);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  stylePanel(header, false);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "溫濕度");
  lv_obj_set_style_text_font(title, fontUi(), 0);
  setTextColor(title, false);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 6, 0);

  batteryLabel = lv_label_create(header);
  lv_label_set_text(batteryLabel, LV_SYMBOL_BATTERY_FULL);
  lv_obj_set_style_text_font(batteryLabel, &lv_font_montserrat_14, 0);
  setTextColor(batteryLabel, false);
  lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID, -4, 0);

  dateLabel = makeLabel(header, 72, 2, 120, false);
  timeLabel = makeLabel(header, 196, 2, 100, false);
  lv_obj_set_style_text_align(timeLabel, LV_TEXT_ALIGN_RIGHT, 0);
}

void createMainPanel() {
  lv_obj_t* panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, SCREEN_W, 180);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 36);
  stylePanel(panel, false);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* tempTitle = makeLabel(panel, 24, 16, 140, false);
  lv_label_set_text(tempTitle, "溫度");

  tempLabel = makeLabel(panel, 24, 44, 160, false);
  lv_label_set_text(tempLabel, "--.- C");
  lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_14, 0);

  lv_obj_t* humTitle = makeLabel(panel, 220, 16, 140, false);
  lv_label_set_text(humTitle, "濕度");

  humLabel = makeLabel(panel, 220, 44, 160, false);
  lv_label_set_text(humLabel, "--.- %");
  lv_obj_set_style_text_font(humLabel, &lv_font_montserrat_14, 0);

  lv_obj_t* phase = makeLabel(panel, 24, 110, 352, false);
  lv_label_set_text(phase, "Phase 1: 硬體驗證");
}

void createStatusBar() {
  statusLabel = lv_label_create(lv_scr_act());
  lv_obj_set_width(statusLabel, SCREEN_W - 12);
  lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(statusLabel, fontUi(), 0);
  setTextColor(statusLabel, false);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 224);

  footerLabel = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(footerLabel, fontUi(), 0);
  setTextColor(footerLabel, false);
  lv_obj_align(footerLabel, LV_ALIGN_BOTTOM_MID, 0, -2);
}
}  // namespace

void uiInit() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

  createHeader();
  createMainPanel();
  createStatusBar();
}

void uiUpdate(bool wifiConnected, bool ntpSynced, float tempC, float humidityPct, bool hasSensor,
              int8_t batteryPct, const char* statusLine) {
  updateClockLabels(ntpSynced);

  if (batteryLabel) {
    lv_label_set_text(batteryLabel, batterySymbol(batteryPct));
  }

  if (tempLabel) {
    if (hasSensor) {
      lv_label_set_text_fmt(tempLabel, "%.1f C", tempC);
    } else {
      lv_label_set_text(tempLabel, "--.- C");
    }
  }

  if (humLabel) {
    if (hasSensor) {
      lv_label_set_text_fmt(humLabel, "%.1f %%", humidityPct);
    } else {
      lv_label_set_text(humLabel, "--.- %");
    }
  }

  if (statusLabel) {
    char line[96];
    snprintf(line, sizeof(line), "%s | WiFi:%s | NTP:%s | SHTC3:%s", statusLine ? statusLine : "",
             wifiConnected ? "OK" : "--", ntpSynced ? "OK" : "--", hasSensor ? "OK" : "ERR");
    lv_label_set_text(statusLabel, line);
  }

  if (footerLabel) {
    if (wifiManagerPortalActive() || !wifiConnected) {
      lv_label_set_text(footerLabel, "連 TempMon-Setup  長KEY:設定  長BOOT:重設");
    } else {
      lv_label_set_text(footerLabel, "短KEY:讀感測  長KEY:NTP同步  長BOOT:WiFi重設");
    }
  }
}
