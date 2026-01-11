#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RACE_BASIS_DISTANCE = 0,   // progress in meters
    RACE_BASIS_TIME,          // progress in seconds
} race_basis_t;

typedef enum {
    RACE_PACE_STRATEGY_EVEN = 0,
    RACE_PACE_STRATEGY_NEGATIVE,
    RACE_PACE_STRATEGY_POSITIVE,
} race_pace_strategy_t;

/**
 * Race config (single block program)
 *
 * Units:
 * - distance: meters
 * - time: seconds
 * - pace: ms per 500m (like rowing split)
 */
typedef struct {
    race_basis_t basis;

    // If basis == DISTANCE:
    uint32_t target_distance_m;
    uint32_t target_finish_time_s;     // optional (0 if not provided)

    // If basis == TIME:
    uint32_t target_duration_s;

    // Target pace (ms/500m). Optional; can be derived if distance+finish_time given.
    uint32_t target_pace_ms_per500;

    // Split setting (how UI/log breaks segments)
    bool     split_by_time;            // false => distance split, true => time split
    uint32_t split_distance_m;         // used when split_by_time == false
    uint32_t split_time_s;             // used when split_by_time == true

    race_pace_strategy_t pace_strategy;
} race_config_t;

/**
 * UI state snapshot
 * - target/progress/remaining follow basis units
 * - split_target/progress/remaining follow split units
 */
typedef struct {
    bool active;
    bool done;

    race_basis_t basis;
    uint32_t target;
    uint32_t progress;
    uint32_t remaining;
    uint16_t progress_permille;    // 0..1000

    bool split_by_time;
    uint32_t split_target;
    uint32_t split_progress;
    uint32_t split_remaining;
    uint32_t split_idx;            // 1..N

    // pace info (ms/500m). 0 if insufficient data.
    uint32_t target_pace_ms_per500;
    uint32_t current_split_pace_ms_per500;

    // current - target (negative => faster, positive => slower). 0 if target missing.
    int32_t  current_split_pace_delta_ms_per500;
} race_ui_state_t;

void race_program_init(void);

void race_program_set_config(const race_config_t *cfg);
void race_program_get_config(race_config_t *out);

void race_program_start(void);
void race_program_stop(void);

// Call this from your main loop when totals are updated
void race_program_update(bool recording,
                         uint32_t session_time_ms,
                         float distance_m,
                         uint32_t stroke_count);

void race_program_get_ui(race_ui_state_t *out);

/* helpers */
uint32_t race_compute_pace_ms_per500_from_distance_time(uint32_t distance_m, uint32_t time_s);
uint32_t race_compute_time_s_from_distance_pace(uint32_t distance_m, uint32_t pace_ms_per500);
uint32_t race_compute_distance_m_from_time_pace(uint32_t time_s, uint32_t pace_ms_per500);

#ifdef __cplusplus
}
#endif
