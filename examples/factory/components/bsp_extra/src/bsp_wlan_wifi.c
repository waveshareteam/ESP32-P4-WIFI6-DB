#include "bsp_wlan_wifi.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:WlanWifi"
#include "esp_lib_utils.h"

#define WIFI_INIT_READY_BIT  BIT0
#define WIFI_INIT_FAILED_BIT BIT1
#define WIFI_STA_READY_BIT   BIT2
#define WIFI_STA_DISCONNECTED_BIT BIT3
#define WIFI_SCAN_REQUEST_BIT BIT0
#define WIFI_DISCONNECT_TIMEOUT_MS 2000U
#define WIFI_RECONNECT_ATTEMPT_COUNT 3U
#define WIFI_UPTIME_MS() ((uint32_t)(esp_timer_get_time() / 1000))

static const uint32_t s_reconnect_delay_ms[WIFI_RECONNECT_ATTEMPT_COUNT] = {
    1000U, 2000U, 5000U
};

static const char *NVS_STORAGE_NAMESPACE = "storage";
static const char *NVS_KEY_WIFI_ENABLE = "wifi_en";

static bool s_netif_initialized;
static bool s_event_loop_initialized;
static bool s_event_handlers_registered;
static bool s_enabled_setting_loaded;
static esp_netif_t *s_sta_netif;
static esp_timer_handle_t s_reconnect_timer;

static EventGroupHandle_t s_init_event_group;
static EventGroupHandle_t s_scan_event_group;
static SemaphoreHandle_t s_cache_mutex;
static SemaphoreHandle_t s_callback_mutex;
static SemaphoreHandle_t s_operation_mutex;

static bsp_extra_wlan_wifi_ap_t s_scan_cache[BSP_EXTRA_WLAN_WIFI_MAX_SCAN_RESULTS];
static uint16_t s_scan_cache_count;
static uint32_t s_scan_cache_generation;
static char s_station_ssid[BSP_EXTRA_WLAN_WIFI_SSID_SIZE];

static atomic_bool s_wifi_initialized;
static atomic_bool s_init_task_started;
static atomic_bool s_wifi_started;
static atomic_bool s_enabled;
static atomic_bool s_station_started;
static atomic_bool s_associated;
static atomic_bool s_connected;
static atomic_bool s_connecting;
static atomic_bool s_scan_task_running;
static atomic_bool s_scan_on_enable_pending;
static atomic_bool s_connect_task_running;
static atomic_bool s_cancel_connect;
static atomic_bool s_suppress_reconnect;
static atomic_uchar s_reconnect_attempts;
static atomic_uint s_association_start_ms;
static atomic_uint s_dhcp_start_ms;

static TaskHandle_t s_connect_task_handle;
static TaskHandle_t s_scan_task_handle;
static bsp_extra_wlan_wifi_ap_t s_connect_access_point;
static char s_connect_password[BSP_EXTRA_WLAN_WIFI_PASSWORD_SIZE];

static bsp_extra_wlan_wifi_event_cb_t s_event_callback;
static void *s_event_context;

static bool read_wifi_config(wifi_config_t *config);
static void update_station_ssid(const char *ssid);
static bool load_enabled_setting(void);
static bool save_enabled_setting(bool enabled);
static bool has_saved_ssid(void);
static bool start_saved_connection(void);
static void cancel_reconnect(void);
static bool schedule_reconnect(void);
static void reconnect_timer_cb(void *arg);
static void notify_event(bsp_extra_wlan_wifi_event_t event);
static esp_err_t init_wifi_infrastructure(void);
static esp_err_t init_wifi(void);
static bool create_wifi_scan_task(void);
static bool enqueue_wifi_scan_request(void);
static void try_schedule_pending_scan(void);
static void store_scan_record(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t *record_count,
    const wifi_ap_record_t *record
);
static void sort_scan_records(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t record_count
);
static void wifi_init_task(void *arg);
static void wifi_scan_task(void *arg);
static void wifi_connect_task(void *arg);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

static bool read_wifi_config(wifi_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    memset(config, 0, sizeof(*config));
    esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, config);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Read Wi-Fi station config failed: %s", esp_err_to_name(ret));
        return false;
    }
    update_station_ssid((const char *)config->sta.ssid);
    return true;
}

static void update_station_ssid(const char *ssid)
{
    if (s_cache_mutex != NULL) {
        xSemaphoreTake(s_cache_mutex, portMAX_DELAY);
    }

    if (ssid == NULL) {
        s_station_ssid[0] = '\0';
    } else {
        size_t ssid_length = strnlen(ssid, sizeof(s_station_ssid) - 1);
        memcpy(s_station_ssid, ssid, ssid_length);
        s_station_ssid[ssid_length] = '\0';
    }

    if (s_cache_mutex != NULL) {
        xSemaphoreGive(s_cache_mutex);
    }
}

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_UTILS_LOGI("SNTP time synced, timezone CST-8 applied");
}

