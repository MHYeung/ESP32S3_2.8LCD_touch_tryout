#include "app_context.h"

sd_mmc_helper_t s_sd;
QueueHandle_t s_act_q = NULL;
TaskHandle_t s_act_worker_task = NULL;
volatile bool s_activity_recording = false;
activity_t s_activity;
uint32_t s_activity_next_id = 1;

float s_session_time_s = 0.0f;
int64_t s_session_last_us = 0;

uint32_t s_last_session_stroke_count = 0;
SemaphoreHandle_t s_activity_mutex = NULL;

QueueHandle_t s_log_q = NULL;
activity_log_t s_act_log;
uint32_t s_current_split_m = 1000;
bool s_interval_done_queued = false;

lv_disp_t *s_disp = NULL;
lv_indev_t *s_indev_touch = NULL;

qmi8658_handle_t s_imu;
i2c_helper_t s_imu_bus;
stroke_detection_t s_stroke;

ui_orientation_t s_current_orient = UI_ORIENT_LANDSCAPE_270;
bool s_auto_rotate_enabled = false;

int16_t s_last_touch_x = 0;
int16_t s_last_touch_y = 0;

bool s_time_synced_from_gps = false;
