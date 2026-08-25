# ESP32 螢幕溫濕度監控器 — 專案計畫

> **專案目標**：在 **Waveshare ESP32-S3-RLCD-4.2** 開發板上建置即時溫濕度監控器，於 4.2" 反射式 LCD 顯示日期、時間、當前溫濕度，並以折線圖呈現過去 12 小時溫度趨勢，同時在模組內部儲存過去 3 天的歷史紀錄。

> 硬體規格參考：[HW_spec_ref.md](./HW_spec_ref.md)（Waveshare ESP32-S3-RLCD-4.2）

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

```
firmware/temp-monitor-esp32/
├── temp-monitor-esp32.ino   # 主程式、FreeRTOS 任務調度
├── config.h                 # 接腳、取樣間隔、NTP 設定
├── st7305_display.cpp       # ST7305 驅動（可從 ETA App 移植）
├── display_lvgl.cpp           # LVGL flush → ST7305
├── ui_lvgl.cpp                # 溫濕度監控 UI（日期、數值、折線圖）
├── board.cpp                  # 電源保持 + SHTC3 讀取
├── shtc3_sensor.cpp           # SHTC3 I²C 驅動
├── clock_manager.cpp          # NTP 同步 + 軟體時鐘
├── record_store.cpp           # 3 天環形緩衝區（NVS / LittleFS）
├── input_buttons.cpp          # BOOT / KEY 按鈕處理
├── battery.cpp                # GPIO4 ADC 電池電量
├── lv_conf.h                  # LVGL 1-bit monochrome 設定
└── build.sh                   # Arduino CLI 建置腳本
```

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

- [ ] Arduino CLI 專案初始化（ESP32-S3 + OPI PSRAM + 16 MB）
- [ ] 移植 ST7305 驅動與 LVGL flush（自 ETA App）
- [ ] SHTC3 讀取測試（Serial 輸出溫濕度）
- [ ] 電源保持（GPIO17 HIGH）+ 電池 ADC 測試
- [ ] NTP 時間同步測試

### Phase 2 — 核心功能

- [ ] 實作 `TempRecord` RAM 環形緩衝區（144 筆）
- [ ] 實作 Flash 持久化（864 筆，NVS 或 LittleFS）
- [ ] 感測器 60 s 讀取、5 min 寫入 Flash
- [ ] 3 天資料滾動覆蓋邏輯
- [ ] 開機載入歷史紀錄至 RAM

### Phase 3 — UI 整合

- [ ] LVGL 版面：日期、時間、溫濕度、電池 icon
- [ ] `lv_chart` 折線圖（12 小時 / 144 點）
- [ ] 每秒更新時鐘、每 60 秒更新數值與圖表
- [ ] 狀態列與錯誤提示
- [ ] BOOT / KEY 按鈕互動

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

### 8.1 Arduino CLI 建置

```bash
chmod +x build.sh
./build.sh

# 燒錄（替換為實際 port）
PORT=/dev/ttyACM0 ./build.sh
```

### 8.2 板子設定

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

### 8.3 依賴函式庫

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

## 11. 與 ETA App 的共用資源

本專案與現有 [HW_spec_ref.md](./HW_spec_ref.md) 中的 ETA App 共用同一塊 **Waveshare ESP32-S3-RLCD-4.2**，可直接移植：

| 模組 | 來源 | 用途 |
|------|------|------|
| `st7305_display.cpp` | ETA App | ST7305 SPI 驅動 |
| `display_epaper.cpp` → `display_lvgl.cpp` | ETA App | LVGL flush callback |
| `board.cpp` | ETA App | 電源保持 + SHTC3 I²C |
| `input_buttons.cpp` | ETA App | BOOT / KEY 處理 |
| `battery.cpp` | ETA App | GPIO4 ADC 電量 |
| `lv_conf.h` | ETA App | 1-bit monochrome LVGL 設定 |
| `font_source_han_sans_18.c` | ETA App | 中文顯示 |

---

## 12. 下一步行動

1. **建立 `firmware/temp-monitor-esp32/` 專案**，從 ETA App 移植 ST7305 + SHTC3 + LVGL 基礎框架。
2. **實作 `record_store.cpp`**：864 筆 Flash 環形緩衝區。
3. **設計 LVGL UI**：標題列 + 溫濕度大字 + `lv_chart` 折線圖 + 狀態列。
4. **整合 NTP 時間同步**與 WiFiManager（選配）。

---

*文件版本：v0.2 | 硬體：Waveshare ESP32-S3-RLCD-4.2 | 更新日期：2026-08-25*
