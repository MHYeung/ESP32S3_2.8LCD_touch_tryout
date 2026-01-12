#include "ui_status_bar.h"
#include "ui_theme.h"
#include "ui.h"
#include "race_program.h"
#include "ui_race_data_page.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_sb;
static lv_obj_t *s_scroll = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_row_target_distance = NULL;
static lv_obj_t *s_row_target_pace = NULL;
static lv_obj_t *s_row_split_length_m = NULL;
static lv_obj_t *s_btn_row = NULL;
static lv_obj_t *s_distance_val_lbl = NULL;
static lv_obj_t *s_pace_val_lbl = NULL;
static lv_obj_t *s_split_val_lbl = NULL;

static uint32_t s_target_distance_m = 2000;
static uint32_t s_target_pace_ms_per500 = 120000;
static uint32_t s_target_split_m = 500;

/* Dialog Handles */
static lv_obj_t *s_value_overlay = NULL;
static lv_obj_t *s_value_spinbox = NULL;

typedef enum
{
    RACE_FIELD_DISTANCE,
    RACE_FIELD_PACE,
    RACE_FIELD_SPLIT,
} race_field_t;

static race_field_t s_active_field = RACE_FIELD_DISTANCE;

// -----------------------
// Async relayout scheduler
// -----------------------
static bool s_relayout_scheduled = false;
static bool s_relayout_running = false;
static bool s_relayout_pending = false;

static void relayout_now(void);
static void relayout_async_cb(void *user_data);

static void relayout_request(void)
{
    // Coalesce multiple requests
    s_relayout_pending = true;

    if (s_relayout_scheduled)
        return;
    s_relayout_scheduled = true;

    lv_async_call(relayout_async_cb, NULL);
}

static void relayout_async_cb(void *user_data)
{
    (void)user_data;

    // allow future schedules
    s_relayout_scheduled = false;

    // page not ready (or was destroyed)
    if (!s_root)
    {
        s_relayout_pending = false;
        return;
    }

    // prevent re-entrancy
    if (s_relayout_running)
    {
        // if something requested while running, schedule another pass
        if (!s_relayout_scheduled)
        {
            s_relayout_scheduled = true;
            lv_async_call(relayout_async_cb, NULL);
        }
        return;
    }

    s_relayout_running = true;

    // drain pending requests
    while (s_relayout_pending)
    {
        s_relayout_pending = false;
        relayout_now();
    }

    s_relayout_running = false;
}

static void btn_inc_cb(lv_event_t *e) { lv_spinbox_increment(lv_event_get_user_data(e)); }
static void btn_dec_cb(lv_event_t *e) { lv_spinbox_decrement(lv_event_get_user_data(e)); }

static void fmt_distance(char *buf, size_t n, uint32_t m)
{
    if (m >= 1000)
        snprintf(buf, n, "%.1f km", m / 1000.0f);
    else
        snprintf(buf, n, "%u m", (unsigned)m);
}

static void fmt_pace(char *buf, size_t n, uint32_t pace_ms_per500)
{
    if (pace_ms_per500 == 0)
    {
        snprintf(buf, n, "--:--");
        return;
    }
    uint32_t total_s = (pace_ms_per500 + 500) / 1000;
    uint32_t mm = total_s / 60;
    uint32_t ss = total_s % 60;
    snprintf(buf, n, "%02u:%02u", (unsigned)mm, (unsigned)ss);
}

static void update_distance_label_text(void)
{
    if (!s_distance_val_lbl)
        return;
    char buf[32];
    fmt_distance(buf, sizeof(buf), s_target_distance_m);
    lv_label_set_text(s_distance_val_lbl, buf);
}

static void update_pace_label_text(void)
{
    if (!s_pace_val_lbl)
        return;
    char buf[32];
    fmt_pace(buf, sizeof(buf), s_target_pace_ms_per500);
    lv_label_set_text(s_pace_val_lbl, buf);
}

static void update_split_label_text(void)
{
    if (!s_split_val_lbl)
        return;
    char buf[32];
    fmt_distance(buf, sizeof(buf), s_target_split_m);
    lv_label_set_text(s_split_val_lbl, buf);
}

static lv_obj_t *create_clickable_row(lv_obj_t *parent, const char *label_txt, lv_event_cb_t click_cb,
                                      lv_obj_t **out_val_lbl)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    ui_theme_apply_surface(row);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_txt);
    ui_theme_apply_label(lbl, false);
    lv_obj_set_flex_grow(lbl, 1);

    lv_obj_t *val = lv_label_create(row);
    ui_theme_apply_label(val, true);
    lv_label_set_text(val, "");

    lv_obj_t *icon = lv_label_create(row);
    lv_label_set_text(icon, LV_SYMBOL_RIGHT);
    ui_theme_apply_label(icon, true);
    lv_obj_set_style_pad_left(icon, 5, 0);

    if (out_val_lbl)
        *out_val_lbl = val;
    return row;
}

