// main/ui/ui_menu_page.c
#include "ui_menu_page.h"
#include "ui.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_menu";

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_status;

static lv_obj_t *s_btn_activity = NULL;
static lv_obj_t *s_btn_settings = NULL;
static lv_obj_t *s_btn_interval = NULL;

static void menu_icon_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    if (!id) return;

    if (strcmp(id, "settings") == 0) {
        ESP_LOGI(TAG, "Settings pressed -> go to settings page");
        ui_go_to_page(UI_PAGE_SETTINGS, true);
        return;
    }

    // Dummy icons for now
    ESP_LOGI(TAG, "Menu icon pressed: %s (dummy)", id);
}

static lv_obj_t *create_icon_btn(lv_obj_t *parent, const char *symbol, const char *text, const char *id)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 84, 84);
    lv_obj_add_event_cb(btn, menu_icon_cb, LV_EVENT_CLICKED, (void *)id);

    // simple vertical layout inside the button
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 8, 0);
    lv_obj_set_style_pad_row(btn, 6, 0);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, symbol);
    ui_theme_apply_label(ic, false);

    lv_obj_t *lb = lv_label_create(btn);
    lv_label_set_text(lb, text);
    ui_theme_apply_label(lb, true);

    return btn;
}

void menu_page_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);

    // 1) Status bar
    ui_status_bar_create(&s_status, s_root);

    // 2) Body
    lv_obj_t *body = lv_obj_create(s_root);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, 16, 0);

    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // icon row
    lv_obj_t *row = lv_obj_create(body);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_width(row, lv_pct(100));

    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 0, 0);

    s_btn_activity  = create_icon_btn(row, LV_SYMBOL_LIST,     "Activity",  "activity");
    s_btn_settings  = create_icon_btn(row, LV_SYMBOL_SETTINGS, "Settings",  "settings");
    s_btn_interval  = create_icon_btn(row, LV_SYMBOL_REFRESH,  "Interval",  "interval");
}

void menu_page_apply_theme(void)
{
    if (!s_root) return;
    ui_status_bar_apply_theme(&s_status);
    // button labels are already themed at creation; add more theme hooks here if needed
}

void menu_page_on_orientation_changed(void)
{
    if (!s_root) return;
    ui_status_bar_force_refresh(&s_status);
}
