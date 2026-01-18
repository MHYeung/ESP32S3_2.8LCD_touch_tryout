#include "app_display.h"

#include "app_context.h"
#include "app_pins.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lcd_st7789.h"
#include "touch_cst328.h"

static const char *TAG = "app";

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    cst328_point_t pt;
    if (cst328_read_point(&pt) == ESP_OK && pt.pressed)
    {
        int x = (int)pt.x;
        int y = (int)pt.y;

        if (x < 0)
            x = 0;
        if (x >= LCD_H_RES)
            x = LCD_H_RES - 1;
        if (y < 0)
            y = 0;
        if (y >= LCD_V_RES)
            y = LCD_V_RES - 1;

        s_last_touch_x = x;
        s_last_touch_y = y;

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = s_last_touch_x;
        data->point.y = s_last_touch_y;
    }
}

void init_display_and_lvgl(void)
{
    ESP_LOGI(TAG, "Init ST7789 via esp_lcd...");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(lcd_st7789_init(&panel_handle, &io_handle));

    ESP_LOGI(TAG, "Init LVGL port...");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Add LVGL display...");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    lv_disp_set_default(s_disp);
}

void init_touch_and_lvgl_input(void)
{
    ESP_LOGI(TAG, "Init CST328 touch (Kconfig pins)...");

    ESP_ERROR_CHECK(cst328_init(TP_I2C_PORT,
                                TP_SDA_GPIO,
                                TP_SCL_GPIO,
                                TP_RST_GPIO,
                                TP_INT_GPIO,
                                TP_I2C_CLK));

    s_indev_touch = lv_indev_create();
    lv_indev_set_type(s_indev_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev_touch, touch_read_cb);
    lv_indev_set_display(s_indev_touch, s_disp);
}
