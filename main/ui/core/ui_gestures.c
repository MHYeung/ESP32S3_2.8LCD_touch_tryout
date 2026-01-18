#include "ui_core_internal.h"

void top_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_PAGE_DATA)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_top_swipe_sum.x = 0;
        ui_s_top_swipe_sum.y = 0;
        ui_s_top_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_top_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_top_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_top_swipe_sum.x += v.x;
        ui_s_top_swipe_sum.y += v.y;

        // Down swipe detected
        if (ui_s_top_swipe_sum.y > 30 && ui_s_top_swipe_sum.y > (LV_ABS(ui_s_top_swipe_sum.x) + 10))
        {
            ui_s_top_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_PAGE_MENU, true);
        }
    }
}

void settings_bottom_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_SETTINGS_PAGE)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_settings_swipe_sum.x = 0;
        ui_s_settings_swipe_sum.y = 0;
        ui_s_settings_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_settings_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_settings_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_settings_swipe_sum.x += v.x;
        ui_s_settings_swipe_sum.y += v.y;

        // Up swipe detected
        if (ui_s_settings_swipe_sum.y < -30 && LV_ABS(ui_s_settings_swipe_sum.y) > (LV_ABS(ui_s_settings_swipe_sum.x) + 10))
        {
            ui_s_settings_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_PAGE_MENU, true);
        }
    }
}

void menu_bottom_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_PAGE_MENU)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_menu_swipe_sum.x = 0;
        ui_s_menu_swipe_sum.y = 0;
        ui_s_menu_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_menu_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_menu_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_menu_swipe_sum.x += v.x;
        ui_s_menu_swipe_sum.y += v.y;

        // Up swipe detected
        if (ui_s_menu_swipe_sum.y < -30 && LV_ABS(ui_s_menu_swipe_sum.y) > (LV_ABS(ui_s_menu_swipe_sum.x) + 10))
        {
            ui_s_menu_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_PAGE_DATA, true);
        }
    }
}

void activity_summary_bottom_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_ACTIVITY_SUMMARY_PAGE)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_act_sum_swipe_sum.x = 0;
        ui_s_act_sum_swipe_sum.y = 0;
        ui_s_act_sum_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_act_sum_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_act_sum_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_act_sum_swipe_sum.x += v.x;
        ui_s_act_sum_swipe_sum.y += v.y;

        if (ui_s_act_sum_swipe_sum.y < -30 &&
            LV_ABS(ui_s_act_sum_swipe_sum.y) > (LV_ABS(ui_s_act_sum_swipe_sum.x) + 10))
        {
            ui_s_act_sum_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_PAGE_MENU, true);
        }
    }
}

void activity_detail_bottom_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_ACTIVITY_DETAIL_PAGE)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_act_detail_swipe_sum.x = 0;
        ui_s_act_detail_swipe_sum.y = 0;
        ui_s_act_detail_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_act_detail_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_act_detail_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_act_detail_swipe_sum.x += v.x;
        ui_s_act_detail_swipe_sum.y += v.y;

        if (ui_s_act_detail_swipe_sum.y < -30 &&
            LV_ABS(ui_s_act_detail_swipe_sum.y) > (LV_ABS(ui_s_act_detail_swipe_sum.x) + 10))
        {
            ui_s_act_detail_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_ACTIVITY_SUMMARY_PAGE, true);
        }
    }
}

void data_right_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_PAGE_DATA || !ui_s_interval_data_visible)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_data_right_swipe_sum.x = 0;
        ui_s_data_right_swipe_sum.y = 0;
        ui_s_data_right_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_data_right_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_data_right_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_data_right_swipe_sum.x += v.x;
        ui_s_data_right_swipe_sum.y += v.y;

        // swipe LEFT to open interval data
        if (ui_s_data_right_swipe_sum.x < -30 &&
            LV_ABS(ui_s_data_right_swipe_sum.x) > (LV_ABS(ui_s_data_right_swipe_sum.y) + 10))
        {
            ui_s_data_right_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_INTERVAL_DATA_PAGE, true);
        }
    }
}

void interval_left_swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_param(e);
    if (!indev || ui_s_transitioning || ui_s_current_page != UI_INTERVAL_DATA_PAGE || !ui_s_interval_data_visible)
        return;

    if (code == LV_EVENT_PRESSED)
    {
        ui_s_interval_left_swipe_sum.x = 0;
        ui_s_interval_left_swipe_sum.y = 0;
        ui_s_interval_left_swipe_armed = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        ui_s_interval_left_swipe_armed = false;
    }
    else if (code == LV_EVENT_PRESSING && ui_s_interval_left_swipe_armed)
    {
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        ui_s_interval_left_swipe_sum.x += v.x;
        ui_s_interval_left_swipe_sum.y += v.y;

        // swipe RIGHT back to data
        if (ui_s_interval_left_swipe_sum.x > 30 &&
            LV_ABS(ui_s_interval_left_swipe_sum.x) > (LV_ABS(ui_s_interval_left_swipe_sum.y) + 10))
        {
            ui_s_interval_left_swipe_armed = false;
            lv_indev_stop_processing(indev);
            lv_indev_wait_release(indev);
            ui_go_to_page(UI_PAGE_DATA, true);
        }
    }
}
