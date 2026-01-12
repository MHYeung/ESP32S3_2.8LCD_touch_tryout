#pragma once

#include "lvgl.h"
#include <stdbool.h>

void race_data_page_create(lv_obj_t *parent);
void race_data_page_apply_theme(void);
void race_data_page_on_orientation_changed(void);

void race_data_page_set_pace_s_per_500m(float pace_s);
void race_data_page_set_spm(float spm);

void race_data_page_show_start_prompt(void);
void race_data_page_hide_start_prompt(void);
void race_data_page_show_complete_prompt(void);
