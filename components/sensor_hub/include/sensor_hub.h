#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_HUB_DISABLED = 0,
    SENSOR_HUB_IDLE,
    SENSOR_HUB_SCANNING,
    SENSOR_HUB_CONNECTING,
    SENSOR_HUB_CONNECTED,
} sensor_hub_state_t;

typedef struct {
    char name[24];
    uint8_t addr[6];
    uint8_t addr_type;
    int8_t rssi;
} sensor_hub_device_t;

esp_err_t sensor_hub_init(void);
sensor_hub_state_t sensor_hub_get_state(void);
uint32_t sensor_hub_get_epoch(void);

esp_err_t sensor_hub_start_scan(void);
esp_err_t sensor_hub_stop_scan(void);
size_t sensor_hub_get_results(sensor_hub_device_t *out, size_t max);

esp_err_t sensor_hub_connect_result(size_t index);
esp_err_t sensor_hub_connect_saved(void);
esp_err_t sensor_hub_disconnect(void);
esp_err_t sensor_hub_forget(void);

bool sensor_hub_get_peer(sensor_hub_device_t *out);
bool sensor_hub_has_saved(void);
bool sensor_hub_get_hr(uint8_t *bpm);

#ifdef __cplusplus
}
#endif
