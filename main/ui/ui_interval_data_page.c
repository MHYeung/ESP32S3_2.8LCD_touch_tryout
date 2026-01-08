#include "ui_interval_data_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "interval_program.h"
#include "ui.h"
#include <stdio.h>
#include <math.h>

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;

static lv_obj_t *s_phase_lbl;
static lv_obj_t *s_round_lbl;
static lv_obj_t *s_rem_lbl;
static lv_obj_t *s_bar;
static lv_timer_t *s_timer = NULL;

static lv_obj_t *s_preview = NULL;
static lv_obj_t *s_live = NULL;
static lv_obj_t *s_live_rem_val = NULL;
static lv_obj_t *s_live_round_val = NULL;
static lv_obj_t *s_live_pace_val = NULL;
static float s_live_pace_s = NAN;

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

static void fmt_target(char *out, size_t n, interval_unit_t u, uint32_t v)
{
    if (u == INTERVAL_UNIT_TIME) {
        uint32_t mm = v / 60;
        uint32_t ss = v % 60;
        snprintf(out, n, "%lu:%02lu", (unsigned long)mm, (unsigned long)ss);
    } else if (u == INTERVAL_UNIT_DISTANCE) {
        snprintf(out, n, "%lum", (unsigned long)v);
    } else {
        snprintf(out, n, "%lu st", (unsigned long)v);
    }
}

static void fmt_pace(float sec, char *out, size_t out_len)
{
    if (!isfinite(sec) || sec <= 0.0f) {
        snprintf(out, out_len, "--:--.-");
        return;
    }
    int total = (int)sec;
    int tenths = (int)lroundf((sec - (float)total) * 10.0f);
    if (tenths >= 10) {
        tenths = 0;
        total += 1;
    }
    int s = total % 60;
    int m = (total / 60) % 60;
    snprintf(out, out_len, "%02d:%02d.%d", m, s, tenths);
}

static lv_obj_t *create_slot(lv_obj_t *parent, const char *title, lv_obj_t **out_value)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, lv_pct(100));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 6, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 2, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(box);
    ui_theme_apply_label(t, true);
    lv_label_set_text(t, title);

    lv_obj_t *v = lv_label_create(box);
    ui_theme_apply_label(v, false);
    lv_label_set_text(v, "--");

    if (out_value)
        *out_value = v;

    return box;
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_root) return;

    interval_ui_state_t st;
    interval_program_get_ui(&st);

    if (!st.active && st.phase == INTERVAL_PHASE_IDLE) {
        interval_config_t cfg = {0};
        interval_program_get_config(&cfg);

        char work[24];
        char rest[24];
        fmt_target(work, sizeof(work), cfg.work.unit, cfg.work.value);
        fmt_target(rest, sizeof(rest), cfg.rest.unit, cfg.rest.value);

        if (s_preview) lv_obj_clear_flag(s_preview, LV_OBJ_FLAG_HIDDEN);
        if (s_live) lv_obj_add_flag(s_live, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(s_phase_lbl, "Interval: READY");
        lv_label_set_text_fmt(s_round_lbl, "Work %s  Rest %s", work, rest);
        lv_label_set_text_fmt(s_rem_lbl, "Rounds: %u", cfg.rounds);
        lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
        return;
    }

    if (s_preview) lv_obj_add_flag(s_preview, LV_OBJ_FLAG_HIDDEN);
    if (s_live) lv_obj_clear_flag(s_live, LV_OBJ_FLAG_HIDDEN);

    const char *ph =
        (st.phase == INTERVAL_PHASE_WORK) ? "WORK" :
        (st.phase == INTERVAL_PHASE_REST) ? "REST" :
        (st.phase == INTERVAL_PHASE_DONE) ? "DONE" : "IDLE";

    char rem[24];
    fmt_val(rem, sizeof(rem), st.unit, st.remaining);
    if (s_live_rem_val)
        lv_label_set_text_fmt(s_live_rem_val, "%s (%s)", rem, ph);

    if (s_live_round_val) {
        if (st.rounds) {
            lv_label_set_text_fmt(s_live_round_val, "%u/%u", st.round_idx, st.rounds);
        } else {
            lv_label_set_text(s_live_round_val, "--");
        }
    }

    if (s_live_pace_val) {
        char pace[16];
        fmt_pace(s_live_pace_s, pace, sizeof(pace));
        lv_label_set_text(s_live_pace_val, pace);
    }

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

    s_preview = lv_obj_create(body);
    lv_obj_set_width(s_preview, lv_pct(100));
    lv_obj_set_style_bg_opa(s_preview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_preview, 0, 0);
    lv_obj_set_style_pad_all(s_preview, 0, 0);
    lv_obj_set_style_pad_row(s_preview, 10, 0);
    lv_obj_set_flex_flow(s_preview, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_preview, LV_OBJ_FLAG_SCROLLABLE);

    s_phase_lbl = lv_label_create(s_preview);
    ui_theme_apply_label(s_phase_lbl, false);
    lv_label_set_text(s_phase_lbl, "Interval: IDLE");

    s_round_lbl = lv_label_create(s_preview);
    ui_theme_apply_label(s_round_lbl, true);

    s_rem_lbl = lv_label_create(s_preview);
    ui_theme_apply_label(s_rem_lbl, false);
    lv_obj_set_width(s_rem_lbl, lv_pct(100));
    lv_obj_set_style_text_align(s_rem_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_rem_lbl, "--");

    s_bar = lv_bar_create(s_preview);
    lv_obj_set_width(s_bar, lv_pct(100));
    lv_bar_set_range(s_bar, 0, 1000);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

    s_live = lv_obj_create(body);
    lv_obj_set_width(s_live, lv_pct(100));
    lv_obj_set_style_bg_opa(s_live, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_live, 0, 0);
    lv_obj_set_style_pad_all(s_live, 0, 0);
    lv_obj_set_style_pad_row(s_live, 8, 0);
    lv_obj_set_flex_flow(s_live, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_live, LV_OBJ_FLAG_HIDDEN);

    create_slot(s_live, "Remaining", &s_live_rem_val);
    create_slot(s_live, "Set", &s_live_round_val);
    create_slot(s_live, "Pace (/500m)", &s_live_pace_val);

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

void interval_data_page_set_pace_s_per_500m(float pace_s)
{
    s_live_pace_s = pace_s;
}
