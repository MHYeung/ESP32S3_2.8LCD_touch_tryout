#include "ui_activity_detail_page.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ui_act_detail";

#define SD_MOUNT "/sdcard"
#define MAX_SPLITS 64

typedef struct
{
    int idx;
    float total_m;
    float split_m;
    char split_time[16];
    char pace[16];
    float spm;
} split_row_t;

static uint32_t s_act_id = 0;
static char s_base_full[192] = {0}; // full base path WITHOUT suffix, e.g. "/sdcard/activities/20260104_2235_01"

// UI handles
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_name_lbl = NULL; // activity name under status bar
static lv_obj_t *s_hdr_row = NULL;  // frozen header row container
static lv_obj_t *s_scroller = NULL; // scrollable container holding table
static lv_obj_t *s_tbl = NULL;      // split rows only (no header row inside)
static lv_obj_t *s_hint = NULL;     // optional hint when no splits
static ui_status_bar_t s_status;

#define SPLIT_COLS 4
static const lv_coord_t s_col_w[SPLIT_COLS] = {30, 56, 72, 72}; // tweak to taste
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

static void build_paths(char *strokes, size_t ns, char *splits, size_t nsp)
{
    snprintf(strokes, ns, "%s_Strokes.csv", s_base_full);
    snprintf(splits, nsp, "%s_Splits.csv", s_base_full);
}

static bool read_last_stroke_summary(float *out_dist_m, char *out_avg_pace, size_t pace_n)
{
    *out_dist_m = 0;
    snprintf(out_avg_pace, pace_n, "--:--.-");

    char strokes[220], splits[220];
    build_paths(strokes, sizeof(strokes), splits, sizeof(splits));

    FILE *f = fopen(strokes, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Cannot open strokes: %s", strokes);
        return false;
    }

    char line[320];
    char last[320] = {0};

    // header
    if (!fgets(line, sizeof(line), f))
    {
        fclose(f);
        return false;
    }

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] != '\r' && line[0] != '\n')
        {
            strncpy(last, line, sizeof(last) - 1);
        }
    }
    fclose(f);

    if (last[0] == 0)
        return false;

    // columns:
    // 0 Global Time
    // 1 Session Time
    // 2 Distance (m)
    // 5 Avg Pace (/500m)
    char tmp[320];
    strncpy(tmp, last, sizeof(tmp) - 1);

    int col = 0;
    char *tok = strtok(tmp, ",");
    char *dist_tok = NULL;
    char *avgpace_tok = NULL;

    while (tok)
    {
        if (col == 2)
            dist_tok = tok;
        if (col == 5)
            avgpace_tok = tok;
        tok = strtok(NULL, ",");
        col++;
    }
    if (!dist_tok || !avgpace_tok)
        return false;

    *out_dist_m = (float)atof(dist_tok);
    snprintf(out_avg_pace, pace_n, "%s", avgpace_tok);
    trim_tail(out_avg_pace);
    return true;
}

static size_t read_splits(split_row_t *rows, size_t cap)
{
    char strokes[220], splits[220];
    build_paths(strokes, sizeof(strokes), splits, sizeof(splits));

    FILE *f = fopen(splits, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Cannot open splits: %s", splits);
        return 0;
    }

    char line[320];
    bool header_found = false;
    size_t n = 0;

    while (fgets(line, sizeof(line), f))
    {
        trim_tail(line);
        if (!header_found)
        {
            if (strncmp(line, "Split #", 6) == 0)
                header_found = true;
            continue;
        }

        if (line[0] == 0)
            continue;
        if (n >= cap)
            break;

        // Split #,Total Dist (m),Split Dist (m),Split Time,Avg Pace (/500m),Avg SPM
        char tmp[320];
        strncpy(tmp, line, sizeof(tmp) - 1);

        char *t0 = strtok(tmp, ",");
        char *t1 = strtok(NULL, ",");
        char *t2 = strtok(NULL, ",");
        char *t3 = strtok(NULL, ",");
        char *t4 = strtok(NULL, ",");
        char *t5 = strtok(NULL, ",");

        if (!t0 || !t1 || !t2 || !t3 || !t4)
            continue;

        rows[n].idx = atoi(t0);
        rows[n].total_m = (float)atof(t1);
        rows[n].split_m = (float)atof(t2);
        strncpy(rows[n].split_time, t3, sizeof(rows[n].split_time) - 1);
        strncpy(rows[n].pace, t4, sizeof(rows[n].pace) - 1);
        rows[n].spm = t5 ? (float)atof(t5) : 0.0f;

        trim_tail(rows[n].split_time);
        trim_tail(rows[n].pace);
        n++;
    }

    fclose(f);
    return n;
}

