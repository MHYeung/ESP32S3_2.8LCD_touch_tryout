#pragma once
#include "lvgl.h"
#include "coach_ui_snapshot.h"

void race_data_page_create(lv_obj_t *parent);
void race_data_page_apply_theme(void);
void race_data_page_on_orientation_changed(void);
void race_data_page_apply_snapshot(const coach_ui_snapshot_t *snap);
void race_data_page_show_start_prompt(void);
void race_data_page_hide_start_prompt(void);
