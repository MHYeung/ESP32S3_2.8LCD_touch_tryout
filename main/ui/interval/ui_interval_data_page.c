#include "ui_interval_data_page.h"
#include "ui_theme.h"
#include "interval_program.h"
#include "ui.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "lvgl.h"

static lv_obj_t *s_root = NULL;
static lv_timer_t *s_timer = NULL;

static lv_obj_t *s_live = NULL;
static lv_obj_t *s_live_rem_val = NULL;
static lv_obj_t *s_live_round_val = NULL;
static lv_obj_t *s_live_pace_val = NULL;
static lv_obj_t *s_live_rem_title = NULL;
static lv_obj_t *s_live_round_title = NULL;
static lv_obj_t *s_live_pace_title = NULL;
static lv_obj_t *s_live_spm_title = NULL;
static lv_obj_t *s_live_spm_val = NULL;
static float s_live_pace_s = NAN;
static float s_live_spm = NAN;

static lv_obj_t *s_prompt_overlay = NULL;
static lv_obj_t *s_prompt_panel = NULL;
static lv_obj_t *s_prompt_rows = NULL;
static lv_obj_t *s_prompt_hint = NULL;
static bool s_prompt_allowed = false;

static lv_obj_t *s_noti_overlay = NULL;
static lv_obj_t *s_noti_panel = NULL;
static lv_obj_t *s_noti_lbl = NULL;
static uint32_t s_noti_hide_at_ms = 0;
static interval_phase_t s_noti_phase = INTERVAL_PHASE_IDLE;
static bool s_noti_show_start = false;

static lv_obj_t *s_complete_overlay = NULL;
static lv_obj_t *s_complete_panel = NULL;
static lv_obj_t *s_complete_lbl = NULL;
static uint32_t s_complete_hide_at_ms = 0;

static void start_prompt_show_async(void *p);
static void start_prompt_hide_async(void *p);
static void complete_prompt_show_async(void *p);

static void label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    if (!label || !text)
        return;
    const char *cur = lv_label_get_text(label);
    if (!cur || strcmp(cur, text) != 0)
    {
        lv_label_set_text(label, text);
    }
}

static void complete_overlay_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_complete_overlay)
    {
        lv_obj_add_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
        s_complete_hide_at_ms = 0;
    }
}

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

#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
extern const lv_font_t lv_font_montserrat_20;
#define SLOT_FONT_TITLE (&lv_font_montserrat_20)
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
extern const lv_font_t lv_font_montserrat_16;
#define SLOT_FONT_TITLE (&lv_font_montserrat_16)
#else
#define SLOT_FONT_TITLE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_44) && LV_FONT_MONTSERRAT_44
extern const lv_font_t lv_font_montserrat_44;
#define SLOT_FONT_VALUE (&lv_font_montserrat_44)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
extern const lv_font_t lv_font_montserrat_40;
#define SLOT_FONT_VALUE (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
extern const lv_font_t lv_font_montserrat_36;
#define SLOT_FONT_VALUE (&lv_font_montserrat_36)
#else
#define SLOT_FONT_VALUE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
extern const lv_font_t lv_font_montserrat_36;
#define SLOT_FONT_VALUE_SMALL (&lv_font_montserrat_36)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
extern const lv_font_t lv_font_montserrat_32;
#define SLOT_FONT_VALUE_SMALL (&lv_font_montserrat_32)
#else
#define SLOT_FONT_VALUE_SMALL (SLOT_FONT_VALUE)
#endif



static bool is_landscape(void)
{
    return ui_is_landscape();
}