static void apply_dialog_value(uint32_t value)
{
    if (s_active_field == RACE_FIELD_DISTANCE)
    {
        s_target_distance_m = value;
        update_distance_label_text();
    }
    else if (s_active_field == RACE_FIELD_PACE)
    {
        s_target_pace_ms_per500 = value * 1000;
        update_pace_label_text();
    }
    else if (s_active_field == RACE_FIELD_SPLIT)
    {
        s_target_split_m = value;
        update_split_label_text();
    }
}

static void configure_spinbox_for_field(lv_obj_t *sb, race_field_t field)
{
    if (field == RACE_FIELD_DISTANCE)
    {
        lv_spinbox_set_range(sb, 500, 50000);
        lv_spinbox_set_step(sb, 50);
        lv_spinbox_set_digit_format(sb, 5, 0);
        lv_spinbox_set_value(sb, (int32_t)s_target_distance_m);
    }
    else if (field == RACE_FIELD_PACE)
    {
        uint32_t pace_s = (s_target_pace_ms_per500 + 500) / 1000;
        lv_spinbox_set_range(sb, 60, 600);
        lv_spinbox_set_step(sb, 1);
        lv_spinbox_set_digit_format(sb, 3, 0);
        lv_spinbox_set_value(sb, (int32_t)pace_s);
    }
    else
    {
        lv_spinbox_set_range(sb, 100, 2000);
        lv_spinbox_set_step(sb, 50);
        lv_spinbox_set_digit_format(sb, 4, 0);
        lv_spinbox_set_value(sb, (int32_t)s_target_split_m);
    }
}

static void value_dialog_event_cb(lv_event_t *e)
{
    const char *action = (const char *)lv_event_get_user_data(e);

    if (strcmp(action, "save") == 0 && s_value_spinbox)
        apply_dialog_value((uint32_t)lv_spinbox_get_value(s_value_spinbox));

    if (s_value_overlay)
    {
        lv_obj_del(s_value_overlay);
        s_value_overlay = NULL;
        s_value_spinbox = NULL;
    }
}