static void start_time_sync_once(void)
{
    if (esp_sntp_enabled()) {
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    ESP_UTILS_LOGI("SNTP time sync started");
}

bool bsp_extra_wlan_wifi_init(void)
{
    if (!s_enabled_setting_loaded) {
        load_enabled_setting();
        s_enabled_setting_loaded = true;
        atomic_store(&s_scan_on_enable_pending, atomic_load(&s_enabled));
    }

    if (s_cache_mutex == NULL) {
        s_cache_mutex = xSemaphoreCreateMutex();
        if (s_cache_mutex == NULL) {
            ESP_UTILS_LOGW("Create Wi-Fi scan cache mutex failed");
            return false;
        }
    }
    if (s_callback_mutex == NULL) {
        s_callback_mutex = xSemaphoreCreateMutex();
        if (s_callback_mutex == NULL) {
            ESP_UTILS_LOGW("Create Wi-Fi callback mutex failed");
            return false;
        }
    }
    if (s_operation_mutex == NULL) {
        s_operation_mutex = xSemaphoreCreateMutex();
        if (s_operation_mutex == NULL) {
            ESP_UTILS_LOGW("Create Wi-Fi operation mutex failed");
            return false;
        }
    }

    esp_err_t ret = init_wifi_infrastructure();
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Wi-Fi infrastructure init deferred: %s", esp_err_to_name(ret));
        return false;
    }

    if (s_init_event_group == NULL) {
        s_init_event_group = xEventGroupCreate();
        if (s_init_event_group == NULL) {
            ESP_UTILS_LOGW("Create Wi-Fi init event group failed");
            return false;
        }
    }

    if ((xEventGroupGetBits(s_init_event_group) & WIFI_INIT_READY_BIT) != 0) {
        return true;
    }

    bool init_task_expected = false;
    if (atomic_compare_exchange_strong(
            &s_init_task_started, &init_task_expected, true)) {
        xEventGroupClearBits(
            s_init_event_group,
            WIFI_INIT_READY_BIT | WIFI_INIT_FAILED_BIT
        );
        BaseType_t task_ret = xTaskCreate(
            wifi_init_task,
            "wifi_init",
            6 * 1024,
            NULL,
            5,
            NULL
        );
        if (task_ret != pdPASS) {
            atomic_store(&s_init_task_started, false);
            xEventGroupSetBits(s_init_event_group, WIFI_INIT_FAILED_BIT);
            ESP_UTILS_LOGW("Create deferred Wi-Fi init task failed");
            return false;
        }
    }

    return true;
}

bool bsp_extra_wlan_wifi_wait_ready(uint32_t timeout_ms)
{
    if (s_init_event_group == NULL) {
        ESP_UTILS_LOGW("Wi-Fi init event group is not ready");
        return false;
    }

    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    const TickType_t wait_start = xTaskGetTickCount();
    EventBits_t bits = xEventGroupWaitBits(
        s_init_event_group,
        WIFI_INIT_READY_BIT | WIFI_INIT_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        timeout_ticks
    );
    if ((bits & WIFI_INIT_READY_BIT) != 0) {
        if (!atomic_load(&s_station_started)) {
            const TickType_t elapsed = xTaskGetTickCount() - wait_start;
            const TickType_t remaining = elapsed < timeout_ticks ?
                                         timeout_ticks - elapsed : 0;
            xEventGroupWaitBits(
                s_init_event_group,
                WIFI_STA_READY_BIT,
                pdFALSE,
                pdFALSE,
                remaining
            );
        }
        if (atomic_load(&s_station_started)) {
            return true;
        }
        ESP_UTILS_LOGW("Timed out waiting for Wi-Fi STA start event");
        return false;
    }
    if ((bits & WIFI_INIT_FAILED_BIT) != 0) {
        ESP_UTILS_LOGW("Deferred Wi-Fi init failed");
    } else {
        ESP_UTILS_LOGW("Timed out waiting for deferred Wi-Fi init");
    }
    return false;
}

bool bsp_extra_wlan_wifi_is_enabled(void)
{
    return atomic_load(&s_enabled);
}

bool bsp_extra_wlan_wifi_is_station_ready(void)
{
    return atomic_load(&s_wifi_started) && atomic_load(&s_station_started);
}

bool bsp_extra_wlan_wifi_is_associated(void)
{
    return atomic_load(&s_associated);
}

bool bsp_extra_wlan_wifi_has_ip(void)
{
    return atomic_load(&s_connected);
}

bool bsp_extra_wlan_wifi_is_connected(void)
{
    return bsp_extra_wlan_wifi_has_ip();
}

bool bsp_extra_wlan_wifi_is_connecting(void)
{
    return atomic_load(&s_connecting);
}

bool bsp_extra_wlan_wifi_is_scanning(void)
{
    return atomic_load(&s_scan_task_running);
}

bool bsp_extra_wlan_wifi_set_enabled(bool enabled)
{
    const bool was_enabled = atomic_exchange(&s_enabled, enabled);
    if (!save_enabled_setting(enabled)) {
        atomic_store(&s_enabled, was_enabled);
        return false;
    }

    if (!enabled) {
        atomic_store(&s_scan_on_enable_pending, false);
        atomic_store(&s_cancel_connect, true);
        atomic_store(&s_suppress_reconnect, true);
        atomic_store(&s_connecting, false);
        atomic_store(&s_reconnect_attempts, 0);
        cancel_reconnect();
        if (atomic_load(&s_scan_task_running) && atomic_load(&s_wifi_started)) {
            esp_err_t ret = esp_wifi_scan_stop();
            if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
                ESP_UTILS_LOGW("Wi-Fi scan stop request failed: %s", esp_err_to_name(ret));
            }
        }
        if (atomic_load(&s_wifi_started)) {
            esp_wifi_disconnect();
        }
        atomic_store(&s_associated, false);
        atomic_store(&s_connected, false);
        atomic_store(&s_association_start_ms, 0);
        atomic_store(&s_dhcp_start_ms, 0);
        update_station_ssid(NULL);
        atomic_store(&s_suppress_reconnect, false);
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
        return true;
    }

    if (!was_enabled) {
        atomic_store(&s_cancel_connect, false);
        atomic_store(&s_scan_on_enable_pending, true);
        if (atomic_load(&s_station_started)) {
            start_saved_connection();
        }
        try_schedule_pending_scan();
    }
    notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
    return true;
}

