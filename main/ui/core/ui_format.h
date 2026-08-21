#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_fmt_time_s(float sec, char *out, size_t out_len);
void ui_fmt_pace_s(float sec, char *out, size_t out_len);
void ui_fmt_distance_m(float m, char *value_out, size_t value_len, const char **unit_out);
void ui_fmt_spm(float spm, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
