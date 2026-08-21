#pragma once
#include "lvgl.h"
#include "coach_ui_snapshot.h"

void interval_data_page_create(lv_obj_t *parent);
void interval_data_page_apply_theme(void);
void interval_data_page_on_orientation_changed(void);
void interval_data_page_apply_snapshot(const coach_ui_snapshot_t *snap);
void interval_data_page_show_start_prompt(void);
void interval_data_page_hide_start_prompt(void);
void interval_data_page_show_complete_prompt(void);
