// main/ui/ui_activity_summary_page.c
#include "ui_activity_summary_page.h"
#include "ui_activity_detail_page.h"
#include "ui.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "ui_act_sum";

#ifndef ACTIVITY_DIR
#define ACTIVITY_DIR "/sdcard/activities"
#endif
#define ACTIVITY_INDEX_PATH ACTIVITY_DIR "/index.csv"

typedef struct
{
    uint32_t id;
    time_t start_ts;
    uint32_t duration_s;
    float distance_m;
    float avg_pace_s_per500;
    char file_path[160];
} activity_meta_t;

#define ACT_MAX 64
static activity_meta_t s_items[ACT_MAX];
static size_t s_item_count = 0;

static lv_obj_t *s_root = NULL;
static ui_status_bar_t s_status;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_empty_lbl = NULL;

static void fmt_time_hms(char *out, size_t n, uint32_t sec)
{
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    snprintf(out, n, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static void fmt_pace_500(char *out, size_t n, float pace_s_per500)
{
    if (pace_s_per500 <= 0.1f)
    {
        snprintf(out, n, "--:--/500");
        return;
    }
    uint32_t sec = (uint32_t)(pace_s_per500 + 0.5f);
    uint32_t mm = sec / 60;
    uint32_t ss = sec % 60;
    snprintf(out, n, "%lu:%02lu/500", (unsigned long)mm, (unsigned long)ss);
}

static void fmt_dist(char *out, size_t n, float m)
{
    if (m < 0.1f)
    {
        snprintf(out, n, "--");
        return;
    }
    if (m >= 1000.0f)
        snprintf(out, n, "%.2f km", (double)(m / 1000.0f));
    else
        snprintf(out, n, "%.0f m", (double)m);
}

static bool load_index_file(void)
{
    s_item_count = 0;

    FILE *f = fopen(ACTIVITY_INDEX_PATH, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "index not found: %s", ACTIVITY_INDEX_PATH);
        return false;
    }

    char line[256];

    // Optional header line: skip if it contains non-numeric at start
    long pos = ftell(f);
    if (fgets(line, sizeof(line), f))
    {
        if (!(line[0] >= '0' && line[0] <= '9'))
        {
            // header; keep going
        }
        else
        {
            fseek(f, pos, SEEK_SET); // not header
        }
    }
    else
    {
        fclose(f);
        return false;
    }

    while (fgets(line, sizeof(line), f) && s_item_count < ACT_MAX)
    {
        // id,start_ts,duration_s,distance_m,avg_pace_s_per500,file_path
        activity_meta_t *it = &s_items[s_item_count];
        memset(it, 0, sizeof(*it));

        // file_path may contain slashes; read it as the remainder after the 5th comma
        char path[160] = {0};
        unsigned long id = 0, start_ts = 0, dur = 0;
        double dist = 0, pace = 0;

        // parse first 5 fields
        int n = sscanf(line, "%lu,%lu,%lu,%lf,%lf,%159[^\n]",
                       &id, &start_ts, &dur, &dist, &pace, path);
        if (n < 6)
            continue;

        it->id = (uint32_t)id;
        it->start_ts = (time_t)start_ts;
        it->duration_s = (uint32_t)dur;
        it->distance_m = (float)dist;
        it->avg_pace_s_per500 = (float)pace;
        strncpy(it->file_path, path, sizeof(it->file_path) - 1);

        s_item_count++;
    }

    fclose(f);
    return (s_item_count > 0);
}

static void activity_card_cb(lv_event_t *e)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= s_item_count)
        return;

    activity_meta_t *it = &s_items[idx];

    // Pass the “log id” + file path into detail page
    activity_detail_page_open(it->id, it->file_path);

    // Navigate
    ui_go_to_page(UI_ACTIVITY_DETAIL_PAGE, true);
}
static void build_list_ui(void)
{
    if (!s_list)
        return;

    lv_obj_clean(s_list);
    if (s_empty_lbl)
    {
        lv_obj_del(s_empty_lbl);
        s_empty_lbl = NULL;
    }

    if (s_item_count == 0)
    {
        s_empty_lbl = lv_label_create(s_list);
        lv_label_set_text(s_empty_lbl, "No activities found.");
        ui_theme_apply_label(s_empty_lbl, true);
        lv_obj_center(s_empty_lbl);
        return;
    }

    for (size_t i = 0; i < s_item_count; i++)
    {
        activity_meta_t *it = &s_items[i];

        lv_obj_t *card = lv_btn_create(s_list);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 78);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_column(card, 12, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        ui_theme_apply_surface(card);

        lv_obj_add_event_cb(card, activity_card_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        // Layout inside card
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Left "thumbnail" column: Distance big
        lv_obj_t *left = lv_obj_create(card);
        lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(left, 0, 0);
        lv_obj_set_size(left, 92, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(left, 2, 0);

        char dist[24];
        fmt_dist(dist, sizeof(dist), it->distance_m);
        lv_obj_t *dist_lbl = lv_label_create(left);
        lv_label_set_text(dist_lbl, dist);
        ui_theme_apply_label(dist_lbl, false);

        // Right column: Pace + Time + Date
        lv_obj_t *right = lv_obj_create(card);
        lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right, 0, 0);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(right, 2, 0);

        char pace[24];
        fmt_pace_500(pace, sizeof(pace), it->avg_pace_s_per500);
        char dur[24];
        fmt_time_hms(dur, sizeof(dur), it->duration_s);

        lv_obj_t *p_lbl = lv_label_create(right);
        lv_label_set_text_fmt(p_lbl, "Pace %s", pace);
        ui_theme_apply_label(p_lbl, true);

        lv_obj_t *t_lbl = lv_label_create(right);
        lv_label_set_text_fmt(t_lbl, "Time %s", dur);
        ui_theme_apply_label(t_lbl, true);

        // Date line (optional)
        struct tm tmv;
        localtime_r(&it->start_ts, &tmv);
        char date[32];
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &tmv);

        lv_obj_t *d_lbl = lv_label_create(right);
        lv_label_set_text(d_lbl, date);
        ui_theme_apply_label(d_lbl, true);

        // Prevent any overflow (important on small width)
        lv_obj_set_width(p_lbl, lv_pct(100));
        lv_label_set_long_mode(p_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(t_lbl, lv_pct(100));
        lv_label_set_long_mode(t_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(d_lbl, lv_pct(100));
        lv_label_set_long_mode(d_lbl, LV_LABEL_LONG_DOT);
    }
}

void activity_summary_page_refresh(void)
{
    bool ok = load_index_file();
    if (!ok)
        s_item_count = 0;
    build_list_ui();
}

void activity_summary_page_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);

    // Status bar
    ui_status_bar_create(&s_status, s_root);

    // Body
    lv_obj_t *body = lv_obj_create(s_root);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, 10, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 8, 0);

    // Header row (Back + Title)
    lv_obj_t *hdr = lv_obj_create(body);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Activities");
    ui_theme_apply_label(title, false);

    // List container
    s_list = lv_obj_create(body);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 6, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);

    // Scrollable list
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);

    activity_summary_page_refresh();
}

void activity_summary_page_apply_theme(void)
{
    if (!s_root)
        return;
    ui_status_bar_apply_theme(&s_status);
    // Cards are themed at build time via ui_theme_apply_surface/label.
}

void activity_summary_page_on_orientation_changed(void)
{
    if (!s_root)
        return;
    ui_status_bar_force_refresh(&s_status);
}
