#!/usr/bin/env bash
set -euo pipefail

SKETCH="temp-monitor-esp32"
FQBN="${FQBN:-esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc}"
BUILD_FLAGS="${BUILD_FLAGS:--DBOARD_HAS_PSRAM -DCORE_DEBUG_LEVEL=0}"
PORT="${PORT:-}"
LVGL_VERSION="${LVGL_VERSION:-8.3.11}"

arduino-cli config init 2>/dev/null || true
arduino-cli config merge arduino-cli.yaml 2>/dev/null || true
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Firmware targets LVGL 8.x API (lv_disp_drv_t). LVGL 9.x is incompatible.
arduino-cli lib uninstall lvgl 2>/dev/null || true
ARDUINOJSON_VERSION="${ARDUINOJSON_VERSION:-7.4.3}"
arduino-cli lib install "lvgl@${LVGL_VERSION}" "ArduinoJson@${ARDUINOJSON_VERSION}" WiFiManager

arduino-cli compile --fqbn "$FQBN" --build-property "compiler.cpp.extra_flags=$BUILD_FLAGS" "$SKETCH"

if [[ -n "$PORT" ]]; then
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"
fi
