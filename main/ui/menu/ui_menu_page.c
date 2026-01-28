// main/ui/ui_menu_page.c
#include "ui_menu_page.h"
#include "ui_activity_summary_page.h"
#include "ui.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <string.h>

#include "lvgl.h"

static const char *TAG = "ui_menu";

static lv_obj_t *s_grid = NULL;

#define MENU_ICON_COUNT 3
static lv_obj_t *s_btns[MENU_ICON_COUNT] = {0};
static uint8_t s_btn_count = 0;

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_status;

static void menu_icon_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    if (!id)
        return;

    if (strcmp(id, "settings") == 0)
    {
        ESP_LOGI(TAG, "Settings pressed -> go to settings page");
        ui_go_to_page(UI_SETTINGS_PAGE, true);
        return;
    }

    if (strcmp(id, "activity_summary") == 0)
    {
        ESP_LOGI(TAG, "Activity pressed -> go to activity summary");
        activity_summary_page_refresh(); // optional: refresh on enter
        ui_go_to_page(UI_ACTIVITY_SUMMARY_PAGE, true);
        return;
    }

    if (strcmp(id, "interval") == 0)
    {
        ESP_LOGI(TAG, "Interval pressed -> go to interval setup");
        ui_go_to_page(UI_INTERVAL_SETUP_PAGE, true);
        return;
    }

    // Dummy icons for now
    ESP_LOGI(TAG, "Menu icon pressed: %s (dummy)", id);
}

static lv_obj_t *create_icon_btn(lv_obj_t *parent, const char *symbol, const char *text,
                                 const char *id, lv_coord_t btn_size)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_size, btn_size);
    lv_obj_add_event_cb(btn, menu_icon_cb, LV_EVENT_CLICKED, (void *)id);

    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 5, 0);
    lv_obj_set_style_pad_row(btn, 3, 0);

    /* Icon (symbol) */
    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, symbol);
    ui_theme_apply_label(ic, false);

    /* Make the symbol bigger */
    // Pick fonts that you have enabled in lv_conf.h (see note below)
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_24, 0);

    /* Text under icon */
    lv_obj_t *lb = lv_label_create(btn);
    lv_label_set_text(lb, text);
    ui_theme_apply_label(lb, true);

    lv_obj_set_width(lb, btn_size); // so centering works nicely
    lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lb, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lb, lv_color_black(), 0);
    /* Make the text smaller/larger */
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_16, 0);

    return btn;
}

static void menu_apply_grid_layout(void)
{
    if (!s_grid)
        return;

    const bool land = ui_is_landscape();
    uint8_t count = s_btn_count > 0 ? s_btn_count : 1;
    int cols = land ? count : 1;
    int rows = land ? 1 : count;
    const bool use_two_by_two = (count == 3);

    if (use_two_by_two)
    {
        cols = 2;
        rows = 2;
    }

    lv_obj_set_layout(s_grid, LV_LAYOUT_GRID);

    // Templates must be persistent (static) for LVGL
    static lv_coord_t col_1[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_1[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    if (cols <= 1)
        lv_obj_set_grid_dsc_array(s_grid, col_1, rows <= 1 ? row_1 : (rows == 2 ? row_2 : row_3));
    else if (cols == 2)
        lv_obj_set_grid_dsc_array(s_grid, col_2, rows <= 1 ? row_1 : row_2);
    else
        lv_obj_set_grid_dsc_array(s_grid, col_3, row_1);

    // Compute a square button size from available width
    lv_obj_update_layout(s_grid);
    lv_coord_t cw = lv_obj_get_content_width(s_grid);
    lv_coord_t ch = lv_obj_get_content_height(lv_obj_get_parent(s_grid));
    if (ch <= 0)
        ch = lv_obj_get_content_height(s_grid);
    lv_coord_t gapc = lv_obj_get_style_pad_column(s_grid, 0);
    lv_coord_t gapr = lv_obj_get_style_pad_row(s_grid, 0);

    lv_coord_t btn_w = (cw - gapc * (cols - 1)) / cols;
    lv_coord_t btn_h = (ch - gapr * (rows - 1)) / rows;
    lv_coord_t btn = btn_w < btn_h ? btn_w : btn_h;
    btn = LV_CLAMP(64, btn, 120);

    // Resize + place buttons in row-major order
    for (uint8_t i = 0; i < s_btn_count; i++)
    {
        lv_obj_set_size(s_btns[i], btn, btn);

        int c = (cols > 1) ? (i % cols) : 0;
        int r = (cols > 1) ? (i / cols) : i;
        int cspan = 1;
        int rspan = 1;

        if (use_two_by_two && i == 2)
        {
            c = 0;
            r = 1;
            cspan = 2;
        }

        lv_obj_set_grid_cell(
            s_btns[i],
            LV_GRID_ALIGN_CENTER, c, cspan,
            LV_GRID_ALIGN_CENTER, r, rspan);
    }

    // Make the grid height fit exactly rows of buttons (so it stays centered nicely)
    lv_obj_set_height(s_grid, rows * btn + gapr * (rows - 1));
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
    lv_obj_set_style_pad_all(body, 10, 0);

    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // icon grid (dynamic based on count + orientation)
    s_grid = lv_obj_create(body);
    lv_obj_set_style_border_width(s_grid, 0, 0);
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_width(s_grid, lv_pct(100));
    lv_obj_set_style_pad_all(s_grid, 0, 0);
    lv_obj_set_style_pad_row(s_grid, 10, 0);
    lv_obj_set_style_pad_column(s_grid, 10, 0);

    lv_coord_t btn_size = 80; // initial size; will be updated by menu_apply_grid_layout()

    // Create 3 buttons
    s_btn_count = 0;
    s_btns[s_btn_count++] = create_icon_btn(s_grid, LV_SYMBOL_LIST, "Activity Summary", "activity_summary", btn_size);
    s_btns[s_btn_count++] = create_icon_btn(s_grid, LV_SYMBOL_REFRESH, "Interval", "interval", btn_size);
    s_btns[s_btn_count++] = create_icon_btn(s_grid, LV_SYMBOL_SETTINGS, "Settings", "settings", btn_size);

    // Apply orientation-dependent layout
    menu_apply_grid_layout();
}

void menu_page_apply_theme(void)
{
    if (!s_root)
        return;
    ui_status_bar_apply_theme(&s_status);
    // button labels are already themed at creation; add more theme hooks here if needed
}

void menu_page_on_orientation_changed(void)
{
    if (!s_root)
        return;
    ui_status_bar_force_refresh(&s_status);
    menu_apply_grid_layout();
}
