# RowCoach — Open DIY Rowing GPS & Stroke Coach

A DIY, open-source rowing GPS and stroke coaching device built on ESP32-S3. Combines IMU-based stroke detection and GPS-based speed/pace with a compact LVGL touchscreen UI — a lower-cost alternative for coaches and athletes.

---

## 1. Features

RowCoach provides:

| Feature | Description |
|--------|-------------|
| **Real-time stroke metrics** | Stroke count, SPM (strokes per minute), and stroke length from onboard IMU (QMI8658) |
| **GPS speed & pace** | Instant and average pace (e.g. /500 m), speed, and distance via GT-U8 GNSS |
| **Configurable data slots** | Three slots on the main data page; choose from time, strokes, SPM, pace, distance, speed, stroke length |
| **Activity logging** | CSV logs to SD card with configurable split intervals |
| **Activity browser** | Browse saved activities, view summaries (distance, duration, avg pace), and drill into split-level detail |
| **Status bar** | Time (from RTC), battery level, GPS fix status |
| **Dark/light theme** | Theme and orientation configurable in settings |
| **Power management** | Power button, shutdown prompt, battery monitoring |

---

## 2. Bill of Materials (BOM)

The base is a **Waveshare 2.8" LCD touch** module (ESP32-S3 + display + touch + QMI8658 IMU + PCF85063 RTC + TF slot + battery charging circuitry). You add the following to complete the speed coach:

| # | Item | Description | Notes |
|---|------|-------------|-------|
| 1 | **Waveshare ESP32-S3 2.8" Touch LCD** | Base module (e.g. ESP32-S3-Touch-LCD-2.8B or 2.8C) | 240×320 or 480×480, ST7789/CST328, QMI8658, PCF85063, TF slot |
| 2 | **RTC backup battery** | CR2032 or compatible coin cell for RTC | Keeps time when main power is off; connect to RTC battery header |
| 3 | **LiPo battery** | 3.7 V LiPo (e.g. 500–1000 mAh) | For portable use; connect to 2-pin MX1.25 battery header |
| 4 | **GT-U8 GPS module** | GPS/BDS dual-mode GNSS module | UART 9600 baud; connect TX→RXD, RX→TXD (GPIO43/44) |
| 5 | **microSD card** | For activity logging | Optional; insert into onboard TF slot |

### Wiring (GT-U8 to base module)

| GT-U8 | Base module (12-pin) |
|-------|----------------------|
| VCC   | 3V3                  |
| GND   | GND                  |
| TX    | RXD (GPIO44)         |
| RX    | TXD (GPIO43)         |

---

## 3. Background

RowCoach aims to make basic rowing performance tools accessible and affordable:

- **Stroke detection** — IMU-driven stroke counting and SPM
- **GPS metrics** — Speed, pace, and distance from GNSS
- **Touchscreen UI** — Start/stop, settings, activity logs

The codebase is small and componentized so you can adapt hardware, tweak algorithms, and extend features. Tested with a Waveshare ESP32-S3 2.8" touch LCD; pin mappings are in `components/` and `sdkconfig`.

**Build & flash:** `idf.py build` and `idf.py flash` (ESP-IDF required). Or use [flash.html](flash.html) for browser-based flashing.

---

## 4. Dev changelog

Recent development entries (newest first):

- 2026-03-09 — settings — added USB device mode
- 2026-01-28 — ui — simplify UI
- 2026-01-21 — data page — replace power with stroke length; ui relayout
- 2026-01-20 — ui relayout; interval_csv update
- 2026-01-18 — update folder structure
- 2026-01-13 — activity_log — refine log and read; simplify menu; remove race
- 2026-01-12 — race_program — init and fine tune; activity_log update
- 2026-01-11 — refine activity_type
- 2026-01-10 — refine activity_detail
- 2026-01-09 — refactor folder; settings — datetime row update
- 2026-01-08 — activity summary — page view UI; interval data page and reminder prompts; activity_detail nav_bar
- 2026-01-07 — interval setup page UI; activity ui layout update

---

## 5. Pins & modules

| Module | Component | Default / Notes |
|--------|-----------|-----------------|
| I2C (sensor bus) | [i2c_helper](components/i2c_helper) | 2 I2C ports; see `sdkconfig` |
| IMU (QMI8658) | [qmi8658](components/qmi8658) | I2C1: SDA=11, SCL=10, INT1=13, INT2=12 |
| GPS (GT-U8) | [gps_gt_u8](components/gps_gt_u8) | UART1: TX=43, RX=44, 9600 baud |
| Display (ST7789) | [lcd_st7789](components/lcd_st7789) | SPI: MOSI=45, SCLK=40, DC=41, CS=42, RST=39, BL=5 |
| Touch (CST328) | [touch_cst328](components/touch_cst328) | I2C0: SDA=1, SCL=3, RST=2, INT=4 |
| Power button | [pwr_key](components/pwr_key) | See component config |
| Battery monitor | [battery_drv](components/battery_drv) | ADC; GPIO8 (CH7), voltage divider |
| RTC (PCF85063) | [rtc_pcf85063](components/rtc_pcf85063) | I2C1 (shared with IMU) |
| SD/MMC | [sd_mmc_helper](components/sd_mmc_helper) | TF slot; SDIO or SPI |

---

See `main/ui/` for UI details and `components/` for hardware drivers.