static void show_hint(const char *msg)
{
    if (s_hint && lv_obj_is_valid(s_hint))
    {
        lv_label_set_text(s_hint, msg);
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

static void refresh_detail(void)
{
    if (!s_tbl || !s_scroller)
        return;
    if (s_base_full[0] == 0)
        return;

    split_row_t rows[MAX_SPLITS];
    memset(rows, 0, sizeof(rows));
    size_t n = read_splits(rows, MAX_SPLITS);

    if (n == 0)
    {
        lv_table_set_row_cnt(s_tbl, 0);
        show_hint("No splits yet.\nRow past 500m to create splits.");
        return;
    }
    hide_hint();

    lv_table_set_row_cnt(s_tbl, (uint16_t)n);

    for (size_t i = 0; i < n; i++)
    {
        char c0[8], c1[16];
        snprintf(c0, sizeof(c0), "%d", rows[i].idx);
        snprintf(c1, sizeof(c1), "%.0fm", (double)rows[i].split_m);

        lv_table_set_cell_value(s_tbl, (uint16_t)i, 0, c0);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 1, c1);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 2, rows[i].split_time);
        lv_table_set_cell_value(s_tbl, (uint16_t)i, 3, rows[i].pace);
    }

    lv_obj_set_height(s_tbl, LV_SIZE_CONTENT);
    lv_obj_update_layout(s_tbl);
}

static void refresh_async(void *p)
{
    (void)p;
    refresh_detail();
}

void activity_detail_page_open(uint32_t activity_id, const char *csv_path)
{
    s_act_id = activity_id;
    normalize_base_full(csv_path);

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
    lv_obj_set_style_pad_top(name_bar, 4, 0);
    lv_obj_set_style_pad_bottom(name_bar, 2, 0);
    lv_obj_clear_flag(name_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_name_lbl = lv_label_create(name_bar);
    ui_theme_apply_label(s_name_lbl, false);
    lv_obj_set_width(s_name_lbl, lv_pct(100));
    lv_label_set_long_mode(s_name_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_name_lbl, "Activity");

    // Frozen header row (NOT scrollable)
    s_hdr_row = lv_obj_create(s_root);
    lv_obj_set_width(s_hdr_row, lv_pct(100));
    lv_obj_set_height(s_hdr_row, 24);
    lv_obj_set_style_border_width(s_hdr_row, 0, 0);
    lv_obj_set_style_pad_left(s_hdr_row, 6, 0);
    lv_obj_set_style_pad_right(s_hdr_row, 6, 0);
    lv_obj_set_style_pad_top(s_hdr_row, 2, 0);
    lv_obj_set_style_pad_bottom(s_hdr_row, 2, 0);
    ui_theme_apply_surface(s_hdr_row); // looks like a header strip
    lv_obj_clear_flag(s_hdr_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_hdr_row, LV_FLEX_FLOW_ROW);

    for (int c = 0; c < SPLIT_COLS; c++)
    {
        lv_obj_t *lbl = lv_label_create(s_hdr_row);
        ui_theme_apply_label(lbl, true);
        lv_label_set_text(lbl, s_col_hdr[c]);
        lv_obj_set_width(lbl, s_col_w[c]);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    }

    // Scrollable area (this scrolls; header does not)
    s_scroller = lv_obj_create(s_root);
    lv_obj_set_width(s_scroller, lv_pct(100));
    lv_obj_set_flex_grow(s_scroller, 1);
    lv_obj_set_style_bg_opa(s_scroller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_scroller, 0, 0);
    lv_obj_set_style_pad_all(s_scroller, 0, 0);
    lv_obj_set_scroll_dir(s_scroller, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scroller, LV_SCROLLBAR_MODE_AUTO);

    // Table inside scroller (table itself NOT scrollable; scroller is)
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
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(s_hint);

    // start empty
    lv_table_set_row_cnt(s_tbl, 0);
}
