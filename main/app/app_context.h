#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "activity.h"
#include "activity_log.h"
#include "i2c_helper.h"
#include "lvgl.h"
#include "qmi8658.h"
#include "sd_mmc_helper.h"
#include "stroke_detection.h"
#include "ui.h"

extern sd_mmc_helper_t s_sd;
extern QueueHandle_t s_act_q;
extern TaskHandle_t s_act_worker_task;
extern bool s_activity_recording;
extern activity_t s_activity;
extern uint32_t s_activity_next_id;
extern float s_session_time_s;
extern int64_t s_session_last_us;
extern uint32_t s_last_session_stroke_count;
extern SemaphoreHandle_t s_activity_mutex;

extern QueueHandle_t s_log_q;
extern activity_log_t s_act_log;
extern uint32_t s_current_split_m;
extern bool s_interval_done_queued;

extern lv_disp_t *s_disp;
extern lv_indev_t *s_indev_touch;

extern qmi8658_handle_t s_imu;
extern i2c_helper_t s_imu_bus;
extern stroke_detection_t s_stroke;

extern ui_orientation_t s_current_orient;
extern bool s_auto_rotate_enabled;

extern int16_t s_last_touch_x;
extern int16_t s_last_touch_y;

extern bool s_time_synced_from_gps;
