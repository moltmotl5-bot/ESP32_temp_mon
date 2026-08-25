# ESP32 螢幕溫濕度監控器 — 專案計畫

> **專案目標**：在 **Waveshare ESP32-S3-RLCD-4.2** 開發板上建置即時溫濕度監控器，於 4.2" 反射式 LCD 顯示日期、時間、當前溫濕度，並以折線圖呈現過去 12 小時溫度趨勢，同時在模組內部儲存過去 3 天的歷史紀錄。

> **上游韌體 repo**：[BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA) — 同一塊 Waveshare 板子的 ETA App，已實作好 ST7305 顯示、SHTC3 感測、LVGL、WiFiManager、電池等底層模組，本專案直接調用，無需從零撰寫。
>
> 硬體規格參考：[HW_spec_ref.md](./HW_spec_ref.md)（摘錄自 BUS-ETA）

---

## 1. 硬體清單

| 元件 | 型號 / 規格 | 備註 |
|------|-------------|------|
| 主控板 | **Waveshare ESP32-S3-RLCD-4.2** | 整合 ST7305 螢幕 + SHTC3 感測器 |
| MCU | ESP32-S3-WROOM-1-N16R8 | 16 MB Flash、8 MB OPI PSRAM |
| 顯示器 | **ST7305** 4.2" 反射式 LCD（RLCD） | 400×300、SPI、1-bit 單色 |
| 溫濕度感測器 | **SHTC3** | I²C 位址 `0x70`，板載已焊接 |
| 電池 | 3.7 V 鋰電池（18650 或軟包） | 參考：2.6 Ah / 9.62 Wh |
| 按鈕 | BOOT（GPIO0）+ KEY（GPIO18） | 板載 2 鍵，可用於互動 |
| RTC | 無外接 RTC | 透過 Wi-Fi NTP 同步，斷線後軟體時鐘續跑 |

### 1.1 系統方塊圖

```
Waveshare ESP32-S3-RLCD-4.2
  ├── ESP32-S3（Wi-Fi / BLE）
  │     ├── NTP 時間同步
  │     ├── NVS / LittleFS（3 天歷史紀錄）
  │     └── 電池 ADC（GPIO4）+ 電源保持（GPIO17）
  ├── ST7305 400×300 RLCD（SPI）
  │     └── LVGL 8.3 UI（日期、時間、溫濕度、折線圖）
  └── SHTC3 溫濕度（I²C 0x70，GPIO13/14）
```

### 1.2 板載接腳對照（Waveshare ESP32-S3-RLCD-4.2）

| GPIO | 功能 |
|------|------|
| 11 | LCD SPI SCK |
| 12 | LCD SPI MOSI |
| 40 | LCD CS |
| 5 | LCD DC |
| 41 | LCD RST |
| 13 | I²C SDA（SHTC3） |
| 14 | I²C SCL |
| 0 | BOOT 按鈕（active LOW） |
| 18 | KEY 按鈕（active LOW） |
| 4 | 電池 ADC（單節鋰電，3× 分壓） |
| 17 | 電池電源保持（維持 HIGH） |

> 官方文件：[Waveshare ESP32-S3-RLCD-4.2](https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice)

### 1.3 電池電壓對照

| 電壓 | 電量 |
|------|------|
| 4.2 V | 100%（滿電） |
| 3.7 V | ~58%（標稱） |
| 3.0 V | 0%（空電） |

百分比在 3.0 V–4.2 V 之間線性插值。可在 `config.h` 調整 `BATTERY_VOLTAGE_EMPTY` / `BATTERY_VOLTAGE_FULL`。

---

## 2. 功能需求

### 2.1 螢幕顯示內容（400×300 單色 RLCD）

