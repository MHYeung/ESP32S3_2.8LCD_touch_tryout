#include "ui_core_internal.h"

#include "ui_activity_detail_page.h"
#include "ui_activity_summary_page.h"
#include "ui_data_page.h"
#include "ui_interval_data_page.h"
#include "ui_interval_setup_page.h"
#include "ui_menu_page.h"
#include "ui_settings_page.h"
#include "ui_theme.h"

void create_pages_ui(void)
{
    ui_s_scr = lv_disp_get_scr_act(ui_s_disp);
    lv_obj_clean(ui_s_scr);
    ui_theme_apply_screen(ui_s_scr);

    // 1. Data Page
    ui_s_page_data = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_data, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_data, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_data, 0, 0);
    lv_obj_set_style_bg_opa(ui_s_page_data, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_page_data, 0, 0);
    data_page_create(ui_s_page_data);

    // 2. Menu Page (NEW)
    ui_s_page_menu = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_menu, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_menu, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_menu, 0, 0);
    ui_theme_apply_screen(ui_s_page_menu);
    menu_page_create(ui_s_page_menu);

    // 3. Settings Page
    ui_s_page_settings = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_settings, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_settings, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_settings, 0, 0);
    ui_theme_apply_screen(ui_s_page_settings);
    settings_page_create(ui_s_page_settings);
    // Activity Summary page
    ui_s_page_activity_summary = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_activity_summary, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_activity_summary, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_activity_summary, 0, 0);
    ui_theme_apply_screen(ui_s_page_activity_summary);
    activity_summary_page_create(ui_s_page_activity_summary);

    // Activity Detail page
    ui_s_page_activity_detail = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_activity_detail, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_activity_detail, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_activity_detail, 0, 0);
    ui_theme_apply_screen(ui_s_page_activity_detail);
    activity_detail_page_create(ui_s_page_activity_detail);

    // Interval Setup page - created lazily on first navigation to reduce startup memory

    // Interval Data page (gallery)
    ui_s_page_interval_data = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_interval_data, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_interval_data, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_interval_data, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_interval_data, 0, 0);
    ui_theme_apply_screen(ui_s_page_interval_data);
    interval_data_page_create(ui_s_page_interval_data);

    // 4. Gestures
    ui_s_top_gesture = lv_obj_create(ui_s_scr);
    lv_obj_set_style_bg_opa(ui_s_top_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_top_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_top_gesture, top_swipe_event_cb, LV_EVENT_ALL, NULL);

    // Menu bottom gesture (Data -> Menu)
    ui_s_menu_bottom_gesture = lv_obj_create(ui_s_page_menu);
    lv_obj_set_style_bg_opa(ui_s_menu_bottom_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_menu_bottom_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_menu_bottom_gesture, menu_bottom_swipe_event_cb, LV_EVENT_ALL, NULL);

    ui_s_activity_sum_bottom_gesture = lv_obj_create(ui_s_page_activity_summary);
    lv_obj_set_style_bg_opa(ui_s_activity_sum_bottom_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_activity_sum_bottom_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_activity_sum_bottom_gesture, activity_summary_bottom_swipe_event_cb, LV_EVENT_ALL, NULL);

    ui_s_activity_detail_bottom_gesture = lv_obj_create(ui_s_page_activity_detail);
    lv_obj_set_style_bg_opa(ui_s_activity_detail_bottom_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_activity_detail_bottom_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_activity_detail_bottom_gesture, activity_detail_bottom_swipe_event_cb, LV_EVENT_ALL, NULL);

    // Setting bottom gesture (Setting -> Menu)
    ui_s_settings_bottom_gesture = lv_obj_create(ui_s_page_settings);
    lv_obj_set_style_bg_opa(ui_s_settings_bottom_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_settings_bottom_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_settings_bottom_gesture, settings_bottom_swipe_event_cb, LV_EVENT_ALL, NULL);

    // Data right-edge gesture (swipe left to interval)
    ui_s_data_right_gesture = lv_obj_create(ui_s_page_data);
    lv_obj_set_style_bg_opa(ui_s_data_right_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_data_right_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_data_right_gesture, data_right_swipe_event_cb, LV_EVENT_ALL, NULL);

    // Interval left-edge gesture (swipe right back)
    ui_s_interval_left_gesture = lv_obj_create(ui_s_page_interval_data);
    lv_obj_set_style_bg_opa(ui_s_interval_left_gesture, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_interval_left_gesture, 0, 0);
    lv_obj_add_event_cb(ui_s_interval_left_gesture, interval_left_swipe_event_cb, LV_EVENT_ALL, NULL);

    ui_s_current_page = UI_PAGE_DATA;
    ui_pages_relayout();
}

void ensure_interval_setup_page(void)
{
    if (ui_s_page_interval_setup)
        return;
    if (!ui_s_scr)
        return;

    ui_s_page_interval_setup = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_interval_setup, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_interval_setup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_interval_setup, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_interval_setup, 0, 0);
    ui_theme_apply_screen(ui_s_page_interval_setup);
    interval_setup_page_create(ui_s_page_interval_setup);

    /* Position the new page correctly (hidden above screen) */
    ui_pages_relayout();
}
