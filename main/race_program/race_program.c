#include "race_program.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static race_config_t s_cfg;
static bool s_active = false;
static bool s_done = false;

// split baseline
static bool     s_split_base_valid = false;
static uint32_t s_split_base_time_ms = 0;
static float    s_split_base_dist_m = 0;

// cached UI values
static race_basis_t s_basis = RACE_BASIS_DISTANCE;
static uint32_t s_target = 0;
static uint32_t s_progress = 0;

static bool s_split_by_time = false;
static uint32_t s_split_target = 0;
static uint32_t s_split_progress = 0;
static uint32_t s_split_idx = 0;

static uint32_t s_target_pace_ms_per500 = 0;
static uint32_t s_cur_split_pace_ms_per500 = 0;
static int32_t  s_cur_split_pace_delta_ms_per500 = 0;

uint32_t race_compute_pace_ms_per500_from_distance_time(uint32_t distance_m, uint32_t time_s)
{
    if (distance_m == 0 || time_s == 0) return 0;
    uint64_t time_ms = (uint64_t)time_s * 1000ULL;
    return (uint32_t)((time_ms * 500ULL) / (uint64_t)distance_m);
}

uint32_t race_compute_time_s_from_distance_pace(uint32_t distance_m, uint32_t pace_ms_per500)
{
    if (distance_m == 0 || pace_ms_per500 == 0) return 0;
    uint64_t time_ms = (uint64_t)pace_ms_per500 * (uint64_t)distance_m;
    time_ms /= 500ULL;
    return (uint32_t)(time_ms / 1000ULL);
}

uint32_t race_compute_distance_m_from_time_pace(uint32_t time_s, uint32_t pace_ms_per500)
{
    if (time_s == 0 || pace_ms_per500 == 0) return 0;
    uint64_t time_ms = (uint64_t)time_s * 1000ULL;
    return (uint32_t)((time_ms * 500ULL) / (uint64_t)pace_ms_per500);
}

static void normalize_cfg(race_config_t *c)
{
    if (!c) return;

    // defaults
    if (c->basis != RACE_BASIS_TIME) c->basis = RACE_BASIS_DISTANCE;

    if (c->basis == RACE_BASIS_DISTANCE) {
        if (c->target_distance_m == 0) c->target_distance_m = 2000;

        // derive missing target pace / time
        if (c->target_pace_ms_per500 == 0 && c->target_finish_time_s > 0) {
            c->target_pace_ms_per500 =
                race_compute_pace_ms_per500_from_distance_time(c->target_distance_m, c->target_finish_time_s);
        }
        if (c->target_finish_time_s == 0 && c->target_pace_ms_per500 > 0) {
            c->target_finish_time_s =
                race_compute_time_s_from_distance_pace(c->target_distance_m, c->target_pace_ms_per500);
        }
        // time-basis field not used
        c->target_duration_s = 0;

    } else { // RACE_BASIS_TIME
        if (c->target_duration_s == 0) c->target_duration_s = 600; // 10:00
        // distance-basis fields not used
        c->target_distance_m = 0;
        c->target_finish_time_s = 0;
    }

    // split defaults
    if (c->split_by_time) {
        if (c->split_time_s == 0) c->split_time_s = 60;
        c->split_distance_m = 0;
    } else {
        if (c->split_distance_m == 0) c->split_distance_m = 500;
        c->split_time_s = 0;
    }
}

static void lock_split_baseline(uint32_t t_ms, float d_m)
{
    s_split_base_time_ms = t_ms;
    s_split_base_dist_m = d_m;
    s_split_base_valid = true;
}

static uint32_t compute_split_progress(uint32_t t_ms, float d_m)
{
    if (!s_split_base_valid) return 0;

    if (s_split_by_time) {
        uint32_t dt_ms = (t_ms >= s_split_base_time_ms) ? (t_ms - s_split_base_time_ms) : 0;
        return dt_ms / 1000U;
    } else {
        float dd = d_m - s_split_base_dist_m;
        if (dd < 0) dd = 0;
        return (uint32_t)(dd + 0.5f);
    }
}

static uint32_t compute_pace_ms_per500(uint32_t t_ms, float d_m)
{
    if (!s_split_base_valid) return 0;

    uint32_t dt_ms = (t_ms >= s_split_base_time_ms) ? (t_ms - s_split_base_time_ms) : 0;
    float dd = d_m - s_split_base_dist_m;
    if (dd <= 1.0f || dt_ms < 200) return 0; // need a bit of data

    // pace(ms/500m) = dt_ms * 500 / dd_m
    uint64_t pace = (uint64_t)dt_ms * 500ULL;
    pace /= (uint64_t)(dd + 0.5f);
    return (uint32_t)pace;
}

