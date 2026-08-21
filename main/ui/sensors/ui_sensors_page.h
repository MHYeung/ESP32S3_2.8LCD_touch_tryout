#pragma once
#include "lvgl.h"
#include "ui.h"

void sensors_page_create(lv_obj_t *parent);
void sensors_page_apply_theme(void);
void sensors_page_on_orientation_changed(void);
void sensors_page_set_return_page(ui_page_t page);
ui_page_t sensors_page_get_return_page(void);