bool bsp_extra_wlan_wifi_copy_scan_cache(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t max_records,
    uint16_t *record_count,
    uint32_t *generation
)
{
    if (records == NULL || record_count == NULL || max_records == 0 || s_cache_mutex == NULL) {
        return false;
    }

    *record_count = 0;
    if (generation != NULL) {
        *generation = 0;
    }

    if (xSemaphoreTake(s_cache_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    uint16_t count = s_scan_cache_count;
    if (count > max_records) {
        count = max_records;
    }
    memcpy(records, s_scan_cache, count * sizeof(bsp_extra_wlan_wifi_ap_t));
    *record_count = count;
    if (generation != NULL) {
        *generation = s_scan_cache_generation;
    }
    xSemaphoreGive(s_cache_mutex);
    return true;
}

bool bsp_extra_wlan_wifi_request_scan(void)
{
    if (!atomic_load(&s_wifi_started) || !atomic_load(&s_station_started)) {
        ESP_UTILS_LOGW("Manual Wi-Fi scan requested before STA is ready");
        return false;
    }
    if (!atomic_load(&s_enabled)) {
        ESP_UTILS_LOGW("Manual Wi-Fi scan requested while Wi-Fi is disabled");
        return false;
    }
    return enqueue_wifi_scan_request();
}

bool bsp_extra_wlan_wifi_get_station_ssid(char *ssid, size_t ssid_size)
{
    if (ssid == NULL || ssid_size == 0 || !atomic_load(&s_wifi_initialized)) {
        return false;
    }

    if (s_cache_mutex != NULL) {
        xSemaphoreTake(s_cache_mutex, portMAX_DELAY);
    }

    strlcpy(ssid, s_station_ssid, ssid_size);

    if (s_cache_mutex != NULL) {
        xSemaphoreGive(s_cache_mutex);
    }
    return ssid[0] != '\0';
}

bool bsp_extra_wlan_wifi_connect(
    const bsp_extra_wlan_wifi_ap_t *access_point,
    const char *password
)
{
    if (access_point == NULL || password == NULL ||
        strnlen(access_point->ssid, sizeof(access_point->ssid)) ==
            sizeof(access_point->ssid) ||
        strnlen(password, BSP_EXTRA_WLAN_WIFI_PASSWORD_SIZE) ==
            BSP_EXTRA_WLAN_WIFI_PASSWORD_SIZE) {
        return false;
    }

    const size_t password_length = strlen(password);
    if (access_point->ssid[0] == '\0' ||
        (access_point->authmode != BSP_EXTRA_WLAN_WIFI_AUTH_OPEN &&
         password_length < 8)) {
        return false;
    }

    if (!atomic_load(&s_enabled) || !atomic_load(&s_wifi_started) ||
        !atomic_load(&s_station_started) ||
        atomic_load(&s_scan_task_running)) {
        return false;
    }

    if (xSemaphoreTake(s_operation_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (atomic_load(&s_scan_task_running)) {
        xSemaphoreGive(s_operation_mutex);
        return false;
    }

    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_connect_task_running, &expected, true)) {
        xSemaphoreGive(s_operation_mutex);
        ESP_UTILS_LOGW("Wi-Fi connection request is already running");
        return false;
    }

    s_connect_access_point = *access_point;
    update_station_ssid((const char *)access_point->ssid);
    strlcpy(s_connect_password, password, sizeof(s_connect_password));
    atomic_store(&s_cancel_connect, false);
    atomic_store(&s_suppress_reconnect, true);
    cancel_reconnect();
    atomic_store(&s_connecting, true);
    atomic_store(&s_reconnect_attempts, 0);
    xSemaphoreGive(s_operation_mutex);
    notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);

    BaseType_t task_ret = xTaskCreate(
        wifi_connect_task,
        "wifi_connect",
        6 * 1024,
        NULL,
        9,
        &s_connect_task_handle
    );
    if (task_ret != pdPASS) {
        atomic_store(&s_suppress_reconnect, false);
        atomic_store(&s_connecting, false);
        atomic_store(&s_connect_task_running, false);
        s_connect_task_handle = NULL;
        ESP_UTILS_LOGW("Create Wi-Fi connection task failed");
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
        return false;
    }
    return true;
}

bool bsp_extra_wlan_wifi_clear_saved_config(void)
{
    if (!atomic_load(&s_wifi_initialized)) {
        return false;
    }

    atomic_store(&s_cancel_connect, true);
    atomic_store(&s_suppress_reconnect, true);
    cancel_reconnect();
    esp_wifi_disconnect();
    esp_err_t ret = esp_wifi_restore();
    atomic_store(&s_suppress_reconnect, false);
    atomic_store(&s_associated, false);
    atomic_store(&s_connected, false);
    atomic_store(&s_association_start_ms, 0);
    atomic_store(&s_dhcp_start_ms, 0);
    update_station_ssid(NULL);
    atomic_store(&s_connecting, false);
    atomic_store(&s_reconnect_attempts, 0);
    notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);

    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Restore Wi-Fi config failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

void bsp_extra_wlan_wifi_set_event_callback(
    bsp_extra_wlan_wifi_event_cb_t callback,
    void *context
)
{
    if (s_callback_mutex != NULL) {
        xSemaphoreTake(s_callback_mutex, portMAX_DELAY);
    }
    s_event_callback = callback;
    s_event_context = context;
    if (s_callback_mutex != NULL) {
        xSemaphoreGive(s_callback_mutex);
    }
}

void bsp_extra_wlan_wifi_clear_event_callback(
    bsp_extra_wlan_wifi_event_cb_t callback,
    void *context
)
{
    if (s_callback_mutex != NULL) {
        xSemaphoreTake(s_callback_mutex, portMAX_DELAY);
    }
    if (s_event_callback == callback && s_event_context == context) {
        s_event_callback = NULL;
        s_event_context = NULL;
    }
    if (s_callback_mutex != NULL) {
        xSemaphoreGive(s_callback_mutex);
    }
}

static esp_err_t init_wifi_infrastructure(void)
{
    if (!s_netif_initialized) {
        esp_err_t ret = esp_netif_init();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_UTILS_LOGE("esp_netif_init failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_netif_initialized = true;
    }

    if (!s_event_loop_initialized) {
        esp_err_t ret = esp_event_loop_create_default();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_UTILS_LOGE("esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_event_loop_initialized = true;
    }
    return ESP_OK;
}

static esp_err_t init_wifi(void)
{
    esp_err_t ret = init_wifi_infrastructure();
    if (ret != ESP_OK) {
        return ret;
    }

    const uint32_t hosted_start_ms = WIFI_UPTIME_MS();
    ESP_UTILS_LOGI(
        "Wi-Fi timing: HOSTED_START uptime=%u ms",
        (unsigned)hosted_start_ms
    );
    int hosted_ret = esp_hosted_connect_to_slave();
    if (hosted_ret != ESP_OK) {
        ESP_UTILS_LOGE(
            "Wi-Fi timing: HOSTED_FAILED error=%d elapsed=%u ms",
            hosted_ret,
            (unsigned)(WIFI_UPTIME_MS() - hosted_start_ms)
        );
        return (esp_err_t)hosted_ret;
    }
    ESP_UTILS_LOGI(
        "Wi-Fi timing: HOSTED_READY elapsed=%u ms uptime=%u ms",
        (unsigned)(WIFI_UPTIME_MS() - hosted_start_ms),
        (unsigned)WIFI_UPTIME_MS()
    );

    if (s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (s_sta_netif == NULL) {
            ESP_UTILS_LOGE("esp_netif_create_default_wifi_sta failed");
            return ESP_FAIL;
        }
    }

    if (!atomic_load(&s_wifi_initialized)) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("esp_wifi_init failed: %s", esp_err_to_name(ret));
            return ret;
        }
        atomic_store(&s_wifi_initialized, true);
    }

    if (!s_event_handlers_registered) {
        ret = esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL
        );
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Register Wi-Fi event handler failed: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = esp_event_handler_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL
        );
        if (ret != ESP_OK) {
            esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
            ESP_UTILS_LOGE("Register IP event handler failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_event_handlers_registered = true;
    }

    if (s_reconnect_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = reconnect_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "wifi_reconnect",
        };
        ret = esp_timer_create(&timer_args, &s_reconnect_timer);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Create Wi-Fi reconnect timer failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("esp_wifi_set_mode returned: %s", esp_err_to_name(ret));
    }

    if (!atomic_load(&s_wifi_started)) {
        ret = esp_wifi_start();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_UTILS_LOGE("esp_wifi_start failed: %s", esp_err_to_name(ret));
            return ret;
        }
        atomic_store(&s_wifi_started, true);
    }

    return ESP_OK;
}

