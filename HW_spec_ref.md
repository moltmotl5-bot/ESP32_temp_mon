# ETA App — ESP32-S3 Firmware

> **完整原始文件**：[BUS-ETA README](https://github.com/moltmotl5-bot/BUS-ETA/blob/main/firmware/README.md)
>
> 本檔為本地摘要，供 `ESP32_temp_mon` 專案參考。溫濕度監控韌體可直接調用 BUS-ETA 中已實作的底層模組（詳見 [README-2.md §11](./README-2.md)）。

MTR bus arrival display for the **Waveshare ESP32-S3-RLCD-4.2** board:

| Component | Model |
|-----------|--------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB PSRAM) |
| Display | **ST7305** 4.2" reflective LCD, 400×300, SPI |
| Sensor | **SHTC3** temperature & humidity (I²C `0x70`) |

Built with **LVGL** and **Arduino CLI**.

## Features

- Up to 5 routes (MTR K51/K51A + KMB 961/960/58M/58X/260X/R33/A34/263/A33X), 2 ETAs each
- LVGL UI on 400×300 ST7305 panel (single-screen layout)
- SHTC3 temp/humidity shown in header
- Battery level icon (GPIO4 ADC) in header top-right
- Settings saved to NVS
- Wi-Fi fetch from MTR open-data API every 60 s

## Board pin map (Waveshare ESP32-S3-RLCD-4.2)

| GPIO | Function |
|------|----------|
| 11 | LCD SPI SCK |
| 12 | LCD SPI MOSI |
| 40 | LCD CS |
| 5 | LCD DC |
| 41 | LCD RST |
| 13 | I²C SDA (SHTC3) |
| 14 | I²C SCL |
| 0 | BOOT button (active LOW) |
| 18 | KEY button (active LOW) |
| 4 | Battery ADC (single-cell Li-ion via 3× divider) |
| 17 | Battery power hold (keep HIGH) |

### Battery

The board holder accepts a single **3.7 V Li-ion** cell (18650 or similar pouch). Reference pack: **2.6 Ah / 3.7 V / 9.62 Wh**.

| Voltage | Icon level |
|---------|------------|
| 4.2 V | 100% (full) |
| 3.7 V | ~58% (nominal) |
| 3.0 V | 0% (empty) |

Percentage is linear between 3.0 V and 4.2 V. Adjust `BATTERY_VOLTAGE_EMPTY` / `BATTERY_VOLTAGE_FULL` in `config.h` if your cell behaves differently.

Official reference: [Waveshare ESP32-S3-RLCD-4.2 docs](https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice)

## Controls (2 onboard buttons)

| Input | Action |
|-------|--------|
| **BOOT** short | Next item: direction → stop → route → next row |
| **BOOT** long | Remove selected route |
| **KEY** short | Change value of highlighted field (去程/回程, stop, K51/K51A) |
| **KEY** long | Refresh ETAs |
| **BOOT + KEY** | Add route (max 5) |

## Setup

### 1. Install Arduino CLI

```bash
brew install arduino-cli   # macOS
```

### 2. Wi-Fi setup (no reflash needed)

Wi-Fi is configured with **WiFiManager** — no `secrets.h` required.

**First boot (or after reset):**

1. The board opens a hotspot: **`ETA-App-Setup`**
2. On your phone, connect to that Wi-Fi network
3. A setup page should open automatically (or browse to `http://192.168.4.1`)
4. Choose your home Wi-Fi, enter password, save
5. The board reboots/connects and saves credentials in flash

**Change Wi-Fi later (without re-uploading firmware):**

| Button | When Wi-Fi is down |
|--------|---------------------|
| **KEY** long press | Open setup portal again |
| **BOOT** long press | Erase saved Wi-Fi + open setup portal |

**Stuck on「連線中」or cannot find `ETA-App-Setup`:**

1. Wait ~10 seconds — the setup hotspot opens automatically if saved Wi-Fi fails
2. Press **KEY once** (short) or **long-press KEY** to force the setup portal
3. To wipe saved credentials, **long-press BOOT**
4. On your phone, connect to Wi-Fi **`ETA-App-Setup`** and open `http://192.168.4.1` if no captive page appears
5. While the portal is open, the screen shows「**WiFi設定**」and the footer lists `ETA-App-Setup`

In Arduino IDE, install library **WiFiManager** by tzapu (v2.x).

Optional compile-time override only: copy `secrets.example.h` → `secrets.h` (normally unused).

### 3. Build & upload

```bash
chmod +x build.sh
./build.sh

# Upload (replace with your port, e.g. /dev/cu.usbmodem* or COM3)
PORT=/dev/ttyACM0 ./build.sh
```

Default board target: `esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc`

**USB CDC is enabled by default** so `Serial Monitor` works over the board's USB port.

The Waveshare N16R8 module needs **OPI PSRAM enabled** — without it the ~360 KB display LUT cannot allocate and the screen stays blank.

