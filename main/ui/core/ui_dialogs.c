#include "ui_core_internal.h"

#include "ui_theme.h"
#include "esp_lvgl_port.h"
#include <stdio.h>
#include <string.h>

static void style_dialog_panel(lv_obj_t *panel, lv_obj_t *btn_box, lv_obj_t *btn1, lv_obj_t *btn2, lv_obj_t *msg_lbl)
{
    if (!panel)
        return;

    bool land = ui_is_landscape();
    lv_coord_t w_panel = land ? 280 : 220; // Default width

    lv_obj_set_width(panel, w_panel);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);

    // Adjust padding: Less padding in landscape to save vertical space
    lv_obj_set_style_pad_all(panel, land ? 10 : 14, 0);
    lv_obj_set_style_pad_row(panel, land ? 4 : 14, 0);

    if (msg_lbl)
    {
        lv_obj_set_width(msg_lbl, lv_pct(100));
        lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);
    }

    if (btn_box)
    {
        lv_obj_set_width(btn_box, lv_pct(100));
        lv_obj_set_height(btn_box, LV_SIZE_CONTENT);

        if (land)
        {
            // Row Layout
            lv_obj_set_flex_flow(btn_box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(btn_box, 4, 0);

            // Percentage width ensures they fit regardless of panel width
            // 47% + 47% + ~6% gap
            if (btn1)
                lv_obj_set_width(btn1, lv_pct(47));
            if (btn2)
                lv_obj_set_width(btn2, lv_pct(47));
        }
        else
        {
            // Column Layout
            lv_obj_set_flex_flow(btn_box, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(btn_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(btn_box, 10, 0);

            if (btn1)
                lv_obj_set_width(btn1, lv_pct(100));
            if (btn2)
                lv_obj_set_width(btn2, lv_pct(100));
        }
    }
}

static void update_modal_active(void)
{
    ui_s_modal_active = (ui_s_shutdown_overlay != NULL || ui_s_stop_save_overlay != NULL);
}

void ui_relayout_dialogs(void)
{
    if (ui_s_shutdown_overlay)
    {
        style_dialog_panel(ui_s_shutdown_panel, ui_s_shutdown_btn_box, ui_s_btn_shutdown, ui_s_btn_shutdown_cancel, ui_s_shutdown_msg);
    }
    if (ui_s_stop_save_overlay)
    {
        style_dialog_panel(ui_s_stop_save_panel, ui_s_stop_save_btn_box, ui_s_btn_save, ui_s_btn_save_cancel, ui_s_stop_save_msg);
    }
}

static void shutdown_btn_event_cb(lv_event_t *e)
{
    const char *tag = (const char *)lv_event_get_user_data(e);
    if (!tag)
        return;

    if (ui_s_shutdown_overlay)
    {
        lv_obj_del(ui_s_shutdown_overlay);
        ui_s_shutdown_overlay = NULL;
        ui_s_shutdown_panel = NULL;
        ui_s_shutdown_btn_box = NULL;
    }
    update_modal_active();

    if (strcmp(tag, "shutdown") == 0)
    {
        if (ui_s_shutdown_confirm_cb)
            ui_s_shutdown_confirm_cb();
    }
}

static void shutdown_prompt_create(void *unused)
{
    (void)unused;
    if (ui_s_shutdown_overlay)
        return;

    lv_obj_t *top = lv_layer_top();

    ui_s_shutdown_overlay = lv_obj_create(top);
    lv_obj_set_size(ui_s_shutdown_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(ui_s_shutdown_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(ui_s_shutdown_overlay, 0, 0);
    lv_obj_set_flex_flow(ui_s_shutdown_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_s_shutdown_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_s_shutdown_panel = lv_obj_create(ui_s_shutdown_overlay);
    ui_theme_apply_surface(ui_s_shutdown_panel);
    // Initial padding (will be updated by relayout immediately)
    lv_obj_set_style_pad_all(ui_s_shutdown_panel, 14, 0);
    lv_obj_set_style_pad_row(ui_s_shutdown_panel, 14, 0);
    lv_obj_set_flex_flow(ui_s_shutdown_panel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(ui_s_shutdown_panel);
    lv_label_set_text(title, "Power Off?");
    ui_theme_apply_label(title, false);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, lv_pct(100));

    ui_s_shutdown_msg = lv_label_create(ui_s_shutdown_panel);
    lv_label_set_text(ui_s_shutdown_msg, "Shut the device down now?");
    ui_theme_apply_label(ui_s_shutdown_msg, true);
    lv_obj_set_style_text_align(ui_s_shutdown_msg, LV_TEXT_ALIGN_CENTER, 0);

    // Button Container
    ui_s_shutdown_btn_box = lv_obj_create(ui_s_shutdown_panel);
    lv_obj_set_style_bg_opa(ui_s_shutdown_btn_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_shutdown_btn_box, 0, 0);
    lv_obj_set_style_pad_all(ui_s_shutdown_btn_box, 0, 0);

    // Buttons
    ui_s_btn_shutdown = lv_btn_create(ui_s_shutdown_btn_box);
    lv_obj_add_event_cb(ui_s_btn_shutdown, shutdown_btn_event_cb, LV_EVENT_CLICKED, (void *)"shutdown");
    lv_obj_set_style_bg_color(ui_s_btn_shutdown, lv_color_hex(0xEF4444), 0); // Red
    lv_obj_t *l1 = lv_label_create(ui_s_btn_shutdown);
    lv_label_set_text(l1, "Shutdown");
    lv_obj_center(l1);

    ui_s_btn_shutdown_cancel = lv_btn_create(ui_s_shutdown_btn_box);
    lv_obj_add_event_cb(ui_s_btn_shutdown_cancel, shutdown_btn_event_cb, LV_EVENT_CLICKED, (void *)"cancel");
    lv_obj_set_style_bg_color(ui_s_btn_shutdown_cancel, lv_color_hex(0x6B7280), 0); // Grey
    lv_obj_t *l2 = lv_label_create(ui_s_btn_shutdown_cancel);
    lv_label_set_text(l2, "Cancel");
    lv_obj_center(l2);

    // Initial Layout
    ui_relayout_dialogs();
    update_modal_active();
}

void ui_show_shutdown_prompt(void)
{
    lv_async_call(shutdown_prompt_create, NULL);
}

static void stop_save_btn_event_cb(lv_event_t *e)
{
    const char *tag = (const char *)lv_event_get_user_data(e);
    if (!tag)
        return;

    if (ui_s_stop_save_overlay)
    {
        lv_obj_del(ui_s_stop_save_overlay);
        ui_s_stop_save_overlay = NULL;
        ui_s_stop_save_panel = NULL;
        ui_s_stop_save_btn_box = NULL;
    }
    update_modal_active();

    if (strcmp(tag, "stop_save") == 0)
    {
        if (ui_s_stop_save_confirm_cb)
            ui_s_stop_save_confirm_cb();
    }
}

static void stop_save_prompt_create(void *unused)
{
    (void)unused;
    if (ui_s_stop_save_overlay)
        return;

    lv_obj_t *top = lv_layer_top();

    ui_s_stop_save_overlay = lv_obj_create(top);
    lv_obj_set_size(ui_s_stop_save_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(ui_s_stop_save_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(ui_s_stop_save_overlay, 0, 0);
    lv_obj_set_flex_flow(ui_s_stop_save_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_s_stop_save_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_s_stop_save_panel = lv_obj_create(ui_s_stop_save_overlay);
    ui_theme_apply_surface(ui_s_stop_save_panel);
    // Initial padding
    lv_obj_set_style_pad_all(ui_s_stop_save_panel, 14, 0);
    lv_obj_set_style_pad_row(ui_s_stop_save_panel, 14, 0);
    lv_obj_set_flex_flow(ui_s_stop_save_panel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(ui_s_stop_save_panel);
    lv_label_set_text(title, "Stop Activity?");
    ui_theme_apply_label(title, false);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, lv_pct(100));

    ui_s_stop_save_msg = lv_label_create(ui_s_stop_save_panel);
    lv_label_set_text(ui_s_stop_save_msg, ui_s_stop_save_prompt_msg);
    ui_theme_apply_label(ui_s_stop_save_msg, true);
    lv_obj_set_style_text_align(ui_s_stop_save_msg, LV_TEXT_ALIGN_CENTER, 0);

    // Button Container
    ui_s_stop_save_btn_box = lv_obj_create(ui_s_stop_save_panel);
    lv_obj_set_style_bg_opa(ui_s_stop_save_btn_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_stop_save_btn_box, 0, 0);
    lv_obj_set_style_pad_all(ui_s_stop_save_btn_box, 0, 0);

    // Buttons
    ui_s_btn_save = lv_btn_create(ui_s_stop_save_btn_box);
    lv_obj_add_event_cb(ui_s_btn_save, stop_save_btn_event_cb, LV_EVENT_CLICKED, (void *)"stop_save");
    ui_theme_apply_button(ui_s_btn_save);
    lv_obj_t *l1 = lv_label_create(ui_s_btn_save);
    lv_label_set_text(l1, "Save");
    lv_obj_center(l1);

    ui_s_btn_save_cancel = lv_btn_create(ui_s_stop_save_btn_box);
    lv_obj_add_event_cb(ui_s_btn_save_cancel, stop_save_btn_event_cb, LV_EVENT_CLICKED, (void *)"cancel");
    lv_obj_set_style_bg_color(ui_s_btn_save_cancel, lv_color_hex(0x6B7280), 0);
    lv_obj_t *l2 = lv_label_create(ui_s_btn_save_cancel);
    lv_label_set_text(l2, "Cancel");
    lv_obj_center(l2);

    // Initial Layout
    ui_relayout_dialogs();
    update_modal_active();
}

void ui_show_stop_save_prompt(void)
{
    snprintf(ui_s_stop_save_prompt_msg, sizeof(ui_s_stop_save_prompt_msg), "Stop and save this session?");
    lv_async_call(stop_save_prompt_create, NULL);
}

void ui_show_stop_save_prompt_with_text(const char *msg)
{
    if (msg && msg[0])
    {
        snprintf(ui_s_stop_save_prompt_msg, sizeof(ui_s_stop_save_prompt_msg), "%s", msg);
    }
    else
    {
        snprintf(ui_s_stop_save_prompt_msg, sizeof(ui_s_stop_save_prompt_msg), "Stop and save this session?");
    }
    lv_async_call(stop_save_prompt_create, NULL);
}
