#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t distance_m;
    float target_pace_s_500;
} race_config_t;

typedef struct {
    bool active;
    bool finished;
    float delta_s;            /* + = behind the virtual boat */
    float remaining_m;
    float projected_finish_s;
} race_ui_state_t;

void race_program_init(void);

void race_program_set_config(const race_config_t *cfg);
void race_program_get_config(race_config_t *out);

void race_program_start(void);
void race_program_stop(void);

void race_program_update(bool recording,
                         float elapsed_s,
                         float distance_m,
                         float avg_speed_mps);

void race_program_get_ui(race_ui_state_t *out);

#ifdef __cplusplus
}
#endif
