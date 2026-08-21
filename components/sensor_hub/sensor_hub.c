#include "sensor_hub.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_helper.h"
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#if defined(CONFIG_BT_ENABLED) && CONFIG_BT_ENABLED && defined(CONFIG_BT_NIMBLE_ENABLED)

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/hci_common.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "sensor_hub";

/* C3 motion service 9b7e1000-2b2f-4f71-9b86-4bb2e6d54f00 (LE wire order). */
static const ble_uuid128_t uuid_motion_svc =
    BLE_UUID128_INIT(0x00, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

#define SCAN_MS        6000
#define CONNECT_MS     10000
#define MAX_RESULTS    6

static SemaphoreHandle_t s_mux;
static sensor_hub_state_t s_state = SENSOR_HUB_IDLE;
static uint32_t s_epoch;
static uint8_t s_own_addr_type;
static bool s_synced;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static sensor_hub_device_t s_found[MAX_RESULTS];
static size_t s_found_n;
static sensor_hub_device_t s_peer;
static bool s_have_peer;

static void bump_epoch(void)
{
    s_epoch++;
}

static int gap_event(struct ble_gap_event *event, void *arg);

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
    (void)param;
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no BLE address rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "addr type rc=%d", rc);
        return;
    }
    s_synced = true;
    ESP_LOGI(TAG, "nimble synced addr_type=%u heap=%u",
             (unsigned)s_own_addr_type,
             (unsigned)esp_get_free_heap_size());
}

static bool addr_eq(const uint8_t a[6], const uint8_t b[6])
{
    return memcmp(a, b, 6) == 0;
}

static bool adv_has_motion_uuid(const struct ble_hs_adv_fields *fields)
{
    for (uint8_t i = 0; i < fields->num_uuids128; i++) {
        if (ble_uuid_cmp(&fields->uuids128[i].u, &uuid_motion_svc.u) == 0) {
            return true;
        }
    }
    return false;
}

static void copy_name(sensor_hub_device_t *dev, const struct ble_hs_adv_fields *fields)
{
    if (!fields->name || fields->name_len == 0) {
        return;
    }
    size_t n = fields->name_len;
    if (n >= sizeof(dev->name)) {
        n = sizeof(dev->name) - 1;
    }
    memcpy(dev->name, fields->name, n);
    dev->name[n] = '\0';
}

static void upsert_result(const struct ble_gap_disc_desc *disc,
                          const struct ble_hs_adv_fields *fields,
                          bool require_uuid)
{
    if (require_uuid && !adv_has_motion_uuid(fields)) {
        return;
    }

    xSemaphoreTake(s_mux, portMAX_DELAY);
    int slot = -1;
    bool structural = false;
    for (size_t i = 0; i < s_found_n; i++) {
        if (addr_eq(s_found[i].addr, disc->addr.val)) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) {
        if (!require_uuid || s_found_n >= MAX_RESULTS) {
            xSemaphoreGive(s_mux);
            return;
        }
        slot = (int)s_found_n++;
        memset(&s_found[slot], 0, sizeof(s_found[slot]));
        memcpy(s_found[slot].addr, disc->addr.val, 6);
        s_found[slot].addr_type = disc->addr.type;
        snprintf(s_found[slot].name, sizeof(s_found[slot].name), "C3-Tracker");
        structural = true;
    }
    char old_name[sizeof(s_found[slot].name)];
    memcpy(old_name, s_found[slot].name, sizeof(old_name));
    s_found[slot].rssi = disc->rssi;
    s_found[slot].addr_type = disc->addr.type;
    copy_name(&s_found[slot], fields);
    if (structural || memcmp(old_name, s_found[slot].name, sizeof(old_name)) != 0)
        bump_epoch();
    xSemaphoreGive(s_mux);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_hs_adv_fields fields;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                                    event->disc.length_data) != 0) {
            return 0;
        }
        if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
            upsert_result(&event->disc, &fields, false);
        } else if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
                   event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND) {
            upsert_result(&event->disc, &fields, true);
        }
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        xSemaphoreTake(s_mux, portMAX_DELAY);
        if (s_state == SENSOR_HUB_SCANNING) {
            s_state = SENSOR_HUB_IDLE;
            bump_epoch();
        }
        xSemaphoreGive(s_mux);
        ESP_LOGI(TAG, "scan done n=%u", (unsigned)s_found_n);
        return 0;

    case BLE_GAP_EVENT_CONNECT: {
        sensor_hub_device_t saved_peer = {0};
        bool connected = (event->connect.status == 0);
        xSemaphoreTake(s_mux, portMAX_DELAY);
        if (connected) {
            s_conn_handle = event->connect.conn_handle;
            s_state = SENSOR_HUB_CONNECTED;
            s_have_peer = true;
            saved_peer = s_peer;
            ESP_LOGI(TAG, "connected to %s heap=%u",
                     s_peer.name[0] ? s_peer.name : "pod",
                     (unsigned)esp_get_free_heap_size());
        } else {
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_state = SENSOR_HUB_IDLE;
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
        }
        bump_epoch();
        xSemaphoreGive(s_mux);
        if (connected) {
            nvs_helper_set_sensor_addr(saved_peer.addr);
            nvs_helper_set_sensor_addr_type(saved_peer.addr_type);
            nvs_helper_set_sensors_enabled(true);
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
        xSemaphoreTake(s_mux, portMAX_DELAY);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_have_peer = false;
        s_state = SENSOR_HUB_IDLE;
        bump_epoch();
        xSemaphoreGive(s_mux);
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        return 0;

    default:
        return 0;
    }
}

