#include "app_power.h"

#include "app_activity.h"
#include "app_context.h"
#include "esp_err.h"
#include "esp_log.h"
#include "pwr_key.h"
#include "ui.h"

static const char *TAG = "app";

void on_shutdown_confirmed(void)
{
    // Optional: save state, flush logs, stop peripherals, etc.
    // Then cut the latch power:
    pwr_key_set_hold(false);
}

static void pwr_evt_cb(pwr_key_event_t evt, void *user)
{
    (void)user;

    switch (evt)
    {
    case PWR_KEY_EVT_ACTIVITY_TOGGLE:
    {
        ui_page_t p = ui_get_current_page();
        if (p != UI_PAGE_DATA && p != UI_INTERVAL_DATA_PAGE)
        {
            ESP_LOGI(TAG, "PWR toggle ignored (page=%d)", (int)p);
            break;
        }

        if (!s_activity_recording)
        {
            act_cmd_t cmd = ACT_CMD_START;
            if (ui_take_interval_start_armed())
                cmd = ACT_CMD_START_INTERVAL_NORMAL;

            xQueueSend(s_act_q, &cmd, 0);
        }
        else
        {
            // Recording -> ask user
            if (p == UI_INTERVAL_DATA_PAGE)
            {
                ui_show_stop_save_prompt_with_text("Finish interval and save?");
            }
            else
            {
                ui_show_stop_save_prompt();
            }
        }
        break;
    }

    case PWR_KEY_EVT_SHUTDOWN_PROMPT:
        // Keep your previous behavior (optional)
        ui_show_shutdown_prompt();
        ESP_LOGI(TAG, "Long press 5s: show shutdown prompt");
        break;

    case PWR_KEY_EVT_SHORT_PRESS:
    default:
        // optional: ignore short press for now
        break;
    }
}

void app_pwr_key_setup(void)
{
    // Start the power key task
    pwr_key_config_t cfg = {
        .key_gpio = GPIO_NUM_6,
        .hold_gpio = GPIO_NUM_7,
        .key_active_low = true,
        .debounce_ms = 30,
        .poll_ms = 20,
        .click_max_ms = 600,
        //.toggle_hold_ms = 1600,
        .prompt_hold_ms = 2000,
    };
    ESP_ERROR_CHECK(pwr_key_init(&cfg, pwr_evt_cb, NULL));

    // Keep power latched on
    pwr_key_set_hold(true);
}
