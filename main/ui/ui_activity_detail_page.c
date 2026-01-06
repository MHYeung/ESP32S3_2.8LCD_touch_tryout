#include "ui_activity_detail_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "activity_store.h"
#include "nvs_helper.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_act_detail";

#define SD_MOUNT "/sdcard"
#define PAGE_ROWS 10

static uint32_t s_act_id = 0;
static char s_base_full[192] = {0}; // full base path WITHOUT suffix, e.g. "/sdcard/activities/20260104_2235_01"
static size_t s_page_index = 0;
static size_t s_total_rows = 0;

// UI handles
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_name_lbl = NULL; // activity name under status bar
static lv_obj_t *s_hdr_row = NULL;  // frozen header row container
static lv_obj_t *s_scroller = NULL; // content container holding table + nav
static lv_obj_t *s_tbl = NULL;      // split rows only (no header row inside)
static lv_obj_t *s_hint = NULL;     // optional hint when no splits
static lv_obj_t *s_nav_row = NULL;
static lv_obj_t *s_btn_prev = NULL;
static lv_obj_t *s_btn_next = NULL;
static lv_obj_t *s_page_lbl = NULL;
static ui_status_bar_t s_status;
static lv_obj_t *s_hdr_lbl[4] = {0};

#define SPLIT_COLS 4
static const lv_coord_t s_col_w[SPLIT_COLS] = {24, 60, 74, 74}; // tweak to taste
static const char *s_col_hdr[SPLIT_COLS] = {"#", "Dist", "Time", "Pace"};

static void format_activity_title(char *out, size_t n)
{
    // default: show base filename (after last '/')
    const char *base = s_base_full;
    const char *p = strrchr(s_base_full, '/');
    if (p)
        base = p + 1;

    // try parse: YYYYMMDD_HHMM_XX  ->  YYYY-MM-DD HH:MM
    if (strlen(base) >= 13)
    {
        bool ok = true;
        for (int i = 0; i < 8; i++)
            if (base[i] < '0' || base[i] > '9')
                ok = false;
        if (base[8] != '_')
            ok = false;
        for (int i = 9; i < 13; i++)
            if (base[i] < '0' || base[i] > '9')
                ok = false;

        if (ok)
        {
            snprintf(out, n, "%.4s-%.2s-%.2s %.2s:%.2s",
                     base, base + 4, base + 6, base + 9, base + 11);
            return;
        }
    }

    snprintf(out, n, "%s", base);
}

static void trim_tail(char *s)
{
    size_t L = strlen(s);
    while (L && (s[L - 1] == '\r' || s[L - 1] == '\n' || s[L - 1] == ' '))
        s[--L] = 0;
}