If you previously built without `CDCOnBoot=cdc`, re-upload after enabling it (see board settings below).

#### Arduino IDE board settings (Waveshare ESP32-S3-RLCD-4.2)

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| CPU Frequency | 240 MHz |
| Flash Size | **16 MB** |
| Partition Scheme | **Huge APP (3 MB)** |
| PSRAM | **OPI PSRAM** |
| Upload Speed | **115200** (use if upload fails at 921600) |

#### Upload failed: `No serial data received`

This is a **connection / download-mode** issue, not a compile error. The sketch size (42%) is fine.

1. **Close Serial Monitor** before uploading (only one program can use the port).
2. Use a **data USB cable** (many USB-C cables are charge-only).
3. On Mac, pick the **`/dev/cu.usbmodem...`** port (not `tty.*`).
4. If the board is crash-looping (Guru Meditation in Serial Monitor), force download mode:
   - **Hold the side BOOT button** (GPIO0)
   - Click **Upload** in Arduino IDE
   - When you see `Connecting....`, keep holding BOOT for ~2 seconds, then release
   - If it still fails: hold BOOT → **unplug USB** → plug back in → release BOOT when `Writing...` starts
5. Lower **Upload Speed** to **115200** in Tools menu.
6. Quit other apps that may grab the port (Screen, other Arduino windows, `screen`, `minicom`).
7. Try another USB port (directly on the Mac, not through a hub if possible).

After a successful upload, open Serial Monitor at **115200** baud to confirm boot logs.

#### Serial Monitor shows nothing (ESP32-S3)

1. **Tools → USB CDC On Boot → Enabled** — re-upload after changing this
2. Pick the **`USB`** / **`usbmodem`** port (not a second UART port if two appear)
3. Open Serial Monitor, then press the board **RESET** button (or re-plug USB)
4. You should see `=== ETA App boot ===` within a few seconds
5. Battery debug lines look like: `Battery: raw=1280mV cell=3.84V -> 70%`
6. On Mac use **`/dev/cu.usbmodem...`** (not `tty.*`) for upload; either often works for monitor

If upload works but Serial stays empty, the flashed firmware was likely built without USB CDC — enable it and upload again.

## Project layout

```
firmware/eta-app-esp32/
├── eta-app-esp32.ino      Main sketch
├── st7305_display.cpp     ST7305 driver (400×300)
├── display_epaper.cpp     LVGL flush → ST7305
├── ui_lvgl.cpp            Route table UI
├── mtr_api.cpp            MTR Bus HTTPS client
├── kmb_api.cpp            KMB/LWB Bus HTTPS client
├── eta_api.cpp            Unified ETA fetch (MTR + KMB)
├── board.cpp              Power hold + SHTC3
├── input_buttons.cpp      BOOT / KEY handling
├── lv_conf.h              LVGL 1-bit monochrome
└── stops.h                K51 / K51A stop names
```

## Libraries (installed by build.sh)

- [lvgl](https://github.com/lvgl/lvgl)
- [WiFiManager](https://github.com/tzapu/WiFiManager) (captive portal setup)
- [ArduinoJson](https://arduinojson.org/)

SHTC3 is read via raw I²C (no extra sensor library required).

**Important:** This project requires **LVGL 8.3.x**. LVGL 9.x uses a different API and will not compile. `build.sh` uninstalls LVGL 9 and installs 8.3.11 automatically.

## Display notes

- The ST7305 panel is a **reflective LCD (RLCD)**, not slow e-ink — refresh is faster but still full-frame via SPI.
- Driver uses Waveshare's **landscape 2×4 pixel packing** (15 KB framebuffer).
- Chinese text uses embedded **Source Han Sans CN Bold** 18px (not SimSun).

## Chinese font (Source Han Sans CN Bold)

The firmware uses a subset of **Source Han Sans CN Bold** converted for LVGL:

- Font file: `public/SourceHanSansCN-Bold-2.otf`
- Generated C array: `firmware/eta-app-esp32/font_source_han_sans_18.c`

Regenerate after changing UI text or stop names:

```bash
# Requires Node.js (npx lv_font_conv)
python3 firmware/scripts/gen_font.py
```

Then recompile/upload in Arduino IDE or `./build.sh`.

## Regenerate stops

MTR stops come from `src/data/stops.ts`; KMB stops (outbound + inbound) are fetched from
[data.etabus.gov.hk](https://data.etabus.gov.hk/v1/transport/kmb) for routes
961, 960, 58M, 58X, 260X, R33, A34, 263, A33X.

```bash
python3 firmware/scripts/gen_stops.py
python3 firmware/scripts/gen_font.py   # after stop names change
```

## Fleet / multi-device

Each node runs the same firmware; route selections are stored locally in NVS (`eta-app` namespace). For a central fleet config, extend `app_state.cpp` to load slots from JSON over HTTP or MQTT.
