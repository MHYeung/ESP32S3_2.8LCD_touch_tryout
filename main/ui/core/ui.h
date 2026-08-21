#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void ui_init(lv_disp_t *disp);

typedef enum {
    UI_ORIENT_PORTRAIT_0,
    UI_ORIENT_LANDSCAPE_90,
    UI_ORIENT_PORTRAIT_180,
    UI_ORIENT_LANDSCAPE_270,
} ui_orientation_t;

typedef enum {
    UI_PAGE_DATA = 0,
    UI_PAGE_MENU,
    UI_INTERVAL_DATA_PAGE,

    UI_ACTIVITY_SUMMARY_PAGE,
    UI_INTERVAL_SETUP_PAGE,
    UI_SETTINGS_PAGE,

    UI_ACTIVITY_DETAIL_PAGE,
    UI_RACE_SETUP_PAGE,
    UI_RACE_DATA_PAGE,
    UI_STEP_SETUP_PAGE,
    UI_SENSORS_PAGE,
    UI_PAGE_COUNT,
} ui_page_t;

bool ui_is_landscape(void);
ui_page_t ui_get_current_page(void);

/* Long Press to shut down */
typedef void (*ui_shutdown_confirm_cb_t)(void);
void ui_register_shutdown_confirm_cb(ui_shutdown_confirm_cb_t cb);
void ui_show_shutdown_prompt(void);

/* Press to confirm stop recording the activity */
typedef void (*ui_stop_save_confirm_cb_t)(void);
void ui_register_stop_save_confirm_cb(ui_stop_save_confirm_cb_t cb);
void ui_show_stop_save_prompt(void);
void ui_show_stop_save_prompt_with_text(const char *msg);
bool ui_is_modal_active(void);

void ui_set_orientation(ui_orientation_t o);
void ui_go_to_page(ui_page_t page, bool animated);
/* Navigate on the next LVGL tick so the click handler can return and IDLE0 can pet the WDT. */
void ui_defer_go_to_page(ui_page_t page);
/* Yield so IDLE0 can run during heavy widget construction on the LVGL task. */
void ui_yield_for_idle(void);

// Interval start arming (set by interval setup confirm)
void ui_set_interval_start_armed(bool armed);
bool ui_take_interval_start_armed(void);
void ui_set_interval_data_visible(bool visible);

// Race start arming (set by race setup confirm)
void ui_set_race_start_armed(bool armed);
bool ui_take_race_start_armed(void);
void ui_set_race_data_visible(bool visible);

// Step-test start arming (set by step setup confirm; reuses interval live page)
void ui_set_step_start_armed(bool armed);
bool ui_take_step_start_armed(void);


/* Theme helpers (currently implemented as light/dark). */
void ui_set_dark_mode(bool enabled);
bool ui_get_dark_mode(void);

typedef void (*ui_dark_mode_cb_t)(bool enabled);
typedef void (*ui_auto_rotate_cb_t)(bool enabled);

void ui_register_dark_mode_cb(ui_dark_mode_cb_t cb);
void ui_register_auto_rotate_cb(ui_auto_rotate_cb_t cb);

/* NEW: internal helpers that per-page code can call */
void ui_notify_dark_mode_changed(bool enabled);
void ui_notify_auto_rotate_changed(bool enabled);

/* Water/touch lock on live screens. Physical PWR still starts/stops. */
void ui_set_touch_lock(bool locked);
bool ui_is_touch_lock(void);
void ui_toggle_touch_lock(void);

void ui_set_brightness_percent(uint8_t percent);
uint8_t ui_get_brightness_percent(void);
void ui_set_auto_dim(bool enabled);
bool ui_get_auto_dim(void);

void ui_set_display_sleep(bool sleep);
void ui_toggle_display_sleep(void);
bool ui_is_display_sleep(void);
/* Hardware PWR is not an LVGL indev; call this so auto-dim / sleep stay in sync. */
void ui_notify_user_activity(void);


