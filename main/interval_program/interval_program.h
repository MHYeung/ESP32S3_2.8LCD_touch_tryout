#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INTERVAL_UNIT_TIME = 0,     // seconds
    INTERVAL_UNIT_DISTANCE,     // meters
    INTERVAL_UNIT_STROKES,      // strokes
} interval_unit_t;

typedef struct {
    interval_unit_t unit;
    uint32_t value;             // seconds / meters / strokes
} interval_target_t;

typedef struct {
    interval_target_t work;
    interval_target_t rest;
    uint16_t rounds;
    bool auto_advance;
} interval_config_t;

typedef enum {
    INTERVAL_PHASE_IDLE = 0,
    INTERVAL_PHASE_WORK,
    INTERVAL_PHASE_REST,
    INTERVAL_PHASE_DONE,
} interval_phase_t;

typedef struct {
    bool active;
    interval_phase_t phase;
    uint16_t round_idx;     // 1..rounds
    uint16_t rounds;

    interval_unit_t unit;   // current phase unit
    uint32_t target;        // current phase target
    uint32_t progress;      // current phase progress
    uint32_t remaining;     // target - progress

    uint16_t progress_permille; // 0..1000
} interval_ui_state_t;

void interval_program_init(void);

void interval_program_set_config(const interval_config_t *cfg);
void interval_program_get_config(interval_config_t *out);

void interval_program_start(void);
void interval_program_stop(void);

// Call this from main loop when you have updated totals
void interval_program_update(bool recording,
                             uint32_t session_time_ms,
                             float distance_m,
                             uint32_t stroke_count);

void interval_program_get_ui(interval_ui_state_t *out);
