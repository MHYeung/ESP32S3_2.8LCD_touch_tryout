#include "race_program.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

// Default: compute split pace on each 500m segment.
#ifndef RACE_DEFAULT_SPLIT_DISTANCE_M
#define RACE_DEFAULT_SPLIT_DISTANCE_M 500U
#endif

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static race_config_t s_cfg;

static bool s_active = false;
static bool s_done = false;

// Baseline for "current split" calculations
static bool     s_split_base_valid = false;
static uint32_t s_split_base_time_ms = 0;
static float    s_split_base_dist_m = 0.0f;

// Cached state for UI
static uint32_t s_target_distance_m = 0;
static uint32_t s_distance_m = 0;

static uint32_t s_split_distance_m = RACE_DEFAULT_SPLIT_DISTANCE_M;
static uint32_t s_split_progress_m = 0;
static uint32_t s_split_idx = 0;

static uint32_t s_target_pace_ms_per500 = 0;
static uint32_t s_cur_split_pace_ms_per500 = 0;
static int32_t  s_cur_split_pace_delta_ms_per500 = 0;

uint32_t race_compute_time_s_from_distance_pace(uint32_t distance_m, uint32_t pace_ms_per500)
{
    if (distance_m == 0 || pace_ms_per500 == 0) return 0;
    // time(ms) = pace(ms/500m) * distance / 500
    uint64_t time_ms = (uint64_t)pace_ms_per500 * (uint64_t)distance_m;
    time_ms /= 500ULL;
    return (uint32_t)(time_ms / 1000ULL);
}

static void normalize_cfg(race_config_t *c)
{
    if (!c) return;

    if (c->race_target_distance_m == 0) c->race_target_distance_m = 2000;
    if (c->race_target_pace_ms_per500 == 0) c->race_target_pace_ms_per500 = 120000; // 2:00.0 /500m
    if (c->race_target_split_length_m == 0) c->race_target_split_length_m = 500; // 500m
}

static void lock_split_baseline(uint32_t t_ms, float d_m)
{
    s_split_base_time_ms = t_ms;
    s_split_base_dist_m = d_m;
    s_split_base_valid = true;
}

static uint32_t compute_split_progress_m(float d_m)
{
    if (!s_split_base_valid) return 0;
    float dd = d_m - s_split_base_dist_m;
    if (dd < 0) dd = 0;
    return (uint32_t)(dd + 0.5f);
}

static uint32_t compute_pace_ms_per500(uint32_t t_ms, float d_m)
{
    if (!s_split_base_valid) return 0;

    uint32_t dt_ms = (t_ms >= s_split_base_time_ms) ? (t_ms - s_split_base_time_ms) : 0;
    float dd = d_m - s_split_base_dist_m;
    if (dd <= 1.0f || dt_ms < 200) return 0;

    // pace(ms/500m) = dt_ms * 500 / dd_m
    uint64_t pace = (uint64_t)dt_ms * 500ULL;
    pace /= (uint64_t)(dd + 0.5f);
    return (uint32_t)pace;
}

static void maybe_advance_split(uint32_t t_ms, float d_m)
{
    if (!s_split_base_valid || s_split_distance_m == 0) return;

    float dd = d_m - s_split_base_dist_m;
    if (dd < (float)s_split_distance_m) return;

    // How many splits passed
    uint32_t n = (uint32_t)(dd / (float)s_split_distance_m);
    if (n == 0) n = 1;
    s_split_idx += n;

    // Reset baseline at "now" for stable UI pace
    lock_split_baseline(t_ms, d_m);
}

void race_program_init(void)
{
    race_config_t def = {
        .race_target_distance_m = 2000,
        .race_target_pace_ms_per500 = 120000,
        .race_target_split_length_m = 500,
    };
    race_program_set_config(&def);
    race_program_stop();
}

void race_program_set_config(const race_config_t *cfg)
{
    if (!cfg) return;
    portENTER_CRITICAL(&s_mux);
    s_cfg = *cfg;
    normalize_cfg(&s_cfg);
    portEXIT_CRITICAL(&s_mux);
}

void race_program_get_config(race_config_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    portEXIT_CRITICAL(&s_mux);
}

