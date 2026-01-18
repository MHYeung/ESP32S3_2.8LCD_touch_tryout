#include "ui_core_internal.h"

#include "esp_lvgl_port.h"
#include "ui_activity_detail_page.h"
#include "ui_activity_summary_page.h"
#include "ui_data_page.h"
#include "ui_interval_data_page.h"
#include "ui_interval_setup_page.h"
#include "ui_menu_page.h"
#include "ui_settings_page.h"

static void anim_set_x(void *var, int32_t v) { lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)v); }
static void anim_set_y(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v); }
static void anim_done_cb(lv_anim_t *a)
{
    (void)a;
    ui_s_transitioning = false;
    ui_pages_relayout();
}

void ui_pages_relayout(void)
{
    if (!ui_s_scr)
        return;
    lv_coord_t h = lv_obj_get_height(ui_s_scr);
    lv_coord_t w = lv_obj_get_width(ui_s_scr);

    if (ui_s_page_data)
    {
        lv_obj_set_size(ui_s_page_data, lv_pct(100), lv_pct(100));
        lv_obj_set_pos(ui_s_page_data, 0, 0);
    }

    if (ui_s_page_interval_data)
    {
        lv_obj_set_size(ui_s_page_interval_data, lv_pct(100), lv_pct(100));
        if (ui_s_interval_data_visible && ui_s_current_page == UI_INTERVAL_DATA_PAGE)
        {
            lv_obj_set_pos(ui_s_page_interval_data, 0, 0);
            lv_obj_clear_flag(ui_s_page_interval_data, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_interval_data, w, 0);
            lv_obj_add_flag(ui_s_page_interval_data, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_menu)
    {
        lv_obj_set_size(ui_s_page_menu, lv_pct(100), lv_pct(100));

        if (ui_s_current_page == UI_PAGE_MENU || ui_s_current_page == UI_SETTINGS_PAGE || ui_s_current_page == UI_INTERVAL_SETUP_PAGE)
        {
            lv_obj_set_pos(ui_s_page_menu, 0, 0);
            lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_menu, 0, -h);
            lv_obj_add_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_settings)
    {
        lv_obj_set_size(ui_s_page_settings, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_SETTINGS_PAGE)
        {
            lv_obj_set_pos(ui_s_page_settings, 0, 0);
            lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_settings, 0, -h);
            lv_obj_add_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_interval_setup)
    {
        lv_obj_set_size(ui_s_page_interval_setup, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_INTERVAL_SETUP_PAGE)
        {
            lv_obj_set_pos(ui_s_page_interval_setup, 0, 0);
            lv_obj_clear_flag(ui_s_page_interval_setup, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_interval_setup, 0, -h);
            lv_obj_add_flag(ui_s_page_interval_setup, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_activity_summary)
    {
        lv_obj_set_size(ui_s_page_activity_summary, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
        {
            lv_obj_set_pos(ui_s_page_activity_summary, 0, 0);
            lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_activity_summary, 0, -h);
            lv_obj_add_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_activity_detail)
    {
        lv_obj_set_size(ui_s_page_activity_detail, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_ACTIVITY_DETAIL_PAGE)
        {
            lv_obj_set_pos(ui_s_page_activity_detail, 0, 0);
            lv_obj_clear_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_activity_detail, 0, -h);
            lv_obj_add_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_top_gesture)
    {
        lv_obj_set_size(ui_s_top_gesture, lv_pct(100), lv_pct(15));
        lv_obj_set_pos(ui_s_top_gesture, 0, 0);
        if (ui_s_current_page == UI_PAGE_DATA && !ui_s_transitioning)
        {
            lv_obj_clear_flag(ui_s_top_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_top_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_top_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_menu_bottom_gesture)
    {
        lv_obj_set_size(ui_s_menu_bottom_gesture, lv_pct(100), lv_pct(15));
        lv_obj_align(ui_s_menu_bottom_gesture, LV_ALIGN_BOTTOM_MID, 0, 0);
        if (ui_s_current_page == UI_PAGE_MENU && !ui_s_transitioning)
        {
            lv_obj_clear_flag(ui_s_menu_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_menu_bottom_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_menu_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_activity_sum_bottom_gesture)
    {
        lv_obj_set_size(ui_s_activity_sum_bottom_gesture, lv_pct(100), lv_pct(15));
        lv_obj_align(ui_s_activity_sum_bottom_gesture, LV_ALIGN_BOTTOM_MID, 0, 0);
        if (ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE && !ui_s_transitioning)
        {
            lv_obj_clear_flag(ui_s_activity_sum_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_activity_sum_bottom_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_activity_sum_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_activity_detail_bottom_gesture)
    {
        lv_obj_set_size(ui_s_activity_detail_bottom_gesture, lv_pct(100), lv_pct(15));
        lv_obj_align(ui_s_activity_detail_bottom_gesture, LV_ALIGN_BOTTOM_MID, 0, 0);
        if (ui_s_current_page == UI_ACTIVITY_DETAIL_PAGE && !ui_s_transitioning)
        {
            lv_obj_clear_flag(ui_s_activity_detail_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_activity_detail_bottom_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_activity_detail_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_settings_bottom_gesture)
    {
        lv_obj_set_size(ui_s_settings_bottom_gesture, lv_pct(100), lv_pct(15));
        lv_obj_align(ui_s_settings_bottom_gesture, LV_ALIGN_BOTTOM_MID, 0, 0);
        if (ui_s_current_page == UI_SETTINGS_PAGE && !ui_s_transitioning)
        {
            lv_obj_clear_flag(ui_s_settings_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_settings_bottom_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_settings_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_data_right_gesture)
    {
        lv_obj_set_size(ui_s_data_right_gesture, lv_pct(15), lv_pct(100));
        lv_obj_align(ui_s_data_right_gesture, LV_ALIGN_RIGHT_MID, 0, 0);
        if (ui_s_current_page == UI_PAGE_DATA && !ui_s_transitioning && ui_s_interval_data_visible)
        {
            lv_obj_clear_flag(ui_s_data_right_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_data_right_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_data_right_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_interval_left_gesture)
    {
        lv_obj_set_size(ui_s_interval_left_gesture, lv_pct(15), lv_pct(100));
        lv_obj_align(ui_s_interval_left_gesture, LV_ALIGN_LEFT_MID, 0, 0);
        if (ui_s_current_page == UI_INTERVAL_DATA_PAGE && !ui_s_transitioning && ui_s_interval_data_visible)
        {
            lv_obj_clear_flag(ui_s_interval_left_gesture, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_s_interval_left_gesture);
        }
        else
        {
            lv_obj_add_flag(ui_s_interval_left_gesture, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_set_orientation(ui_orientation_t o)
{
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
    lv_display_set_rotation(ui_s_disp, rot);

    // Updates
    ui_pages_relayout();
    ui_relayout_dialogs();
    menu_page_on_orientation_changed();
    settings_page_on_orientation_changed();
    activity_summary_page_on_orientation_changed();
    activity_detail_page_on_orientation_changed();
    interval_setup_page_on_orientation_changed();
    interval_data_page_on_orientation_changed();
    lvgl_port_unlock();

    data_page_set_orientation(o);
}

void ui_go_to_page(ui_page_t target, bool animated)
{
    if (target == ui_s_current_page || ui_s_transitioning)
        return;
    if (!ui_s_scr)
        return;
    if ((target == UI_PAGE_MENU) && !ui_s_page_menu)
        return;
    if ((target == UI_SETTINGS_PAGE) && !ui_s_page_settings)
        return;
    if ((target == UI_ACTIVITY_SUMMARY_PAGE) && !ui_s_page_activity_summary)
        return;
    if ((target == UI_ACTIVITY_DETAIL_PAGE) && !ui_s_page_activity_detail)
        return;
    if ((target == UI_INTERVAL_SETUP_PAGE) && !ui_s_page_interval_setup)
        return;
    if ((target == UI_INTERVAL_DATA_PAGE) && (!ui_s_page_interval_data || !ui_s_interval_data_visible))
        return;

    if (target != UI_INTERVAL_DATA_PAGE)
        ui_s_interval_start_armed = false;

    lvgl_port_lock(0);
    lv_coord_t h = lv_obj_get_height(ui_s_scr);
    lv_coord_t w = lv_obj_get_width(ui_s_scr);

    if (!animated)
    {
        ui_s_current_page = target;
        ui_pages_relayout();
        lvgl_port_unlock();
        return;
    }

    // Hide gesture layers during transition
    if (ui_s_top_gesture)
        lv_obj_add_flag(ui_s_top_gesture, LV_OBJ_FLAG_HIDDEN);
    if (ui_s_menu_bottom_gesture)
        lv_obj_add_flag(ui_s_menu_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
    if (ui_s_settings_bottom_gesture)
        lv_obj_add_flag(ui_s_settings_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
    if (ui_s_activity_sum_bottom_gesture)
        lv_obj_add_flag(ui_s_activity_sum_bottom_gesture, LV_OBJ_FLAG_HIDDEN);
    if (ui_s_activity_detail_bottom_gesture)
        lv_obj_add_flag(ui_s_activity_detail_bottom_gesture, LV_OBJ_FLAG_HIDDEN);

    bool use_x = false;
    lv_obj_t *anim_obj = NULL;
    int32_t from_y = 0, to_y = 0;
    ui_page_t next_page = target;

    // DATA -> MENU (open menu)
    if (target == UI_PAGE_MENU && ui_s_current_page == UI_PAGE_DATA)
    {
        anim_obj = ui_s_page_menu;
        lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_menu, -h);
        lv_obj_move_foreground(ui_s_page_menu);
        from_y = -h;
        to_y = 0;
        next_page = UI_PAGE_MENU;
    }
    // MENU -> DATA (close menu)
    else if (target == UI_PAGE_DATA && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_menu;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_DATA;
    }
    else if (target == UI_INTERVAL_DATA_PAGE && ui_s_current_page == UI_PAGE_DATA)
    {
        anim_obj = ui_s_page_interval_data;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(anim_obj, w, 0);
        lv_obj_move_foreground(anim_obj);

        // we'll animate X, reusing from_y/to_y vars as from_x/to_x
        from_y = w;
        to_y = 0;
        next_page = UI_INTERVAL_DATA_PAGE;

        // special flag: animate x
        // (see below where you set exec_cb)
        use_x = true;
    }
    else if (target == UI_PAGE_DATA && ui_s_current_page == UI_INTERVAL_DATA_PAGE)
    {
        anim_obj = ui_s_page_interval_data;
        from_y = 0;
        to_y = w;
        next_page = UI_PAGE_DATA;
        use_x = true;
    }
    // MENU -> ACTIVITY_SUMMARY
    else if (target == UI_ACTIVITY_SUMMARY_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_activity_summary;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_ACTIVITY_SUMMARY_PAGE;
    }
    // ACTIVITY_SUMMARY -> MENU
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
    {
        anim_obj = ui_s_page_activity_summary;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    // ACTIVITY_SUMMARY -> ACTIVITY_DETAIL
    else if (target == UI_ACTIVITY_DETAIL_PAGE && ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
    {
        anim_obj = ui_s_page_activity_detail;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_ACTIVITY_DETAIL_PAGE;
    }
    // ACTIVITY_DETAIL -> ACTIVITY_SUMMARY
    else if (target == UI_ACTIVITY_SUMMARY_PAGE && ui_s_current_page == UI_ACTIVITY_DETAIL_PAGE)
    {
        // Make sure summary is ready behind the outgoing detail page
        lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_activity_summary, 0);

        anim_obj = ui_s_page_activity_detail;
        from_y = 0;
        to_y = -h;
        next_page = UI_ACTIVITY_SUMMARY_PAGE;
    }
    // Menu -> Interval Setup
    else if (target == UI_INTERVAL_SETUP_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_interval_setup;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_INTERVAL_SETUP_PAGE;
    }
    // Interval Setup -> Menu
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_INTERVAL_SETUP_PAGE)
    {
        anim_obj = ui_s_page_interval_setup;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    // INTERVAL_SETUP -> INTERVAL_DATA (Confirm)
    else if (target == UI_INTERVAL_DATA_PAGE && ui_s_current_page == UI_INTERVAL_SETUP_PAGE)
    {
        anim_obj = ui_s_page_interval_data;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(anim_obj, w, 0); // start off-screen to the right
        lv_obj_move_foreground(anim_obj);

        // animate X: w -> 0
        from_y = w;
        to_y = 0;
        next_page = UI_INTERVAL_DATA_PAGE;
        use_x = true;
    }
    // MENU -> SETTINGS (open settings)
    else if (target == UI_SETTINGS_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_settings;
        lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_settings, -h);
        lv_obj_move_foreground(ui_s_page_settings);
        from_y = -h;
        to_y = 0;
        next_page = UI_SETTINGS_PAGE;
    }
    // SETTINGS -> MENU (close settings)
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_SETTINGS_PAGE)
    {
        anim_obj = ui_s_page_settings;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    else
    {
        // Not handled yet (future pages)
        lvgl_port_unlock();
        return;
    }

    ui_s_current_page = next_page;
    ui_s_transitioning = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, anim_obj);
    lv_anim_set_exec_cb(&a, use_x ? anim_set_x : anim_set_y);
    lv_anim_set_values(&a, from_y, to_y);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, anim_done_cb);
    lv_anim_start(&a);

    lvgl_port_unlock();
}