static bool load_enabled_setting(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Open Wi-Fi settings failed: %s", esp_err_to_name(ret));
        atomic_store(&s_enabled, false);
        return false;
    }

    int32_t enabled = 1;
    ret = nvs_get_i32(nvs_handle, NVS_KEY_WIFI_ENABLE, &enabled);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = nvs_set_i32(nvs_handle, NVS_KEY_WIFI_ENABLE, enabled);
        if (ret == ESP_OK) {
            ret = nvs_commit(nvs_handle);
        }
    }
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Read Wi-Fi enable setting failed: %s", esp_err_to_name(ret));
        atomic_store(&s_enabled, false);
        return false;
    }

    atomic_store(&s_enabled, enabled != 0);
    return true;
}

static bool save_enabled_setting(bool enabled)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Open Wi-Fi settings for write failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = nvs_set_i32(nvs_handle, NVS_KEY_WIFI_ENABLE, enabled ? 1 : 0);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Save Wi-Fi enable setting failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

static bool has_saved_ssid(void)
{
    wifi_config_t config = {0};
    return read_wifi_config(&config) && config.sta.ssid[0] != '\0';
}

static esp_err_t start_wifi_connection(const char *source)
{
    const uint32_t start_ms = WIFI_UPTIME_MS();
    atomic_store(&s_association_start_ms, start_ms);
    atomic_store(&s_dhcp_start_ms, 0);
    ESP_UTILS_LOGI(
        "Wi-Fi timing: ASSOCIATION_START source=%s uptime=%u ms",
        source,
        (unsigned)start_ms
    );

    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        atomic_store(&s_association_start_ms, 0);
    }
    return ret;
}

