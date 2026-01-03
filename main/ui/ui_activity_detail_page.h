#pragma once
#include <stdint.h>
#include "lvgl.h"

void activity_detail_page_create(lv_obj_t *parent);
void activity_detail_page_open(uint32_t activity_id, const char *csv_path);
