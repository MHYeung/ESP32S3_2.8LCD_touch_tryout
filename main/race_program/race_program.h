#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simplified race program
 *
 * - Distance-based only
 * - Config only needs: target distance + target pace
 * - Splits are distance splits (default 500m) and used to compute "current split pace"
 *
 * Units:
 * - distance: meters
 * - pace: ms per 500m (rowing split)
 */

typedef struct {
    uint32_t race_target_distance_m;
    uint32_t race_target_pace_ms_per500;
    uint32_t race_target_split_length_m;
} race_config_t;

typedef struct {
    bool active;
    bool done;

    uint32_t target_distance_m;
    uint32_t distance_m;
    uint32_t remaining_m;
    uint16_t progress_permille;    // 0..1000

    // Distance split (default 500m)
    uint32_t split_distance_m;
    uint32_t split_progress_m;
    uint32_t split_remaining_m;
    uint32_t split_idx;            // 1..N

    // pace info (ms/500m). 0 if insufficient data.
    uint32_t target_pace_ms_per500;
    uint32_t current_split_pace_ms_per500;
    int32_t  current_split_pace_delta_ms_per500; // current - target (negative => faster)
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

// Helper (still useful for UI if you want to show predicted finish time)
uint32_t race_compute_time_s_from_distance_pace(uint32_t distance_m, uint32_t pace_ms_per500);

#ifdef __cplusplus
}
#endif
