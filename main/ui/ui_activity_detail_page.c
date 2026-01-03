#include "ui_activity_detail_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_act_detail";
static uint32_t s_act_id = 0;
static char s_csv_path[160] = {0};

void activity_detail_page_open(uint32_t activity_id, const char *csv_path)
{
    s_act_id = activity_id;
    strncpy(s_csv_path, csv_path ? csv_path : "", sizeof(s_csv_path) - 1);
    ESP_LOGI(TAG, "Open detail: id=%lu path=%s", (unsigned long)s_act_id, s_csv_path);

    // Later: parse CSV + populate UI
}

void activity_detail_page_create(lv_obj_t *parent)
{
    lv_obj_clean(parent); // important
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);

    static ui_status_bar_t sb;
    ui_status_bar_create(&sb, parent);

    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "Activity Detail (stub)");
    ui_theme_apply_label(lbl, false);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}
