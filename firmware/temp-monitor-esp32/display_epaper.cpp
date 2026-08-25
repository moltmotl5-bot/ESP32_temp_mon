#include "display_epaper.h"

#include <Arduino.h>

#include "config.h"
#include "lvgl_setup.h"
#include "st7305_display.h"

namespace {
St7305Display panel;
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[SCREEN_W * 40];
static lv_disp_drv_t dispDrv;
static bool panelReady = false;

bool lvglPixelIsBlack(lv_color_t color) {
  // LVGL 1-bit: dark pixels must map to cleared ST7305 bits (black).
  return lv_color_to1(color) == 0;
}

void rlcdFlush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;

  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      const lv_color_t px = color_p[y * w + x];
      panel.setPixel(area->x1 + x, area->y1 + y, lvglPixelIsBlack(px));
    }
  }

  panel.flush();
  lv_disp_flush_ready(drv);
}
}  // namespace

bool displayReady() { return panelReady; }

void displayShowBootSplash() {
  if (!panelReady) return;

  panel.clearWhite();
  panel.drawRect(0, 0, SCREEN_W, SCREEN_H, true);
  panel.drawRect(8, 8, SCREEN_W - 16, SCREEN_H - 16, true);
  panel.fillRect(20, 130, SCREEN_W - 40, 24, true);
  panel.fillRect(20, 170, 180, 16, true);
  panel.flush();
}

bool displayInit() {
  panelReady = false;

  if (!panel.begin(LCD_CS, LCD_DC, LCD_RST, LCD_SCK, LCD_MOSI)) {
    Serial.println("ST7305 init failed (framebuffer/LUT allocation?)");
    return false;
  }

  panelReady = true;
  displayShowBootSplash();

  lv_init();
  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, SCREEN_W * 40);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_W;
  dispDrv.ver_res = SCREEN_H;
  dispDrv.flush_cb = rlcdFlush;
  dispDrv.full_refresh = 1;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  return true;
}

void displayTick() {
  if (!panelReady) return;
  static uint32_t lastTickMs = 0;
  const uint32_t now = millis();
  if (lastTickMs != 0) {
    lv_tick_inc(now - lastTickMs);
  }
  lastTickMs = now;
  lv_timer_handler();
}

void displayRefreshFull() {
  if (!panelReady) return;
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(nullptr);
}
