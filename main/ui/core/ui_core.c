#include "ui_core_internal.h"

#include "esp_lvgl_port.h"
#include "interval_program/interval_program.h"
#include "ui_activity_detail_page.h"
#include "ui_activity_summary_page.h"
#include "ui_data_page.h"
#include "ui_interval_data_page.h"
#include "ui_interval_setup_page.h"
#include "ui_menu_page.h"
#include "ui_settings_page.h"
#include "ui_theme.h"

bool ui_is_landscape(void)
{
    if (!ui_s_disp)
        return false;
    lv_display_rotation_t r = lv_display_get_rotation(ui_s_disp);
    return (r == LV_DISPLAY_ROTATION_90 || r == LV_DISPLAY_ROTATION_270);
}

void ui_register_stop_save_confirm_cb(ui_stop_save_confirm_cb_t cb) { ui_s_stop_save_confirm_cb = cb; }
void ui_register_shutdown_confirm_cb(ui_shutdown_confirm_cb_t cb) { ui_s_shutdown_confirm_cb = cb; }
void ui_register_dark_mode_cb(ui_dark_mode_cb_t cb) { ui_s_dark_mode_cb = cb; }
void ui_register_auto_rotate_cb(ui_auto_rotate_cb_t cb) { ui_s_auto_rotate_cb = cb; }

void ui_set_interval_start_armed(bool armed) { ui_s_interval_start_armed = armed; }
bool ui_take_interval_start_armed(void)
{
    bool armed = ui_s_interval_start_armed;
    ui_s_interval_start_armed = false;
    return armed;
}

void ui_set_interval_data_visible(bool visible)
{
    ui_s_interval_data_visible = visible;

    if (!visible && ui_s_current_page == UI_INTERVAL_DATA_PAGE)
    {
        ui_go_to_page(UI_PAGE_DATA, true);
    }
}

void ui_notify_dark_mode_changed(bool enabled)
{
    ui_set_dark_mode(enabled);
    if (ui_s_dark_mode_cb)
        ui_s_dark_mode_cb(enabled);
}

void ui_notify_auto_rotate_changed(bool enabled)
{
    if (ui_s_auto_rotate_cb)
        ui_s_auto_rotate_cb(enabled);
}

void ui_set_dark_mode(bool enabled)
{
    ui_s_dark_mode = enabled;
    lvgl_port_lock(0);
    ui_theme_set(enabled ? UI_THEME_DARK : UI_THEME_LIGHT);
    data_page_apply_theme();
    settings_page_apply_theme();
    menu_page_apply_theme();
    activity_summary_page_apply_theme();
    activity_detail_page_apply_theme();
    interval_setup_page_apply_theme();
    interval_data_page_apply_theme();
    lvgl_port_unlock();
}

bool ui_get_dark_mode(void) { return ui_s_dark_mode; }

ui_page_t ui_get_current_page(void)
{
    return ui_s_current_page;
}

bool ui_is_modal_active(void)
{
    return ui_s_modal_active;
}

void ui_init(lv_disp_t *disp)
{
    ui_s_disp = disp;
    lvgl_port_lock(0);
    ui_theme_init(ui_s_disp);
    create_pages_ui();
    interval_program_init();
    lvgl_port_unlock();
}
