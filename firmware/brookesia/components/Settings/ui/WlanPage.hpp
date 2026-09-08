#pragma once

#include "bsp_wlan_wifi.h"
#include "esp_brookesia.hpp"
#include "lvgl.h"

#include <atomic>
#include <vector>

namespace esp_brookesia::apps {

class WlanPage : public systems::phone::App {
public:
    static WlanPage *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    WlanPage(bool use_status_bar, bool use_navigation_bar);
    virtual ~WlanPage();

    bool run() override;
    bool back() override;
    bool close() override;

private:
    struct ButtonUserData {
        uint16_t index;
        WlanPage *page;
    };

    static constexpr uint16_t DISPLAY_AP_COUNT = BSP_EXTRA_WLAN_WIFI_MAX_SCAN_RESULTS;

    static WlanPage *_instance;
    static lv_obj_t *ta;
    static lv_obj_t *kb;

    lv_obj_t *page_root = nullptr;
    lv_obj_t *label = nullptr;
    lv_obj_t *list1 = nullptr;
    lv_obj_t *status_btn = nullptr;
    lv_obj_t *wlan_switch = nullptr;
    lv_obj_t *scan_btn = nullptr;
    lv_obj_t *wifi_icon = nullptr;
    lv_obj_t *spinner = nullptr;
    lv_obj_t *password_title = nullptr;

    lv_obj_t *connected_text = nullptr;
    lv_obj_t *scan_wait_spinner = nullptr;
    lv_obj_t *conn_btn = nullptr;
    lv_obj_t *available_text = nullptr;
    std::vector<lv_obj_t *> wifi_btns;

    lv_style_t style_list;
    lv_style_t style_list_btn;
    lv_style_t style_list_text;
    lv_style_t style_list_btn_pressed;

    std::atomic_bool page_active{false};
    bool wifi_cache_loaded = false;
    uint32_t wifi_cache_generation = 0;
    uint16_t scanned_ap_count = 0;
    bsp_extra_wlan_wifi_ap_t ap_info[DISPLAY_AP_COUNT] = {};
    bsp_extra_wlan_wifi_ap_t selected_ap = {};
    char wifi_pwd[BSP_EXTRA_WLAN_WIFI_PASSWORD_SIZE] = {};

    void CreateWifiUI();
    void refreshWifiUi();
    void refreshWifiList(const bsp_extra_wlan_wifi_ap_t *records,
                         uint16_t record_count,
                         uint32_t generation);
    void updateWifiEnabledUi(bool enabled);
    void toggleWifiUI(bool enabled);
    void showPasswordInput(uint16_t index);
    void submitPassword();

    static void wifi_event_cb(void *context, bsp_extra_wlan_wifi_event_t event);
    static void refresh_wifi_ui_async_cb(void *context);
    static void wifi_btn_cb(lv_event_t *event);
    static void scan_btn_cb(lv_event_t *event);
    static void kb_event_cb(lv_event_t *event);
    static void ta_event_cb(lv_event_t *event);
    static void connected_btn_cb(lv_event_t *event);
};

} // namespace esp_brookesia::apps
