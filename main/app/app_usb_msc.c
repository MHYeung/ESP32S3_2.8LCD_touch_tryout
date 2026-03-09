#include "app_usb_msc.h"
#include "app_context.h"
#include "sd_mmc_helper.h"
#include "driver/sdmmc_host.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "usb_msc";

static bool s_usb_msc_active = false;
static sdmmc_card_t *s_msc_card = NULL;
static tinyusb_msc_storage_handle_t s_msc_storage = NULL;

bool app_usb_msc_is_active(void)
{
    return s_usb_msc_active;
}

esp_err_t app_usb_msc_enter(void)
{
    if (s_usb_msc_active)
    {
        ESP_LOGW(TAG, "USB MSC already active");
        return ESP_OK;
    }

    if (s_activity_recording)
    {
        ESP_LOGE(TAG, "Cannot enter USB mode while recording");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_sd.mounted)
    {
        ESP_LOGE(TAG, "SD not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = sd_mmc_helper_unmount(&s_sd);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SD unmount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* FAT unmount does not deinit SDMMC host; deinit so raw init can reinit cleanly */
    sdmmc_host_deinit();

    ret = sd_mmc_helper_init_sdmmc_raw(&s_msc_card);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SD raw init failed: %s", esp_err_to_name(ret));
        sd_mmc_helper_mount(&s_sd, "/sdcard");
        return ret;
    }

    tinyusb_msc_storage_config_t msc_cfg = { 0 };
    msc_cfg.medium.card = s_msc_card;
    msc_cfg.fat_fs.base_path = NULL;
    msc_cfg.fat_fs.config.format_if_mount_failed = false;
    msc_cfg.fat_fs.config.max_files = 5;
    msc_cfg.fat_fs.config.allocation_unit_size = 16 * 1024;
    msc_cfg.fat_fs.do_not_format = true;
    msc_cfg.fat_fs.format_flags = 0;
    msc_cfg.mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB;

    tinyusb_msc_driver_config_t driver_cfg = { 0 };
    ret = tinyusb_msc_install_driver(&driver_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "MSC driver install failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = tinyusb_msc_new_storage_sdmmc(&msc_cfg, &s_msc_storage);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MSC storage new failed: %s", esp_err_to_name(ret));
        tinyusb_msc_uninstall_driver();
        goto fail;
    }

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "TinyUSB install failed: %s", esp_err_to_name(ret));
        tinyusb_msc_delete_storage(s_msc_storage);
        tinyusb_msc_uninstall_driver();
        goto fail;
    }

    s_usb_msc_active = true;
    ESP_LOGI(TAG, "USB storage mode active - plug USB to computer");
    return ESP_OK;

fail:
    sd_mmc_helper_deinit_sdmmc_raw(s_msc_card);
    s_msc_card = NULL;
    sd_mmc_helper_mount(&s_sd, "/sdcard");
    return ret;
}

esp_err_t app_usb_msc_leave(void)
{
    if (!s_usb_msc_active)
    {
        ESP_LOGW(TAG, "USB MSC not active");
        return ESP_OK;
    }

    /* Order: stop MSC storage, then TinyUSB, then MSC driver, then SD */
    (void)tinyusb_msc_delete_storage(s_msc_storage);
    s_msc_storage = NULL;
    tinyusb_driver_uninstall();
    (void)tinyusb_msc_uninstall_driver();

    sd_mmc_helper_deinit_sdmmc_raw(s_msc_card);
    s_msc_card = NULL;

    esp_err_t ret = sd_mmc_helper_mount(&s_sd, "/sdcard");
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SD remount failed: %s", esp_err_to_name(ret));
    }

    s_usb_msc_active = false;
    ESP_LOGI(TAG, "USB storage mode off, SD remounted");
    return ret;
}