static bool start_saved_connection(void)
{
    if (!atomic_load(&s_enabled) || !atomic_load(&s_wifi_started) ||
        !atomic_load(&s_station_started) || atomic_load(&s_associated) ||
        atomic_load(&s_connected)) {
        return false;
    }
    if (!has_saved_ssid()) {
        atomic_store(&s_connecting, false);
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
        return true;
    }

    atomic_store(&s_connecting, true);
    atomic_store(&s_reconnect_attempts, 0);
    esp_err_t ret = start_wifi_connection("saved");
    if (ret != ESP_OK) {
        atomic_store(&s_connecting, false);
        ESP_UTILS_LOGW("Connect saved Wi-Fi failed: %s", esp_err_to_name(ret));
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
        return false;
    }
    notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
    return true;
}

static void cancel_reconnect(void)
{
    if (s_reconnect_timer != NULL) {
        (void)esp_timer_stop(s_reconnect_timer);
    }
}

static bool schedule_reconnect(void)
{
    if (s_reconnect_timer == NULL || !atomic_load(&s_enabled) ||
        atomic_load(&s_suppress_reconnect)) {
        return false;
    }

    const uint8_t attempt = atomic_fetch_add(&s_reconnect_attempts, 1);
    if (attempt >= WIFI_RECONNECT_ATTEMPT_COUNT) {
        atomic_store(&s_reconnect_attempts, 0);
        return false;
    }

    const uint32_t delay_ms = s_reconnect_delay_ms[attempt];
    cancel_reconnect();
    esp_err_t ret = esp_timer_start_once(
        s_reconnect_timer,
        (uint64_t)delay_ms * 1000U
    );
    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Schedule Wi-Fi reconnect failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_UTILS_LOGI(
        "Wi-Fi reconnect attempt %u scheduled in %u ms",
        (unsigned)(attempt + 1),
        (unsigned)delay_ms
    );
    return true;
}

static void reconnect_timer_cb(void *arg)
{
    (void)arg;
    if (!atomic_load(&s_enabled) || atomic_load(&s_suppress_reconnect) ||
        atomic_load(&s_cancel_connect) || atomic_load(&s_connected)) {
        return;
    }

    esp_err_t ret = start_wifi_connection("reconnect");
    if (ret == ESP_OK) {
        return;
    }

    ESP_UTILS_LOGW("Wi-Fi reconnect failed: %s", esp_err_to_name(ret));
    if (!schedule_reconnect()) {
        atomic_store(&s_connecting, false);
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
    }
}

static void notify_event(bsp_extra_wlan_wifi_event_t event)
{
    bsp_extra_wlan_wifi_event_cb_t callback = NULL;
    void *context = NULL;

    if (s_callback_mutex != NULL) {
        xSemaphoreTake(s_callback_mutex, portMAX_DELAY);
        callback = s_event_callback;
        context = s_event_context;
        xSemaphoreGive(s_callback_mutex);
    } else {
        callback = s_event_callback;
        context = s_event_context;
    }

    if (callback != NULL) {
        callback(context, event);
    }
}

static bool create_wifi_scan_task(void)
{
    if (s_scan_event_group == NULL) {
        s_scan_event_group = xEventGroupCreate();
        if (s_scan_event_group == NULL) {
            ESP_UTILS_LOGW("Create Wi-Fi scan event group failed");
            return false;
        }
    }

    if (s_scan_task_handle != NULL) {
        return true;
    }

    BaseType_t task_ret = xTaskCreate(
        wifi_scan_task,
        "wifi_scan",
        6 * 1024,
        NULL,
        5,
        &s_scan_task_handle
    );
    if (task_ret != pdPASS) {
        s_scan_task_handle = NULL;
        ESP_UTILS_LOGW("Create Wi-Fi scan task failed");
        return false;
    }

    return true;
}

static bool enqueue_wifi_scan_request(void)
{
    if (s_scan_event_group == NULL || s_scan_task_handle == NULL) {
        ESP_UTILS_LOGW("Wi-Fi scan task is not ready");
        return false;
    }

    if (xSemaphoreTake(s_operation_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (atomic_load(&s_connecting) || atomic_load(&s_connect_task_running)) {
        xSemaphoreGive(s_operation_mutex);
        ESP_UTILS_LOGI("Wi-Fi scan rejected while connection is in progress");
        return false;
    }

    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_scan_task_running, &expected, true)) {
        xSemaphoreGive(s_operation_mutex);
        ESP_UTILS_LOGI("Wi-Fi scan request is already pending");
        return false;
    }

    atomic_store(&s_scan_on_enable_pending, false);
    xSemaphoreGive(s_operation_mutex);
    xEventGroupSetBits(s_scan_event_group, WIFI_SCAN_REQUEST_BIT);
    notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_SCAN_STARTED);
    return true;
}