static void normalize_base_full(const char *csv_path)
{
    s_base_full[0] = 0;
    if (!csv_path || !csv_path[0])
        return;

    // build full path first
    if (strncmp(csv_path, SD_MOUNT "/", strlen(SD_MOUNT "/")) == 0)
    {
        strncpy(s_base_full, csv_path, sizeof(s_base_full) - 1);
    }
    else
    {
        snprintf(s_base_full, sizeof(s_base_full), SD_MOUNT "/%s", csv_path);
    }
    trim_tail(s_base_full);

    // strip known suffixes if user passed strokes/splits csv
    const char *suffixes[] = {"_Strokes.csv", "_Splits.csv", ".csv"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
    {
        size_t bl = strlen(s_base_full);
        size_t sl = strlen(suffixes[i]);
        if (bl > sl && strcmp(s_base_full + (bl - sl), suffixes[i]) == 0)
        {
            s_base_full[bl - sl] = 0;
        }
    }
}

static void show_hint(const char *msg)
{
    if (s_hint && lv_obj_is_valid(s_hint))
    {
        lv_label_set_text(s_hint, msg);
        lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void hide_hint(void)
{
    if (s_hint && lv_obj_is_valid(s_hint))
    {
        lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_btn_enabled(lv_obj_t *btn, bool enabled)
{
    if (!btn)
        return;
    if (enabled)
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
    else
        lv_obj_add_state(btn, LV_STATE_DISABLED);
}

static void update_nav_controls(void)
{
    size_t total_pages = (s_total_rows + PAGE_ROWS - 1) / PAGE_ROWS;
    if (total_pages == 0)
        total_pages = 1;

    if (s_page_index >= total_pages)
        s_page_index = total_pages - 1;

    if (s_page_lbl && lv_obj_is_valid(s_page_lbl))
    {
        char buf[24];
        if (s_total_rows == 0)
            snprintf(buf, sizeof(buf), "0/0");
        else
            snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(s_page_index + 1), (unsigned)total_pages);
        lv_label_set_text(s_page_lbl, buf);
    }

    set_btn_enabled(s_btn_prev, s_total_rows > 0 && s_page_index > 0);
    set_btn_enabled(s_btn_next, s_total_rows > 0 && (s_page_index + 1) < total_pages);
}

static void format_split_time(char *out, size_t out_len, const char *in)
{
    if (!out || out_len == 0)
        return;
    out[0] = 0;
    if (!in || !in[0])
    {
        snprintf(out, out_len, "--:--.--");
        return;
    }

    int hh = 0, mm = 0, ss = 0, ms = 0;
    int n = sscanf(in, "%d:%d:%d.%d", &hh, &mm, &ss, &ms);
    if (n < 3)
    {
        snprintf(out, out_len, "%s", in);
        return;
    }
    if (n < 4)
        ms = 0;

    int total_min = hh * 60 + mm;
    int hundredths = ms / 10;
    snprintf(out, out_len, "%d:%02d.%02d", total_min, ss, hundredths);
}

static void refresh_detail(void)
{
    if (!s_tbl || !s_scroller)
        return;
    if (s_base_full[0] == 0)
        return;

    activity_store_split_t rows[PAGE_ROWS];
    memset(rows, 0, sizeof(rows));
    size_t count = 0;
    size_t total = 0;
    size_t start_index = s_page_index * PAGE_ROWS;

    esp_err_t r = activity_store_load_splits_page(s_base_full,
                                                  start_index,
                                                  rows,
                                                  PAGE_ROWS,
                                                  &count,
                                                  &total,
                                                  NULL);

    if (total > 0)
    {
        size_t total_pages = (total + PAGE_ROWS - 1) / PAGE_ROWS;
        if (start_index >= total)
        {
            s_page_index = total_pages - 1;
            start_index = s_page_index * PAGE_ROWS;
            r = activity_store_load_splits_page(s_base_full,
                                                start_index,
                                                rows,
                                                PAGE_ROWS,
                                                &count,
                                                &total,
                                                NULL);
        }
    }

    s_total_rows = total;

    if (r != ESP_OK || total == 0)
    {
        uint32_t s_current_m = nvs_helper_get_split_len();
        lv_table_set_row_cnt(s_tbl, 0);
        char hint_msg[64];
        snprintf(hint_msg, sizeof(hint_msg), "No splits yet.\nRow past %um to create splits.", (unsigned int)s_current_m);
        show_hint(hint_msg);
        update_nav_controls();
        return;
    }
    hide_hint();

    lv_table_set_row_cnt(s_tbl, (uint16_t)count);

    for (size_t i = 0; i < count; i++)
    {
        char c0[8], c1[16], c2[16];
        snprintf(c0, sizeof(c0), "%lu", (unsigned long)rows[i].split_index);
        activity_store_format_dist(c1, sizeof(c1), rows[i].split_dist_m);
        format_split_time(c2, sizeof(c2), rows[i].split_time_str);

        lv_table_set_cell_value(s_tbl, (uint16_t)i, 0, c0);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 1, c1);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 2, c2);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 3, rows[i].pace_str);
    }

    lv_obj_set_style_border_side(s_tbl, LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_tbl, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_opa(s_tbl, LV_OPA_60, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_tbl, lv_palette_main(LV_PALETTE_GREY), LV_PART_ITEMS);
    lv_obj_set_style_radius(s_tbl, 0, LV_PART_ITEMS);

    lv_obj_set_height(s_tbl, LV_SIZE_CONTENT);
    lv_obj_update_layout(s_tbl);

    update_nav_controls();
}

static void refresh_async(void *p)
{
    (void)p;
    refresh_detail();
}

static void nav_btn_event_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    if (!id)
        return;

    if (strcmp(id, "prev") == 0)
    {
        if (s_page_index > 0)
            s_page_index--;
    }
    else if (strcmp(id, "next") == 0)
    {
        s_page_index++;
    }
    else
    {
        return;
    }

    lv_async_call(refresh_async, NULL);
}

static void relayout(void)
{
    if (!s_root)
        return;

    bool land = ui_is_landscape();
    lv_coord_t w = lv_obj_get_width(s_root);

    // tighter padding in landscape
    lv_coord_t pad_lr = land ? 6 : 1;
    lv_coord_t name_h = land ? 20 : 26;
    lv_coord_t hdr_h = land ? 20 : 24;
    lv_coord_t nav_h = land ? 22 : 26;

    // Name bar sizing
    if (s_name_lbl)
    {
        lv_obj_set_height(s_name_lbl, name_h);
        lv_obj_set_style_pad_left(s_name_lbl, pad_lr, 0);
        lv_obj_set_style_pad_right(s_name_lbl, pad_lr, 0);
        lv_obj_set_style_pad_top(s_name_lbl, 0, 0);
        lv_obj_set_style_pad_bottom(s_name_lbl, 0, 0);
    }

    // Header row sizing
    if (s_hdr_row)
    {
        lv_obj_set_height(s_hdr_row, hdr_h);
        lv_obj_set_style_pad_left(s_hdr_row, pad_lr, 0);
        lv_obj_set_style_pad_right(s_hdr_row, pad_lr, 0);
        lv_obj_set_style_pad_top(s_hdr_row, 1, 0);
        lv_obj_set_style_pad_bottom(s_hdr_row, 1, 0);
    }

    if (s_hint)
    {
        lv_obj_set_width(s_hint, land ? lv_pct(70) : lv_pct(90));
        lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_hint);
    }

    if (s_nav_row)
    {
        lv_obj_set_height(s_nav_row, nav_h);
        lv_obj_set_style_pad_left(s_nav_row, pad_lr, 0);
        lv_obj_set_style_pad_right(s_nav_row, pad_lr, 0);
    }

    if (s_btn_prev)
        lv_obj_set_size(s_btn_prev, land ? 44 : 52, nav_h);
    if (s_btn_next)
        lv_obj_set_size(s_btn_next, land ? 44 : 52, nav_h);

    // Column widths depend on screen width/orientation
    lv_coord_t usable = w - (pad_lr * 2);

    lv_coord_t c1 = land ? 75 : 50;        // Dist
    lv_coord_t c2 = land ? 96 : 76;        // Time
    lv_coord_t c3 = land ? 96 : 76;        // Pace
    lv_coord_t c0 = usable - c3 - c1 - c2; // #

    // Guard minimum for last column
    lv_coord_t c3_min = land ? 90 : 70;
    if (c3 < c3_min)
    {
        lv_coord_t need = c3_min - c3;
        // steal from c2 first, then c1
        lv_coord_t steal2 = (c2 > (land ? 80 : 60)) ? (need > 20 ? 20 : need) : 0;
        c2 -= steal2;
        need -= steal2;
        lv_coord_t steal1 = (need > 0 && c1 > (land ? 60 : 44)) ? need : 0;
        c1 -= steal1;
        need -= steal1;
        c3 = usable - c0 - c1 - c2;
    }

    // Apply header label widths
    if (s_hdr_lbl[0])
        lv_obj_set_width(s_hdr_lbl[0], c0);
    if (s_hdr_lbl[1])
        lv_obj_set_width(s_hdr_lbl[1], c1);
    if (s_hdr_lbl[2])
        lv_obj_set_width(s_hdr_lbl[2], c2);
    if (s_hdr_lbl[3])
        lv_obj_set_width(s_hdr_lbl[3], c3);

    // Apply table column widths
    if (s_tbl)
    {
        lv_table_set_col_cnt(s_tbl, 4);
        lv_table_set_col_width(s_tbl, 0, c0);
        lv_table_set_col_width(s_tbl, 1, c1);
        lv_table_set_col_width(s_tbl, 2, c2);
        lv_table_set_col_width(s_tbl, 3, c3);
    }

    // Ensure hint is centered after rotation
    if (s_hint)
        lv_obj_center(s_hint);
}

