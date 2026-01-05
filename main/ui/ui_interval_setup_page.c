#include "ui_interval_setup_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "ui.h"
#include "interval_program.h"
#include <stdio.h>

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;

static lv_obj_t *dd_work, *sb_work;
static lv_obj_t *dd_rest, *sb_rest;
static lv_obj_t *sb_rounds;

static interval_unit_t dd_to_unit(uint32_t idx)
{
    if (idx == 0) return INTERVAL_UNIT_TIME;
    if (idx == 1) return INTERVAL_UNIT_DISTANCE;
    return INTERVAL_UNIT_STROKES;
}

static void configure_spinbox_for_unit(lv_obj_t *sb, interval_unit_t u)
{
    if (u == INTERVAL_UNIT_TIME) {
        lv_spinbox_set_range(sb, 10, 3600);
        lv_spinbox_set_step(sb, 10);
        lv_spinbox_set_digit_format(sb, 4, 0);
        lv_spinbox_set_value(sb, 60);
    } else if (u == INTERVAL_UNIT_DISTANCE) {
        lv_spinbox_set_range(sb, 50, 10000);
        lv_spinbox_set_step(sb, 50);
        lv_spinbox_set_digit_format(sb, 5, 0);
        lv_spinbox_set_value(sb, 500);
    } else {
        lv_spinbox_set_range(sb, 1, 500);
        lv_spinbox_set_step(sb, 1);
        lv_spinbox_set_digit_format(sb, 3, 0);
        lv_spinbox_set_value(sb, 20);
    }
}

static void dd_changed_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint32_t idx = lv_dropdown_get_selected(dd);

    if (dd == dd_work) configure_spinbox_for_unit(sb_work, dd_to_unit(idx));
    if (dd == dd_rest) configure_spinbox_for_unit(sb_rest, dd_to_unit(idx));
}

static void btn_inc_cb(lv_event_t *e) { lv_spinbox_increment(lv_event_get_user_data(e)); }
static void btn_dec_cb(lv_event_t *e) { lv_spinbox_decrement(lv_event_get_user_data(e)); }

static void make_row(lv_obj_t *parent, const char *title, lv_obj_t **out_dd, lv_obj_t **out_sb)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 6, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    ui_theme_apply_label(lbl, false);

    lv_obj_t *line = lv_obj_create(row);
    lv_obj_set_width(line, lv_pct(100));
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(line, 8, 0);

    lv_obj_t *dd = lv_dropdown_create(line);
    lv_dropdown_set_options(dd, "Time\nDistance\nStrokes");
    lv_dropdown_set_selected(dd, 0);
    lv_obj_add_event_cb(dd, dd_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *dec = lv_btn_create(line);
    lv_obj_t *d = lv_label_create(dec); lv_label_set_text(d, "-"); lv_obj_center(d);

    lv_obj_t *sb = lv_spinbox_create(line);
    lv_obj_set_width(sb, 76);

    lv_obj_t *inc = lv_btn_create(line);
    lv_obj_t *i = lv_label_create(inc); lv_label_set_text(i, "+"); lv_obj_center(i);

    lv_obj_add_event_cb(inc, btn_inc_cb, LV_EVENT_CLICKED, sb);
    lv_obj_add_event_cb(dec, btn_dec_cb, LV_EVENT_CLICKED, sb);

    configure_spinbox_for_unit(sb, INTERVAL_UNIT_TIME);

    *out_dd = dd;
    *out_sb = sb;
}

static void start_cb(lv_event_t *e)
{
    (void)e;
    interval_config_t cfg = {0};

    cfg.work.unit = dd_to_unit(lv_dropdown_get_selected(dd_work));
    cfg.work.value = (uint32_t)lv_spinbox_get_value(sb_work);

    cfg.rest.unit = dd_to_unit(lv_dropdown_get_selected(dd_rest));
    cfg.rest.value = (uint32_t)lv_spinbox_get_value(sb_rest);

    cfg.rounds = (uint16_t)lv_spinbox_get_value(sb_rounds);
    if (cfg.rounds == 0) cfg.rounds = 1;
    cfg.auto_advance = true;

    interval_program_set_config(&cfg);
    interval_program_start();

    // Jump into interval data page
    ui_go_to_page(UI_PAGE_DATA, false);
    ui_go_to_page(UI_INTERVAL_DATA_PAGE, true);
}

static void cancel_cb(lv_event_t *e)
{
    (void)e;
    ui_go_to_page(UI_PAGE_MENU, true);
}

void interval_setup_page_create(lv_obj_t *parent)
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
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, 10, 0);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(body);
    lv_label_set_text(title, "Interval Setup");
    ui_theme_apply_label(title, false);

    make_row(body, "Work", &dd_work, &sb_work);
    make_row(body, "Rest", &dd_rest, &sb_rest);

    // rounds row
    lv_obj_t *rrow = lv_obj_create(body);
    lv_obj_set_width(rrow, lv_pct(100));
    lv_obj_set_style_bg_opa(rrow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rrow, 0, 0);
    lv_obj_set_style_pad_all(rrow, 0, 0);
    lv_obj_set_flex_flow(rrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *rl = lv_label_create(rrow);
    lv_label_set_text(rl, "Rounds");
    ui_theme_apply_label(rl, true);

    sb_rounds = lv_spinbox_create(rrow);
    lv_spinbox_set_range(sb_rounds, 1, 50);
    lv_spinbox_set_value(sb_rounds, 10);
    lv_obj_set_width(sb_rounds, 76);

    // buttons
    lv_obj_t *btns = lv_obj_create(body);
    lv_obj_set_width(btns, lv_pct(100));
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *b_cancel = lv_btn_create(btns);
    lv_obj_add_event_cb(b_cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(b_cancel); lv_label_set_text(lc, "Cancel"); lv_obj_center(lc);

    lv_obj_t *b_start = lv_btn_create(btns);
    lv_obj_add_event_cb(b_start, start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ls = lv_label_create(b_start); lv_label_set_text(ls, "Start"); lv_obj_center(ls);
}

void interval_setup_page_apply_theme(void)
{
    if (!s_root) return;
    ui_status_bar_apply_theme(&s_sb);
}
void interval_setup_page_on_orientation_changed(void)
{
    if (!s_root) return;
    ui_status_bar_force_refresh(&s_sb);
}