```
┌────────────────────────────────────────────── 400px ──┐
│ 2026-08-25          07:00:32              [電池 70%] │  ← 標題列（48 px）
├──────────────────────────────────────────────────────┤
│                                                      │
│   溫度                    濕度                        │  ← 數值區（80 px）
│   26.5 °C                 58.2 %                     │
│                                                      │
├──────────────────────────────────────────────────────┤
│  過去 12 小時溫度                                     │
│  30┤      ╭─╮                                        │  ← 折線圖（~160 px）
│  25┤  ╭───╯ ╰──╮                                     │
│  20┤──╯        ╰──                                   │
│    └────────────────────────                         │
│    -12h                              現在             │
├──────────────────────────────────────────────────────┤
│  ● 已同步 NTP    取樣：5 min    紀錄：864/864        │  ← 狀態列（24 px）
└──────────────────────────────────────────────────────┘
```

| 區塊 | 內容 | 更新頻率 |
|------|------|----------|
| 日期 | `YYYY-MM-DD` | 每秒 |
| 時間 | `HH:MM:SS` | 每秒 |
| 溫度 | 小數一位，°C | 每 60 秒 |
| 濕度 | 小數一位，% | 每 60 秒 |
| 電池 | 百分比圖示（右上角） | 每 60 秒 |
| 折線圖 | 過去 12 小時溫度曲線 | 每 60 秒重繪 |
| 狀態列 | NTP / Wi-Fi / 儲存筆數 | 每 10 秒 |

### 2.2 資料儲存需求

| 項目 | 規格 |
|------|------|
| 儲存位置 | ESP32 Flash（**NVS 或 LittleFS** 分割區） |
| 可用 Flash | 16 MB（分割區可另行規劃資料區） |
| 保留天數 | **3 天** |
| 取樣間隔 | **5 分鐘** |
| 每筆紀錄 | 時間戳 4 B + 溫度 4 B + 濕度 4 B = **12 B** |
| 3 天總筆數 | 864 筆（288 筆/天 × 3 天） |
| 預估容量 | ~10 KB（16 MB Flash 綽綽有餘） |

> 16 MB Flash 容量充足，無需過度擔心磨損；仍建議 5 分鐘取樣，避免每秒寫入。

### 2.3 折線圖資料來源

- 12 小時 ÷ 5 分鐘 = **144 個資料點**
- 從 3 天環形緩衝區讀取最近 144 筆
- Y 軸自動縮放（min − 2°C 至 max + 2°C，下限不低於 0°C）
- 使用 **LVGL `lv_chart`** 繪製（適配 1-bit 單色 RLCD）

---

## 3. 軟體架構

### 3.1 技術堆疊

| 層級 | 選型 | 理由 |
|------|------|------|
| 開發框架 | **Arduino CLI** + ESP32 Arduino core | 與現有 ETA App 專案一致 |
| 目標板 | `esp32:esp32:esp32s3` | Huge APP + OPI PSRAM + 16 MB Flash |
| 顯示驅動 | **ST7305** 自訂驅動 + **LVGL 8.3.x** | 板載 RLCD，1-bit 單色，需 LVGL 8（非 9.x） |
| 感測器 | **SHTC3**（raw I²C） | 板載感測器，無需額外函式庫 |
| 時間 | **NTP**（`configTime` / NTPClient） | 無外接 RTC；斷 Wi-Fi 後軟體時鐘續跑 |
| Wi-Fi 設定 | **WiFiManager**（選配） | 沿用 ETA App 的 captive portal 流程 |
| 儲存 | **NVS** 或 **LittleFS** + Ring Buffer | 864 筆紀錄約 10 KB |
| JSON | **ArduinoJson**（選配） | Web 匯出歷史資料時使用 |

> **重要**：必須啟用 **OPI PSRAM**，否則 ST7305 的 ~360 KB 顯示 LUT 無法配置，螢幕會維持空白。

### 3.2 專案目錄結構

本 repo（`ESP32_temp_mon`）新增溫濕度監控韌體；底層驅動從 [BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA) 複製或 submodule 引用。

