#include "ui_sensors_page.h"

#include "sensor_hub.h"
#include "ui_settings_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_sensors";

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;
static lv_obj_t *s_status_lbl = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_btn_primary = NULL;
static lv_obj_t *s_btn_secondary = NULL;
static lv_obj_t *s_lbl_primary = NULL;
static lv_obj_t *s_lbl_secondary = NULL;
static lv_timer_t *s_timer = NULL;
static ui_page_t s_return_page = UI_PAGE_MENU;
static uint32_t s_seen_epoch = 0xFFFFFFFFu;
static bool s_entered = false;

static const char *state_text(sensor_hub_state_t st)
{
    switch (st) {
    case SENSOR_HUB_DISABLED:
        return "Bluetooth off in this build";
    case SENSOR_HUB_SCANNING:
        return "Scanning for C3 tracker…";
    case SENSOR_HUB_CONNECTING:
        return "Connecting…";
    case SENSOR_HUB_CONNECTED:
        return "Connected";
    default:
        return "Idle — tap Scan";
    }
}

static void sync_actions(sensor_hub_state_t st)
{
    if (!s_btn_primary || !s_btn_secondary || !s_lbl_primary || !s_lbl_secondary)
        return;

    bool connected = (st == SENSOR_HUB_CONNECTED);
    bool busy = (st == SENSOR_HUB_SCANNING || st == SENSOR_HUB_CONNECTING);

    if (connected) {
        lv_label_set_text(s_lbl_primary, "Disconnect");
        lv_label_set_text(s_lbl_secondary, "Forget");
        lv_obj_clear_flag(s_btn_secondary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(s_btn_primary, LV_STATE_DISABLED);
        return;
    }

    lv_label_set_text(s_lbl_primary, busy ? "Scanning" : "Scan");
    if (busy)
        lv_obj_add_state(s_btn_primary, LV_STATE_DISABLED);
    else
        lv_obj_clear_state(s_btn_primary, LV_STATE_DISABLED);

    if (sensor_hub_has_saved()) {
        lv_label_set_text(s_lbl_secondary, "Reconnect");
        lv_obj_clear_flag(s_btn_secondary, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_btn_secondary, LV_OBJ_FLAG_HIDDEN);
    }
}

static void primary_cb(lv_event_t *e)
{
    (void)e;
    if (sensor_hub_get_state() == SENSOR_HUB_CONNECTED) {
        (void)sensor_hub_disconnect();
        return;
    }
    esp_err_t err = sensor_hub_start_scan();
    if (err != ESP_OK)
        ESP_LOGW(TAG, "scan: %s", esp_err_to_name(err));
}

static void secondary_cb(lv_event_t *e)
{
    (void)e;
    if (sensor_hub_get_state() == SENSOR_HUB_CONNECTED) {
        (void)sensor_hub_forget();
        return;
    }
    esp_err_t err = sensor_hub_connect_saved();
    if (err != ESP_OK)
        ESP_LOGW(TAG, "saved connect: %s", esp_err_to_name(err));
}

static void result_click_cb(lv_event_t *e)
{
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    esp_err_t err = sensor_hub_connect_result(idx);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "connect[%u]: %s", (unsigned)idx, esp_err_to_name(err));
}

