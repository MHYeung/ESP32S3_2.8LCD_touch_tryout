#include "app_activity.h"

#include "app_context.h"
#include "activity.h"
#include "activity_log.h"
#include "esp_log.h"
#include "interval_program.h"
#include "ui.h"
#include "ui_data_page.h"
#include "ui_interval_data_page.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "app";

void activity_worker_task(void *arg)
{
    (void)arg;

    act_cmd_t cmd;

    for (;;)
    {
        if (xQueueReceive(s_act_q, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        if (cmd == ACT_CMD_START_INTERVAL_NORMAL)
        {
            ui_go_to_page(UI_INTERVAL_DATA_PAGE, true);
        }
        else
        {
            ui_go_to_page(UI_PAGE_DATA, true);
        }

        if (s_activity_mutex)
            xSemaphoreTake(s_activity_mutex, portMAX_DELAY);

        if (cmd == ACT_CMD_START)
        {
            s_activity_recording = true;
            s_session_time_s = 0.0f;
            ui_set_interval_data_visible(false);
            s_interval_done_queued = false;

            uint32_t id = s_activity_next_id++;
            activity_init(&s_activity, id);
            activity_start(&s_activity, time(NULL));
            s_activity.activity_type = ACTIVITY_NORMAL;

            activity_log_set_split_interval(&s_act_log, s_current_split_m);

            if (s_sd.mounted)
            {
                // Starts the per-stroke CSV log file on the SD card
                activity_log_start(&s_act_log, &s_sd, s_activity.start_ts, s_activity.id, ACTIVITY_NORMAL);
            }

            s_last_session_stroke_count = 0;

            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            data_page_show_activity_toast(true);
            ESP_LOGI("ACT", "START id=%lu", (unsigned long)id);
        }

        if (cmd == ACT_CMD_START_INTERVAL_NORMAL)
        {
            // Start normal activity logging first (same as ACT_CMD_START)
            s_activity_recording = true;
            s_session_time_s = 0.0f;
            s_interval_done_queued = false;

            ui_set_interval_data_visible(true);
            interval_data_page_hide_start_prompt();
            ui_go_to_page(UI_INTERVAL_DATA_PAGE, false);
            uint32_t id = s_activity_next_id++;
            activity_init(&s_activity, id);
            activity_start(&s_activity, time(NULL));
            s_activity.activity_type = ACTIVITY_INTERVAL_NORMAL;
            resolve_interval_split_m();
            activity_log_set_split_interval(&s_act_log, s_current_split_m);

            if (s_sd.mounted)
            {
                activity_log_start(&s_act_log, &s_sd, s_activity.start_ts, s_activity.id, ACTIVITY_INTERVAL_NORMAL);
            }

            s_last_session_stroke_count = 0;

            // Start interval program (you likely already have these APIs or can add them)
            interval_program_start();

            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            data_page_show_activity_toast(true);
            ESP_LOGI("ACT", "START_INTERVAL id=%lu", (unsigned long)id);
        }

        if (cmd == ACT_CMD_STOP_SAVE)
        {
            s_activity_recording = false;
            ui_set_interval_data_visible(false);
            interval_data_page_hide_start_prompt();
            interval_program_stop();
            s_interval_done_queued = false;

            // Stop logic updates end time and averages
            activity_stop(&s_activity, time(NULL));

            activity_t snapshot = s_activity;
            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            data_page_show_activity_toast(false);
            ESP_LOGI("ACT", "STOP id=%lu Dist=%.1fm", (unsigned long)snapshot.id, (double)snapshot.distance_m);

            // STOP THE LOGGER: This flushes and closes the CSV file.
            // The file is now complete and saved on the SD card.
            activity_log_stop(&s_act_log);

            FILE *f = fopen("/sdcard/activities/index.csv", "a");
            if (f)
            {
                uint32_t duration_s = (uint32_t)(snapshot.duration_ms / 1000);
                float avg_pace_s_per500 = 0.0f;
                if (snapshot.avg_speed_mps > 0.01f)
                {
                    avg_pace_s_per500 = 500.0f / snapshot.avg_speed_mps; // seconds per 500m
                }

                // IMPORTANT: store the base path (no _Splits/_Strokes suffix)
                // In your activity_log.c you build: "activities/<base_name>"
                // Use the field you actually have in activity_log_t:
                //   - if it's log->filename_base use that
                //   - if it's log->rel_path rename accordingly
                fprintf(f, "%lu,%ld,%lu,%.2f,%.2f,%s\n",
                        (unsigned long)snapshot.id,
                        (long)snapshot.start_ts,
                        (unsigned long)duration_s,
                        (double)snapshot.distance_m,
                        (double)avg_pace_s_per500,
                        s_act_log.filename_base /* or s_act_log.rel_path */);

                fclose(f);
            }
        }
    }
}

void on_stop_save_confirmed(void)
{
    act_cmd_t cmd = ACT_CMD_STOP_SAVE;
    xQueueSend(s_act_q, &cmd, 0);
}

void activity_logger_task(void *arg)
{
    (void)arg;

    activity_log_row_t row;
    for (;;)
    {
        if (xQueueReceive(s_log_q, &row, portMAX_DELAY) == pdTRUE)
        {
            // Only append if file is open
            if (s_act_log.opened)
            {
                activity_log_append(&s_act_log, &row);
            }
        }
    }
}

void on_split_interval_changed(uint32_t length_m)
{
    ESP_LOGI(TAG, "UI Callback: Split Interval changed to %lu meters", length_m);

    // Update the logger configuration immediately
    s_current_split_m = length_m;
    activity_log_set_split_interval(&s_act_log, length_m);
}

uint32_t resolve_interval_split_m(void)
{
    interval_config_t cfg = {0};
    interval_program_get_config(&cfg);

    if (cfg.work.unit == INTERVAL_UNIT_DISTANCE && cfg.work.value > 0)
        return cfg.work.value;

    return s_current_split_m;
}