```
ESP32_temp_mon/
├── README-2.md                          # 本計畫
├── HW_spec_ref.md                       # 硬體規格摘要
└── firmware/
    ├── build.sh                         # 改 SKETCH=temp-monitor-esp32（自 BUS-ETA 複製）
    ├── arduino-cli.yaml                 # 自 BUS-ETA 複製
    └── temp-monitor-esp32/
        ├── temp-monitor-esp32.ino       # ★ 新建：主程式（參考 eta-app-esp32.ino 骨架）
        ├── config.h                     # ★ 新建：自 BUS-ETA config.h 精簡（移除 ETA 專用常數）
        ├── ui_lvgl.cpp / .h             # ★ 新建：溫濕度 UI + lv_chart 折線圖
        ├── record_store.cpp / .h        # ★ 新建：3 天環形緩衝區（參考 app_state.cpp NVS 模式）
        │
        │  ── 以下直接從 BUS-ETA 複製，原封不動 ──
        ├── st7305_display.cpp / .h      # BUS-ETA: firmware/eta-app-esp32/
        ├── display_epaper.cpp / .h      # BUS-ETA: LVGL flush → ST7305
        ├── board.cpp / .h               # BUS-ETA: 電源保持 + SHTC3 + 電池 ADC
        ├── input_buttons.cpp / .h       # BUS-ETA: BOOT / KEY 處理
        ├── wifi_manager.cpp / .h        # BUS-ETA: WiFiManager captive portal
        ├── lv_conf.h                    # BUS-ETA: LVGL 1-bit monochrome
        ├── lvgl_setup.h                 # BUS-ETA
        └── font_source_han_sans_18.c/.h # BUS-ETA: 中文字型
```

> **不複製**的 BUS-ETA 模組（與溫濕度監控無關）：`mtr_api.*`、`kmb_api.*`、`eta_api.*`、`app_state.*`、`stops.h`

### 3.3 FreeRTOS 任務分工

| 任務 | 週期 | 優先級 | 職責 |
|------|------|--------|------|
| `sensorTask` | 60 s | 2 | SHTC3 讀取溫濕度、更新 RAM 緩衝區 |
| `displayTask` | 1 s | 1 | LVGL tick、更新時鐘與 UI |
| `storageTask` | 5 min | 2 | 寫入 Flash 環形緩衝區 |
| `timeSyncTask` | 3600 s | 0 | NTP 重新同步 |
| `batteryTask` | 60 s | 0 | ADC 讀取電池電量 |

> ESP32-S3 雙核心：Core 0 跑 Wi-Fi / NTP，Core 1 跑 LVGL 與感測器。

### 3.4 按鈕操作（建議）

| 輸入 | 動作 |
|------|--------|
| **KEY** 短按 | 手動刷新感測器讀數 |
| **KEY** 長按 | 強制 NTP 時間同步 |
| **BOOT** 短按 | 切換折線圖 Y 軸：自動 / 固定 15–35°C |
| **BOOT** 長按 | 清除 3 天歷史紀錄（需確認） |
| **BOOT + KEY** | 進入 Wi-Fi 設定 portal（沿用 WiFiManager） |

---

## 4. 資料結構設計

### 4.1 單筆紀錄

```cpp
struct TempRecord {
    uint32_t timestamp;   // Unix epoch（秒）
    float    temperature; // °C
    float    humidity;    // %
};
// sizeof(TempRecord) == 12 bytes
```

### 4.2 環形緩衝區

```
RAM Ring Buffer（144 筆）          Flash Ring Buffer（864 筆）
┌───┬───┬───┬ ... ─┬───┐          ┌───┬───┬ ... ─┬───┐
│ 0 │ 1 │ 2 │      │143│          │ 0 │ 1 │      │863│
└───┴───┴───┴ ... ─┴───┘          └───┴───┴ ... ─┴───┘
  ↑ lv_chart 折線圖資料               ↑ 3 天歷史，每 5 分鐘寫入
                                    開機時從 Flash 載入至 RAM
```

### 4.3 SHTC3 讀取（raw I²C，參考 ETA App）

```cpp
// I²C 位址 0x70，命令 0x7866（測量 T+RH，低功耗模式）
// 回傳 6 bytes：T[15:0], RH[15:0], CRC
// 溫度 = -45 + 175 × (rawT / 65535)
// 濕度 = 100 × (rawRH / 65535)
```

