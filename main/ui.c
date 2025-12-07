#include "ui.h"
#include "esp_lvgl_port.h" // for lvgl_port_lock/unlock
#include <stdio.h>
#include <string.h>

/* Keep LVGL objects private to this module */
static lv_disp_t *s_disp = NULL;

static lv_obj_t *s_tabview;
static lv_obj_t *s_tab_controls;
static lv_obj_t *s_tab_imu;
static lv_obj_t *s_tab_system;
static lv_obj_t *s_tab_settings;

static lv_obj_t *s_label_imu;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_ser_x;
static lv_chart_series_t *s_ser_y;
static lv_chart_series_t *s_ser_z;

static ui_sd_test_cb_t s_sd_test_cb = NULL;
static ui_dark_mode_cb_t s_dark_mode_cb = NULL;
static ui_auto_rotate_cb_t s_auto_rotate_cb = NULL;

/* ------------ Internal UI callbacks ------------ */

static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);

    const char *txt = lv_label_get_text(label);
    if (strcmp(txt, "Click me") == 0)
    {
        lv_label_set_text(label, "Touched!");
    }
    else
    {
        lv_label_set_text(label, "Click me");
    }
}

void ui_register_sd_test_cb(ui_sd_test_cb_t cb)
{
    s_sd_test_cb = cb;
}

void ui_register_dark_mode_cb(ui_dark_mode_cb_t cb)
{
    s_dark_mode_cb = cb;
}
void ui_register_auto_rotate_cb(ui_auto_rotate_cb_t cb)
{
    s_auto_rotate_cb = cb;
}

static lv_obj_t *create_settings_row(lv_obj_t *parent,
                                     const char *label_txt,
                                     lv_event_cb_t switch_event_cb,
                                     bool initial_state)
{
    // Row container (horizontal flex)
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_START,   // main axis
                          LV_FLEX_ALIGN_CENTER,  // cross axis
                          LV_FLEX_ALIGN_CENTER); // track

    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_border_width(row, 0, 0);

    // Label on the left
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_txt);
    lv_obj_set_flex_grow(lbl, 1); // take remaining width

    // Switch on the right
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (initial_state)
        lv_obj_add_state(sw, LV_STATE_CHECKED);

    return sw; // in case you later want to store it
}

static void sw_dark_mode_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (s_dark_mode_cb)
    {
        s_dark_mode_cb(on);
    }
}

static void sw_auto_rotate_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (s_auto_rotate_cb)
    {
        s_auto_rotate_cb(on);
    }
}

void ui_set_orientation(ui_orientation_t o)
{
    /* Map our enum -> LVGL’s rotation enum */
    lv_display_rotation_t rot = LV_DISPLAY_ROTATION_0;

    switch (o)
    {
    case UI_ORIENT_PORTRAIT_0:
        rot = LV_DISPLAY_ROTATION_0;
        break;
    case UI_ORIENT_LANDSCAPE_90:
        rot = LV_DISPLAY_ROTATION_90;
        break;
    case UI_ORIENT_PORTRAIT_180:
        rot = LV_DISPLAY_ROTATION_180;
        break;
    case UI_ORIENT_LANDSCAPE_270:
        rot = LV_DISPLAY_ROTATION_270;
        break;
    }

    lvgl_port_lock(0);
    lv_display_set_rotation(s_disp, rot); // LVGL 9 API
    lvgl_port_unlock();
}

