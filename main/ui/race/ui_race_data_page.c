#include "ui_race_data_page.h"
#include "ui_theme.h"
#include "race_program.h"
#include "ui.h"
#include <stdio.h>
#include <math.h>

static lv_obj_t *s_root = NULL;
static lv_timer_t *s_timer = NULL;

static lv_obj_t *s_live = NULL;
static lv_obj_t *s_live_rem_val = NULL;
static lv_obj_t *s_live_split_val = NULL;
static lv_obj_t *s_live_pace_val = NULL;
static lv_obj_t *s_live_rem_title = NULL;
static lv_obj_t *s_live_split_title = NULL;
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

static lv_obj_t *s_complete_overlay = NULL;
static lv_obj_t *s_complete_panel = NULL;
static lv_obj_t *s_complete_lbl = NULL;
static uint32_t s_complete_hide_at_ms = 0;

static void start_prompt_show_async(void *p);
static void start_prompt_hide_async(void *p);
static void complete_prompt_show_async(void *p);

static void complete_overlay_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_complete_overlay)
    {
        lv_obj_add_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
        s_complete_hide_at_ms = 0;
    }
}

static void fmt_distance(char *out, size_t n, uint32_t m)
{
    if (m >= 1000)
        snprintf(out, n, "%.1f km", m / 1000.0f);
    else
        snprintf(out, n, "%lum", (unsigned long)m);
}

static void fmt_pace(char *out, size_t out_len, uint32_t pace_ms_per500)
{
    if (pace_ms_per500 == 0)
    {
        snprintf(out, out_len, "--:--.-");
        return;
    }
    float sec = pace_ms_per500 / 1000.0f;
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

static void fmt_pace_live(float sec, char *out, size_t out_len)
{
    if (!isfinite(sec) || sec <= 0.0f)
    {
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
    lv_obj_set_style_border_color(box, lv_palette_main(LV_PALETTE_RED), 0);
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
    if (!s_live || !s_live_rem_val || !s_live_split_val || !s_live_pace_val || !s_live_spm_val)
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

        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_split_val),
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
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_split_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_spm_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 2, 1);
        lv_obj_set_grid_cell(lv_obj_get_parent(s_live_pace_val),
                             LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 3, 1);
    }

    const lv_font_t *bottom = land ? SLOT_FONT_VALUE_SMALL : SLOT_FONT_VALUE;
    lv_obj_set_style_text_font(s_live_split_val, bottom, 0);
    lv_obj_set_style_text_font(s_live_spm_val, bottom, 0);
    lv_obj_set_style_text_font(s_live_pace_val, bottom, 0);
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_root) return;

    race_ui_state_t st;
    race_program_get_ui(&st);

    if (!st.active && !st.done)
    {
        race_config_t cfg = {0};
        race_program_get_config(&cfg);

        if (s_live_rem_val)
            lv_label_set_text(s_live_rem_val, "READY");
        if (s_live_rem_title)
            lv_label_set_text(s_live_rem_title, "Remaining");
        if (s_live_split_val)
            lv_label_set_text(s_live_split_val, "--");
        if (s_live_split_title)
            lv_label_set_text(s_live_split_title, "Split");
        if (s_live_pace_val)
            lv_label_set_text(s_live_pace_val, "--:--.-");
        if (s_live_pace_title)
            lv_label_set_text(s_live_pace_title, "Pace (/500m)");
        if (s_live_spm_val)
            lv_label_set_text(s_live_spm_val, "--");
        if (s_live_spm_title)
            lv_label_set_text(s_live_spm_title, "SPM");
        return;
    }

    char rem[24];
    fmt_distance(rem, sizeof(rem), st.remaining_m);
    if (s_live_rem_val)
        lv_label_set_text(s_live_rem_val, rem);

    if (s_live_rem_title)
        lv_label_set_text(s_live_rem_title, st.done ? "Done" : "Remaining");

    if (s_live_split_val)
    {
        uint32_t total = 0;
        if (st.split_distance_m > 0)
            total = (st.target_distance_m + st.split_distance_m - 1) / st.split_distance_m;
        if (total > 0)
            lv_label_set_text_fmt(s_live_split_val, "%u/%u", (unsigned)st.split_idx, (unsigned)total);
        else
            lv_label_set_text_fmt(s_live_split_val, "%u", (unsigned)st.split_idx);
    }
    if (s_live_split_title)
        lv_label_set_text(s_live_split_title, "Split");

    if (s_live_pace_val)
    {
        char pace[16];
        fmt_pace_live(s_live_pace_s, pace, sizeof(pace));
        lv_label_set_text(s_live_pace_val, pace);
    }
    if (s_live_pace_title)
        lv_label_set_text(s_live_pace_title, "Pace (/500m)");

    if (s_live_spm_val)
    {
        if (!isfinite(s_live_spm))
        {
            lv_label_set_text(s_live_spm_val, "--");
        }
        else
        {
            char buf[12];
            float frac = fabsf(s_live_spm - floorf(s_live_spm));
            if (fabsf(frac - 0.5f) < 0.01f)
                snprintf(buf, sizeof(buf), "%.1f", (double)s_live_spm);
            else
                snprintf(buf, sizeof(buf), "%.0f", (double)s_live_spm);
            lv_label_set_text(s_live_spm_val, buf);
        }
    }
    if (s_live_spm_title)
        lv_label_set_text(s_live_spm_title, "SPM");

    if (s_complete_overlay && s_complete_hide_at_ms > 0)
    {
        if ((int32_t)(lv_tick_get() - s_complete_hide_at_ms) >= 0)
        {
            lv_obj_add_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
            s_complete_hide_at_ms = 0;
        }
    }
}

