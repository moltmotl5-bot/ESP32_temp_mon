# Temp Monitor — ESP32-S3 Firmware

Phase 1 hardware verification for **Waveshare ESP32-S3-RLCD-4.2**.

Shared drivers copied from [BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA).

## Phase 1 checklist

- [x] ST7305 400×300 display + LVGL UI
- [x] SHTC3 temperature & humidity (Serial + screen)
- [x] Battery ADC (GPIO4) icon
- [x] NTP time sync (date + clock on screen)
- [x] WiFiManager captive portal (`TempMon-Setup`)

## Phase 2 checklist

- [x] NVS ring buffer — 864 records × 12 B (~10 KB), 3-day rolling history
- [x] 5-minute aligned samples (requires NTP)
- [x] Boot reload from flash; status bar `Rec:N/864`
- [x] BOOT long press clears history

## Build

**不要用 `sudo`**。若看到路徑含 `/private/var/root/`，代表用了 root 帳號，請改以一般使用者執行。

```bash
cd firmware
chmod +x build.sh
./build.sh
```

若失敗，腳本會列出 `error:` 行；也可手動：

```bash
cd firmware
arduino-cli compile --verbose \
  --fqbn "esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc" \
  temp-monitor-esp32 2>&1 | tee build.log
grep -i error build.log
```

### 常見 build 錯誤

| 問題 | 解法 |
|------|------|
| 路徑含 `/var/root/` | 不要用 sudo；`brew install arduino-cli` 後以一般使用者執行 |
| LVGL 9.x 不相容 | `arduino-cli lib uninstall lvgl && arduino-cli lib install lvgl@8.3.11` |
| 找不到 sketch | 必須在 `firmware/` 目錄內執行 `./build.sh` |

## Upload

```bash
# macOS：先找 port
ls /dev/cu.usbmodem*

cd firmware
PORT=/dev/cu.usbmodem101 ./build.sh
```

若 upload 失敗（`No serial data received`）：

1. 關閉 Serial Monitor
2. 確認 USB 為資料線
3. 按住 **BOOT** 鍵再執行 upload
4. 或較慢速度：

```bash
arduino-cli upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc,UploadSpeed=115200" \
  temp-monitor-esp32
```

Board FQBN: `esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc`

**PSRAM must be OPI** — without it the ST7305 LUT cannot allocate.

## Controls

| Button | Action |
|--------|--------|
| KEY short | Force sensor read |
| KEY long | NTP re-sync |
| BOOT long | Erase Wi-Fi + open portal |
| KEY short/long (offline) | Open Wi-Fi portal |

## Serial output (115200)

```
=== Temp Monitor Phase 1 boot ===
Display OK
SHTC3: OK
Battery: 70%
Sensor: 26.5 C, 58.2 %RH
WiFi ready: MyNetwork
NTP synced: 2026-08-25 15:00:32
```
