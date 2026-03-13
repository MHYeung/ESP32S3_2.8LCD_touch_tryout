#include "app_imu.h"

#include "app_activity.h"
#include "app_context.h"
#include "app_pins.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gps_gtu8.h"
#include "interval_program.h"
#include "math.h"
#include "ui.h"
#include "ui_data_page.h"
#include "ui_interval_data_page.h"
#include "ui_status_bar.h"
#include <time.h>
#include <string.h>
#include "qmi8658.h"
#include "stroke_detection.h"

static const char *TAG = "app";

static const char *interval_unit_to_csv(interval_unit_t unit)
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

static const char *interval_phase_to_str(interval_phase_t phase)
{
    return (phase == INTERVAL_PHASE_WORK) ? "WORK" : "REST";
}

static void build_interval_row(activity_log_interval_row_t *row,
                               interval_phase_t phase,
                               uint16_t round_idx,
                               const interval_target_t *target,
                               float phase_time_s,
                               float phase_dist_m,
                               uint32_t phase_strokes)
{
    if (!row)
        return;

    if (phase_time_s < 0.0f)
        phase_time_s = 0.0f;
    if (phase_dist_m < 0.0f)
        phase_dist_m = 0.0f;

    memset(row, 0, sizeof(*row));
    row->round_index = round_idx;
    strncpy(row->phase, interval_phase_to_str(phase), sizeof(row->phase) - 1);
    if (target)
    {
        row->target_value = target->value;
        strncpy(row->target_unit, interval_unit_to_csv(target->unit), sizeof(row->target_unit) - 1);
    }
    row->phase_time_s = phase_time_s;
    row->phase_distance_m = phase_dist_m;
    row->phase_pace_s = (phase_dist_m > 0.1f) ? (phase_time_s / (phase_dist_m / 500.0f)) : 0.0f;
    row->avg_spm = (phase_time_s > 0.1f) ? ((float)phase_strokes * 60.0f / phase_time_s) : 0.0f;
}

static ui_orientation_t decide_orientation_from_accel(float ax, float ay, float az)
{
    /* Convert to "g" just so thresholds are ~1.0 */
    const float g = 9.80665f;
    float gx = ax / g;
    float gy = ay / g;
    float gz = az / g;

    float ax_abs = fabsf(gx);
    float ay_abs = fabsf(gy);
    float az_abs = fabsf(gz);

    /* If device is almost flat (gravity on Z), don't change orientation */
    if (az_abs > 0.8f)
    {
        return UI_ORIENT_PORTRAIT_0; // we'll treat "no change" separately later
    }

    /* Decide whether it's more "portrait" or "landscape" */
    if (ax_abs > ay_abs)
    {
        /* More tilt in X -> landscape */
        if (gx > 0.0f)
        {
            return UI_ORIENT_PORTRAIT_180; // you may swap 90/270 after testing
        }
        else
        {
            return UI_ORIENT_PORTRAIT_0;
        }
    }
    else
    {
        /* More tilt in Y -> portrait */
        if (gy > 0.0f)
        {
            return UI_ORIENT_LANDSCAPE_90; // board upside down
        }
        else
        {
            return UI_ORIENT_LANDSCAPE_270; // "normal" portrait
        }
    }
}

void init_imu(void)
{
    ESP_LOGI(TAG, "Init IMU I2C bus + QMI8658...");

    ESP_ERROR_CHECK(i2c_helper_init(&s_imu_bus,
                                    IMU_I2C_PORT,
                                    IMU_SDA_GPIO,
                                    IMU_SCL_GPIO,
                                    IMU_I2C_CLK));

    ESP_ERROR_CHECK(qmi8658_init(&s_imu, &s_imu_bus, QMI8658_I2C_ADDR));
}

