#include "ui_interval_data_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "interval_program.h"
#include "ui.h"
#include <stdio.h>

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;

static lv_obj_t *s_phase_lbl;
static lv_obj_t *s_round_lbl;
static lv_obj_t *s_rem_lbl;
static lv_obj_t *s_bar;
static lv_timer_t *s_timer = NULL;

static void fmt_val(char *out, size_t n, interval_unit_t u, uint32_t v)
{
    if (u == INTERVAL_UNIT_TIME) {
        uint32_t mm = v / 60;
        uint32_t ss = v % 60;
        snprintf(out, n, "%lu:%02lu", (unsigned long)mm, (unsigned long)ss);
    } else if (u == INTERVAL_UNIT_DISTANCE) {
        snprintf(out, n, "%lum", (unsigned long)v);
    } else {
        snprintf(out, n, "%lu", (unsigned long)v);
    }
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_root) return;

    interval_ui_state_t st;
    interval_program_get_ui(&st);

    const char *ph =
        (st.phase == INTERVAL_PHASE_WORK) ? "WORK" :
        (st.phase == INTERVAL_PHASE_REST) ? "REST" :
        (st.phase == INTERVAL_PHASE_DONE) ? "DONE" : "IDLE";

    lv_label_set_text_fmt(s_phase_lbl, "Interval: %s", ph);

    if (st.active && st.rounds) {
        lv_label_set_text_fmt(s_round_lbl, "Round %u/%u", st.round_idx, st.rounds);
    } else {
        lv_label_set_text(s_round_lbl, "");
    }

    char rem[24];
    fmt_val(rem, sizeof(rem), st.unit, st.remaining);
    lv_label_set_text(s_rem_lbl, rem);

    lv_bar_set_value(s_bar, st.progress_permille, LV_ANIM_OFF);
}

void interval_data_page_create(lv_obj_t *parent)
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

    s_phase_lbl = lv_label_create(body);
    ui_theme_apply_label(s_phase_lbl, false);
    lv_label_set_text(s_phase_lbl, "Interval: IDLE");

    s_round_lbl = lv_label_create(body);
    ui_theme_apply_label(s_round_lbl, true);

    s_rem_lbl = lv_label_create(body);
    ui_theme_apply_label(s_rem_lbl, false);
    lv_obj_set_width(s_rem_lbl, lv_pct(100));
    lv_obj_set_style_text_align(s_rem_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_rem_lbl, "--");

    s_bar = lv_bar_create(body);
    lv_obj_set_width(s_bar, lv_pct(100));
    lv_bar_set_range(s_bar, 0, 1000);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

    if (!s_timer) s_timer = lv_timer_create(tick_cb, 200, NULL);
    tick_cb(NULL);
}

void interval_data_page_apply_theme(void)
{
    if (!s_root) return;
    ui_status_bar_apply_theme(&s_sb);
}
void interval_data_page_on_orientation_changed(void)
{
    if (!s_root) return;
    ui_status_bar_force_refresh(&s_sb);
}