static esp_err_t start_connect(const sensor_hub_device_t *dev)
{
    if (!s_synced) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)ble_gap_disc_cancel();

    ble_addr_t addr = {
        .type = dev->addr_type,
    };
    memcpy(addr.val, dev->addr, 6);

    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_peer = *dev;
    s_have_peer = true;
    s_state = SENSOR_HUB_CONNECTING;
    bump_epoch();
    xSemaphoreGive(s_mux);

    int rc = ble_gap_connect(s_own_addr_type, &addr, CONNECT_MS, NULL, gap_event, NULL);
    if (rc != 0) {
        xSemaphoreTake(s_mux, portMAX_DELAY);
        s_state = SENSOR_HUB_IDLE;
        bump_epoch();
        xSemaphoreGive(s_mux);
        ESP_LOGE(TAG, "ble_gap_connect rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t sensor_hub_init(void)
{
    s_mux = xSemaphoreCreateMutex();
    if (!s_mux) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "heap before NimBLE: %u", (unsigned)esp_get_free_heap_size());
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init: %s", esp_err_to_name(err));
        s_state = SENSOR_HUB_DISABLED;
        return err;
    }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "C3 tracker client ready");
    return ESP_OK;
}

sensor_hub_state_t sensor_hub_get_state(void)
{
    return s_state;
}

uint32_t sensor_hub_get_epoch(void)
{
    return s_epoch;
}

esp_err_t sensor_hub_start_scan(void)
{
    if (!s_synced) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state == SENSOR_HUB_CONNECTED || s_state == SENSOR_HUB_CONNECTING) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ble_gap_disc_active()) {
        (void)ble_gap_disc_cancel();
    }

    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_found_n = 0;
    s_state = SENSOR_HUB_SCANNING;
    bump_epoch();
    xSemaphoreGive(s_mux);

    struct ble_gap_disc_params dp = {
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 0,
    };
    int rc = ble_gap_disc(s_own_addr_type, SCAN_MS, &dp, gap_event, NULL);
    if (rc != 0) {
        xSemaphoreTake(s_mux, portMAX_DELAY);
        s_state = SENSOR_HUB_IDLE;
        bump_epoch();
        xSemaphoreGive(s_mux);
        ESP_LOGE(TAG, "ble_gap_disc rc=%d", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "scanning for C3 tracker");
    return ESP_OK;
}

esp_err_t sensor_hub_stop_scan(void)
{
    (void)ble_gap_disc_cancel();
    xSemaphoreTake(s_mux, portMAX_DELAY);
    if (s_state == SENSOR_HUB_SCANNING) {
        s_state = SENSOR_HUB_IDLE;
        bump_epoch();
    }
    xSemaphoreGive(s_mux);
    return ESP_OK;
}