static void rebuild_list(void)
{
    if (!s_list)
        return;
    lv_obj_clean(s_list);

    sensor_hub_device_t peer;
    if (sensor_hub_get_peer(&peer)) {
        lv_obj_t *row = lv_obj_create(s_list);
        ui_theme_apply_surface(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lb = lv_label_create(row);
        char buf[48];
        snprintf(buf, sizeof(buf), "%s  %d dBm",
                 peer.name[0] ? peer.name : "Tracker", (int)peer.rssi);
        lv_label_set_text(lb, buf);
        ui_theme_apply_label(lb, false);
        lv_obj_center(lb);
        return;
    }

    sensor_hub_device_t devs[6];
    size_t n = sensor_hub_get_results(devs, 6);
    if (n == 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No trackers yet");
        ui_theme_apply_label(empty, true);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        lv_obj_t *row = lv_btn_create(s_list);
        ui_theme_apply_button(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_add_event_cb(row, result_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *lb = lv_label_create(row);
        char buf[48];
        snprintf(buf, sizeof(buf), "%s  %d dBm",
                 devs[i].name[0] ? devs[i].name : "C3-Tracker",
                 (int)devs[i].rssi);
        lv_label_set_text(lb, buf);
        lv_obj_center(lb);
    }
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_root)
        return;
    if (ui_get_current_page() != UI_SENSORS_PAGE) {
        s_entered = false;
        return;
    }

    if (!s_entered) {
        s_entered = true;
        if (sensor_hub_get_state() == SENSOR_HUB_IDLE)
            (void)sensor_hub_start_scan();
    }

    uint32_t epoch = sensor_hub_get_epoch();
    if (epoch == s_seen_epoch)
        return;
    s_seen_epoch = epoch;

    sensor_hub_state_t st = sensor_hub_get_state();
    if (s_status_lbl)
        lv_label_set_text(s_status_lbl, state_text(st));
    sync_actions(st);
    rebuild_list();
    settings_page_sync_sensors_state();
}

void sensors_page_set_return_page(ui_page_t page)
{
    s_return_page = page;
}

ui_page_t sensors_page_get_return_page(void)
{
    return s_return_page;
}

void sensors_page_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    ui_status_bar_create(&s_sb, s_root);

    lv_obj_t *body = lv_obj_create(s_root);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 4, 0);
    lv_obj_set_style_pad_row(body, 4, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    s_status_lbl = lv_label_create(body);
    lv_label_set_text(s_status_lbl, state_text(sensor_hub_get_state()));
    ui_theme_apply_label(s_status_lbl, true);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_lbl, lv_pct(100));

    lv_obj_t *bar = lv_obj_create(body);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 36);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_primary = lv_btn_create(bar);
    ui_theme_apply_button(s_btn_primary);
    lv_obj_set_width(s_btn_primary, lv_pct(48));
    lv_obj_set_height(s_btn_primary, 32);
    lv_obj_add_event_cb(s_btn_primary, primary_cb, LV_EVENT_CLICKED, NULL);
    s_lbl_primary = lv_label_create(s_btn_primary);
    lv_label_set_text(s_lbl_primary, "Scan");
    lv_obj_center(s_lbl_primary);

    s_btn_secondary = lv_btn_create(bar);
    ui_theme_apply_button(s_btn_secondary);
    lv_obj_set_width(s_btn_secondary, lv_pct(48));
    lv_obj_set_height(s_btn_secondary, 32);
    lv_obj_add_event_cb(s_btn_secondary, secondary_cb, LV_EVENT_CLICKED, NULL);
    s_lbl_secondary = lv_label_create(s_btn_secondary);
    lv_label_set_text(s_lbl_secondary, "Reconnect");
    lv_obj_center(s_lbl_secondary);

    s_list = lv_obj_create(body);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 4, 0);
    lv_obj_set_style_pad_bottom(s_list, 48, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    sync_actions(sensor_hub_get_state());
    rebuild_list();
    s_timer = lv_timer_create(tick_cb, 300, NULL);
}

void sensors_page_apply_theme(void)
{
    if (!s_root)
        return;
    ui_status_bar_apply_theme(&s_sb);
    if (s_status_lbl)
        ui_theme_apply_label(s_status_lbl, true);
    if (s_btn_primary)
        ui_theme_apply_button(s_btn_primary);
    if (s_btn_secondary)
        ui_theme_apply_button(s_btn_secondary);
}

void sensors_page_on_orientation_changed(void)
{
    if (!s_root)
        return;
    ui_status_bar_force_refresh(&s_sb);
}
