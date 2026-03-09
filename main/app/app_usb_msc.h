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
 * Enter USB storage mode: unmount SD, expose as MSC.
 * Fails if recording or SD not mounted. Caller must ensure no activity is recording.
 */
esp_err_t app_usb_msc_enter(void);

/**
 * Leave USB storage mode: stop MSC, remount SD.
 */
esp_err_t app_usb_msc_leave(void);
