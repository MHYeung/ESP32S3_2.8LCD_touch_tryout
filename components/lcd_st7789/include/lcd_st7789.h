#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif


#define LCD_H_RES 240
#define LCD_V_RES 320

esp_err_t lcd_st7789_init(esp_lcd_panel_handle_t *out_panel,
                          esp_lcd_panel_io_handle_t *out_io);

/** PWM backlight, 0–100 percent. 0 is off; keep a small floor for outdoor use. */
esp_err_t lcd_st7789_set_backlight_percent(uint8_t percent);

#ifdef __cplusplus
}
#endif
