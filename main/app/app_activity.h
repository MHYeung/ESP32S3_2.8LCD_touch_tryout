#pragma once

#include <stdint.h>

typedef enum
{
    ACT_CMD_START = 0,
    ACT_CMD_START_INTERVAL_NORMAL,
    ACT_CMD_START_INTERVAL_STEP,
    ACT_CMD_START_RACE,
    ACT_CMD_STOP_SAVE,
} act_cmd_t;

void activity_worker_task(void *arg);
void activity_logger_task(void *arg);
void on_stop_save_confirmed(void);
void on_split_interval_changed(uint32_t length_m);
uint32_t resolve_interval_split_m(void);
