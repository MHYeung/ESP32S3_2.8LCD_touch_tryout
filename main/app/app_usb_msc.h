#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * USB Mass Storage mode: exposes SD card as USB drive when connected to PC.
 * When active, FAT is unmounted; when inactive, FAT is mounted for normal use.
 */

/** Check if USB storage mode is currently active. */
bool app_usb_msc_is_active(void);

/**
 * Queue USB storage enter on the USB worker.
 * Safe from the LVGL thread (does not block on SD/TinyUSB).
 */
esp_err_t app_usb_msc_request_enter(void);

/**
 * Queue USB storage leave on the USB worker.
 * Safe from the LVGL thread.
 */
esp_err_t app_usb_msc_request_leave(void);