static void create_value_dialog(race_field_t field)
{
    if (s_value_overlay)
        return;

    s_active_field = field;

    const char *title_txt = "Value";
    if (field == RACE_FIELD_DISTANCE)
        title_txt = "Target Distance (m)";
    else if (field == RACE_FIELD_PACE)
        title_txt = "Target Pace (sec/500m)";
    else if (field == RACE_FIELD_SPLIT)
        title_txt = "Split Length (m)";

    lv_obj_t *top = lv_layer_top();
    s_value_overlay = lv_obj_create(top);
    lv_obj_set_size(s_value_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_value_overlay, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(s_value_overlay, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_value_overlay, 0, 0);
    lv_obj_set_flex_flow(s_value_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_value_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *panel = lv_obj_create(s_value_overlay);
    ui_theme_apply_surface(panel);
    lv_obj_set_width(panel, 240);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_set_style_pad_row(panel, 15, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, title_txt);
    ui_theme_apply_label(title, false);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, lv_pct(100));

    lv_obj_t *line = lv_obj_create(panel);
    lv_obj_set_width(line, lv_pct(100));
    lv_obj_set_height(line, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(line, 8, 0);

    lv_obj_t *dec = lv_btn_create(line);
    lv_obj_t *d = lv_label_create(dec);
    lv_label_set_text(d, "-");
    lv_obj_center(d);

    s_value_spinbox = lv_spinbox_create(line);
    lv_obj_set_width(s_value_spinbox, 90);
    lv_obj_set_style_text_align(s_value_spinbox, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    configure_spinbox_for_field(s_value_spinbox, field);

    lv_obj_t *inc = lv_btn_create(line);
    lv_obj_t *i = lv_label_create(inc);
    lv_label_set_text(i, "+");
    lv_obj_center(i);

    lv_obj_add_event_cb(inc, btn_inc_cb, LV_EVENT_CLICKED, s_value_spinbox);
    lv_obj_add_event_cb(dec, btn_dec_cb, LV_EVENT_CLICKED, s_value_spinbox);

    lv_obj_t *btns = lv_obj_create(panel);
    lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_save = lv_btn_create(btns);
    lv_obj_set_width(btn_save, lv_pct(47));
    ui_theme_apply_button(btn_save);
    lv_obj_add_event_cb(btn_save, value_dialog_event_cb, LV_EVENT_CLICKED, (void *)"save");
    lv_obj_t *l1 = lv_label_create(btn_save);
    lv_label_set_text(l1, "OK");
    lv_obj_center(l1);

    lv_obj_t *btn_cancel = lv_btn_create(btns);
    lv_obj_set_width(btn_cancel, lv_pct(47));
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x6B7280), 0);
    lv_obj_add_event_cb(btn_cancel, value_dialog_event_cb, LV_EVENT_CLICKED, (void *)"cancel");
    lv_obj_t *l2 = lv_label_create(btn_cancel);
    lv_label_set_text(l2, "Cancel");
    lv_obj_center(l2);
}

static void confirm_cb(lv_event_t *e)
{
    (void)e;

    race_config_t cfg = {0};
    cfg.race_target_distance_m = s_target_distance_m;
    cfg.race_target_pace_ms_per500 = s_target_pace_ms_per500;
    cfg.race_target_split_length_m = s_target_split_m;

    race_program_set_config(&cfg);
    race_program_stop(); // IMPORTANT: do NOT start here

    // Go to interval data page (await user press Start there)
    ui_set_race_start_armed(true);
    ui_set_race_data_visible(true);
    ui_go_to_page(UI_RACE_DATA_PAGE, true);
    race_data_page_show_start_prompt();
}

static void cancel_cb(lv_event_t *e)
{
    (void)e;
    ui_set_race_start_armed(false);
    ui_set_race_data_visible(false);
    race_data_page_hide_start_prompt();
    ui_go_to_page(UI_PAGE_MENU, true);
}

static void relayout_now(void)
{
    if (!s_root)
        return;

    bool land = ui_is_landscape();
    lv_coord_t pad_lr = land ? 6 : 2;
    lv_coord_t row_pad = land ? 4 : 6;

    if (s_scroll)
    {
        lv_obj_set_style_pad_left(s_scroll, pad_lr, 0);
        lv_obj_set_style_pad_right(s_scroll, pad_lr, 0);
        lv_obj_set_style_pad_row(s_scroll, row_pad, 0);
    }

    if (s_btn_row)
        lv_obj_set_style_pad_row(s_btn_row, row_pad, 0);
}

static void distance_row_click_cb(lv_event_t *e)
{
    (void)e;
    create_value_dialog(RACE_FIELD_DISTANCE);
}

static void pace_row_click_cb(lv_event_t *e)
{
    (void)e;
    create_value_dialog(RACE_FIELD_PACE);
}

static void split_row_click_cb(lv_event_t *e)
{
    (void)e;
    create_value_dialog(RACE_FIELD_SPLIT);
}


//Pages Init
void race_setup_page_create(lv_obj_t *parent)
{
    race_config_t cfg = {0};
    race_program_get_config(&cfg);
    s_target_distance_m = cfg.race_target_distance_m;
    s_target_pace_ms_per500 = cfg.race_target_pace_ms_per500;
    s_target_split_m = cfg.race_target_split_length_m;

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
    lv_label_set_text(s_title, "Race Setup");
    ui_theme_apply_label(s_title, false);
    lv_obj_set_height(s_title, LV_SIZE_CONTENT);

    //Target Distance
    s_row_target_distance = create_clickable_row(s_scroll, "Target Distance", distance_row_click_cb,
                                                 &s_distance_val_lbl);
    update_distance_label_text();

    //Target Pace
    s_row_target_pace = create_clickable_row(s_scroll, "Target Pace", pace_row_click_cb, &s_pace_val_lbl);
    update_pace_label_text();

    //Split Length
    s_row_split_length_m = create_clickable_row(s_scroll, "Split Length", split_row_click_cb, &s_split_val_lbl);
    update_split_label_text();

    // Buttons row INSIDE scroll (so they scroll with the column)
    s_btn_row = lv_obj_create(s_scroll);
    lv_obj_set_width(s_btn_row, lv_pct(100));
    lv_obj_set_height(s_btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_btn_row, 0, 0);
    lv_obj_set_style_pad_all(s_btn_row, 0, 0);
    lv_obj_set_flex_flow(s_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *b_cancel = lv_btn_create(s_btn_row);
    lv_obj_add_event_cb(b_cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_width(b_cancel, lv_pct(48));
    lv_obj_t *lc = lv_label_create(b_cancel);
    lv_label_set_text(lc, "Cancel");
    lv_obj_center(lc);

    lv_obj_t *b_confirm = lv_btn_create(s_btn_row);
    lv_obj_add_event_cb(b_confirm, confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_width(b_confirm, lv_pct(48));
    lv_obj_t *lq = lv_label_create(b_confirm);
    lv_label_set_text(lq, "Confirm");
    lv_obj_center(lq);

    relayout_request();
}

void race_setup_page_apply_theme(void)
{
    if (!s_root)
        return;
    ui_status_bar_apply_theme(&s_sb);
    if (s_distance_val_lbl)
        ui_theme_apply_label(s_distance_val_lbl, true);
    if (s_pace_val_lbl)
        ui_theme_apply_label(s_pace_val_lbl, true);
    if (s_split_val_lbl)
        ui_theme_apply_label(s_split_val_lbl, true);
}
void race_setup_page_on_orientation_changed(void)
{
    if (!s_root)
        return;
    ui_status_bar_force_refresh(&s_sb);
    relayout_request();
}