static void create_tabs_ui(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);

    /* LVGL 9 tabview creation */
    s_tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(s_tabview, LV_DIR_TOP); // optional, top bar

    /* Create tabs */
    s_tab_controls = lv_tabview_add_tab(s_tabview, "Controls");
    s_tab_imu = lv_tabview_add_tab(s_tabview, "IMU");
    s_tab_system = lv_tabview_add_tab(s_tabview, "System");
    s_tab_settings = lv_tabview_add_tab(s_tabview, "Settings");

    /* -------- Demo Test Tab -------- */
    lv_obj_t *label1 = lv_label_create(s_tab_controls);
    lv_label_set_text(label1, "Page 1: Button test");
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *btn = lv_button_create(s_tab_controls);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click me");
    lv_obj_center(btn_label);

    /* -------- IMU Tab -------- */
    s_label_imu = lv_label_create(s_tab_imu);
    lv_label_set_text(s_label_imu, "ax=?  ay=?  az=?  m/s^2");
    lv_obj_align(s_label_imu, LV_ALIGN_TOP_MID, 0, 10);

    s_chart = lv_chart_create(s_tab_imu);
    lv_obj_set_size(s_chart, lv_pct(95), lv_pct(70));
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 60);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, -2000, 2000);

    s_ser_x = lv_chart_add_series(s_chart,
                                  lv_palette_main(LV_PALETTE_RED),
                                  LV_CHART_AXIS_PRIMARY_Y);
    s_ser_y = lv_chart_add_series(s_chart,
                                  lv_palette_main(LV_PALETTE_GREEN),
                                  LV_CHART_AXIS_PRIMARY_Y);
    s_ser_z = lv_chart_add_series(s_chart,
                                  lv_palette_main(LV_PALETTE_BLUE),
                                  LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_all_value(s_chart, s_ser_x, 0);
    lv_chart_set_all_value(s_chart, s_ser_y, 0);
    lv_chart_set_all_value(s_chart, s_ser_z, 0);

    lv_obj_t *sys_label = lv_label_create(s_tab_system);
    lv_label_set_text(sys_label, "System info (placeholder)");
    lv_obj_align(sys_label, LV_ALIGN_TOP_LEFT, 4, 4);

    /* -------- "Settings Tab" -------- */
    lv_obj_t *cont = lv_obj_create(s_tab_settings);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_center(cont);

    // Use flex layout: vertical list
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 10, 0); // spacing between rows
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_border_width(cont, 0, 0); // optional: no border

    // Dark mode row
    create_settings_row(cont,
                        "Dark mode",
                        sw_dark_mode_event_cb,
                        false /* initial off, or true if you default to dark */);

    // Auto-rotate row
    create_settings_row(cont,
                        "Auto rotate",
                        sw_auto_rotate_event_cb,
                        true /* if you default auto-rotation on */);
}

/* ------------ Public API ------------ */

void ui_init(lv_disp_t *disp)
{
    s_disp = disp;

    /* UI creation must hold the LVGL lock */
    lvgl_port_lock(0);
    create_tabs_ui();
    lvgl_port_unlock();
}

void ui_update_imu(float ax, float ay, float az)
{
    /* Convert to centi-m/s^2 for the chart */
    int16_t cx = (int16_t)(ax * 100.0f);
    int16_t cy = (int16_t)(ay * 100.0f);
    int16_t cz = (int16_t)(az * 100.0f);

    lvgl_port_lock(0);

    if (s_label_imu)
    {
        static char buf[64];
        snprintf(buf, sizeof(buf),
                 "ax=%.2f  ay=%.2f  az=%.2f  m/s^2",
                 ax, ay, az);
        lv_label_set_text(s_label_imu, buf);
    }

    if (s_chart)
    {
        lv_chart_set_next_value(s_chart, s_ser_x, cx);
        lv_chart_set_next_value(s_chart, s_ser_y, cy);
        lv_chart_set_next_value(s_chart, s_ser_z, cz);
    }

    lvgl_port_unlock();
}

void ui_go_to_page(ui_page_t page, bool animated)
{
    if (!s_tabview)
        return;

    /* Make sure the page index is valid */
    if (page < 0 || page >= UI_PAGE_COUNT)
    {
        return;
    }

    lvgl_port_lock(0);
    lv_tabview_set_active(
        s_tabview,
        (uint32_t)page, // index = enum value
        animated ? LV_ANIM_ON : LV_ANIM_OFF);
    lvgl_port_unlock();
}
