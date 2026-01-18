#include "app_time.h"

#include "app_context.h"
#include "esp_log.h"
#include "rtc_pcf85063.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "app";

static time_t mktime_utc(struct tm *t)
{
    // mktime interprets as local time. Convert as UTC by temporarily setting TZ.
    char *old = getenv("TZ");
    char old_copy[64] = {0};
    if (old)
        strncpy(old_copy, old, sizeof(old_copy) - 1);

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(t);

    if (old)
        setenv("TZ", old_copy, 1);
    else
        unsetenv("TZ");
    tzset();

    return epoch;
}

void gps_fix_cb(const gps_fix_t *fix, void *user)
{
    (void)user;
    if (!fix)
        return;

    if (!s_time_synced_from_gps && fix->valid_time && fix->valid_date)
    {
        // 1) set system time (epoch in UTC)
        struct tm t = fix->utc_tm;
        time_t epoch_utc = mktime_utc(&t);
        if (epoch_utc > 1700000000)
        { // sanity check (>= ~2023)
            struct timeval tv = {.tv_sec = epoch_utc, .tv_usec = 0};
            settimeofday(&tv, NULL);

            // 2) convert to local time (Taiwan = UTC+8, no DST)
            setenv("TZ", "CST-8", 1);
            tzset();

            struct tm local_tm;
            localtime_r(&epoch_utc, &local_tm);

            datetime_t dt = {0};
            dt.year = local_tm.tm_year + 1900;
            dt.month = local_tm.tm_mon + 1;
            dt.day = local_tm.tm_mday;
            dt.dotw = local_tm.tm_wday; // check your RTC expects 0=Sun; adjust if needed
            dt.hour = local_tm.tm_hour;
            dt.minute = local_tm.tm_min;
            dt.second = local_tm.tm_sec;

            PCF85063_set_all(dt);

            s_time_synced_from_gps = true;
        }
    }
    ESP_LOGI("GPS", "fix=%d time=%d date=%d lat=%.7f lon=%.7f speed=%.2f sats=%d hdop=%.1f",
             fix->valid_fix, fix->valid_time, fix->valid_date,
             fix->lat_deg, fix->lon_deg, fix->speed_mps, fix->sats, fix->hdop);
}

static uint8_t calc_dotw(uint16_t y, uint8_t m, uint8_t d)
{
    // Sakamoto: returns 0=Sunday .. 6=Saturday
    static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= (m < 3);
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}

static datetime_t app_default_datetime(void)
{
    datetime_t dt = {
        .year = 2026,
        .month = 01,
        .day = 01,
        .hour = 12,
        .minute = 0,
        .second = 0,
    };
    dt.dotw = calc_dotw(dt.year, dt.month, dt.day); // 2026.01.01  (Thurs)
    return dt;
}

esp_err_t app_set_time_from_rtc(void)
{
    // Set timezone to UTC+8 for Taiwan (POSIX TZ sign is reversed)
    setenv("TZ", "CST-8", 1);
    tzset();

    bool valid = false;
    esp_err_t err = PCF85063_is_time_valid(&valid);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "RTC validity check failed: %s", esp_err_to_name(err));
        return err;
    }

    if (!valid)
    {
        datetime_t def = app_default_datetime();

        ESP_LOGW(TAG,
                 "RTC time invalid (OSF set). Seeding RTC to default: %04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)def.year, (unsigned)def.month, (unsigned)def.day,
                 (unsigned)def.hour, (unsigned)def.minute, (unsigned)def.second);

        esp_err_t se = PCF85063_set_all(def); // writes full datetime + clears OSF
        if (se != ESP_OK)
        {
            ESP_LOGW(TAG, "RTC seed failed: %s (system time not updated)", esp_err_to_name(se));
            return se;
        }

        // Now treat as valid and continue to read RTC + set system time
        valid = true;
    }

    datetime_t dt;
    err = PCF85063_read_time(&dt);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "RTC read failed: %s", esp_err_to_name(err));
        return err;
    }

    struct tm tm_local = {0};
    tm_local.tm_year = (int)dt.year - 1900;
    tm_local.tm_mon = (int)dt.month - 1;
    tm_local.tm_mday = (int)dt.day;
    tm_local.tm_hour = (int)dt.hour;
    tm_local.tm_min = (int)dt.minute;
    tm_local.tm_sec = (int)dt.second;
    tm_local.tm_isdst = -1;

    time_t epoch = mktime(&tm_local);
    if (epoch < 0)
    {
        ESP_LOGW(TAG, "mktime() failed, not setting system time");
        return ESP_FAIL;
    }

    struct timeval tv = {
        .tv_sec = epoch,
        .tv_usec = 0};
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "System time set from RTC: %04u-%02u-%02u %02u:%02u:%02u",
             (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
             (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);

    return ESP_OK;
}