### 4.4 Flash 分割區建議

沿用 ETA App 的 **Huge APP (3 MB)** 方案，另關資料分割區：

| 設定 | 值 |
|------|-----|
| Board | ESP32S3 Dev Module |
| Flash Size | **16 MB** |
| Partition Scheme | **Huge APP (3 MB)** 或自訂含 `spiffs`/`littlefs` |
| PSRAM | **OPI PSRAM** |
| USB CDC On Boot | **Enabled** |

864 筆 × 12 B ≈ 10 KB，可存入 NVS namespace `temp-mon` 或獨立 LittleFS 分割區。

---

## 5. UI 設計細節（1-bit 單色 RLCD）

### 5.1 顯示特性

- ST7305 為**反射式 LCD**，非彩色 TFT，僅黑 / 白兩色
- 無背光控制；靠環境光反射，適合長時間常駐顯示
- 全幅刷新經 SPI，使用 LVGL 局部重繪降低閃爍
- 中文字型：嵌入 **Source Han Sans CN Bold**（可沿用 ETA App 的 `font_source_han_sans_18.c`）

### 5.2 LVGL 元件配置

| UI 元件 | LVGL 類型 | 說明 |
|---------|-----------|------|
| 日期 / 時間 | `lv_label` | 標題列，18 px 字型 |
| 溫度 / 濕度 | `lv_label` | 32 px 大字，分欄顯示 |
| 折線圖 | `lv_chart` | `LV_CHART_TYPE_LINE`，144 點，隱藏點標記 |
| 電池圖示 | `lv_label` 或自訂 icon | 右上角，沿用 ETA App 電池 icon |
| 狀態列 | `lv_label` | 底部 12 px，NTP / 儲存狀態 |

### 5.3 折線圖演算法

1. 從 RAM Ring Buffer 取出最近 144 筆有效紀錄
2. 計算 `yMin = min(temp) − 2`，`yMax = max(temp) + 2`
3. 呼叫 `lv_chart_set_range()` 設定 Y 軸
4. 以 `lv_chart_set_next_value()` 填入序列
5. 每 60 秒更新；僅重繪 chart 區域（`lv_obj_invalidate`）

### 5.4 錯誤狀態顯示

| 狀況 | 顯示 |
|------|------|
| SHTC3 讀取失敗 | 溫濕度顯示 `--.-` |
| Wi-Fi 斷線 | 狀態列顯示「離線」，時間靠軟體時鐘 |
| NTP 未同步 | 日期時間顯示 `--`，提示連接 Wi-Fi |
| Flash 寫入失敗 | 狀態列顯示「儲存異常」 |
| 低電量 (< 10%) | 電池 icon 閃爍警告 |

---

## 6. 時間同步策略

```
開機
  │
  ├─ WiFi 可用 ──→ NTP 同步（pool.ntp.org / time.google.com）
  │                 └── 設定 ESP32 軟體 RTC（configTime + settimeofday）
  │
  └─ WiFi 不可用 ─→ 使用上次 NTP 同步的軟體時鐘續跑
                    │
                    └─ 從未同步 ─→ 顯示 --:--:--，提示設定 Wi-Fi

運行中：每小時 NTP 重新同步；誤差 > 2 秒才校正
```

> 本板**無 DS3231**，斷電後若未同步 NTP，時間需重新取得。可將最後同步時間戳存入 NVS 作為參考。

---

## 7. 開發階段計畫

### Phase 1 — 硬體驗證

- [x] Arduino CLI 專案初始化（ESP32-S3 + OPI PSRAM + 16 MB）
- [x] 移植 ST7305 驅動與 LVGL flush（自 BUS-ETA）
- [x] SHTC3 讀取測試（Serial + 螢幕顯示溫濕度）
- [x] 電源保持（GPIO17 HIGH）+ 電池 ADC 測試
- [x] NTP 時間同步測試（螢幕顯示日期時間）

