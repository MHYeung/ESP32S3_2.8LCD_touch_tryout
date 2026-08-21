#include "race_program.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include <math.h>
#include <string.h>

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static race_config_t s_cfg = {
    .distance_m = 1000,
    .target_pace_s_500 = 120.0f,
};
static bool s_active = false;
static bool s_finished = false;
static float s_delta_s = NAN;
static float s_remaining_m = NAN;
static float s_projected_s = NAN;

void race_program_init(void)
{
    race_config_t def = {
        .distance_m = 1000,
        .target_pace_s_500 = 120.0f,
    };
    race_program_set_config(&def);
    race_program_stop();
}

void race_program_set_config(const race_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_cfg = *cfg;
    if (s_cfg.distance_m == 0) {
        s_cfg.distance_m = 1000;
    }
    if (!(s_cfg.target_pace_s_500 > 10.0f)) {
        s_cfg.target_pace_s_500 = 120.0f;
    }
    portEXIT_CRITICAL(&s_mux);
}

void race_program_get_config(race_config_t *out)
{
    if (!out) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    portEXIT_CRITICAL(&s_mux);
}

void race_program_start(void)
{
    portENTER_CRITICAL(&s_mux);
    s_active = true;
    s_finished = false;
    s_delta_s = 0.0f;
    s_remaining_m = (float)s_cfg.distance_m;
    s_projected_s = NAN;
    portEXIT_CRITICAL(&s_mux);
}

void race_program_stop(void)
{
    portENTER_CRITICAL(&s_mux);
    s_active = false;
    s_finished = false;
    s_delta_s = NAN;
    s_remaining_m = NAN;
    s_projected_s = NAN;
    portEXIT_CRITICAL(&s_mux);
}

void race_program_update(bool recording,
                         float elapsed_s,
                         float distance_m,
                         float avg_speed_mps)
{
    portENTER_CRITICAL(&s_mux);
    if (!s_active || !recording) {
        portEXIT_CRITICAL(&s_mux);
        return;
    }

    float v_target = 500.0f / s_cfg.target_pace_s_500;
    if (!(v_target > 0.01f)) {
        v_target = 500.0f / 120.0f;
    }

    float covered = distance_m;
    if (!(covered > 0.0f) || !isfinite(covered)) {
        covered = 0.0f;
    }

    float boat_s = covered / v_target;
    float elapsed = (isfinite(elapsed_s) && elapsed_s > 0.0f) ? elapsed_s : 0.0f;
    s_delta_s = elapsed - boat_s;

    float rem = (float)s_cfg.distance_m - covered;
    if (rem <= 0.0f) {
        rem = 0.0f;
        s_finished = true;
        s_active = false;
    }
    s_remaining_m = rem;

    if (isfinite(avg_speed_mps) && avg_speed_mps > 0.1f) {
        s_projected_s = (float)s_cfg.distance_m / avg_speed_mps;
    } else {
        s_projected_s = NAN;
    }

    portEXIT_CRITICAL(&s_mux);
}

void race_program_get_ui(race_ui_state_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    portENTER_CRITICAL(&s_mux);
    out->active = s_active;
    out->finished = s_finished;
    out->delta_s = s_delta_s;
    out->remaining_m = s_remaining_m;
    out->projected_finish_s = s_projected_s;
    portEXIT_CRITICAL(&s_mux);
}
