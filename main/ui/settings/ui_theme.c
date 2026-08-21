#include "ui_theme.h"

static lv_disp_t *s_disp = NULL;
static ui_theme_t s_theme = UI_THEME_LIGHT;
static ui_theme_palette_t s_palette;

static bool s_styles_inited = false;
static lv_style_t s_style_screen;
static lv_style_t s_style_surface;
static lv_style_t s_style_surface_border;
static lv_style_t s_style_text;
static lv_style_t s_style_text_muted;
static lv_style_t s_style_button;
static lv_style_t s_style_button_pressed;
static lv_style_t s_style_switch_track;
static lv_style_t s_style_switch_track_checked;
static lv_style_t s_style_switch_knob;
static lv_style_t s_style_tile;
static lv_style_t s_style_tile_pressed;

static ui_theme_palette_t palette_light(void)
{
    return (ui_theme_palette_t){
        .bg = lv_color_hex(0xF5F6F8),
        .surface = lv_color_hex(0xFFFFFF),
        .text = lv_color_hex(0x111827),
        .text_muted = lv_color_hex(0x6B7280),
        .border = lv_color_hex(0xD1D5DB),
        .accent = lv_color_hex(0x2563EB),
        .accent_text = lv_color_hex(0xFFFFFF),
        .work = lv_color_hex(0xEA580C),
        .rest = lv_color_hex(0x16A34A),
        .alert = lv_color_hex(0xDC2626),
        .rec = lv_color_hex(0xEF4444),
    };
}

static ui_theme_palette_t palette_dark(void)
{
    return (ui_theme_palette_t){
        .bg = lv_color_hex(0x0B1220),
        .surface = lv_color_hex(0x111827),
        .text = lv_color_hex(0xF9FAFB),
        .text_muted = lv_color_hex(0x9CA3AF),
        .border = lv_color_hex(0x374151),
        .accent = lv_color_hex(0x3B82F6),
        .accent_text = lv_color_hex(0xFFFFFF),
        .work = lv_color_hex(0xFB923C),
        .rest = lv_color_hex(0x4ADE80),
        .alert = lv_color_hex(0xF87171),
        .rec = lv_color_hex(0xF87171),
    };
}

static void styles_init_once(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_style_screen);
    lv_style_init(&s_style_surface);
    lv_style_init(&s_style_surface_border);
    lv_style_init(&s_style_text);
    lv_style_init(&s_style_text_muted);
    lv_style_init(&s_style_button);
    lv_style_init(&s_style_button_pressed);
    lv_style_init(&s_style_switch_track);
    lv_style_init(&s_style_switch_track_checked);
    lv_style_init(&s_style_switch_knob);
    lv_style_init(&s_style_tile);
    lv_style_init(&s_style_tile_pressed);

    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_screen, 0);
    lv_style_set_border_width(&s_style_screen, 0);

    lv_style_set_bg_opa(&s_style_surface, LV_OPA_COVER);
    lv_style_set_radius(&s_style_surface, 10);
    lv_style_set_pad_all(&s_style_surface, 10);

    lv_style_set_bg_opa(&s_style_surface_border, LV_OPA_COVER);
    lv_style_set_radius(&s_style_surface_border, 10);
    lv_style_set_pad_all(&s_style_surface_border, 10);
    lv_style_set_border_width(&s_style_surface_border, 1);

    lv_style_set_text_opa(&s_style_text, LV_OPA_COVER);
    lv_style_set_text_opa(&s_style_text_muted, LV_OPA_COVER);

    lv_style_set_radius(&s_style_button, 8);
    lv_style_set_bg_opa(&s_style_button, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_button, 0);

    lv_style_set_radius(&s_style_button_pressed, 8);
    lv_style_set_bg_opa(&s_style_button_pressed, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_button_pressed, 0);
    lv_style_set_opa(&s_style_button_pressed, LV_OPA_90);

    lv_style_set_bg_opa(&s_style_switch_track, LV_OPA_COVER);
    lv_style_set_radius(&s_style_switch_track, LV_RADIUS_CIRCLE);

    lv_style_set_bg_opa(&s_style_switch_track_checked, LV_OPA_COVER);
    lv_style_set_radius(&s_style_switch_track_checked, LV_RADIUS_CIRCLE);

    lv_style_set_bg_opa(&s_style_switch_knob, LV_OPA_COVER);
    lv_style_set_radius(&s_style_switch_knob, LV_RADIUS_CIRCLE);

    lv_style_set_bg_opa(&s_style_tile, LV_OPA_COVER);
    lv_style_set_radius(&s_style_tile, 10);
    lv_style_set_border_width(&s_style_tile, 1);
    lv_style_set_pad_all(&s_style_tile, 4);

    lv_style_set_bg_opa(&s_style_tile_pressed, LV_OPA_COVER);
    lv_style_set_radius(&s_style_tile_pressed, 10);
    lv_style_set_border_width(&s_style_tile_pressed, 1);
    lv_style_set_pad_all(&s_style_tile_pressed, 4);
}

