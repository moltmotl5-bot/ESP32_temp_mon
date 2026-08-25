# ESP32 螢幕溫濕度監控器 — 專案計畫

> **專案目標**：在 ESP32 上建置一個即時溫濕度監控器，於螢幕顯示日期、時間、當前溫濕度，並以折線圖呈現過去 12 小時溫度趨勢，同時在模組內部儲存過去 3 天的歷史紀錄。

---

## 1. 硬體清單（請依實際規格填入）

> ⚠️ **待確認**：請將您手邊的 ESP32 硬體規格補充至下表。若規格與預設假設不同，需同步調整接腳與函式庫選型。

| 元件 | 型號 / 規格 | 數量 | 備註 |
|------|-------------|------|------|
| 主控板 | `___________`（例：ESP32-WROOM-32 DevKit） | 1 | |
| 溫濕度感測器 | `___________`（例：DHT22 / BME280 / SHT31） | 1 | 需確認 I2C 或單線協定 |
| 顯示器 | `___________`（例：2.4" ILI9341 320×240 TFT） | 1 | 需確認 SPI / I2C 介面 |
| RTC 時鐘模組 | `___________`（例：DS3231，選配） | 0–1 | 無 WiFi 時必備 |
| 電源 | `___________`（例：5V 1A USB / 18650） | 1 | |
| 連接線 | 杜邦線 / 排針 | 若干 | |

### 1.1 預設硬體假設（若尚未提供規格，以此為開發基準）

若您的硬體規格尚未確認，本計畫預設以下組合作為開發起點：

```
ESP32 DevKit V1
  ├── DHT22（溫濕度，單線 GPIO）
  ├── ILI9341 TFT 2.4" 320×240（SPI）
  ├── DS3231 RTC（I2C，備援時間來源）
  └── WiFi（NTP 時間同步）
```

### 1.2 建議接腳配置（預設）

| 功能 | ESP32 GPIO | 備註 |
|------|-----------|------|
| DHT22 DATA | GPIO 4 | 需 4.7kΩ 上拉至 3.3V |
| TFT MOSI | GPIO 23 | SPI |
| TFT SCLK | GPIO 18 | SPI |
| TFT CS | GPIO 5 | |
| TFT DC | GPIO 2 | |
| TFT RST | GPIO 15 | |
| TFT BL（背光） | GPIO 21 | PWM 可調亮度 |
| RTC SDA | GPIO 21 | I2C（與背光衝突時改 GPIO 22） |
| RTC SCL | GPIO 22 | I2C |

> 接腳衝突時優先保留 SPI 顯示器與感測器，RTC 可改接或使用軟體 I2C。

---

## 2. 功能需求

### 2.1 螢幕顯示內容

```
┌─────────────────────────────────────┐
│  2026-08-25        07:00:32         │  ← 日期 + 時間
├─────────────────────────────────────┤
│                                     │
│   溫度          濕度                 │
│   26.5 °C       58.2 %              │  ← 當前數值（大字體）
│                                     │
├─────────────────────────────────────┤
│  過去 12 小時溫度                    │
│  30┤      ╭─╮                       │
│  25┤  ╭───╯ ╰──╮                    │  ← 折線圖
│  20┤──╯        ╰──                  │
│    └────────────────────────        │
│    -12h              現在            │
└─────────────────────────────────────┘
```

| 區塊 | 內容 | 更新頻率 |
|------|------|----------|
| 日期 | `YYYY-MM-DD` | 每秒 |
| 時間 | `HH:MM:SS` | 每秒 |
| 溫度 | 小數一位，單位 °C | 每 60 秒（感測器讀取週期） |
| 濕度 | 小數一位，單位 % | 每 60 秒 |
| 折線圖 | 過去 12 小時溫度曲線 | 每 60 秒重繪 |

### 2.2 資料儲存需求

| 項目 | 規格 |
|------|------|
| 儲存位置 | ESP32 內建 Flash（LittleFS 分割區） |
| 保留天數 | **3 天** |
| 取樣間隔 | **5 分鐘**（可調整） |
| 每筆紀錄 | 時間戳（4 B）+ 溫度 float（4 B）+ 濕度 float（4 B）= **12 B** |
| 3 天總筆數 | 864 筆（288 筆/天 × 3 天） |
| 預估容量 | ~10 KB + 檔案系統開銷 ≈ **32 KB 分割區** |

### 2.3 折線圖資料來源

- 12 小時 ÷ 5 分鐘 = **144 個資料點**
- 直接從 3 天環形緩衝區（Ring Buffer）讀取最近 144 筆
- Y 軸自動縮放（min − 2°C 至 max + 2°C，下限不低於 0°C）

