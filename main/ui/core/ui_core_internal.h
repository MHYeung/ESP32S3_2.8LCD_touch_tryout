#pragma once

#include "ui.h"
#include "lvgl.h"
#include <stdbool.h>

extern lv_disp_t *ui_s_disp;
extern lv_obj_t *ui_s_scr;
extern lv_obj_t *ui_s_page_data;
extern lv_obj_t *ui_s_page_settings;
extern lv_obj_t *ui_s_page_menu;
extern lv_obj_t *ui_s_page_activity_summary;
extern lv_obj_t *ui_s_page_activity_detail;
extern lv_obj_t *ui_s_page_interval_setup;
extern lv_obj_t *ui_s_page_interval_data;

extern lv_obj_t *ui_s_top_gesture;
extern lv_obj_t *ui_s_settings_bottom_gesture;
extern lv_obj_t *ui_s_menu_bottom_gesture;
extern lv_obj_t *ui_s_activity_sum_bottom_gesture;
extern lv_obj_t *ui_s_activity_detail_bottom_gesture;
extern lv_obj_t *ui_s_data_right_gesture;
extern lv_obj_t *ui_s_interval_left_gesture;

extern ui_page_t ui_s_current_page;
extern bool ui_s_transitioning;
extern bool ui_s_interval_start_armed;
extern bool ui_s_interval_data_visible;
extern volatile bool ui_s_modal_active;

extern bool ui_s_top_swipe_armed;
extern lv_point_t ui_s_top_swipe_sum;

extern bool ui_s_menu_swipe_armed;
extern lv_point_t ui_s_menu_swipe_sum;

extern bool ui_s_settings_swipe_armed;
extern lv_point_t ui_s_settings_swipe_sum;

extern bool ui_s_act_sum_swipe_armed;
extern lv_point_t ui_s_act_sum_swipe_sum;

extern bool ui_s_act_detail_swipe_armed;
extern lv_point_t ui_s_act_detail_swipe_sum;

extern bool ui_s_data_right_swipe_armed;
extern lv_point_t ui_s_data_right_swipe_sum;

extern bool ui_s_interval_left_swipe_armed;
extern lv_point_t ui_s_interval_left_swipe_sum;

extern ui_shutdown_confirm_cb_t ui_s_shutdown_confirm_cb;
extern lv_obj_t *ui_s_shutdown_overlay;
extern lv_obj_t *ui_s_shutdown_panel;
extern lv_obj_t *ui_s_shutdown_btn_box;
extern lv_obj_t *ui_s_shutdown_msg;
extern lv_obj_t *ui_s_btn_shutdown;
extern lv_obj_t *ui_s_btn_shutdown_cancel;

extern ui_stop_save_confirm_cb_t ui_s_stop_save_confirm_cb;
extern lv_obj_t *ui_s_stop_save_overlay;
extern lv_obj_t *ui_s_stop_save_panel;
extern lv_obj_t *ui_s_stop_save_btn_box;
extern lv_obj_t *ui_s_stop_save_msg;
extern lv_obj_t *ui_s_btn_save;
extern lv_obj_t *ui_s_btn_save_cancel;
extern char ui_s_stop_save_prompt_msg[64];

extern ui_dark_mode_cb_t ui_s_dark_mode_cb;
extern ui_auto_rotate_cb_t ui_s_auto_rotate_cb;
extern bool ui_s_dark_mode;

void ui_pages_relayout(void);
void ui_relayout_dialogs(void);
void create_pages_ui(void);

void top_swipe_event_cb(lv_event_t *e);
void settings_bottom_swipe_event_cb(lv_event_t *e);
void menu_bottom_swipe_event_cb(lv_event_t *e);
void activity_summary_bottom_swipe_event_cb(lv_event_t *e);
void activity_detail_bottom_swipe_event_cb(lv_event_t *e);
void data_right_swipe_event_cb(lv_event_t *e);
void interval_left_swipe_event_cb(lv_event_t *e);