size_t sensor_hub_get_results(sensor_hub_device_t *out, size_t max)
{
    if (!out || max == 0) {
        return 0;
    }
    xSemaphoreTake(s_mux, portMAX_DELAY);
    size_t n = s_found_n < max ? s_found_n : max;
    memcpy(out, s_found, n * sizeof(*out));
    xSemaphoreGive(s_mux);
    return n;
}

esp_err_t sensor_hub_connect_result(size_t index)
{
    sensor_hub_device_t dev;
    xSemaphoreTake(s_mux, portMAX_DELAY);
    if (index >= s_found_n) {
        xSemaphoreGive(s_mux);
        return ESP_ERR_NOT_FOUND;
    }
    if (s_state == SENSOR_HUB_CONNECTED || s_state == SENSOR_HUB_CONNECTING) {
        xSemaphoreGive(s_mux);
        return ESP_ERR_INVALID_STATE;
    }
    dev = s_found[index];
    xSemaphoreGive(s_mux);
    return start_connect(&dev);
}

esp_err_t sensor_hub_connect_saved(void)
{
    sensor_hub_device_t dev = {0};
    nvs_helper_get_sensor_addr(dev.addr);
    uint8_t zero[6] = {0};
    if (memcmp(dev.addr, zero, 6) == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    dev.addr_type = nvs_helper_get_sensor_addr_type();
    snprintf(dev.name, sizeof(dev.name), "Saved pod");
    return start_connect(&dev);
}

esp_err_t sensor_hub_disconnect(void)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_OK;
    }
    int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_hub_forget(void)
{
    uint8_t zero[6] = {0};
    (void)sensor_hub_disconnect();
    nvs_helper_set_sensor_addr(zero);
    nvs_helper_set_sensor_addr_type(0);
    nvs_helper_set_sensors_enabled(false);
    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_have_peer = false;
    memset(&s_peer, 0, sizeof(s_peer));
    bump_epoch();
    xSemaphoreGive(s_mux);
    return ESP_OK;
}

bool sensor_hub_get_peer(sensor_hub_device_t *out)
{
    if (!out) {
        return false;
    }
    xSemaphoreTake(s_mux, portMAX_DELAY);
    bool ok = s_have_peer && s_state == SENSOR_HUB_CONNECTED;
    if (ok) {
        *out = s_peer;
    }
    xSemaphoreGive(s_mux);
    return ok;
}

bool sensor_hub_has_saved(void)
{
    uint8_t addr[6];
    uint8_t zero[6] = {0};
    nvs_helper_get_sensor_addr(addr);
    return memcmp(addr, zero, 6) != 0;
}

bool sensor_hub_get_hr(uint8_t *bpm)
{
    if (bpm) {
        *bpm = 0;
    }
    return false;
}

#else /* Bluetooth disabled */

static const char *TAG = "sensor_hub";

esp_err_t sensor_hub_init(void)
{
    ESP_LOGI(TAG, "Bluetooth disabled in this build");
    return ESP_OK;
}

sensor_hub_state_t sensor_hub_get_state(void)
{
    return SENSOR_HUB_DISABLED;
}

uint32_t sensor_hub_get_epoch(void)
{
    return 0;
}

esp_err_t sensor_hub_start_scan(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sensor_hub_stop_scan(void)
{
    return ESP_OK;
}

size_t sensor_hub_get_results(sensor_hub_device_t *out, size_t max)
{
    (void)out;
    (void)max;
    return 0;
}

esp_err_t sensor_hub_connect_result(size_t index)
{
    (void)index;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sensor_hub_connect_saved(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sensor_hub_disconnect(void)
{
    return ESP_OK;
}

esp_err_t sensor_hub_forget(void)
{
    return ESP_OK;
}

bool sensor_hub_get_peer(sensor_hub_device_t *out)
{
    (void)out;
    return false;
}

bool sensor_hub_has_saved(void)
{
    return false;
}

bool sensor_hub_get_hr(uint8_t *bpm)
{
    if (bpm) {
        *bpm = 0;
    }
    return false;
}

#endif