> 韌體路徑：`firmware/temp-monitor-esp32/` | 建置：`cd firmware && ./build.sh`

### Phase 2 — 核心功能

- [x] 實作 `TempRecord` RAM 環形緩衝區（864 筆 NVS + `recordStoreCopyRecent` 供 Phase 3 圖表）
- [x] 實作 Flash 持久化（864 筆，NVS `temp-mon` namespace）
- [x] 感測器 60 s 讀取、5 min 寫入 Flash
- [x] 3 天資料滾動覆蓋邏輯
- [x] 開機載入歷史紀錄（`recordStoreInit`）

> 狀態列顯示 `Rec:123/864`；**BOOT 長按**清除歷史紀錄

### Phase 3 — UI 整合

- [x] LVGL 版面：日期、時間、溫濕度、電池 icon
- [x] `lv_chart` 折線圖（12 小時 / 144 點）
- [x] 每 60 秒更新數值與圖表
- [x] 狀態列與錯誤提示
- [x] BOOT 短按切換 Y 軸（自動 / 固定 15–35°C）

### Phase 4 — 穩定化與測試

- [ ] 斷電恢復測試（3 天紀錄完整）
- [ ] 72 小時連續運行（電池供電 + USB 供電）
- [ ] Wi-Fi 斷線 / 重連、NTP 重同步
- [ ] 低電量行為（是否進入省電模式）

### Phase 5 — 延伸功能（選配）

- [ ] WiFiManager captive portal（沿用 ETA App 流程）
- [ ] Web 介面查詢 3 天歷史（ESP32 HTTP Server）
- [ ] OTA 韌體更新
- [ ] 溫度超標告警（螢幕閃爍 / Serial 通知）
- [ ] 資料匯出 CSV（Serial / Web）

---

## 8. 建置與燒錄

### 8.1 初始化（從 BUS-ETA 取得共用模組）

```bash
# 1. Clone BUS-ETA（若尚未有）
git clone https://github.com/moltmotl5-bot/BUS-ETA.git /tmp/BUS-ETA

# 2. 複製共用韌體檔案至本專案
SRC=/tmp/BUS-ETA/firmware/eta-app-esp32
DST=firmware/temp-monitor-esp32
mkdir -p "$DST"
for f in st7305_display display_epaper board input_buttons wifi_manager \
         lv_conf lvgl_setup font_source_han_sans_18; do
  cp "$SRC/${f}.cpp" "$SRC/${f}.h" "$DST/" 2>/dev/null || \
  cp "$SRC/${f}.c"  "$SRC/${f}.h" "$DST/" 2>/dev/null || true
done
cp /tmp/BUS-ETA/firmware/build.sh /tmp/BUS-ETA/firmware/arduino-cli.yaml firmware/

# 3. 修改 firmware/build.sh 的 SKETCH 變數
#    SKETCH="temp-monitor-esp32"
```

### 8.2 Arduino CLI 建置

```bash
cd firmware
chmod +x build.sh
./build.sh

# 燒錄（替換為實際 port）
PORT=/dev/ttyACM0 ./build.sh
```

### 8.3 板子設定

| 設定 | 值 |
|------|-----|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| CPU Frequency | 240 MHz |
| Flash Size | **16 MB** |
| Partition Scheme | **Huge APP (3 MB)** |
| PSRAM | **OPI PSRAM** |
| Upload Speed | 115200 |

預設 FQBN：

```
esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,FlashMode=qio,CDCOnBoot=cdc
```

### 8.4 依賴函式庫

