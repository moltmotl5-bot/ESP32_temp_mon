#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SKETCH="temp-monitor-esp32"
FQBN="${FQBN:-esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc}"
BUILD_FLAGS="${BUILD_FLAGS:--DBOARD_HAS_PSRAM -DCORE_DEBUG_LEVEL=0}"
PORT="${PORT:-}"
LVGL_VERSION="${LVGL_VERSION:-8.3.11}"
ESP32_URL="https://espressif.github.io/arduino-esp32/package_esp32_index.json"

if [[ "$(id -u)" -eq 0 ]]; then
  echo "WARNING: 請不要用 sudo 執行 build.sh（會使用 root 的 Arduino 目錄，容易出錯）"
  echo "         正確做法：cd firmware && ./build.sh"
fi

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "ERROR: 找不到 arduino-cli。macOS 安裝：brew install arduino-cli"
  exit 1
fi

arduino-cli config init 2>/dev/null || true
arduino-cli config add board_manager.additional_urls "$ESP32_URL" 2>/dev/null || true

echo "==> Updating ESP32 core index..."
arduino-cli core update-index
arduino-cli core install esp32:esp32

# LVGL 9.x API is incompatible — force 8.3.11
echo "==> Installing libraries (LVGL ${LVGL_VERSION})..."
arduino-cli lib uninstall lvgl 2>/dev/null || true
ARDUINOJSON_VERSION="${ARDUINOJSON_VERSION:-7.4.3}"
arduino-cli lib install "lvgl@${LVGL_VERSION}" "ArduinoJson@${ARDUINOJSON_VERSION}" WiFiManager

echo "==> Compiling ${SKETCH}..."
set +e
COMPILE_OUT="$(mktemp)"
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=$BUILD_FLAGS" \
  "$SKETCH" 2>&1 | tee "$COMPILE_OUT"
COMPILE_STATUS=${PIPESTATUS[0]}
set -e

if [[ "$COMPILE_STATUS" -ne 0 ]]; then
  echo ""
  echo "=== BUILD FAILED (exit ${COMPILE_STATUS}) ==="
  echo "常見原因："
  echo "  1. 用了 sudo — 改為一般使用者執行"
  echo "  2. LVGL 9.x 殘留 — 執行：arduino-cli lib uninstall lvgl && arduino-cli lib install lvgl@8.3.11"
  echo "  3. 不在 firmware 目錄 — 請 cd firmware 再執行"
  echo ""
  echo "詳細錯誤（grep error）："
  grep -i "error:" "$COMPILE_OUT" || true
  echo ""
  echo "完整 log 已存於：$COMPILE_OUT"
  exit "$COMPILE_STATUS"
fi

echo "==> Build OK"

if [[ -n "$PORT" ]]; then
  echo "==> Uploading to ${PORT}..."
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"
  echo "==> Upload OK"
fi
