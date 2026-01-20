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
#include <string.h>
#include <time.h>

static const char *TAG = "app";

static const char *interval_unit_to_str(interval_unit_t unit)
{
    switch (unit)
    {
    case INTERVAL_UNIT_TIME:
        return "s";
    case INTERVAL_UNIT_DISTANCE:
        return "m";
    case INTERVAL_UNIT_STROKES:
        return "st";
    default:
        return "";
    }
}

static void fill_interval_log_cfg(activity_log_interval_cfg_t *out, const interval_config_t *cfg)
{
    if (!out || !cfg)
        return;
    memset(out, 0, sizeof(*out));
    out->work_value = cfg->work.value;
    strncpy(out->work_unit, interval_unit_to_str(cfg->work.unit), sizeof(out->work_unit) - 1);
    out->rest_value = cfg->rest.value;
    strncpy(out->rest_unit, interval_unit_to_str(cfg->rest.unit), sizeof(out->rest_unit) - 1);
    out->rounds = cfg->rounds;
    out->auto_advance = cfg->auto_advance;
}

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

            activity_log_set_interval_config(&s_act_log, NULL);
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

            interval_config_t cfg = {0};
            interval_program_get_config(&cfg);
            activity_log_interval_cfg_t log_cfg = {0};
            fill_interval_log_cfg(&log_cfg, &cfg);
            activity_log_set_interval_config(&s_act_log, &log_cfg);

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

            float final_time_s = snapshot.duration_ms / 1000.0f;
            if (s_act_log.opened)
            {
                bool need_final_row = !s_act_log.has_last_row ||
                                      (snapshot.distance_m - s_act_log.last_row_distance_m) >= 0.1f ||
                                      (final_time_s - s_act_log.last_row_time_s) >= 0.1f;

                if (need_final_row)
                {
                    activity_log_row_t final_row = {0};
                    float avg_pace_s = (snapshot.avg_speed_mps > 0.1f) ? (500.0f / snapshot.avg_speed_mps) : 0.0f;

                    final_row.rtc_time = snapshot.end_ts;
                    final_row.session_time_s = final_time_s;
                    final_row.total_distance_m = snapshot.distance_m;
                    final_row.pace_500m_s = avg_pace_s;
                    final_row.spm_instant = snapshot.avg_spm;
                    final_row.avg_pace_500m_s = avg_pace_s;
                    final_row.avg_speed_mps = snapshot.avg_speed_mps;
                    final_row.stroke_length_m = 0.0f;
                    final_row.stroke_count = snapshot.stroke_count;
                    final_row.gps_lat = 0.0;
                    final_row.gps_lon = 0.0;
                    final_row.power_w = 0.0f;
                    final_row.drive_time_s = 0.0f;
                    final_row.recovery_time_s = 0.0f;
                    final_row.recovery_ratio = 0.0f;

                    activity_log_append(&s_act_log, &final_row);
                }

                activity_log_finalize(&s_act_log, snapshot.distance_m, final_time_s, snapshot.avg_spm);
            }

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

    activity_log_msg_t msg;
    for (;;)
    {
        if (xQueueReceive(s_log_q, &msg, portMAX_DELAY) == pdTRUE)
        {
            // Only append if file is open
            if (s_act_log.opened)
            {
                if (msg.kind == ACT_LOG_ROW_STROKE)
                {
                    activity_log_append(&s_act_log, &msg.data.stroke);
                }
                else if (msg.kind == ACT_LOG_ROW_INTERVAL)
                {
                    activity_log_append_interval(&s_act_log, &msg.data.interval);
                }
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