void activity_detail_page_open(uint32_t activity_id, const char *csv_path)
{
    s_act_id = activity_id;
    normalize_base_full(csv_path);
    s_page_index = 0;
    s_total_rows = 0;

    if (s_name_lbl && lv_obj_is_valid(s_name_lbl))
    {
        char title[48];
        format_activity_title(title, sizeof(title));
        lv_label_set_text(s_name_lbl, title);
    }

    ESP_LOGI(TAG, "Open detail: id=%lu base=%s", (unsigned long)s_act_id, s_base_full);

    // update UI safely
    lv_async_call(refresh_async, NULL);
}

void activity_detail_page_create(lv_obj_t *parent)
{
    // page background (must cover what's behind)
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    ui_theme_apply_screen(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Root column layout: status bar -> name label -> frozen header -> scroller
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);

    ui_status_bar_create(&s_status, s_root);

    // Activity name bar (just a label under status bar)
    lv_obj_t *name_bar = lv_obj_create(s_root);
    lv_obj_set_width(name_bar, lv_pct(100));
    lv_obj_set_height(name_bar, 26);
    lv_obj_set_style_bg_opa(name_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_bar, 0, 0);
    lv_obj_set_style_pad_left(name_bar, 10, 0);
    lv_obj_set_style_pad_right(name_bar, 10, 0);
    lv_obj_set_style_pad_top(name_bar, 2, 0);
    lv_obj_set_style_pad_bottom(name_bar, 0, 0);
    lv_obj_clear_flag(name_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_name_lbl = lv_label_create(name_bar);
    ui_theme_apply_label(s_name_lbl, false);
    lv_obj_set_width(s_name_lbl, lv_pct(100));
    lv_label_set_long_mode(s_name_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_name_lbl, "Activity");

    // Page navigation row
    s_nav_row = lv_obj_create(s_root);
    lv_obj_set_width(s_nav_row, lv_pct(100));
    lv_obj_set_height(s_nav_row, 26);
    lv_obj_set_style_bg_opa(s_nav_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_nav_row, 0, 0);
    lv_obj_set_style_pad_all(s_nav_row, 0, 0);
    lv_obj_set_flex_flow(s_nav_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nav_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_btn_prev = lv_btn_create(s_nav_row);
    ui_theme_apply_button(s_btn_prev);
    lv_obj_add_event_cb(s_btn_prev, nav_btn_event_cb, LV_EVENT_CLICKED, (void *)"prev");
    lv_obj_t *prev_lbl = lv_label_create(s_btn_prev);
    ui_theme_apply_label(prev_lbl, true);
    lv_label_set_text(prev_lbl, LV_SYMBOL_LEFT);

    s_page_lbl = lv_label_create(s_nav_row);
    ui_theme_apply_label(s_page_lbl, true);
    lv_label_set_text(s_page_lbl, "0/0");

    s_btn_next = lv_btn_create(s_nav_row);
    ui_theme_apply_button(s_btn_next);
    lv_obj_add_event_cb(s_btn_next, nav_btn_event_cb, LV_EVENT_CLICKED, (void *)"next");
    lv_obj_t *next_lbl = lv_label_create(s_btn_next);
    ui_theme_apply_label(next_lbl, true);
    lv_label_set_text(next_lbl, LV_SYMBOL_RIGHT);

    // Frozen header row (NOT scrollable)
    s_hdr_row = lv_obj_create(s_root);
    lv_obj_set_width(s_hdr_row, lv_pct(100));
    lv_obj_set_height(s_hdr_row, 24);
    lv_obj_set_style_border_width(s_hdr_row, 0, 0);
    lv_obj_set_style_pad_left(s_hdr_row, 4, 0);
    lv_obj_set_style_pad_right(s_hdr_row, 4, 0);
    lv_obj_set_style_pad_top(s_hdr_row, 2, 0);
    lv_obj_set_style_pad_bottom(s_hdr_row, 1, 0);
    ui_theme_apply_surface(s_hdr_row); // looks like a header strip
    lv_obj_clear_flag(s_hdr_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_hdr_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int c = 0; c < SPLIT_COLS; c++)
    {
        s_hdr_lbl[c] = lv_label_create(s_hdr_row);
        ui_theme_apply_label(s_hdr_lbl[c], true);
        lv_label_set_text(s_hdr_lbl[c], s_col_hdr[c]);
        lv_obj_set_width(s_hdr_lbl[c], s_col_w[c]);
        lv_obj_set_flex_grow(s_hdr_lbl[c], 0);
        lv_obj_set_style_text_align(s_hdr_lbl[c], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_hdr_lbl[c], LV_LABEL_LONG_CLIP);
    }

    // Content area (paged, non-scrollable)
    s_scroller = lv_obj_create(s_root);
    lv_obj_set_width(s_scroller, lv_pct(100));
    lv_obj_set_flex_grow(s_scroller, 1);
    lv_obj_set_style_bg_opa(s_scroller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_scroller, 0, 0);
    lv_obj_set_style_pad_all(s_scroller, 0, 0);
    lv_obj_set_flex_flow(s_scroller, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_scroller, 4, 0);
    lv_obj_add_flag(s_scroller, LV_OBJ_FLAG_SCROLLABLE);

    // Table inside content area
    s_tbl = lv_table_create(s_scroller);
    lv_obj_set_width(s_tbl, lv_pct(100));
    lv_obj_set_height(s_tbl, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_tbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_tbl, LV_SCROLLBAR_MODE_OFF);

    lv_table_set_col_cnt(s_tbl, SPLIT_COLS);
    for (int c = 0; c < SPLIT_COLS; c++)
    {
        lv_table_set_col_width(s_tbl, c, s_col_w[c]);
    }

    lv_obj_set_style_pad_all(s_tbl, 2, LV_PART_ITEMS);
    lv_obj_set_style_text_align(s_tbl, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);

    // Hint label (shown when no splits)
    s_hint = lv_label_create(s_scroller);
    ui_theme_apply_label(s_hint, true);
    lv_label_set_text(s_hint, "");
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(s_hint);

    // start empty
    lv_table_set_row_cnt(s_tbl, 0);

    relayout();
    update_nav_controls();
}

void activity_detail_page_apply_theme(void)
{
    if (!s_root)
        return;
    ui_status_bar_apply_theme(&s_status);
    // s_hdr_row uses ui_theme_apply_surface at creation; if theme changes at runtime
    // and you want it to re-apply, call ui_theme_apply_surface(s_hdr_row) here.
    if (s_hdr_row)
        ui_theme_apply_surface(s_hdr_row);
    if (s_btn_prev)
        ui_theme_apply_button(s_btn_prev);
    if (s_btn_next)
        ui_theme_apply_button(s_btn_next);
    if (s_page_lbl)
        ui_theme_apply_label(s_page_lbl, true);
}

void activity_detail_page_on_orientation_changed(void)
{
    if (!s_root)
        return;
    ui_status_bar_force_refresh(&s_status);
    relayout();
}