void race_data_page_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_live = s_root;

    create_slot(s_live, "Remaining", &s_live_rem_title, &s_live_rem_val);
    create_slot(s_live, "Split", &s_live_split_title, &s_live_split_val);
    create_slot(s_live, "SPM", &s_live_spm_title, &s_live_spm_val);
    create_slot(s_live, "Pace (/500m)", &s_live_pace_title, &s_live_pace_val);

    apply_live_layout();

    if (!s_timer)
        s_timer = lv_timer_create(tick_cb, 200, NULL);
    tick_cb(NULL);
}

void race_data_page_apply_theme(void)
{
    if (!s_root) return;

    if (s_live)
    {
        if (s_live_rem_title)
            ui_theme_apply_label(s_live_rem_title, true);
        if (s_live_split_title)
            ui_theme_apply_label(s_live_split_title, true);
        if (s_live_spm_title)
            ui_theme_apply_label(s_live_spm_title, true);
        if (s_live_pace_title)
            ui_theme_apply_label(s_live_pace_title, true);
        if (s_live_rem_val)
        {
            style_box(lv_obj_get_parent(s_live_rem_val));
            ui_theme_apply_label(s_live_rem_val, false);
        }
        if (s_live_split_val)
        {
            style_box(lv_obj_get_parent(s_live_split_val));
            ui_theme_apply_label(s_live_split_val, false);
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

void race_data_page_on_orientation_changed(void)
{
    if (!s_root) return;
    apply_live_layout();
}

void race_data_page_set_pace_s_per_500m(float pace_s)
{
    s_live_pace_s = pace_s;
}

void race_data_page_set_spm(float spm)
{
    s_live_spm = spm;
}

static void start_prompt_show_async(void *p)
{
    (void)p;
    if (!s_root || !s_prompt_allowed)
        return;

    race_config_t cfg = {0};
    race_program_get_config(&cfg);

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
        lv_label_set_text(title, "Race Ready");

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

        char dist[24];
        char pace[24];
        char split[24];
        fmt_distance(dist, sizeof(dist), cfg.race_target_distance_m);
        fmt_pace(pace, sizeof(pace), cfg.race_target_pace_ms_per500);
        fmt_distance(split, sizeof(split), cfg.race_target_split_length_m);

        lv_obj_t *r1 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r1, true);
        lv_label_set_text_fmt(r1, "Distance: %s", dist);

        lv_obj_t *r2 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r2, true);
        lv_label_set_text_fmt(r2, "Pace: %s", pace);

        lv_obj_t *r3 = lv_label_create(s_prompt_rows);
        ui_theme_apply_label(r3, true);
        lv_label_set_text_fmt(r3, "Split: %s", split);
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
        lv_obj_set_style_bg_color(s_complete_panel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_border_width(s_complete_panel, 0, 0);
        lv_obj_set_style_radius(s_complete_panel, 6, 0);
        lv_obj_center(s_complete_panel);
        lv_obj_clear_flag(s_complete_panel, LV_OBJ_FLAG_SCROLLABLE);

        s_complete_lbl = lv_label_create(s_complete_panel);
        ui_theme_apply_label(s_complete_lbl, false);
        lv_label_set_text(s_complete_lbl, "Race complete");
        lv_obj_center(s_complete_lbl);
    }

    s_complete_hide_at_ms = lv_tick_get() + 5000;
    if (s_complete_overlay)
        lv_obj_clear_flag(s_complete_overlay, LV_OBJ_FLAG_HIDDEN);
}

void race_data_page_show_start_prompt(void)
{
    s_prompt_allowed = true;
    lv_async_call(start_prompt_show_async, NULL);
}

void race_data_page_hide_start_prompt(void)
{
    s_prompt_allowed = false;
    lv_async_call(start_prompt_hide_async, NULL);
}

void race_data_page_show_complete_prompt(void)
{
    lv_async_call(complete_prompt_show_async, NULL);
}
