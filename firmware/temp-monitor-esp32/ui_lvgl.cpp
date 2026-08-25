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

// Font subset copied from BUS-ETA — only covers ETA-app glyphs.
// Use Montserrat for ASCII; use CJK font only for chars known to exist.
const lv_font_t* fontAscii() { return &lv_font_montserrat_14; }
const lv_font_t* fontCjk() { return &font_source_han_sans_18; }

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

void createMainPanel() {
  lv_obj_t* panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, SCREEN_W, 180);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 36);
  stylePanel(panel, false);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* tempTitle = makeAsciiLabel(panel, 24, 16, 140);
  lv_label_set_text(tempTitle, "Temp");

  tempLabel = makeAsciiLabel(panel, 24, 44, 160);
  lv_label_set_text(tempLabel, "--.- C");

  lv_obj_t* humTitle = makeAsciiLabel(panel, 220, 16, 140);
  lv_label_set_text(humTitle, "Humidity");

  humLabel = makeAsciiLabel(panel, 220, 44, 160);
  lv_label_set_text(humLabel, "--.- %");

  lv_obj_t* phase = makeAsciiLabel(panel, 24, 110, 352);
  lv_label_set_text(phase, "Phase 2: recording");
}

void createStatusBar() {
  statusLabel = makeAsciiLabel(lv_scr_act(), 6, 224, SCREEN_W - 12);
  footerLabel = makeAsciiLabel(lv_scr_act(), 0, 0, SCREEN_W - 8);
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
              int8_t batteryPct, uint16_t recordCount, uint16_t recordMax, const char* statusLine) {
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
    char line[112];
    snprintf(line, sizeof(line), "%s | WiFi:%s | NTP:%s | SHTC3:%s | Rec:%u/%u",
             statusLine ? statusLine : "", wifiConnected ? "OK" : "--", ntpSynced ? "OK" : "--",
             hasSensor ? "OK" : "ERR", recordCount, recordMax);
    lv_label_set_text(statusLabel, line);
  }

  if (footerLabel) {
    if (wifiManagerPortalActive() || !wifiConnected) {
      lv_label_set_text(footerLabel, "Join TempMon-Setup | KEY: setup | BOOT: reset");
    } else {
      lv_label_set_text(footerLabel, "KEY: read | KEY long: NTP | BOOT long: clear");
    }
  }
}
