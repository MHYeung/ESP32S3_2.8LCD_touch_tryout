#pragma once
#include "lvgl.h"

void activity_summary_page_create(lv_obj_t *parent);
void activity_summary_page_apply_theme(void);
void activity_summary_page_on_orientation_changed(void);

/* call this when entering the page or after deleting an activity */
void activity_summary_page_refresh(void);