static void try_schedule_pending_scan(void)
{
    if (!atomic_load(&s_scan_on_enable_pending) ||
        !atomic_load(&s_enabled) ||
        !atomic_load(&s_wifi_started) ||
        !atomic_load(&s_station_started) ||
        atomic_load(&s_connected) ||
        atomic_load(&s_connecting) ||
        atomic_load(&s_connect_task_running)) {
        return;
    }

    if (enqueue_wifi_scan_request()) {
        ESP_UTILS_LOGI("Wi-Fi scan scheduled after Wi-Fi was enabled");
    } else if (atomic_load(&s_scan_task_running)) {
        atomic_store(&s_scan_on_enable_pending, false);
    }
}

static void store_scan_record(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t *record_count,
    const wifi_ap_record_t *record
)
{
    if (records == NULL || record_count == NULL || record == NULL ||
        record->ssid[0] == '\0') {
        return;
    }

    for (uint16_t i = 0; i < *record_count; ++i) {
        if (strncmp(
                records[i].ssid,
                (const char *)record->ssid,
                sizeof(records[i].ssid)) == 0) {
            if (record->rssi > records[i].rssi) {
                records[i].rssi = record->rssi;
                records[i].authmode = (uint8_t)record->authmode;
            }
            return;
        }
    }

    uint16_t store_index = *record_count;
    if (store_index >= BSP_EXTRA_WLAN_WIFI_MAX_SCAN_RESULTS) {
        store_index = 0;
        for (uint16_t i = 1; i < *record_count; ++i) {
            if (records[i].rssi < records[store_index].rssi) {
                store_index = i;
            }
        }
        if (record->rssi <= records[store_index].rssi) {
            return;
        }
    } else {
        ++(*record_count);
    }

    strlcpy(
        records[store_index].ssid,
        (const char *)record->ssid,
        sizeof(records[store_index].ssid)
    );
    records[store_index].rssi = record->rssi;
    records[store_index].authmode = (uint8_t)record->authmode;
}

static void sort_scan_records(
    bsp_extra_wlan_wifi_ap_t *records,
    uint16_t record_count
)
{
    for (uint16_t i = 1; i < record_count; ++i) {
        const bsp_extra_wlan_wifi_ap_t record = records[i];
        uint16_t insert_index = i;
        while (insert_index > 0 &&
               records[insert_index - 1].rssi < record.rssi) {
            records[insert_index] = records[insert_index - 1];
            --insert_index;
        }
        records[insert_index] = record;
    }
}

static void wifi_init_task(void *arg)
{
    (void)arg;
    ESP_UTILS_LOGI("Deferred ESP32-C5 Wi-Fi init start");
    esp_err_t ret = init_wifi();
    if (ret == ESP_OK && !create_wifi_scan_task()) {
        ret = ESP_FAIL;
    }
    if (s_init_event_group != NULL) {
        xEventGroupSetBits(
            s_init_event_group,
            ret == ESP_OK ? WIFI_INIT_READY_BIT : WIFI_INIT_FAILED_BIT
        );
    }

    if (ret != ESP_OK) {
        ESP_UTILS_LOGW("Deferred ESP32-C5 Wi-Fi init failed: %s", esp_err_to_name(ret));
    } else {
        try_schedule_pending_scan();
    }
    atomic_store(&s_init_task_started, false);
    vTaskDelete(NULL);
}

static void wifi_scan_task(void *arg)
{
    (void)arg;
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            s_scan_event_group,
            WIFI_SCAN_REQUEST_BIT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        if ((bits & WIFI_SCAN_REQUEST_BIT) == 0) {
            continue;
        }

        while ((atomic_load(&s_connecting) || atomic_load(&s_connect_task_running)) &&
               atomic_load(&s_wifi_started) &&
               atomic_load(&s_station_started) &&
               atomic_load(&s_enabled)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (!atomic_load(&s_wifi_started) || !atomic_load(&s_station_started) ||
            !atomic_load(&s_enabled)) {
            ESP_UTILS_LOGW("Wi-Fi scan aborted because Wi-Fi is not ready or enabled");
            atomic_store(&s_scan_task_running, false);
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_SCAN_UPDATED);
            continue;
        }

        bsp_extra_wlan_wifi_ap_t records[BSP_EXTRA_WLAN_WIFI_MAX_SCAN_RESULTS] = {0};
        uint16_t scanned_ap_count = 0;
        uint16_t stored_ap_count = 0;
        bool cache_update_ready = false;

        esp_err_t ret = esp_wifi_scan_start(NULL, true);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGW("Wi-Fi scan start failed: %s", esp_err_to_name(ret));
        } else {
            ret = esp_wifi_scan_get_ap_num(&scanned_ap_count);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGW("Wi-Fi scan get count failed: %s", esp_err_to_name(ret));
            } else {
                cache_update_ready = true;
                // Read fixed-size records to avoid the Hosted batch-record RPC path
                // returning a NULL output list on an otherwise successful response.
                for (uint16_t i = 0; i < scanned_ap_count; ++i) {
                    wifi_ap_record_t record = {0};
                    ret = esp_wifi_scan_get_ap_record(&record);
                    if (ret != ESP_OK) {
                        ESP_UTILS_LOGW(
                            "Wi-Fi scan get record %u failed: %s",
                            (unsigned)i,
                            esp_err_to_name(ret)
                        );
                        break;
                    }

                    store_scan_record(records, &stored_ap_count, &record);
                }

                sort_scan_records(records, stored_ap_count);

                ret = esp_wifi_clear_ap_list();
                if (ret != ESP_OK) {
                    ESP_UTILS_LOGW("Wi-Fi scan clear list failed: %s", esp_err_to_name(ret));
                }
            }
        }

        if (cache_update_ready && s_cache_mutex != NULL &&
            xSemaphoreTake(s_cache_mutex, portMAX_DELAY) == pdTRUE) {
            memset(s_scan_cache, 0, sizeof(s_scan_cache));
            memcpy(
                s_scan_cache,
                records,
                stored_ap_count * sizeof(bsp_extra_wlan_wifi_ap_t)
            );
            s_scan_cache_count = stored_ap_count;
            ++s_scan_cache_generation;
            xSemaphoreGive(s_cache_mutex);

            ESP_UTILS_LOGI(
                "Wi-Fi scan: found %u APs, cached %u",
                (unsigned)scanned_ap_count,
                (unsigned)stored_ap_count
            );
        }

        atomic_store(&s_scan_task_running, false);
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_SCAN_UPDATED);
    }
}