void stroke_task(void *arg)
{
    (void)arg;

    // GPS smoothing state
    static float s_gps_speed_filt = NAN;
    static double s_gps_lat = NAN;
    static double s_gps_lon = NAN;
    static float s_total_distance = NAN;

    static bool s_last_recording_ui = false;
    static bool s_interval_prev_recording = false;
    static bool s_interval_phase_active = false;
    static interval_phase_t s_interval_log_phase = INTERVAL_PHASE_IDLE;
    static uint16_t s_interval_log_round = 0;
    static float s_interval_phase_start_time_s = 0.0f;
    static float s_interval_phase_start_dist_m = 0.0f;
    static uint32_t s_interval_phase_start_strokes = 0;
    static bool s_interval_cfg_valid = false;
    static interval_config_t s_interval_log_cfg = {0};

    const float fs_hz = 200.0f;
    const stroke_detection_cfg_t cfg = {
        .fs_hz = fs_hz,

        // Gravity rejection: Slower is better for a steady hull to isolate "surge" from "tilt"
        .gravity_tau_s = 1.0f, // Was 0.8f

        // Axis detection: Hull acceleration is linear, so we hold the decision longer
        .axis_window_s = 4.0f,
        .axis_hold_s = 1.0f,          // Was 0.5f - reduces switching noise
        .accel_use_fixed_axis = true, // Keep true if you know the mounting orientation
        .accel_fixed_axis = 2,        // Ensure this matches your physical mount (2 = Z-axis usually)

        // FILTERS: pass the slow rhythmic rowing surge (0.5-1.0 Hz), cut bus vibration.
        .hpf_hz = 0.1f,  // pass very slow drive start
        .lpf_hz = 2.0f,  // tightened from 3.0 Hz to cut more high-freq bus vibration

        // TIMING:
        .min_stroke_period_s = 0.9f, // ~66 SPM max
        .max_stroke_period_s = 6.0f, // 10 SPM min
        .min_catch_interval_s = 0.75f, // tightened debounce (was 0.55s) — rejects rapid bus bumps

        // THRESHOLDS: raised to ignore bus/hand shake (~0.05-0.3g) while catching real rowing strokes.
        // Real rowing catch on a hull: 0.5-2g; bus shake: <0.3g (~2.9 m/s²).
        .thr_k = 2.5f,    // was 1.6 — need signal much larger than noise floor
        .thr_floor = 1.8f, // was 0.55 (~0.056g); now ~0.18g — filters casual shaking
    };

    stroke_detection_init(&s_stroke, &cfg);

    int64_t t0_us = esp_timer_get_time();
    int64_t prev_us = t0_us;

    float s_last_valid_spm = NAN;
    float s_last_spm_t_s = -1.0f;

    const TickType_t sample_delay = pdMS_TO_TICKS((int)(1000.0f / fs_hz));
    const TickType_t ui_period = pdMS_TO_TICKS(100); // 10Hz
    TickType_t last_ui_tick = xTaskGetTickCount();

    ui_orientation_t last_orient = s_current_orient;
    int stable_count = 0;

    while (1)
    {
        float ax, ay, az, gx, gy, gz;
        if (qmi8658_read_accel_gyro(&s_imu, &ax, &ay, &az, &gx, &gy, &gz) == ESP_OK)
        {
            int64_t now_us = esp_timer_get_time();
            float t_s = (float)(now_us - t0_us) * 1e-6f;
            float dt_s = (float)(now_us - prev_us) * 1e-6f;
            prev_us = now_us;
            if (dt_s < 0.0f)
                dt_s = 0.0f;
            if (dt_s > 0.1f)
                dt_s = 0.1f;

            stroke_metrics_t m = {0};
            stroke_event_t ev = stroke_detection_update(&s_stroke, t_s, ax, ay, az, gx, gy, gz, &m);

            if (ev != STROKE_EVENT_NONE)
            {
                ESP_LOGI("STROKE", "ev=%d count=%lu spm=%.1f period=%.2fs",
                         (int)ev, (unsigned long)m.stroke_count, (double)m.spm, (double)m.stroke_period_s);
            }

            // Orientation Logic
            if (s_auto_rotate_enabled)
            {
                ui_orientation_t candidate = decide_orientation_from_accel(ax, ay, az);
                if (candidate == last_orient)
                {
                    if (stable_count < 20)
                        stable_count++;
                }
                else
                {
                    last_orient = candidate;
                    stable_count = 0;
                }
                if (stable_count >= 8 && candidate != s_current_orient)
                {
                    s_current_orient = candidate;
                    ui_set_orientation(candidate);
                }
            }

            if (isfinite(m.spm) && m.spm >= 10.0f && m.spm <= 80.0f)
            {
                s_last_valid_spm = m.spm;
                s_last_spm_t_s = t_s;
            }

            float spm_raw = s_last_valid_spm;
            if (s_last_spm_t_s > 0.0f && (t_s - s_last_spm_t_s) > 12.0f)
                spm_raw = NAN;
            if (!isfinite(spm_raw))
                spm_raw = 0.0f;

            // GPS Logic
            gps_fix_t fix;
            bool gps_connected = false;
            uint8_t gps_bars = 0; // 0 means "no fix / searching"
            bool gps_ok = false;

            if (gps_gtu8_get_latest(&fix))
            {
                int64_t age_us = esp_timer_get_time() - fix.rx_time_us;
                gps_connected = (age_us < 2000000);

                if (gps_connected && fix.valid_fix)
                {
                    // ok for UI quality
                    int sats = fix.sats;
                    float hdop = fix.hdop;

                    if (sats >= 10 && hdop <= 1.2f)
                        gps_bars = 4;
                    else if (sats >= 8 && hdop <= 1.8f)
                        gps_bars = 3;
                    else if (sats >= 6 && hdop <= 2.8f)
                        gps_bars = 2;
                    else
                        gps_bars = 1;
                }
                else if (gps_connected)
                {
                    gps_bars = 0; // connected but no fix -> RED dot
                }

                // your gps_ok logic can stay stricter:
                if (gps_connected && fix.valid_fix && isfinite(fix.speed_mps))
                {
                    gps_ok = true;
                    s_gps_lat = fix.lat_deg;
                    s_gps_lon = fix.lon_deg;
                    s_total_distance += fix.speed_mps * dt_s;

                    const float tau = 2.2f;
                    float alpha = dt_s / (tau + dt_s);
                    if (!isfinite(s_gps_speed_filt))
                        s_gps_speed_filt = fix.speed_mps;
                    else
                        s_gps_speed_filt += alpha * (fix.speed_mps - s_gps_speed_filt);
                }
            }
            else
            {
                gps_connected = false;
                gps_bars = 0;
            }

            // Always update UI on change
            static bool last_conn = false;
            static uint8_t last_bars = 255;
            if (gps_connected != last_conn || gps_bars != last_bars)
            {
                ui_status_bar_set_gps_default_safe(gps_connected, gps_bars);
                last_conn = gps_connected;
                last_bars = gps_bars;
            }

            float speed_mps = gps_ok ? s_gps_speed_filt : 0.0f;
            float dist_delta_m = speed_mps * dt_s;

            // --- 1. Calculate Derived Metrics for Logging ---

            // Instant Pace (s/500m)
            float instant_pace_s = (speed_mps > 0.1f) ? (500.0f / speed_mps) : 0.0f;

            // Stroke Length (m) = Speed * Period
            float stroke_len_m = 0.0f;
            if (isfinite(m.stroke_period_s) && m.stroke_period_s > 0.0f)
            {
                stroke_len_m = speed_mps * m.stroke_period_s;
            }

            // Recovery Ratio = Recovery / Drive
            float recov_ratio = 0.0f;
            if (m.drive_time_s > 0.01f)
            {
                recov_ratio = m.recovery_time_s / m.drive_time_s;
            }
            // --- End Derived Metrics ---

            bool need_log = false;
            bool need_interval_log = false;
            bool interval_was_recording = s_interval_prev_recording;
            activity_log_row_t row = {0};
            activity_log_interval_row_t interval_row = {0};

            if (s_activity_mutex)
                xSemaphoreTake(s_activity_mutex, portMAX_DELAY);

            if (s_activity_recording)
            {
                s_session_time_s += dt_s;

                uint32_t stroke_delta = (ev == STROKE_EVENT_CATCH) ? 1 : 0;

                // Update Session Model (Activity.c)
                activity_update(&s_activity,
                                dt_s,
                                speed_mps,
                                spm_raw,
                                0.0f, // Power placeholder
                                dist_delta_m,
                                stroke_delta);

                bool is_interval = activity_type_is_interval(s_activity.activity_type);
                if (is_interval && !interval_was_recording)
                {
                    interval_program_get_config(&s_interval_log_cfg);
                    s_interval_cfg_valid = true;
                    s_interval_phase_active = false;
                    s_interval_log_phase = INTERVAL_PHASE_IDLE;
                    s_interval_log_round = 0;
                }

                interval_ui_state_t ist = {0};
                bool interval_ui_valid = false;
                if (s_activity.activity_type == ACTIVITY_INTERVAL_NORMAL)
                {
                    uint32_t t_ms = (uint32_t)(s_session_time_s * 1000.0f);
                    interval_program_update(true, t_ms, s_activity.distance_m, s_activity.stroke_count);
                    interval_program_get_ui(&ist);
                    interval_ui_valid = true;
                    if (!s_interval_done_queued && ist.phase == INTERVAL_PHASE_DONE)
                    {
                        s_interval_done_queued = true;
                        interval_data_page_show_complete_prompt();
                        act_cmd_t done_cmd = ACT_CMD_STOP_SAVE;
                        xQueueSend(s_act_q, &done_cmd, 0);
                    }
                }

                if (is_interval && interval_ui_valid)
                {
                    if (ist.phase == INTERVAL_PHASE_WORK || ist.phase == INTERVAL_PHASE_REST)
                    {
                        if (!s_interval_phase_active)
                        {
                            s_interval_phase_active = true;
                            s_interval_log_phase = ist.phase;
                            s_interval_log_round = ist.round_idx;
                            s_interval_phase_start_time_s = s_session_time_s;
                            s_interval_phase_start_dist_m = s_activity.distance_m;
                            s_interval_phase_start_strokes = s_activity.stroke_count;
                        }
                        else if (ist.phase != s_interval_log_phase || ist.round_idx != s_interval_log_round)
                        {
                            const interval_target_t *target = NULL;
                            if (s_interval_cfg_valid)
                            {
                                target = (s_interval_log_phase == INTERVAL_PHASE_WORK) ? &s_interval_log_cfg.work : &s_interval_log_cfg.rest;
                            }
                            build_interval_row(&interval_row,
                                               s_interval_log_phase,
                                               s_interval_log_round,
                                               target,
                                               s_session_time_s - s_interval_phase_start_time_s,
                                               s_activity.distance_m - s_interval_phase_start_dist_m,
                                               s_activity.stroke_count - s_interval_phase_start_strokes);
                            need_interval_log = true;

                            s_interval_log_phase = ist.phase;
                            s_interval_log_round = ist.round_idx;
                            s_interval_phase_start_time_s = s_session_time_s;
                            s_interval_phase_start_dist_m = s_activity.distance_m;
                            s_interval_phase_start_strokes = s_activity.stroke_count;
                        }
                    }
                    else if (s_interval_phase_active &&
                             (ist.phase == INTERVAL_PHASE_DONE || ist.phase == INTERVAL_PHASE_IDLE))
                    {
                        const interval_target_t *target = NULL;
                        if (s_interval_cfg_valid)
                        {
                            target = (s_interval_log_phase == INTERVAL_PHASE_WORK) ? &s_interval_log_cfg.work : &s_interval_log_cfg.rest;
                        }
                        build_interval_row(&interval_row,
                                           s_interval_log_phase,
                                           s_interval_log_round,
                                           target,
                                           s_session_time_s - s_interval_phase_start_time_s,
                                           s_activity.distance_m - s_interval_phase_start_dist_m,
                                           s_activity.stroke_count - s_interval_phase_start_strokes);
                        need_interval_log = true;
                        s_interval_phase_active = false;
                        s_interval_log_phase = INTERVAL_PHASE_IDLE;
                        s_interval_log_round = 0;
                    }
                }

                // Calculate Avg Pace from Session Avg Speed
                float avg_pace_s = (s_activity.avg_speed_mps > 0.1f) ? (500.0f / s_activity.avg_speed_mps) : 0.0f;

                // Only log on CATCH
                if (ev == STROKE_EVENT_CATCH)
                {

                    // --- Populate the 16-Column Row ---

                    // 1. RTC Time
                    row.rtc_time = time(NULL);
                    // 2. Session Time
                    row.session_time_s = s_session_time_s;
                    // 3. Distance (Total)
                    row.total_distance_m = s_activity.distance_m;
                    // 4. Instant Pace
                    row.pace_500m_s = instant_pace_s;
                    // 5. SPM Instant
                    row.spm_instant = spm_raw;
                    // 6. Avg Pace
                    row.avg_pace_500m_s = avg_pace_s;
                    // 7. Avg Speed
                    row.avg_speed_mps = s_activity.avg_speed_mps;
                    // 8. Stroke Length
                    row.stroke_length_m = stroke_len_m;
                    // 9. Stroke Count
                    row.stroke_count = s_activity.stroke_count;
                    // 10. GPS Lat
                    row.gps_lat = gps_ok ? s_gps_lat : 0.0;
                    // 11. GPS Long
                    row.gps_lon = gps_ok ? s_gps_lon : 0.0;
                    // 12. Power
                    row.power_w = 0.0f;
                    // 13. Drive Time
                    row.drive_time_s = m.drive_time_s;
                    // 14. Recovery Time
                    row.recovery_time_s = m.recovery_time_s;
                    // 15. Recovery Ratio
                    row.recovery_ratio = recov_ratio;

                    need_log = true;
                }
            }
            else
            {
                if (interval_was_recording && s_interval_phase_active)
                {
                    const interval_target_t *target = NULL;
                    if (s_interval_cfg_valid)
                    {
                        target = (s_interval_log_phase == INTERVAL_PHASE_WORK) ? &s_interval_log_cfg.work : &s_interval_log_cfg.rest;
                    }
                    build_interval_row(&interval_row,
                                       s_interval_log_phase,
                                       s_interval_log_round,
                                       target,
                                       s_session_time_s - s_interval_phase_start_time_s,
                                       s_activity.distance_m - s_interval_phase_start_dist_m,
                                       s_activity.stroke_count - s_interval_phase_start_strokes);
                    need_interval_log = true;
                    s_interval_phase_active = false;
                    s_interval_log_phase = INTERVAL_PHASE_IDLE;
                    s_interval_log_round = 0;
                }
                s_interval_cfg_valid = false;
                s_session_time_s = 0.0f;
            }

            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            if (need_log && s_log_q)
            {
                activity_log_msg_t msg = {0};
                msg.kind = ACT_LOG_ROW_STROKE;
                msg.data.stroke = row;
                xQueueSend(s_log_q, &msg, 0);
            }
            if (need_interval_log && s_log_q)
            {
                activity_log_msg_t msg = {0};
                msg.kind = ACT_LOG_ROW_INTERVAL;
                msg.data.interval = interval_row;
                xQueueSend(s_log_q, &msg, 0);
            }

            bool recording = s_activity_recording;
            s_interval_prev_recording = s_activity_recording;

            // UI Update
            if (!ui_is_modal_active())
            {
                // 1) Time only @ 10Hz
                TickType_t now = xTaskGetTickCount();
                if ((now - last_ui_tick) >= ui_period)
                {
                    last_ui_tick = now;
                    data_page_set_time_s(recording ? s_session_time_s : NAN);
                }
                // 2) Non-time metrics only on stroke count OR state change
                bool force_full_redraw = (recording != s_last_recording_ui);
                if (force_full_redraw)
                {
                    s_last_recording_ui = recording;
                }
                if (force_full_redraw || ev == STROKE_EVENT_CATCH)
                {
                    float spm_raw_ui = s_last_valid_spm;
                    if (s_last_spm_t_s > 0.0f && (t_s - s_last_spm_t_s) > 12.0f)
                        spm_raw_ui = NAN;

                    float spm_disp = spm_raw_ui;
                    if (isfinite(spm_disp))
                        spm_disp = ceilf(spm_disp * 2.0f) / 2.0f;

                    float pace = (speed_mps > 0.2f) ? (500.0f / speed_mps) : NAN;
                    float avg_pace_s = (s_activity.avg_speed_mps > 0.1f) ? (500.0f / s_activity.avg_speed_mps) : NAN;

                float stroke_len_disp = NAN;
                if (recording && stroke_len_m > 0.01f)
                    stroke_len_disp = stroke_len_m;

                data_values_t v = {
                    .time_s = recording ? s_session_time_s : NAN,
                    .distance_m = recording ? s_activity.distance_m : NAN,
                    .pace_s_per_500m = recording ? pace : NAN,
                    .avg_pace_s_per_500m = recording ? avg_pace_s : NAN,
                    .speed_mps = recording ? speed_mps : NAN,
                    .spm = spm_disp,   /* show live SPM even when not recording */
                    .stroke_len_m = stroke_len_disp,
                    .stroke_count = recording ? s_activity.stroke_count : UINT32_MAX,
                };
                    data_page_set_values(&v);
                    if (recording && s_activity.activity_type == ACTIVITY_INTERVAL_NORMAL)
                    {
                        interval_data_page_set_pace_s_per_500m(pace);
                        interval_data_page_set_spm(spm_disp);
                    }
                    else
                    {
                        interval_data_page_set_pace_s_per_500m(NAN);
                        interval_data_page_set_spm(NAN);
                    }
                }
            }
        }
        vTaskDelay(sample_delay);
    }
}
