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
    case PWR_KEY_EVT_SHORT_PRESS:
        ui_toggle_display_sleep();
        break;

    case PWR_KEY_EVT_ACTIVITY_TOGGLE:
    {
        if (ui_is_display_sleep())
        {
            ui_set_display_sleep(false);
            break;
        }
        ui_notify_user_activity();

        ui_page_t p = ui_get_current_page();
        bool race_armed = ui_take_race_start_armed();
        bool step_armed = ui_take_step_start_armed();
        bool interval_armed = ui_take_interval_start_armed();
        bool on_live = (p == UI_PAGE_DATA ||
                        p == UI_INTERVAL_DATA_PAGE ||
                        p == UI_RACE_DATA_PAGE);

        if (!s_activity_recording)
        {
            act_cmd_t cmd = ACT_CMD_START;
            if (race_armed || p == UI_RACE_DATA_PAGE)
                cmd = ACT_CMD_START_RACE;
            else if (step_armed)
                cmd = ACT_CMD_START_INTERVAL_STEP;
            else if (interval_armed || p == UI_INTERVAL_DATA_PAGE)
                cmd = ACT_CMD_START_INTERVAL_NORMAL;
            else if (p != UI_PAGE_DATA)
            {
                ESP_LOGI(TAG, "PWR toggle ignored (page=%d)", (int)p);
                break;
            }

            esp_err_t err = app_activity_post_cmd(cmd);
            if (err != ESP_OK)
                ESP_LOGW(TAG, "start post: %s", esp_err_to_name(err));
        }
        else if (!on_live)
        {
            ESP_LOGI(TAG, "PWR toggle ignored (page=%d)", (int)p);
            break;
        }
        else
        {
            if (p == UI_INTERVAL_DATA_PAGE)
            {
                ui_show_stop_save_prompt_with_text("Finish interval and save?");
            }
            else if (p == UI_RACE_DATA_PAGE)
            {
                ui_show_stop_save_prompt_with_text("Finish race and save?");
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

    default:
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
        .prompt_hold_ms = 5000,
    };
    ESP_ERROR_CHECK(pwr_key_init(&cfg, pwr_evt_cb, NULL));

    // Keep power latched on
    pwr_key_set_hold(true);
}