static void maybe_advance_split(uint32_t t_ms, float d_m)
{
    if (!s_split_base_valid || s_split_target == 0) return;

    if (s_split_by_time) {
        uint32_t dt_ms = (t_ms >= s_split_base_time_ms) ? (t_ms - s_split_base_time_ms) : 0;
        uint32_t target_ms = s_split_target * 1000U;
        if (dt_ms >= target_ms) {
            // how many splits passed (coarse)
            uint32_t n = dt_ms / target_ms;
            if (n == 0) n = 1;
            s_split_idx += n;
            // reset baseline at "now" for stable UI pace
            lock_split_baseline(t_ms, d_m);
        }
    } else {
        float dd = d_m - s_split_base_dist_m;
        if (dd >= (float)s_split_target) {
            uint32_t n = (uint32_t)(dd / (float)s_split_target);
            if (n == 0) n = 1;
            s_split_idx += n;
            lock_split_baseline(t_ms, d_m);
        }
    }
}

void race_program_init(void)
{
    race_config_t def = {
        .basis = RACE_BASIS_DISTANCE,
        .target_distance_m = 2000,
        .target_finish_time_s = 0,
        .target_duration_s = 0,
        .target_pace_ms_per500 = 0,

        .split_by_time = false,
        .split_distance_m = 500,
        .split_time_s = 0,

        .pace_strategy = RACE_PACE_STRATEGY_EVEN,
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

    s_basis = s_cfg.basis;
    s_target = (s_basis == RACE_BASIS_DISTANCE) ? s_cfg.target_distance_m : s_cfg.target_duration_s;
    s_progress = 0;

    s_split_by_time = s_cfg.split_by_time;
    s_split_target = s_split_by_time ? s_cfg.split_time_s : s_cfg.split_distance_m;
    s_split_progress = 0;
    s_split_idx = 1;

    s_target_pace_ms_per500 = s_cfg.target_pace_ms_per500;
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

    s_basis = s_cfg.basis;
    s_target = 0;
    s_progress = 0;

    s_split_by_time = s_cfg.split_by_time;
    s_split_target = 0;
    s_split_progress = 0;
    s_split_idx = 0;

    s_target_pace_ms_per500 = s_cfg.target_pace_ms_per500;
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

    // refresh cached target/split in case config was updated live
    s_basis = s_cfg.basis;
    s_target = (s_basis == RACE_BASIS_DISTANCE) ? s_cfg.target_distance_m : s_cfg.target_duration_s;

    s_split_by_time = s_cfg.split_by_time;
    s_split_target = s_split_by_time ? s_cfg.split_time_s : s_cfg.split_distance_m;
    s_target_pace_ms_per500 = s_cfg.target_pace_ms_per500;

    if (d_m < 0) d_m = 0;

    // overall progress
    if (s_basis == RACE_BASIS_DISTANCE) {
        s_progress = (uint32_t)(d_m + 0.5f);
    } else {
        s_progress = t_ms / 1000U;
    }

    // split baseline
    if (!s_split_base_valid) {
        lock_split_baseline(t_ms, d_m);
    }

    // compute split progress + pace
    s_split_progress = compute_split_progress(t_ms, d_m);
    s_cur_split_pace_ms_per500 = compute_pace_ms_per500(t_ms, d_m);

    if (s_target_pace_ms_per500 > 0 && s_cur_split_pace_ms_per500 > 0) {
        s_cur_split_pace_delta_ms_per500 =
            (int32_t)s_cur_split_pace_ms_per500 - (int32_t)s_target_pace_ms_per500;
    } else {
        s_cur_split_pace_delta_ms_per500 = 0;
    }

    // split boundary
    maybe_advance_split(t_ms, d_m);
    // after boundary reset, update cached progress for UI
    s_split_progress = compute_split_progress(t_ms, d_m);

    // done?
    if (s_target > 0 && s_progress >= s_target) {
        s_done = true;
        s_active = false;
        s_progress = s_target;
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

    out->basis = s_basis;
    out->target = s_target;
    out->progress = s_progress;
    out->remaining = (s_progress >= s_target) ? 0 : (s_target - s_progress);
    out->progress_permille = (s_target > 0) ? (uint16_t)((1000ULL * s_progress) / s_target) : 0;

    out->split_by_time = s_split_by_time;
    out->split_target = s_split_target;
    out->split_progress = s_split_progress;
    out->split_remaining = (s_split_progress >= s_split_target) ? 0 : (s_split_target - s_split_progress);
    out->split_idx = s_split_idx;

    out->target_pace_ms_per500 = s_target_pace_ms_per500;
    out->current_split_pace_ms_per500 = s_cur_split_pace_ms_per500;
    out->current_split_pace_delta_ms_per500 = s_cur_split_pace_delta_ms_per500;

    portEXIT_CRITICAL(&s_mux);
}
