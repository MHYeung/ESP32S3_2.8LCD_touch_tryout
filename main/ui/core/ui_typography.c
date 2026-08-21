#include "ui_typography.h"

extern const lv_font_t lv_font_num_56;
extern const lv_font_t lv_font_num_32;

const lv_font_t *ui_font_caption(void)
{
#if defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *ui_font_value_sm(void)
{
    return &lv_font_num_32;
}

const lv_font_t *ui_font_value_lg(void)
{
    return &lv_font_num_56;
}

const lv_font_t *ui_font_symbol(void)
{
#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
    return &lv_font_montserrat_28;
#else
    return LV_FONT_DEFAULT;
#endif
}