static void styles_apply_palette(const ui_theme_palette_t *p)
{
    lv_style_set_bg_color(&s_style_screen, p->bg);
    lv_style_set_text_color(&s_style_screen, p->text);

    lv_style_set_bg_color(&s_style_surface, p->surface);
    lv_style_set_text_color(&s_style_surface, p->text);

    lv_style_set_bg_color(&s_style_surface_border, p->surface);
    lv_style_set_border_color(&s_style_surface_border, p->border);
    lv_style_set_text_color(&s_style_surface_border, p->text);

    lv_style_set_text_color(&s_style_text, p->text);
    lv_style_set_text_color(&s_style_text_muted, p->text_muted);

    lv_style_set_bg_color(&s_style_button, p->accent);
    lv_style_set_text_color(&s_style_button, p->accent_text);
    lv_style_set_bg_color(&s_style_button_pressed, p->accent);
    lv_style_set_text_color(&s_style_button_pressed, p->accent_text);

    lv_style_set_bg_color(&s_style_switch_track, p->border);
    lv_style_set_bg_color(&s_style_switch_track_checked, p->accent);
    lv_style_set_bg_color(&s_style_switch_knob, p->surface);

    lv_style_set_bg_color(&s_style_tile, p->surface);
    lv_style_set_border_color(&s_style_tile, p->border);
    lv_style_set_text_color(&s_style_tile, p->text);

    lv_color_t pressed = lv_color_mix(p->accent, p->surface, 64);
    lv_style_set_bg_color(&s_style_tile_pressed, pressed);
    lv_style_set_border_color(&s_style_tile_pressed, p->accent);
    lv_style_set_text_color(&s_style_tile_pressed, p->text);
}

static void apply_to_active(void)
{
    if (!s_disp) return;

    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    if (scr) ui_theme_apply_screen(scr);
}

void ui_theme_init(lv_disp_t *disp)
{
    s_disp = disp;

    styles_init_once();
    s_palette = palette_light();
    styles_apply_palette(&s_palette);

    apply_to_active();
}

void ui_theme_set(ui_theme_t theme)
{
    s_theme = theme;
    ui_theme_palette_t p = (theme == UI_THEME_DARK) ? palette_dark() : palette_light();
    ui_theme_set_palette(&p);
}

ui_theme_t ui_theme_get(void)
{
    return s_theme;
}

void ui_theme_set_palette(const ui_theme_palette_t *palette)
{
    if (!palette) return;

    s_palette = *palette;
    styles_apply_palette(&s_palette);

    apply_to_active();
}

const ui_theme_palette_t *ui_theme_palette(void)
{
    return &s_palette;
}

void ui_theme_apply_screen(lv_obj_t *screen)
{
    if (!screen) return;
    lv_obj_remove_style(screen, &s_style_screen, 0);
    lv_obj_add_style(screen, &s_style_screen, 0);
}

void ui_theme_apply_surface(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style(obj, &s_style_surface, 0);
    lv_obj_add_style(obj, &s_style_surface, 0);
}

void ui_theme_apply_surface_border(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style(obj, &s_style_surface_border, 0);
    lv_obj_add_style(obj, &s_style_surface_border, 0);
}

void ui_theme_apply_label(lv_obj_t *label, bool muted)
{
    if (!label) return;
    lv_obj_remove_style(label, &s_style_text, 0);
    lv_obj_remove_style(label, &s_style_text_muted, 0);
    lv_obj_add_style(label, &s_style_text, 0);
    if (muted) lv_obj_add_style(label, &s_style_text_muted, 0);
}

void ui_theme_apply_button(lv_obj_t *btn)
{
    if (!btn) return;
    lv_obj_remove_style(btn, &s_style_button, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(btn, &s_style_button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(btn, &s_style_button, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &s_style_button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
}

void ui_theme_apply_switch(lv_obj_t *sw)
{
    if (!sw) return;
    lv_obj_remove_style(sw, &s_style_switch_track, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_remove_style(sw, &s_style_switch_track_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_remove_style(sw, &s_style_switch_knob, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &s_style_switch_track, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &s_style_switch_track_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(sw, &s_style_switch_knob, LV_PART_KNOB | LV_STATE_DEFAULT);
}

void ui_theme_apply_tile(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style(obj, &s_style_tile, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, &s_style_tile_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(obj, &s_style_tile, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_style_tile_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
}

lv_color_t ui_theme_color_work(void)
{
    return s_palette.work;
}

lv_color_t ui_theme_color_rest(void)
{
    return s_palette.rest;
}

lv_color_t ui_theme_color_alert(void)
{
    return s_palette.alert;
}

lv_color_t ui_theme_color_rec(void)
{
    return s_palette.rec;
}

lv_color_t ui_theme_color_accent(void)
{
    return s_palette.accent;
}
