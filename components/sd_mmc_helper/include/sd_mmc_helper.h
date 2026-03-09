#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"

/**
 * Simple wrapper around SDMMC + FATFS mount for the Waveshare ESP32-S3
 * 2.8" Touch LCD board.
 *
 * Uses SDMMC 1-bit bus with pins:
 *   CLK = IO14
 *   CMD = IO17
 *   D0  = IO16
 * (D1..D3 unused)
 *
 * You can later move these to Kconfig if you want.
 */

typedef struct {
    bool          mounted;
    sdmmc_card_t *card;
    const char   *mount_point;   // e.g. "/sdcard"
} sd_mmc_helper_t;

/**
 * Mount the SD card and register FATFS at `mount_point`
 * (e.g. "/sdcard").
 *
 * After this, you can use standard C file APIs on paths like
 * "/sdcard/imu_log.csv".
 */
esp_err_t sd_mmc_helper_mount(sd_mmc_helper_t *sd,
                              const char *mount_point);

/**
 * Unmount the SD card and detach the VFS.
 */
esp_err_t sd_mmc_helper_unmount(sd_mmc_helper_t *sd);

/**
 * Convenience helper to write a text buffer as a file.
 *
 * relative_path: path relative to mount_point ("imu_dummy.csv")
 * data:          zero-terminated text buffer
 * append:        false → overwrite/create, true → append
 */
esp_err_t sd_mmc_helper_write_text(sd_mmc_helper_t *sd,
                                   const char *relative_path,
                                   const char *data,
                                   bool append);

/**
 * Init SD card for raw block access (e.g. TinyUSB MSC).
 * Call when FAT is NOT mounted. Uses same pins as mount.
 * Caller must free *out_card with sd_mmc_helper_deinit_sdmmc_raw() when done.
 */
esp_err_t sd_mmc_helper_init_sdmmc_raw(sdmmc_card_t **out_card);

/**
 * Deinit SD card from raw access. Frees the card.
 */
esp_err_t sd_mmc_helper_deinit_sdmmc_raw(sdmmc_card_t *card);