---

## 3. 軟體架構

### 3.1 技術堆疊

| 層級 | 選型 | 理由 |
|------|------|------|
| 開發框架 | **PlatformIO** + Arduino ESP32 core | 依賴管理方便、除錯容易 |
| 顯示驅動 | **TFT_eSPI** 或 **LovyanGFX** | 高效能 SPI 繪圖，支援折線圖 |
| 感測器 | **DHT sensor library** / **Adafruit BME280** | 依實際感測器選型 |
| 時間 | **NTPClient** + **DS3231 RTC** 雙來源 | WiFi 斷線時 RTC 維持時間 |
| 儲存 | **LittleFS** + 自訂 Ring Buffer | 比 SPIFFS 更穩定，支援磨損均衡 |
| JSON 序列化 | **ArduinoJson**（選配） | 若需匯出歷史資料 |

### 3.2 模組划分

```
src/
├── main.cpp              # 程式進入點、FreeRTOS 任務調度
├── config.h              # 接腳、取樣間隔、分割區大小等常數
├── sensors/
│   └── temp_humidity.cpp   # 感測器讀取與異常處理
├── display/
│   ├── ui_layout.cpp       # 版面配置
│   └── chart_renderer.cpp  # 折線圖繪製邏輯
├── time/
│   └── clock_manager.cpp   # NTP + RTC 同步
└── storage/
    ├── record_store.cpp    # 環形緩衝區讀寫
    └── record_store.h      # Record { uint32_t ts; float temp; float hum; }
```

### 3.3 FreeRTOS 任務分工

| 任務 | 週期 | 優先級 | 職責 |
|------|------|--------|------|
| `sensorTask` | 60 s | 2 | 讀取溫濕度、寫入 Ring Buffer |
| `displayTask` | 1 s | 1 | 更新時鐘、重繪數值與圖表 |
| `timeSyncTask` | 3600 s | 0 | NTP 同步、寫入 RTC |
| `storageTask` | 事件驅動 | 2 | 持久化 Ring Buffer 至 LittleFS |

> ESP32 雙核心：Core 0 跑 WiFi/NTP，Core 1 跑感測器與顯示，避免 SPI 與 WiFi 干擾。

---

## 4. 資料結構設計

### 4.1 單筆紀錄

```cpp
struct TempRecord {
    uint32_t timestamp;  // Unix epoch（秒）
    float    temperature; // °C
    float    humidity;    // %
};
// sizeof(TempRecord) == 12 bytes
```

### 4.2 環形緩衝區（RAM + Flash 雙層）

```
RAM Ring Buffer（144 筆）          Flash Ring Buffer（864 筆）
┌───┬───┬───┬ ... ─┬───┐          ┌───┬───┬ ... ─┬───┐
│ 0 │ 1 │ 2 │      │143│          │ 0 │ 1 │      │863│
└───┴───┴───┴ ... ─┴───┘          └───┴───┴ ... ─┴───┘
  ↑ 折線圖直接讀取                    ↑ 3 天歷史，每 5 分鐘寫入
                                    每 10 分鐘 flush 至 LittleFS
```

### 4.3 Flash 分割區配置

在 `platformio.ini` 或 `partitions.csv` 中新增：

```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x140000
spiffs,   data, spiffs,  0x150000,0xB0000
```

> 建議使用 **LittleFS** 取代 SPIFFS（ESP32 Arduino core 3.x 已預設支援）。

---

## 5. UI 設計細節

### 5.1 色彩配置（深色主題，適合長時間顯示）

| 元素 | 色碼 | 用途 |
|------|------|------|
| 背景 | `#1A1A2E` | 深藍灰 |
| 標題文字 | `#EAEAEA` | 日期時間 |
| 溫度數值 | `#FF6B6B` | 紅色強調 |
| 濕度數值 | `#4ECDC4` | 青色 |
| 折線圖線條 | `#FFD93D` | 金黃色 |
| 格線 | `#333355` | 低對比輔助線 |

### 5.2 折線圖演算法

1. 從 Ring Buffer 取出最近 144 筆有效紀錄
2. 計算 `yMin = min(temp) − 2`，`yMax = max(temp) + 2`
3. 將每筆 `(index, temp)` 映射至螢幕像素 `(x, y)`
4. 使用 `TFT_eSPI::drawLine()` 或 `drawFastVLine` 批次繪製
5. 僅在數值區域變化時局部重繪（避免全螢幕閃爍）

### 5.3 錯誤狀態顯示