static void style_box(lv_obj_t *box)
{
    ui_theme_apply_surface_border(box);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_border_color(box, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(box, LV_DIR_NONE);
}

static lv_obj_t *create_slot(lv_obj_t *parent, const char *title, lv_obj_t **out_title, lv_obj_t **out_value)
{
    lv_obj_t *box = lv_obj_create(parent);
    style_box(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *t = lv_label_create(box);
    lv_obj_set_style_text_font(t, SLOT_FONT_TITLE, 0);
    ui_theme_apply_label(t, true);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *v = lv_label_create(box);
    lv_obj_set_style_text_font(v, SLOT_FONT_VALUE, 0);
    ui_theme_apply_label(v, false);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);

    if (out_title)
        *out_title = t;
    if (out_value)
        *out_value = v;

    return box;
}

static void apply_live_layout(void)
{
    if (!s_live || !s_live_rem_val || !s_live_round_val || !s_live_pace_val || !s_live_spm_val)
        return;

    bool land = is_landscape();
    lv_obj_set_layout(s_live, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(s_live, 0, 0);
    lv_obj_set_style_pad_row(s_live, 0, 0);
    lv_obj_set_style_pad_column(s_live, 0, 0);
    lv_obj_set_style_border_width(s_live, 0, 0);

    if (land)
    {
        static int32_t s_col_land3[] = {LV_GRID_FR(3), LV_GRID_FR(4), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
        static int32_t s_row_land2[] = {LV_GRID_FR(4), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(s_live, s_col_land3, s_row_land2);

        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_rem_val),
                             LV_GRID_ALIGN_STRETCH, 0, 3,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_round_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_spm_val),
                             LV_GRID_ALIGN_STRETCH, 1, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_pace_val),
                             LV_GRID_ALIGN_STRETCH, 2, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
    }
    else
    {
        static int32_t s_col_port1[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static int32_t s_row_port4[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(s_live, s_col_port1, s_row_port4);

        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_rem_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_round_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_spm_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 2, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_pace_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 3, 1);
    }

    // bottom slots use smaller font in landscape to match data_page feel
    const lv_font_t *bottom = land ? SLOT_FONT_VALUE_SMALL : SLOT_FONT_VALUE;
    lv_obj_set_style_text_font(s_live_round_val, bottom, 0);
    lv_obj_set_style_text_font(s_live_spm_val, bottom, 0);
    lv_obj_set_style_text_font(s_live_pace_val, bottom, 0);
}

static void update_noti(interval_ui_state_t st)
{
    if (st.phase != s_noti_phase)
    {
        s_noti_phase = st.phase;
        if (st.phase == INTERVAL_PHASE_WORK || st.phase == INTERVAL_PHASE_REST)
        {
            s_noti_show_start = true;
            s_noti_hide_at_ms = lv_tick_get() + 3000;
        }
    }

    if (s_noti_overlay && s_noti_hide_at_ms > 0)
    {
        if ((int32_t)(lv_tick_get() - s_noti_hide_at_ms) >= 0)
        {
            lv_obj_add_flag(s_noti_overlay, LV_OBJ_FLAG_HIDDEN);
            s_noti_hide_at_ms = 0;
            s_noti_show_start = false;
        }
    }

    if (st.phase == INTERVAL_PHASE_DONE || st.phase == INTERVAL_PHASE_IDLE)
        return;

    if (!s_noti_overlay)
    {
        lv_obj_t *top = lv_layer_top();
        s_noti_overlay = lv_obj_create(top);
        lv_obj_set_size(s_noti_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_opa(s_noti_overlay, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_noti_overlay, 0, 0);
        lv_obj_clear_flag(s_noti_overlay, LV_OBJ_FLAG_SCROLLABLE);

        s_noti_panel = lv_obj_create(s_noti_overlay);
        lv_obj_set_size(s_noti_panel, 160, 60);
        lv_obj_set_style_bg_opa(s_noti_panel, LV_OPA_80, 0);
        lv_obj_set_style_bg_color(s_noti_panel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_border_width(s_noti_panel, 0, 0);
        lv_obj_set_style_radius(s_noti_panel, 6, 0);
        lv_obj_center(s_noti_panel);
        lv_obj_clear_flag(s_noti_panel, LV_OBJ_FLAG_SCROLLABLE);

        s_noti_lbl = lv_label_create(s_noti_panel);
        ui_theme_apply_label(s_noti_lbl, false);
        lv_obj_center(s_noti_lbl);
    }

    lv_obj_set_style_bg_color(s_noti_panel,
                              (st.phase == INTERVAL_PHASE_WORK)
                                  ? lv_palette_main(LV_PALETTE_ORANGE)
                                  : lv_palette_main(LV_PALETTE_GREEN),
                              0);
    if (s_noti_lbl)
    {
        if (s_noti_show_start)
        {
            const char *start = (st.phase == INTERVAL_PHASE_WORK) ? "WORK start" : "REST start";
            label_set_text_if_changed(s_noti_lbl, start);
        }
        else if (st.unit == INTERVAL_UNIT_TIME && st.remaining <= 5)
        {
            const char *next = (st.phase == INTERVAL_PHASE_WORK) ? "REST" : "WORK";
            char buf[24];
            snprintf(buf, sizeof(buf), "%s in %us", next, (unsigned)st.remaining);
            label_set_text_if_changed(s_noti_lbl, buf);
        }
        else
        {
            lv_obj_add_flag(s_noti_overlay, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }

    if (s_noti_overlay)
        lv_obj_clear_flag(s_noti_overlay, LV_OBJ_FLAG_HIDDEN);
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

        if (s_live_rem_val)
            label_set_text_if_changed(s_live_rem_val, "READY");
        if (s_live_rem_title)
            label_set_text_if_changed(s_live_rem_title, "Remaining");
        if (s_live_round_val)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "0/%u", cfg.rounds);
            label_set_text_if_changed(s_live_round_val, buf);
        }
        if (s_live_round_title)
            label_set_text_if_changed(s_live_round_title, "Set");
        if (s_live_pace_val)
            label_set_text_if_changed(s_live_pace_val, "--:--.-");
        if (s_live_pace_title)
            label_set_text_if_changed(s_live_pace_title, "Pace (/500m)");
        if (s_live_spm_val)
            label_set_text_if_changed(s_live_spm_val, "--");
        if (s_live_spm_title)
            label_set_text_if_changed(s_live_spm_title, "SPM");
        if (s_noti_overlay)
            lv_obj_add_flag(s_noti_overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char rem[24];
    fmt_val(rem, sizeof(rem), st.unit, st.remaining);
    if (s_live_rem_val)
        label_set_text_if_changed(s_live_rem_val, rem);

    if (s_live_rem_title)
    {
        const char *ph =
            (st.phase == INTERVAL_PHASE_WORK) ? "WORK" :
            (st.phase == INTERVAL_PHASE_REST) ? "REST" :
            (st.phase == INTERVAL_PHASE_DONE) ? "DONE" : "IDLE";
        char title[32];
        snprintf(title, sizeof(title), "Remaining: %s", ph);
        label_set_text_if_changed(s_live_rem_title, title);
    }

    if (s_live_round_val) {
        if (st.rounds) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%u/%u", st.round_idx, st.rounds);
            label_set_text_if_changed(s_live_round_val, buf);
        } else {
            label_set_text_if_changed(s_live_round_val, "--");
        }
    }
    if (s_live_round_title)
        label_set_text_if_changed(s_live_round_title, "Set");

    if (s_live_pace_val) {
        char pace[16];
        fmt_pace(s_live_pace_s, pace, sizeof(pace));
        label_set_text_if_changed(s_live_pace_val, pace);
    }
    if (s_live_pace_title)
        label_set_text_if_changed(s_live_pace_title, "Pace (/500m)");

    if (s_live_spm_val) {
        if (!isfinite(s_live_spm)) {
            label_set_text_if_changed(s_live_spm_val, "--");
        } else {
            char buf[12];
            float frac = fabsf(s_live_spm - floorf(s_live_spm));
            if (fabsf(frac - 0.5f) < 0.01f)
                snprintf(buf, sizeof(buf), "%.1f", (double)s_live_spm);
            else
                snprintf(buf, sizeof(buf), "%.0f", (double)s_live_spm);
            label_set_text_if_changed(s_live_spm_val, buf);
        }
    }
    if (s_live_spm_title)
        label_set_text_if_changed(s_live_spm_title, "SPM");

    update_noti(st);

    if (s_complete_overlay && s_complete_hide_at_ms > 0)
    {
        if ((int32_t)(lv_tick_get() - s_complete_hide_at_ms) >= 0)
        {
            lv_obj_add_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
            s_complete_hide_at_ms = 0;
        }
    }
}

void interval_data_page_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_live = s_root;

    create_slot(s_live, "Remaining", &s_live_rem_title, &s_live_rem_val);
    create_slot(s_live, "Set", &s_live_round_title, &s_live_round_val);
    create_slot(s_live, "SPM", &s_live_spm_title, &s_live_spm_val);
    create_slot(s_live, "Pace (/500m)", &s_live_pace_title, &s_live_pace_val);

    apply_live_layout();

    if (!s_timer) s_timer = lv_timer_create(tick_cb, 200, NULL);
    tick_cb(NULL);
}

void interval_data_page_apply_theme(void)
{
    if (!s_root) return;

    if (s_live)
    {
        if (s_live_rem_title)
            ui_theme_apply_label(s_live_rem_title, true);
        if (s_live_round_title)
            ui_theme_apply_label(s_live_round_title, true);
        if (s_live_spm_title)
            ui_theme_apply_label(s_live_spm_title, true);
        if (s_live_pace_title)
            ui_theme_apply_label(s_live_pace_title, true);
        if (s_live_rem_val)
        {
            style_box(lv_obj_get_parent(s_live_rem_val));
            ui_theme_apply_label(s_live_rem_val, false);
        }
        if (s_live_round_val)
        {
            style_box(lv_obj_get_parent(s_live_round_val));
            ui_theme_apply_label(s_live_round_val, false);
        }
        if (s_live_spm_val)
        {
            style_box(lv_obj_get_parent(s_live_spm_val));
            ui_theme_apply_label(s_live_spm_val, false);
        }
        if (s_live_pace_val)
        {
            style_box(lv_obj_get_parent(s_live_pace_val));
            ui_theme_apply_label(s_live_pace_val, false);
        }
    }
}
void interval_data_page_on_orientation_changed(void)
{
    if (!s_root) return;
    apply_live_layout();
}

void interval_data_page_set_pace_s_per_500m(float pace_s)
{
    s_live_pace_s = pace_s;
}

void interval_data_page_set_spm(float spm)
{
    s_live_spm = spm;
}

static void start_prompt_show_async(void *p)
{
    (void)p;
    if (!s_root || !s_prompt_allowed)
        return;

    interval_config_t cfg = {0};
    interval_program_get_config(&cfg);

    if (!s_prompt_overlay)
    {
        lv_obj_t *top = lv_layer_top();
        s_prompt_overlay = lv_obj_create(top);
        lv_obj_set_size(s_prompt_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_opa(s_prompt_overlay, LV_OPA_70, 0);
        lv_obj_set_style_bg_color(s_prompt_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(s_prompt_overlay, 0, 0);
        lv_obj_set_flex_flow(s_prompt_overlay, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(s_prompt_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_prompt_panel = lv_obj_create(s_prompt_overlay);
        ui_theme_apply_surface(s_prompt_panel);
        lv_obj_set_width(s_prompt_panel, lv_pct(94));
        lv_obj_set_height(s_prompt_panel, lv_pct(85));
        lv_obj_set_style_pad_all(s_prompt_panel, 14, 0);
        lv_obj_set_style_pad_row(s_prompt_panel, 10, 0);
        lv_obj_set_flex_flow(s_prompt_panel, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *title = lv_label_create(s_prompt_panel);
        ui_theme_apply_label(title, false);
        lv_label_set_text(title, "Interval Ready");

        s_prompt_rows = lv_obj_create(s_prompt_panel);
        lv_obj_set_style_bg_opa(s_prompt_rows, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_prompt_rows, 0, 0);
        lv_obj_set_style_pad_all(s_prompt_rows, 0, 0);
        lv_obj_set_flex_flow(s_prompt_rows, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(s_prompt_rows, 6, 0);
        lv_obj_set_flex_grow(s_prompt_rows, 1);

        s_prompt_hint = lv_label_create(s_prompt_panel);
        ui_theme_apply_label(s_prompt_hint, true);
        lv_label_set_long_mode(s_prompt_hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(s_prompt_hint, LV_TEXT_ALIGN_CENTER, 0);
    }

    if (s_prompt_rows)
    {
        lv_obj_clean(s_prompt_rows);

        char work[24];
        char rest[24];
        fmt_target(work, sizeof(work), cfg.work.unit, cfg.work.value);
        fmt_target(rest, sizeof(rest), cfg.rest.unit, cfg.rest.value);

        lv_obj_t *r1 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r1, true);
        lv_label_set_text_fmt(r1, "Work: %s", work);

        lv_obj_t *r2 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r2, true);
        lv_label_set_text_fmt(r2, "Rest: %s", rest);

        lv_obj_t *r3 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r3, true);
        lv_label_set_text_fmt(r3, "Rounds: %u", cfg.rounds);
    }

    if (s_prompt_hint)
    {
        lv_label_set_text(s_prompt_hint, "Press PWR key to start");
    }

    if (s_prompt_overlay)
        lv_obj_clear_flag(s_prompt_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void start_prompt_hide_async(void *p)
{
    (void)p;
    if (s_prompt_overlay)
        lv_obj_add_flag(s_prompt_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void complete_prompt_show_async(void *p)
{
    (void)p;
    if (!s_root)
        return;

    if (!s_complete_overlay)
    {
        lv_obj_t *top = lv_layer_top();
        s_complete_overlay = lv_obj_create(top);
        lv_obj_set_size(s_complete_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_opa(s_complete_overlay, LV_OPA_40, 0);
        lv_obj_set_style_bg_color(s_complete_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(s_complete_overlay, 0, 0);
        lv_obj_clear_flag(s_complete_overlay, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_event_cb(s_complete_overlay, complete_overlay_event_cb, LV_EVENT_CLICKED, NULL);

        s_complete_panel = lv_obj_create(s_complete_overlay);
        lv_obj_set_size(s_complete_panel, 180, 70);
        lv_obj_set_style_bg_opa(s_complete_panel, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(s_complete_panel, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_border_width(s_complete_panel, 0, 0);
        lv_obj_set_style_radius(s_complete_panel, 6, 0);
        lv_obj_center(s_complete_panel);
        lv_obj_clear_flag(s_complete_panel, LV_OBJ_FLAG_SCROLLABLE);

        s_complete_lbl = lv_label_create(s_complete_panel);
        ui_theme_apply_label(s_complete_lbl, false);
        lv_label_set_text(s_complete_lbl, "Interval complete");
        lv_obj_center(s_complete_lbl);
    }

    s_complete_hide_at_ms = lv_tick_get() + 5000;
    if (s_complete_overlay)
        lv_obj_clear_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
}

void interval_data_page_show_start_prompt(void)
{
    s_prompt_allowed = true;
    lv_async_call(start_prompt_show_async, NULL);
}

void interval_data_page_hide_start_prompt(void)
{
    s_prompt_allowed = false;
    lv_async_call(start_prompt_hide_async, NULL);
}

void interval_data_page_show_complete_prompt(void)
{
    lv_async_call(complete_prompt_show_async, NULL);
}
