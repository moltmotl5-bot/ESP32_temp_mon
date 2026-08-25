# Temp Monitor — ESP32-S3 Firmware

Phase 1 hardware verification for **Waveshare ESP32-S3-RLCD-4.2**.

Shared drivers copied from [BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA).

## Phase 1 checklist

- [x] ST7305 400×300 display + LVGL UI
- [x] SHTC3 temperature & humidity (Serial + screen)
- [x] Battery ADC (GPIO4) icon
- [x] NTP time sync (date + clock on screen)
- [x] WiFiManager captive portal (`TempMon-Setup`)

## Build

```bash
cd firmware
chmod +x build.sh
./build.sh

# Upload
PORT=/dev/ttyACM0 ./build.sh
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