| 函式庫 | 版本 | 用途 |
|--------|------|------|
| [LVGL](https://github.com/lvgl/lvgl) | **8.3.x**（非 9.x） | UI 框架、折線圖 |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | 2.x | Wi-Fi captive portal（選配） |
| [ArduinoJson](https://arduinojson.org/) | 7.x | JSON 序列化（選配） |

SHTC3 以 raw I²C 讀取，**無需額外感測器函式庫**（可移植 ETA App 的 `board.cpp`）。

---

## 9. 風險與注意事項

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| 未啟用 OPI PSRAM | 螢幕空白 | 建置時必選 OPI PSRAM |
| LVGL 9.x API 不相容 | 編譯失敗 | 鎖定 LVGL **8.3.11** |
| RLCD 全幅刷新慢 | UI 卡頓 | LVGL 局部重繪；chart 區域獨立 invalidate |
| 斷電失去時間 | 日期錯誤 | NTP 同步後寫入 NVS；Wi-Fi 恢復即重同步 |
| 電池耗盡 | 資料遺失 | 低電量警告；重要資料即時 flush Flash |
| SPI 與 Wi-Fi 共存 | 偶發干擾 | 顯示刷新與 Wi-Fi 分核心執行 |

---

## 10. 驗收標準

- [ ] 400×300 RLCD 正確顯示日期（YYYY-MM-DD）與時間（HH:MM:SS）
- [ ] SHTC3 溫濕度每 60 秒更新，精度 ±0.5°C / ±2% RH
- [ ] 折線圖顯示過去 12 小時溫度，144 個有效點
- [ ] 3 天（864 筆）歷史紀錄存於 Flash，斷電重啟不遺失
- [ ] 電池電量 icon 正確顯示
- [ ] 連續運行 72 小時無當機
- [ ] SHTC3 異常時顯示 `--.-`，系統不當機

---

## 11. 從 BUS-ETA 調用已有模組

> Repo：[https://github.com/moltmotl5-bot/BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA)
>
> 路徑前綴：`firmware/eta-app-esp32/`

### 11.1 可直接複製（零修改或僅改 config.h）

| BUS-ETA 檔案 | 功能 | 本專案用途 |
|-------------|------|-----------|
| `st7305_display.cpp/.h` | ST7305 SPI 驅動、framebuffer、像素繪製 | 顯示硬體層，**原封不動** |
| `display_epaper.cpp/.h` | LVGL init、flush callback、`displayTick()` | LVGL ↔ ST7305 橋接，**原封不動** |
| `board.cpp/.h` | `boardPowerInit()`、`sensorInit/Read()`、`batteryInit/Read()` | SHTC3 + 電池 + 電源保持，**原封不動** |
| `input_buttons.cpp/.h` | BOOT/KEY 短按、長按、組合鍵偵測 | 按鈕輸入，**原封不動**（僅改事件對應動作） |
| `wifi_manager.cpp/.h` | WiFiManager captive portal、連線管理 | Wi-Fi 設定，**原封不動**（改 portal SSID 名稱） |
| `lv_conf.h` | LVGL 1-bit 單色、記憶體配置 | LVGL 設定，**原封不動** |
| `lvgl_setup.h` | LVGL header 統一引入 | **原封不動** |
| `font_source_han_sans_18.c/.h` | Source Han Sans CN Bold 18 px | 中文 UI 字型，**原封不動** |
| `config.h` | GPIO 接腳、螢幕尺寸、電池常數 | **精簡複製**（移除 `MTR_API_URL`、`MAX_ROUTES` 等 ETA 常數） |
| `firmware/build.sh` | Arduino CLI 建置 + LVGL 8.3.11 鎖版 | 改 `SKETCH=temp-monitor-esp32` |
| `firmware/arduino-cli.yaml` | Arduino CLI 設定 | **原封不動** |

### 11.2 參考實作（複製骨架後改寫）

| BUS-ETA 檔案 | 參考內容 | 本專案對應 |
|-------------|---------|-----------|
| `eta-app-esp32.ino` | 開機流程、`waitForTimeSync()`、`readSensor()`、`uiTick()` 主迴圈 | → `temp-monitor-esp32.ino` |
| `ui_lvgl.cpp` | `stylePanel()`、`makeLabel()`、`createBatteryIcon()`、`batterySymbol()` | → 新 UI：日期時間、溫濕度大字、`lv_chart` |
| `app_state.cpp` | `Preferences` NVS 讀寫模式（`prefs.begin("eta-app")`） | → `record_store.cpp`（namespace 改 `temp-mon`） |

### 11.3 BUS-ETA 中可重用的程式片段

**NTP 時間同步**（摘自 `eta-app-esp32.ino`）：

```cpp
void waitForTimeSync() {
  configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");
  const uint32_t start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 15000) {
    wifiManagerLoop(uiTick);
    delay(250);
  }
}
```

**SHTC3 讀取**（已在 `board.cpp`，直接呼叫）：

```cpp
float tempC, humidityPct;
if (sensorRead(&tempC, &humidityPct)) { /* 更新 UI + 寫入 record_store */ }
```

**NVS 環形緩衝區**（參考 `app_state.cpp` 的 `Preferences` 模式）：

```cpp
// record_store.cpp — 沿用 BUS-ETA 的 Preferences 寫法
Preferences prefs;
prefs.begin("temp-mon", false);
prefs.putBytes("records", buffer, sizeof(buffer));
prefs.putUInt("head", headIndex);
prefs.end();
```

**電池 icon**（摘自 `ui_lvgl.cpp`）：

```cpp
// batterySymbol() + createBatteryIcon() 可直接搬至新 ui_lvgl.cpp
const char* batterySymbol(int8_t pct) { /* LV_SYMBOL_BATTERY_* */ }
```

### 11.4 不需複製的 BUS-ETA 模組

| 檔案 | 原因 |
|------|------|
| `mtr_api.*` / `kmb_api.*` / `eta_api.*` | 巴士 ETA API，與溫濕度無關 |
| `app_state.*` / `stops.h` | 路線設定狀態，由 `record_store.*` 取代 |
| `firmware/scripts/gen_stops.py` | 巴士站名產生器，不需要 |

### 11.5 兩個 repo 的關係

```
BUS-ETA (上游)                         ESP32_temp_mon (本專案)
├── firmware/eta-app-esp32/            ├── firmware/temp-monitor-esp32/
│   ├── board.cpp          ──複製──→   │   ├── board.cpp          (共用)
│   ├── st7305_display.*   ──複製──→   │   ├── st7305_display.*   (共用)
│   ├── display_epaper.*   ──複製──→   │   ├── display_epaper.*   (共用)
│   ├── ui_lvgl.*          ──參考──→   │   ├── ui_lvgl.*          (新建：chart UI)
│   ├── eta-app-esp32.ino  ──參考──→   │   ├── temp-monitor.ino   (新建)
│   └── app_state.*        ──參考──→   │   └── record_store.*     (新建：3天紀錄)
│   ├── mtr/kmb/eta_api.*  ✗ 不複製
│   └── stops.h            ✗ 不複製
```

> 長期維護建議：將 `board.*`、`st7305_display.*`、`display_epaper.*` 等共用檔案抽成獨立 git submodule 或 shared library，避免兩 repo 漂移。初期可直接 copy-paste，BUS-ETA 已驗證可正常運作。

---

## 12. 下一步行動

1. **從 BUS-ETA 複製共用模組**至 `firmware/temp-monitor-esp32/`（見 §8.1 腳本）。
2. **建立 `temp-monitor-esp32.ino`**：參考 `eta-app-esp32.ino` 的開機 / Wi-Fi / NTP / 感測器主迴圈。
3. **新建 `ui_lvgl.cpp`**：日期時間標題列 + 溫濕度大字 + `lv_chart` 折線圖（複用 `stylePanel`、`batterySymbol`）。
4. **新建 `record_store.cpp`**：864 筆 NVS 環形緩衝區（參考 `app_state.cpp` 的 `Preferences` 模式）。
5. **修改 `config.h`**：移除 ETA 常數，新增 `SAMPLE_INTERVAL_MS`、`RECORD_MAX`、portal SSID 改 `TempMon-Setup`。

---

*文件版本：v0.3 | 硬體：Waveshare ESP32-S3-RLCD-4.2 | 上游：[BUS-ETA](https://github.com/moltmotl5-bot/BUS-ETA) | 更新日期：2026-08-25*