static void wifi_connect_task(void *arg)
{
    (void)arg;
    bool success = false;

    if (atomic_load(&s_connect_task_running) && atomic_load(&s_enabled) &&
        !atomic_load(&s_cancel_connect)) {
        ESP_UTILS_LOGI("Connecting to SSID: %s", s_connect_access_point.ssid);
        ESP_UTILS_LOGI(
            "Wi-Fi password length: %u",
            (unsigned)strlen(s_connect_password)
        );

        const bool station_connection_active =
            atomic_load(&s_associated) ||
            atomic_load(&s_connected) ||
            atomic_load(&s_association_start_ms) != 0 ||
            atomic_load(&s_dhcp_start_ms) != 0;
        bool disconnect_complete = true;
        if (station_connection_active) {
            xEventGroupClearBits(s_init_event_group, WIFI_STA_DISCONNECTED_BIT);
            esp_err_t disconnect_ret = esp_wifi_disconnect();
            disconnect_complete = disconnect_ret != ESP_OK;
            if (!disconnect_complete) {
                EventBits_t bits = xEventGroupWaitBits(
                    s_init_event_group,
                    WIFI_STA_DISCONNECTED_BIT,
                    pdTRUE,
                    pdFALSE,
                    pdMS_TO_TICKS(WIFI_DISCONNECT_TIMEOUT_MS)
                );
                disconnect_complete = (bits & WIFI_STA_DISCONNECTED_BIT) != 0;
            }
        }

        if (!disconnect_complete) {
            ESP_UTILS_LOGW(
                "Timed out waiting for previous Wi-Fi connection to stop; "
                "trying the new configuration"
            );
        }
        if (!atomic_load(&s_cancel_connect) && atomic_load(&s_enabled)) {
            wifi_config_t config = {0};
            update_station_ssid(s_connect_access_point.ssid);
            strlcpy(
                (char *)config.sta.ssid,
                s_connect_access_point.ssid,
                sizeof(config.sta.ssid)
            );
            strlcpy(
                (char *)config.sta.password,
                s_connect_password,
                sizeof(config.sta.password)
            );
            config.sta.threshold.authmode =
                (wifi_auth_mode_t)s_connect_access_point.authmode;

            esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &config);
            if (ret == ESP_OK) {
                ret = start_wifi_connection("manual");
            }
            if (ret == ESP_OK) {
                success = true;
            } else {
                ESP_UTILS_LOGW("Connect Wi-Fi failed: %s", esp_err_to_name(ret));
            }
        }
    }

    atomic_store(&s_suppress_reconnect, false);
    if (!success) {
        atomic_store(&s_connecting, false);
        notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
    }
    atomic_store(&s_connect_task_running, false);
    s_connect_task_handle = NULL;
    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            atomic_store(&s_station_started, true);
            atomic_store(&s_associated, false);
            atomic_store(&s_connected, false);
            atomic_store(&s_association_start_ms, 0);
            atomic_store(&s_dhcp_start_ms, 0);
            if (s_init_event_group != NULL) {
                xEventGroupSetBits(s_init_event_group, WIFI_STA_READY_BIT);
            }
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            if (atomic_load(&s_enabled)) {
                start_saved_connection();
                try_schedule_pending_scan();
            }
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            const uint32_t now_ms = WIFI_UPTIME_MS();
            const uint32_t association_start_ms =
                atomic_exchange(&s_association_start_ms, 0);

            atomic_store(&s_associated, true);
            atomic_store(&s_connected, false);
            atomic_store(&s_dhcp_start_ms, now_ms);
            if (association_start_ms != 0) {
                ESP_UTILS_LOGI(
                    "Wi-Fi timing: ASSOCIATION_READY elapsed=%u ms uptime=%u ms",
                    (unsigned)(now_ms - association_start_ms),
                    (unsigned)now_ms
                );
            } else {
                ESP_UTILS_LOGI(
                    "Wi-Fi timing: ASSOCIATION_READY uptime=%u ms",
                    (unsigned)now_ms
                );
            }
            ESP_UTILS_LOGI(
                "Wi-Fi timing: DHCP_START uptime=%u ms",
                (unsigned)now_ms
            );
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
            const uint32_t now_ms = WIFI_UPTIME_MS();
            const bool reconnect_suppressed =
                atomic_load(&s_suppress_reconnect);
            const bool was_associated =
                atomic_exchange(&s_associated, false);
            const bool had_ip = atomic_exchange(&s_connected, false);
            const wifi_event_sta_disconnected_t *event =
                (const wifi_event_sta_disconnected_t *)event_data;

            if (event != NULL) {
                ESP_UTILS_LOGW(
                    "Wi-Fi STA disconnected: reason=%u, rssi=%d, "
                    "associated=%s, had_ip=%s, reconnect_suppressed=%s, "
                    "uptime=%u ms",
                    (unsigned)event->reason,
                    (int)event->rssi,
                    was_associated ? "true" : "false",
                    had_ip ? "true" : "false",
                    reconnect_suppressed ? "true" : "false",
                    (unsigned)now_ms
                );
            } else {
                ESP_UTILS_LOGW(
                    "Wi-Fi STA disconnected: no event data, uptime=%u ms",
                    (unsigned)now_ms
                );
            }

            if (!reconnect_suppressed) {
                const uint32_t association_start_ms =
                    atomic_exchange(&s_association_start_ms, 0);
                const uint32_t dhcp_start_ms =
                    atomic_exchange(&s_dhcp_start_ms, 0);

                if (!was_associated && association_start_ms != 0) {
                    ESP_UTILS_LOGW(
                        "Wi-Fi timing: ASSOCIATION_FAILED elapsed=%u ms "
                        "uptime=%u ms",
                        (unsigned)(now_ms - association_start_ms),
                        (unsigned)now_ms
                    );
                } else if (was_associated && !had_ip && dhcp_start_ms != 0) {
                    ESP_UTILS_LOGW(
                        "Wi-Fi timing: DHCP_INTERRUPTED elapsed=%u ms "
                        "uptime=%u ms",
                        (unsigned)(now_ms - dhcp_start_ms),
                        (unsigned)now_ms
                    );
                }
            }

            update_station_ssid(NULL);
            if (reconnect_suppressed || !atomic_load(&s_enabled)) {
                atomic_store(&s_connecting, false);
                atomic_store(&s_reconnect_attempts, 0);
                notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
                if (s_init_event_group != NULL) {
                    xEventGroupSetBits(
                        s_init_event_group,
                        WIFI_STA_DISCONNECTED_BIT
                    );
                }
                break;
            }

            if (has_saved_ssid()) {
                atomic_store(&s_connecting, true);
                if (!schedule_reconnect()) {
                    atomic_store(&s_connecting, false);
                }
            } else {
                atomic_store(&s_connecting, false);
                atomic_store(&s_reconnect_attempts, 0);
                try_schedule_pending_scan();
            }
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            break;
        }
        case WIFI_EVENT_STA_STOP:
            atomic_store(&s_station_started, false);
            if (s_init_event_group != NULL) {
                xEventGroupClearBits(s_init_event_group, WIFI_STA_READY_BIT);
            }
            atomic_store(&s_associated, false);
            atomic_store(&s_connected, false);
            atomic_store(&s_association_start_ms, 0);
            atomic_store(&s_dhcp_start_ms, 0);
            update_station_ssid(NULL);
            atomic_store(&s_connecting, false);
            cancel_reconnect();
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            const uint32_t now_ms = WIFI_UPTIME_MS();
            const uint32_t dhcp_start_ms =
                atomic_exchange(&s_dhcp_start_ms, 0);
            const ip_event_got_ip_t *event =
                (const ip_event_got_ip_t *)event_data;

            atomic_store(&s_associated, true);
            atomic_store(&s_connected, true);
            atomic_store(&s_connecting, false);
            atomic_store(&s_reconnect_attempts, 0);
            cancel_reconnect();
            atomic_store(&s_scan_on_enable_pending, false);
            if (dhcp_start_ms != 0) {
                ESP_UTILS_LOGI(
                    "Wi-Fi timing: DHCP_READY elapsed=%u ms uptime=%u ms",
                    (unsigned)(now_ms - dhcp_start_ms),
                    (unsigned)now_ms
                );
            } else {
                ESP_UTILS_LOGI(
                    "Wi-Fi timing: DHCP_READY uptime=%u ms",
                    (unsigned)now_ms
                );
            }
            if (event != NULL) {
                ESP_UTILS_LOGI("Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            }
            start_time_sync_once();
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            break;
        }
        case IP_EVENT_STA_LOST_IP: {
            const uint32_t now_ms = WIFI_UPTIME_MS();

            atomic_store(&s_connected, false);
            if (atomic_load(&s_associated)) {
                atomic_store(&s_connecting, true);
                atomic_store(&s_dhcp_start_ms, now_ms);
                ESP_UTILS_LOGW(
                    "Wi-Fi timing: DHCP_RESTART uptime=%u ms",
                    (unsigned)now_ms
                );
            } else {
                atomic_store(&s_dhcp_start_ms, 0);
            }
            ESP_UTILS_LOGW(
                "Wi-Fi lost IP address, uptime=%u ms",
                (unsigned)now_ms
            );
            notify_event(BSP_EXTRA_WLAN_WIFI_EVENT_STATE_CHANGED);
            break;
        }
        default:
            break;
        }
    }
}
