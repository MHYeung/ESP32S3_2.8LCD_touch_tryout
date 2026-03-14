#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
#include "ui.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *time_label;

    /* GPS widget: icon + 4-bar signal strength indicator */
    lv_obj_t *gps_cont;
    lv_obj_t *gps_icon;
    lv_obj_t *gps_bars[4]; /* ascending-height bars, bottom-aligned */

    lv_obj_t *batt_label;
    lv_timer_t *clock_timer;
    lv_timer_t *batt_timer;
    uint32_t clock_start_ms;
    uint32_t clock_start_sec;
    ui_orientation_t orient;
    bool gps_connected;
    uint8_t gps_bars_count; /* last known bar count (0-4) */
} ui_status_bar_t;

void ui_status_bar_create(ui_status_bar_t *bar, lv_obj_t *parent);
void ui_status_bar_apply_theme(ui_status_bar_t *bar);
void ui_status_bar_set_orientation(ui_status_bar_t *bar, ui_orientation_t o);
void ui_status_bar_set_gps_status(ui_status_bar_t *bar, bool connected, uint8_t bars_0_to_4);
void ui_status_bar_set_battery(ui_status_bar_t *bar, int percent);
void ui_status_bar_set_time_base(ui_status_bar_t *bar, uint32_t start_sec);
lv_obj_t *ui_status_bar_root(ui_status_bar_t *bar);

void ui_status_bar_force_refresh(ui_status_bar_t *bar);

// Call from ANY task
void ui_status_bar_set_gps_default_safe(bool connected, uint8_t bars_0_to_4);

// Optional: set which status bar is the default (if you ever have multiple)
void ui_status_bar_set_default(ui_status_bar_t *bar);