void race_program_start(void)
{
    portENTER_CRITICAL(&s_mux);

    s_active = true;
    s_done = false;

    s_target_distance_m = s_cfg.race_target_distance_m;
    s_target_pace_ms_per500 = s_cfg.race_target_pace_ms_per500;

    s_distance_m = 0;

    s_split_distance_m = s_cfg.race_target_split_length_m;
    s_split_progress_m = 0;
    s_split_idx = 1;

    s_cur_split_pace_ms_per500 = 0;
    s_cur_split_pace_delta_ms_per500 = 0;

    s_split_base_valid = false;
    s_split_base_time_ms = 0;
    s_split_base_dist_m = 0;

    portEXIT_CRITICAL(&s_mux);
}

void race_program_stop(void)
{
    portENTER_CRITICAL(&s_mux);

    s_active = false;
    s_done = false;

    s_target_distance_m = s_cfg.race_target_distance_m;
    s_target_pace_ms_per500 = s_cfg.race_target_pace_ms_per500;

    s_distance_m = 0;

    s_split_progress_m = 0;
    s_split_idx = 0;
    s_cur_split_pace_ms_per500 = 0;
    s_cur_split_pace_delta_ms_per500 = 0;

    s_split_base_valid = false;

    portEXIT_CRITICAL(&s_mux);
}

void race_program_update(bool recording,
                         uint32_t t_ms,
                         float d_m,
                         uint32_t stroke_count)
{
    (void)stroke_count;

    portENTER_CRITICAL(&s_mux);

    if (!s_active || s_done) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }
    if (!recording) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }

    // refresh cached targets in case config changed live
    s_target_distance_m = s_cfg.race_target_distance_m;
    s_target_pace_ms_per500 = s_cfg.race_target_pace_ms_per500;

    if (d_m < 0) d_m = 0;
    s_distance_m = (uint32_t)(d_m + 0.5f);

    if (!s_split_base_valid) {
        lock_split_baseline(t_ms, d_m);
    }

    // compute split progress + pace
    s_split_progress_m = compute_split_progress_m(d_m);
    s_cur_split_pace_ms_per500 = compute_pace_ms_per500(t_ms, d_m);

    if (s_target_pace_ms_per500 > 0 && s_cur_split_pace_ms_per500 > 0) {
        s_cur_split_pace_delta_ms_per500 =
            (int32_t)s_cur_split_pace_ms_per500 - (int32_t)s_target_pace_ms_per500;
    } else {
        s_cur_split_pace_delta_ms_per500 = 0;
    }

    // advance split if boundary crossed (this also resets baseline)
    maybe_advance_split(t_ms, d_m);
    s_split_progress_m = compute_split_progress_m(d_m);

    // done?
    if (s_target_distance_m > 0 && s_distance_m >= s_target_distance_m) {
        s_done = true;
        s_active = false;
        s_distance_m = s_target_distance_m;
    }

    portEXIT_CRITICAL(&s_mux);
}

void race_program_get_ui(race_ui_state_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    portENTER_CRITICAL(&s_mux);

    out->active = s_active;
    out->done = s_done;

    out->target_distance_m = s_target_distance_m;
    out->distance_m = s_distance_m;
    out->remaining_m = (s_distance_m >= s_target_distance_m) ? 0 : (s_target_distance_m - s_distance_m);
    out->progress_permille = (s_target_distance_m > 0)
                                 ? (uint16_t)((1000ULL * s_distance_m) / s_target_distance_m)
                                 : 0;

    out->split_distance_m = s_split_distance_m;
    out->split_progress_m = s_split_progress_m;
    out->split_remaining_m = (s_split_progress_m >= s_split_distance_m) ? 0 : (s_split_distance_m - s_split_progress_m);
    out->split_idx = s_split_idx;

    out->target_pace_ms_per500 = s_target_pace_ms_per500;
    out->current_split_pace_ms_per500 = s_cur_split_pace_ms_per500;
    out->current_split_pace_delta_ms_per500 = s_cur_split_pace_delta_ms_per500;

    portEXIT_CRITICAL(&s_mux);
}
