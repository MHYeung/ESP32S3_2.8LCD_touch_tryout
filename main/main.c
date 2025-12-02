#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "lcd_st7789.h"
#include "touch_cst328.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "app";
static esp_lcd_panel_handle_t s_panel = NULL;

// Draw 3 horizontal color bands (red/green/blue)
static void draw_test_pattern(void)
{
    uint16_t line[LCD_H_RES];
    uint16_t colors[3] = { 0xF800, 0x07E0, 0x001F };

    for (int band = 0; band < 3; ++band) {
        for (int x = 0; x < LCD_H_RES; ++x) {
            line[x] = colors[band];
        }
        int y_start = (LCD_V_RES / 3) * band;
        int y_end   = (LCD_V_RES / 3) * (band + 1);
        if (band == 2) y_end = LCD_V_RES;

        for (int y = y_start; y < y_end; ++y) {
            esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + 1, line);
        }
    }
}

// Draw a filled 8x8 block at (x,y)
static void draw_touch_block(int x, int y, uint16_t color)
{
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;

    // Center block around touch point
    int x0 = x - 4;
    int y0 = y - 4;
    int x1 = x0 + 8;
    int y1 = y0 + 8;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > LCD_H_RES) x1 = LCD_H_RES;
    if (y1 > LCD_V_RES) y1 = LCD_V_RES;

    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) return;

    uint16_t buf[8 * 8];
    for (int i = 0; i < w * h; ++i) buf[i] = color;

    // Draw row by row (to handle edges cleanly)
    for (int row = 0; row < h; ++row) {
        esp_lcd_panel_draw_bitmap(
            s_panel,
            x0, y0 + row,
            x0 + w, y0 + row + 1,
            buf + row * w
        );
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Init LCD...");
    ESP_ERROR_CHECK(lcd_st7789_init(&s_panel));

    ESP_LOGI(TAG, "Draw color bars background");
    draw_test_pattern();

    ESP_LOGI(TAG, "Init touch (Kconfig pins)...");
    i2c_port_t port = CONFIG_TOUCH_CST328_I2C_PORT;
    gpio_num_t sda  = CONFIG_TOUCH_CST328_SDA;
    gpio_num_t scl  = CONFIG_TOUCH_CST328_SCL;
    gpio_num_t rst  = CONFIG_TOUCH_CST328_RST;
    gpio_num_t irq  = CONFIG_TOUCH_CST328_INT;
    uint32_t clk    = CONFIG_TOUCH_CST328_I2C_CLK;

    ESP_ERROR_CHECK(cst328_init(port, sda, scl, rst, irq, clk));

    uint16_t draw_color = 0x0000; // black squares
    bool last_pressed = false;

    while (1) {
        static int loop_cnt = 0;
        if ((loop_cnt++ % 50) == 0) {
            ESP_LOGI(TAG, "Touch loop alive");
        }

        cst328_point_t pt;
        esp_err_t err = cst328_read_point(&pt);
        if (err == ESP_OK) {
            // Always log raw state a bit for debugging
            ESP_LOGD(TAG, "raw: pressed=%d x=%u y=%u p=%u",
                     pt.pressed, pt.x, pt.y, pt.pressure);

            if (pt.pressed) {
                // For this board, raw coords should already be ~0..(239,319).
                int x = (int)pt.x;
                int y = (int)pt.y;

                // Clamp to screen
                if (x < 0) x = 0;
                if (x >= LCD_H_RES) x = LCD_H_RES - 1;
                if (y < 0) y = 0;
                if (y >= LCD_V_RES) y = LCD_V_RES - 1;

                if (!last_pressed) {
                    ESP_LOGI(TAG, "Touch DOWN at raw(%u,%u) -> (%d,%d)",
                             pt.x, pt.y, x, y);
                } else {
                    ESP_LOGI(TAG, "Touch MOVE raw(%u,%u) -> (%d,%d)",
                             pt.x, pt.y, x, y);
                }

                draw_touch_block(x, y, draw_color);
                last_pressed = true;
            } else {
                if (last_pressed) {
                    ESP_LOGI(TAG, "Touch UP");
                }
                last_pressed = false;
            }
        } else {
            ESP_LOGW(TAG, "cst328_read_point error: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
