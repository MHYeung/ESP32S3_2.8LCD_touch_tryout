#include "ui_race_setup_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "ui.h"
#include "race_program.h"
#include "ui_race_data_page.h"
#include "nvs_helper.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;
static lv_obj_t *s_scroll = NULL;
static lv_obj_t *s_title = NULL;

static lv_obj_t *s_row_target = NULL;
static lv_obj_t *s_row_target2 = NULL;
static lv_obj_t *s_row_split = NULL;
static lv_obj_t *s_row_strategy = NULL;
static lv_obj_t *s_btn_row = NULL;

static lv_obj_t *s_line_target = NULL;
static lv_obj_t *s_line_target2 = NULL;
static lv_obj_t *s_line_split = NULL;
static lv_obj_t *s_line_strategy = NULL;

static lv_obj_t *sb_target = NULL;
static lv_obj_t *dd_target2 = NULL, *sb_target2 = NULL;
static lv_obj_t *dd_strategy = NULL;
static int s_build_step = -1;
static bool s_building = false;
static lv_timer_t *s_build_timer = NULL;
static lv_obj_t *s_build_line = NULL;
static lv_obj_t *s_build_dec_btn = NULL;
static lv_obj_t *s_build_inc_btn = NULL;
static lv_obj_t *s_split_val_lbl = NULL;

/* Split dialog handles */
static lv_obj_t *s_split_overlay = NULL;
static lv_obj_t *s_split_roller = NULL;

static uint32_t s_current_split_m = 500;

static void build_step_timer_cb(lv_timer_t *t);

// -----------------------
// Async relayout scheduler
// -----------------------
static bool s_relayout_scheduled = false;
static bool s_relayout_running = false;
static bool s_relayout_pending = false;

void race_setup_page_create(lv_obj_t *parent){
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    // Fixed status bar
    ui_status_bar_create(&s_sb, s_root);

    // ONE scroll container for everything else
    s_scroll = lv_obj_create(s_root);
    lv_obj_set_width(s_scroll, lv_pct(100));
    lv_obj_set_flex_grow(s_scroll, 1); // take remaining height under status bar
    lv_obj_set_style_bg_opa(s_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_scroll, 0, 0);
    lv_obj_set_style_pad_left(s_scroll, 2, 0);
    lv_obj_set_style_pad_right(s_scroll, 2, 0);
    lv_obj_set_style_pad_row(s_scroll, 1, 0);

    lv_obj_add_flag(s_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scroll, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_set_flex_flow(s_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scroll, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Title inside scroll (so everything is one column)
    s_title = lv_label_create(s_scroll);
    lv_label_set_text(s_title, "Interval Setup");
    ui_theme_apply_label(s_title, false);
    lv_obj_set_height(s_title, LV_SIZE_CONTENT);
}
