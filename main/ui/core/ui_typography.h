#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Caption / unit labels (~14–16 px). */
const lv_font_t *ui_font_caption(void);

/** Secondary numeric values (~32 px, tabular). */
const lv_font_t *ui_font_value_sm(void);

/** Primary pace / countdown (~56 px, tabular). */
const lv_font_t *ui_font_value_lg(void);

/** LVGL FontAwesome symbols (play/stop/etc). Not the numeric value fonts. */
const lv_font_t *ui_font_symbol(void);

#ifdef __cplusplus
}
#endif
