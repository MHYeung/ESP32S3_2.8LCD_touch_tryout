#include "ui_format.h"

#include <math.h>
#include <stdio.h>

void ui_fmt_time_s(float sec, char *out, size_t out_len)
{
    if (!isfinite(sec) || sec < 0.0f) {
        snprintf(out, out_len, "--:--.-");
        return;
    }

    int total = (int)sec;
    int tenths = (int)lroundf((sec - (float)total) * 10.0f);
    if (tenths >= 10) {
        tenths = 0;
        total += 1;
    }

    int s = total % 60;
    int m = (total / 60) % 60;
    int h = total / 3600;

    if (h > 0) {
        snprintf(out, out_len, "%d:%02d:%02d", h, m, s);
    } else {
        snprintf(out, out_len, "%02d:%02d.%d", m, s, tenths);
    }
}

void ui_fmt_pace_s(float sec, char *out, size_t out_len)
{
    if (!isfinite(sec) || sec <= 0.0f) {
        snprintf(out, out_len, "--:--.-");
        return;
    }
    ui_fmt_time_s(sec, out, out_len);
}

void ui_fmt_distance_m(float m, char *value_out, size_t value_len, const char **unit_out)
{
    if (!isfinite(m) || m < 0.0f) {
        snprintf(value_out, value_len, "--");
        *unit_out = "m";
        return;
    }

    if (m >= 1000.0f) {
        snprintf(value_out, value_len, "%.2f", (double)(m / 1000.0f));
        *unit_out = "km";
    } else {
        snprintf(value_out, value_len, "%.0f", (double)m);
        *unit_out = "m";
    }
}

void ui_fmt_spm(float spm, char *out, size_t out_len)
{
    if (!isfinite(spm)) {
        snprintf(out, out_len, "--");
        return;
    }
    float frac = fabsf(spm - floorf(spm));
    if (fabsf(frac - 0.5f) < 0.01f) {
        snprintf(out, out_len, "%.1f", (double)spm);
    } else {
        snprintf(out, out_len, "%.0f", (double)spm);
    }
}
