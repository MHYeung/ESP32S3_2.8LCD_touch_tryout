#include "coach_ui_snapshot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <math.h>
#include <string.h>

static SemaphoreHandle_t s_lock;
static coach_ui_snapshot_t s_snap;
static bool s_valid;

void coach_ui_snapshot_init(void)
{
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
    }
    memset(&s_snap, 0, sizeof(s_snap));
    s_snap.pace_s_per_500m = NAN;
    s_snap.avg_pace_s_per_500m = NAN;
    s_snap.speed_mps = NAN;
    s_snap.spm = NAN;
    s_snap.stroke_len_m = NAN;
    s_snap.time_s = NAN;
    s_snap.distance_m = NAN;
    s_snap.last_split_pace_s = NAN;
    s_snap.last_split_delta_s = NAN;
    s_valid = false;
}

void coach_ui_snapshot_publish(const coach_ui_snapshot_t *src)
{
    if (!src || !s_lock) {
        return;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    s_snap = *src;
    s_valid = true;
    xSemaphoreGive(s_lock);
}

bool coach_ui_snapshot_copy(coach_ui_snapshot_t *dst)
{
    if (!dst || !s_lock) {
        return false;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }
    bool ok = s_valid;
    if (ok) {
        *dst = s_snap;
    }
    xSemaphoreGive(s_lock);
    return ok;
}