| 狀況 | 顯示 |
|------|------|
| 感測器讀取失敗 | 溫濕度顯示 `--.-` |
| WiFi 斷線 | 時間來源切換至 RTC，右上角顯示 📡✗ |
| Flash 寫入失敗 | 底部警告列顯示「儲存異常」 |

---

## 6. 時間同步策略

```
開機
  │
  ├─ WiFi 可用 ──→ NTP 同步 ──→ 寫入 DS3231 RTC
  │
  └─ WiFi 不可用 ─→ 讀取 DS3231 RTC
                    │
                    └─ RTC 無效 ─→ 顯示 0000-00-00，提示設定

運行中：每小時 NTP 重新同步，誤差 > 2 秒才更新 RTC
```

---

## 7. 開發階段計畫

### Phase 1 — 硬體驗證（基礎）

- [ ] 確認接腳與實際硬體規格
- [ ] PlatformIO 專案初始化
- [ ] 感測器單元測試（Serial 輸出溫濕度）
- [ ] 顯示器單元測試（顯示文字與色塊）
- [ ] RTC / NTP 時間讀取測試

### Phase 2 — 核心功能

- [ ] 實作 `TempRecord` 環形緩衝區（RAM）
- [ ] 實作 LittleFS 持久化讀寫
- [ ] 感測器定時取樣（60 s 讀取、5 min 寫入 Flash）
- [ ] 3 天資料滾動覆蓋邏輯

### Phase 3 — UI 整合

- [ ] 版面配置：日期、時間、溫濕度大字顯示
- [ ] 折線圖渲染（12 小時 / 144 點）
- [ ] 每秒更新時鐘、每分鐘更新數值與圖表
- [ ] 錯誤狀態與 WiFi 指示

### Phase 4 — 穩定化與測試

- [ ] 斷電恢復測試（重開機後歷史資料完整）
- [ ] 72 小時連續運行測試（記憶體洩漏、Flash 磨損）
- [ ] WiFi 斷線 / 重連測試
- [ ] 感測器異常容錯測試

### Phase 5 — 延伸功能（選配）

- [ ] Web 介面查詢 3 天歷史（ESP32 內建 HTTP Server）
- [ ] OTA 韌體更新
- [ ] 溫度超標告警（蜂鳴器 / 螢幕閃爍）
- [ ] 資料匯出（CSV via Serial / Web）

---

## 8. 依賴套件（platformio.ini 範例）

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

board_build.filesystem = littlefs
board_build.partitions = partitions.csv

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    adafruit/DHT sensor library@^1.4.6
    adafruit/Adafruit Unified Sensor@^1.1.14
    arduino-libraries/NTPClient@^3.2.1
    rtclib/RTClib@^2.1.4
    bblanchon/ArduinoJson@^7.0.4

build_flags =
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    ; 依實際接腳修改 TFT_eSPI 設定
```

---

## 9. 風險與注意事項

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| Flash 頻繁寫入 | 壽命縮短 | 5 分鐘取樣 + 批次 flush，避免每秒寫入 |
| SPI 顯示與 WiFi 干擾 | 畫面花屏 | 顯示更新時暫停 WiFi 或使用 DMA |
| DHT22 讀取不穩定 | 數值跳動 | 連續 3 次讀取取中位數；改 BME280 可解決 |
| 記憶體不足 | 重啟 | 折線圖用 int16 座標陣列（144×4 = 576 B） |
| 無 WiFi 且無 RTC | 時間錯誤 | 開機提示，保留上次同步時間 |

---

## 10. 驗收標準

- [ ] 螢幕正確顯示即時日期（YYYY-MM-DD）與時間（HH:MM:SS）
- [ ] 溫度、濕度每 60 秒更新，精度 ±0.5°C / ±2% RH
- [ ] 折線圖顯示過去 12 小時溫度，至少 144 個有效點
- [ ] 斷電重啟後，3 天內歷史紀錄不遺失
- [ ] 連續運行 72 小時無當機、無記憶體洩漏
- [ ] 感測器拔除時顯示錯誤狀態，不當機

---

## 11. 下一步行動

1. **請補充硬體規格**：將實際的 ESP32 板型、感測器型號、螢幕型號與尺寸填入第 1 節表格。
2. **確認取樣間隔**：預設 5 分鐘；若需更細緻的折線圖可改為 1 分鐘（Flash 用量 ×5）。
3. **開始 Phase 1**：硬體接線完成後，建立 PlatformIO 專案並執行感測器 / 顯示器單元測試。

---

*文件版本：v0.1 | 建立日期：2026-08-25*
