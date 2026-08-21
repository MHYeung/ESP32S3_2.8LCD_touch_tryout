#include "interval_program.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static interval_config_t s_cfg;
static bool s_active = false;
static bool s_baseline_valid = false;

static interval_phase_t s_phase = INTERVAL_PHASE_IDLE;
static uint16_t s_round_idx = 0;

// baselines captured at phase start
static uint32_t s_base_time_ms = 0;
static float    s_base_dist_m = 0;
static uint32_t s_base_strokes = 0;

// cached progress for UI
static interval_unit_t s_unit = INTERVAL_UNIT_TIME;
static uint32_t s_target = 0;
static uint32_t s_progress = 0;

static const interval_target_t *phase_target(interval_phase_t ph)
{
    if (ph == INTERVAL_PHASE_WORK) return &s_cfg.work;
    if (ph == INTERVAL_PHASE_REST) return &s_cfg.rest;
    return NULL;
}

static void lock_baseline(uint32_t t_ms, float d_m, uint32_t strokes)
{
    s_base_time_ms = t_ms;
    s_base_dist_m = d_m;
    s_base_strokes = strokes;
    s_baseline_valid = true;
}

static uint32_t compute_progress(const interval_target_t *t,
                                 uint32_t t_ms, float d_m, uint32_t strokes)
{
    if (!t) return 0;

    switch (t->unit) {
    case INTERVAL_UNIT_TIME: {
        uint32_t dt_ms = (t_ms >= s_base_time_ms) ? (t_ms - s_base_time_ms) : 0;
        return dt_ms / 1000;
    }
    case INTERVAL_UNIT_DISTANCE: {
        float dd = d_m - s_base_dist_m;
        if (dd < 0) dd = 0;
        return (uint32_t)(dd + 0.5f);
    }
    case INTERVAL_UNIT_STROKES: {
        return (strokes >= s_base_strokes) ? (strokes - s_base_strokes) : 0;
    }
    default:
        return 0;
    }
}

static void advance_phase(uint32_t t_ms, float d_m, uint32_t strokes)
{
    if (s_phase == INTERVAL_PHASE_WORK) {
        s_phase = INTERVAL_PHASE_REST;
        lock_baseline(t_ms, d_m, strokes);
        return;
    }

    if (s_phase == INTERVAL_PHASE_REST) {
        if (s_round_idx >= s_cfg.rounds) {
            s_phase = INTERVAL_PHASE_DONE;
            s_active = false;
            return;
        }
        s_round_idx++;
        s_phase = INTERVAL_PHASE_WORK;
        lock_baseline(t_ms, d_m, strokes);
        return;
    }
}

void interval_program_init(void)
{
    interval_config_t def = {
        .work = {.unit = INTERVAL_UNIT_TIME, .value = 60},
        .rest = {.unit = INTERVAL_UNIT_TIME, .value = 60},
        .rounds = 10,
        .auto_advance = true,
        .spm_start = 0,
        .spm_step = 0,
    };
    interval_program_set_config(&def);
    interval_program_stop();
}

void interval_program_set_config(const interval_config_t *cfg)
{
    if (!cfg) return;
    portENTER_CRITICAL(&s_mux);
    s_cfg = *cfg;
    if (s_cfg.rounds == 0) s_cfg.rounds = 1;
    portEXIT_CRITICAL(&s_mux);
}

void interval_program_get_config(interval_config_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    portEXIT_CRITICAL(&s_mux);
}

void interval_program_start(void)
{
    portENTER_CRITICAL(&s_mux);
    s_active = true;
    s_baseline_valid = false;
    s_phase = INTERVAL_PHASE_WORK;
    s_round_idx = 1;
    s_progress = 0;
    s_target = phase_target(s_phase)->value;
    s_unit = phase_target(s_phase)->unit;
    portEXIT_CRITICAL(&s_mux);
}

void interval_program_stop(void)
{
    portENTER_CRITICAL(&s_mux);
    s_active = false;
    s_baseline_valid = false;
    s_phase = INTERVAL_PHASE_IDLE;
    s_round_idx = 0;
    s_progress = 0;
    s_target = 0;
    s_unit = INTERVAL_UNIT_TIME;
    portEXIT_CRITICAL(&s_mux);
}

void interval_program_update(bool recording,
                             uint32_t t_ms,
                             float d_m,
                             uint32_t strokes)
{
    portENTER_CRITICAL(&s_mux);

    if (!s_active || s_phase == INTERVAL_PHASE_IDLE || s_phase == INTERVAL_PHASE_DONE) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }
    if (!recording) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }

    if (!s_baseline_valid) lock_baseline(t_ms, d_m, strokes);

    const interval_target_t *t = phase_target(s_phase);
    if (!t) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }

    s_unit = t->unit;
    s_target = t->value;
    s_progress = compute_progress(t, t_ms, d_m, strokes);

    if (s_cfg.auto_advance && s_progress >= s_target) {
        advance_phase(t_ms, d_m, strokes);
        // reset cached progress for the new phase
        const interval_target_t *t2 = phase_target(s_phase);
        if (t2) {
            s_unit = t2->unit;
            s_target = t2->value;
            s_progress = 0;
        }
    }

    portEXIT_CRITICAL(&s_mux);
}

void interval_program_get_ui(interval_ui_state_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    portENTER_CRITICAL(&s_mux);

    out->active = s_active;
    out->phase = s_phase;
    out->round_idx = s_round_idx;
    out->rounds = s_cfg.rounds;

    out->unit = s_unit;
    out->target = s_target;
    out->progress = s_progress;

    out->remaining = (s_progress >= s_target) ? 0 : (s_target - s_progress);
    out->progress_permille = (s_target > 0) ? (uint16_t)((1000ULL * s_progress) / s_target) : 0;

    portEXIT_CRITICAL(&s_mux);
}

uint8_t interval_program_target_spm(void)
{
    uint8_t target = 0;
    portENTER_CRITICAL(&s_mux);
    if (s_cfg.spm_start > 0) {
        uint16_t idx = (s_round_idx > 0) ? (s_round_idx - 1) : 0;
        uint16_t stepped = (uint16_t)s_cfg.spm_start + (uint16_t)idx * (uint16_t)s_cfg.spm_step;
        if (stepped > 255) {
            stepped = 255;
        }
        target = (uint8_t)stepped;
    }
    portEXIT_CRITICAL(&s_mux);
    return target;
}
