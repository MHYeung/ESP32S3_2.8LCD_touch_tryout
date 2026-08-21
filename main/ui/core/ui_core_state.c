#include "ui_core_internal.h"

lv_disp_t *ui_s_disp = NULL;
lv_obj_t *ui_s_scr = NULL;
lv_obj_t *ui_s_page_data = NULL;
lv_obj_t *ui_s_page_settings = NULL;
lv_obj_t *ui_s_page_menu = NULL;
lv_obj_t *ui_s_page_activity_summary = NULL;
lv_obj_t *ui_s_page_activity_detail = NULL;
lv_obj_t *ui_s_page_interval_setup = NULL;
lv_obj_t *ui_s_page_interval_data = NULL;
lv_obj_t *ui_s_page_race_setup = NULL;
lv_obj_t *ui_s_page_race_data = NULL;
lv_obj_t *ui_s_page_step_setup = NULL;
lv_obj_t *ui_s_page_sensors = NULL;

lv_obj_t *ui_s_top_gesture = NULL;
lv_obj_t *ui_s_settings_bottom_gesture = NULL;
lv_obj_t *ui_s_menu_bottom_gesture = NULL;
lv_obj_t *ui_s_activity_sum_bottom_gesture = NULL;
lv_obj_t *ui_s_activity_detail_bottom_gesture = NULL;
lv_obj_t *ui_s_sensors_bottom_gesture = NULL;
lv_obj_t *ui_s_data_right_gesture = NULL;
lv_obj_t *ui_s_interval_left_gesture = NULL;

ui_page_t ui_s_current_page = UI_PAGE_DATA;
bool ui_s_transitioning = false;
bool ui_s_interval_start_armed = false;
bool ui_s_interval_data_visible = false;
bool ui_s_race_start_armed = false;
bool ui_s_race_data_visible = false;
bool ui_s_step_start_armed = false;
volatile bool ui_s_modal_active = false;

bool ui_s_top_swipe_armed = false;
lv_point_t ui_s_top_swipe_sum = {0};

bool ui_s_menu_swipe_armed = false;
lv_point_t ui_s_menu_swipe_sum = {0};

bool ui_s_settings_swipe_armed = false;
lv_point_t ui_s_settings_swipe_sum = {0};

bool ui_s_act_sum_swipe_armed = false;
lv_point_t ui_s_act_sum_swipe_sum = {0};

bool ui_s_act_detail_swipe_armed = false;
lv_point_t ui_s_act_detail_swipe_sum = {0};

bool ui_s_overlay_swipe_armed = false;
lv_point_t ui_s_overlay_swipe_sum = {0};

bool ui_s_data_right_swipe_armed = false;
lv_point_t ui_s_data_right_swipe_sum = {0};

bool ui_s_interval_left_swipe_armed = false;
lv_point_t ui_s_interval_left_swipe_sum = {0};

ui_shutdown_confirm_cb_t ui_s_shutdown_confirm_cb = NULL;
lv_obj_t *ui_s_shutdown_overlay = NULL;
lv_obj_t *ui_s_shutdown_panel = NULL;
lv_obj_t *ui_s_shutdown_btn_box = NULL;
lv_obj_t *ui_s_shutdown_msg = NULL;
lv_obj_t *ui_s_btn_shutdown = NULL;
lv_obj_t *ui_s_btn_shutdown_cancel = NULL;

ui_stop_save_confirm_cb_t ui_s_stop_save_confirm_cb = NULL;
lv_obj_t *ui_s_stop_save_overlay = NULL;
lv_obj_t *ui_s_stop_save_panel = NULL;
lv_obj_t *ui_s_stop_save_btn_box = NULL;
lv_obj_t *ui_s_stop_save_msg = NULL;
lv_obj_t *ui_s_btn_save = NULL;
lv_obj_t *ui_s_btn_save_cancel = NULL;
char ui_s_stop_save_prompt_msg[64] = "Stop and save this session?";

ui_dark_mode_cb_t ui_s_dark_mode_cb = NULL;
ui_auto_rotate_cb_t ui_s_auto_rotate_cb = NULL;
bool ui_s_dark_mode = true;
volatile bool ui_s_touch_locked = false;
ui_status_bar_t ui_s_live_status;
