#include "app_settings.h"

#include "app_context.h"
#include "esp_log.h"
#include "ui.h"

static const char *TAG = "app";

void on_auto_rotate_setting_changed(bool enabled)
{
    s_auto_rotate_enabled = enabled;
    ESP_LOGI(TAG, "Auto-rotate %s", enabled ? "ON" : "OFF");
}

void on_dark_mode_setting_changed(bool enabled)
{
    ESP_LOGI(TAG, "Dark mode %s", enabled ? "ON" : "OFF");

    // Simple example: call a UI helper you implement later
    // ui_set_dark_mode(enabled);
}
