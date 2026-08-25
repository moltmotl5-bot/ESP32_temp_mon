/**
 * @file lv_conf.h
 * LVGL 8.x configuration for ESP32-S3 + ST7305 monochrome (400x300)
 */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 1
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 500
#define LV_INDEV_DEF_READ_PERIOD 30

#define LV_USE_LOG 0

#define LV_SPRINTF_USE_FLOAT 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FONT_SUBPX 0

#define LV_USE_LABEL 1
#define LV_USE_BTN 1

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0

#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
#endif
