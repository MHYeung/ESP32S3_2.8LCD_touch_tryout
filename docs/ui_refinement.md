# ESP32-S3 Speed Coach — UI and product architecture

Portrait 240×320 is the reference on-water layout. Landscape is supported but secondary.

## Live screen model

Active screens are instruments, not app pages:

- No conventional app bar
- Compact 22 px status rail: recording, water-lock, GPS quality (including stale), battery
- Wall-clock time stays on menu / settings / history
- Primary value: instant pace `/500 m` in tabular 56 px numerals
- Secondary pair: SPM and distance (32 px tabular)
- Slim split row: progress in the configured split, average pace, last-split delta vs average

Metric slots persist in NVS. While recording, taps and swipes are locked unless the athlete long-presses the status rail. The physical PWR key always starts/stops.

Interval live page: phase + remaining is dominant; SPM and pace share the middle; round is compact. WORK/REST is a persistent border color. The last five seconds of a time interval recolor the remaining slot; they do not cover the numbers. Interval CSV logs phase rows only — distance splits are not written during interval sessions.

## Threading

`stroke_task` (200 Hz) is the single producer of `activity_update` and of `coach_ui_snapshot_t`. The LVGL task consumes the snapshot at 5 Hz. No LVGL widget APIs from the IMU task.

GPS dropout: speed is omitted from averages when there is no valid fix. Session duration and strokes still advance.

## Immediate vs later

Shipped in this pass: water lock, stale GPS, split progress/delta, brightness + auto-dim, PWR shortcuts, NVS activity IDs, persisted metric slots, race mode (virtual-boat ahead/behind), step-test rate ladder, themed menu tiles, C3 tracker BLE connect.

### BLE: C3 tracker pod (connection first)

The menu **Sensors** tile scans for the C3 motion-tracker service UUID `9b7e1000-2b2f-4f71-9b86-4bb2e6d54f00` (see `esp32c3_motion_tracker` `sensor_protocol.h`). NimBLE is a **central + observer only** — the coach does not advertise. A tap connects at GAP level and stores the address in NVS. Motion CCCD subscribe is **not** enabled yet: writing that CCCD is what starts the pod’s 56–448 Hz notify stream.

USB mass-storage stays on the Settings **Export via USB** row, not on the menu.

BLE callbacks must not touch LVGL. The sensors page polls `sensor_hub` at 300 ms. Do not enable PSRAM to hide DRAM cost; log free internal heap before and after NimBLE init.

HR straps (`0x180D`) remain a later pass. ESP32-S3 has no ANT+ radio.

### Shipped: race mode

Target pace plus race distance. The live race page shows seconds vs a virtual boat (`delta_s = elapsed - distance / v_target`), remaining distance, and projected finish from session average speed. Green fill = ahead by more than 1 s; red = behind by more than 1 s; a 1 s deadband avoids strobing. `ACTIVITY_RACE` / `ACT_CMD_START_RACE` start this session type.

### Later: link mode

Versioned, sequence-numbered peer packets with monotonic timestamps. Evaluate ESP-NOW (low latency broadcast) vs BLE coexistence and battery before choosing transport. Same-module comparison only after the snapshot path is stable.

### Later: presets and sport profiles

Saved interval workouts. Rowing / dragon boat / generic paddle profiles that override IMU axis, stroke-period limits, and default metric slots — not the UI shell.

## Hardware checklist

Portrait first, then all four rotations:

- Long values (`9:59.9`, `99.9 km/h`) do not clip
- Sunlight contrast in dark and light themes
- Water lock blocks metric taps and menu swipe; PWR still works
- GPS loss shows stale/red and does not pull average pace to `--` via zeros
- Interval WORK/REST color and 5 s cue without covering numbers
- Race ahead/behind box color with 1 s deadband; projected finish from average speed
- Step test SPM target rises each piece; SPM box tints outside +/-1 SPM
- Settings Sensors row opens the tracker scan/connect page; USB export stays in Settings
- Menu Sensors tile scans for the C3 pod UUID and connects without subscribing to motion notifies
- Menu tiles readable in light and dark (surface fill, accent icon, contrast text)
- Split rollover updates progress and delta
- Start/stop toast, save, USB export
- Auto-dim after 15 s idle while recording; touch restores
- `idf.py size-components`: flash delta from tabular fonts; LVGL stack HWM; free internal heap
- Active vs idle current before enabling `CONFIG_PM_ENABLE`

### Build environment note (2026-08-19)

This tree targets **ESP-IDF 6.0.1**. Local components now `REQUIRES` the split IDF 6 drivers (`esp_driver_i2c`, `esp_driver_gpio`, `esp_driver_spi`, `esp_driver_ledc`, `esp_driver_sdmmc`) instead of the old catch-all `driver` component. LCD/SD slot pins use `gpio_num_t` / `GPIO_NUM_NC`.

Export `C:\Espressif\.espressif\v6.0.1\esp-idf` then:

```
idf.py fullclean
idf.py build
idf.py size-components
```

Then flash and walk the checklist above. Do not enable PSRAM to hide RAM cost.

## Acceptance

- Primary pace readable at arm’s length on 240×320
- No modal covers live data during normal interval work/rest
- No LVGL calls outside the LVGL-owned context
- GPS loss does not corrupt averages
- `sdkconfig.defaults` captures target, TinyUSB, fonts, and NimBLE central
- Stroke detection and CSV columns unchanged aside from device name
