#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_EXTRA_WLAN_WIFI_MAX_SCAN_RESULTS 20U
#define BSP_EXTRA_WLAN_WIFI_SSID_SIZE        33U
#define BSP_EXTRA_WLAN_WIFI_PASSWORD_SIZE    65U
#define BSP_EXTRA_WLAN_WIFI_AUTH_OPEN        0U

typedef struct {
    char ssid[BSP_EXTRA_WLAN_WIFI_SSID_SIZE];
    int8_t rssi;
    uint8_t authmode;
} bsp_extra_wlan_wifi_ap_t;

typedef enum {
    BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED = 0,
    BSP_EXTRA_WLAN_WIFI_EVENT_SCAN_STARTED,
    BSP_EXTRA_WLAN_WIFI_EVENT_SCAN_UPDATED,
} bsp_extra_wlan_wifi_event_t;

typedef void (*bsp_extra_wlan_wifi_event_cb_t)(
    void *context,
    bsp_extra_wlan_wifi_event_t event
);

bool bsp_extra_wlan_wifi_init(void);
bool bsp_extra_wlan_wifi_wait_ready(uint32_t timeout_ms);

bool bsp_extra_wlan_wifi_is_enabled(void);
bool bsp_extra_wlan_wifi_is_station_ready(void);
bool bsp_extra_wlan_wifi_is_associated(void);
bool bsp_extra_wlan_wifi_has_ip(void);
bool bsp_extra_wlan_wifi_is_connected(void);
bool bsp_extra_wlan_wifi_is_connecting(void);
bool bsp_extra_wlan_wifi_is_scanning(void);
bool bsp_extra_wlan_wifi_set_enabled(bool enabled);

bool bsp_extra_wlan_wifi_copy_scan_cache(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t max_records,
    uint16_t *record_count,
    uint32_t *generation
);
bool bsp_extra_wlan_wifi_request_scan(void);

bool bsp_extra_wlan_wifi_get_station_ssid(char *ssid, size_t ssid_size);
bool bsp_extra_wlan_wifi_connect(
    const bsp_extra_wlan_wifi_ap_t *access_point,
    const char *password
);
bool bsp_extra_wlan_wifi_clear_saved_config(void);

void bsp_extra_wlan_wifi_set_event_callback(
    bsp_extra_wlan_wifi_event_cb_t callback,
    void *context
);
void bsp_extra_wlan_wifi_clear_event_callback(
    bsp_extra_wlan_wifi_event_cb_t callback,
    void *context
);

#ifdef __cplusplus
}
#endif
