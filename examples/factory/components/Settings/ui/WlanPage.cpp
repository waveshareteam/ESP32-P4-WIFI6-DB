#include "WlanPage.hpp"

#include "../Settings.hpp"
#include "bsp_wlan_wifi.h"
#include "SettingsUI.hpp"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:WlanPage"
#include "esp_lib_utils.h"

#include <stdlib.h>
#include <string.h>

LV_IMG_DECLARE(wifi_1);
LV_IMG_DECLARE(wifi_2);
LV_IMG_DECLARE(wifi_3);
LV_IMG_DECLARE(wifi_4);

namespace esp_brookesia::apps {

WlanPage *WlanPage::_instance = nullptr;
lv_obj_t *WlanPage::ta = nullptr;
lv_obj_t *WlanPage::kb = nullptr;

static void sort_wifi_records_by_rssi(
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

WlanPage *WlanPage::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new WlanPage(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

WlanPage::WlanPage(bool use_status_bar, bool use_navigation_bar)
    : App("WLAN", nullptr, true, use_status_bar, use_navigation_bar)
{
}

WlanPage::~WlanPage() = default;

bool WlanPage::run()
{
    ESP_UTILS_LOGI("WlanPage Run");
    page_active = true;
    wifi_cache_loaded = false;
    wifi_cache_generation = 0;
    scanned_ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    memset(&selected_ap, 0, sizeof(selected_ap));
    memset(wifi_pwd, 0, sizeof(wifi_pwd));

    if (!bsp_extra_wlan_wifi_wait_ready(5000)) {
        ESP_UTILS_LOGW("ESP32-C5 Wi-Fi is not ready");
        page_active = false;
        return false;
    }

    bsp_extra_wlan_wifi_set_event_callback(wifi_event_cb, this);
    CreateWifiUI();

    const bool enabled = bsp_extra_wlan_wifi_is_enabled();
    updateWifiEnabledUi(enabled);
    refreshWifiUi();
    return true;
}

bool WlanPage::back()
{
    ESP_UTILS_LOGI("WlanPage Back");
    Settings::requestInstance()->showRootPage();
    return true;
}

bool WlanPage::close()
{
    ESP_UTILS_LOGI("WlanPage Close");
    page_active = false;

    bsp_extra_wlan_wifi_clear_event_callback(wifi_event_cb, this);

    if (page_root != nullptr) {
        lv_obj_del(page_root);
        page_root = nullptr;
        label = nullptr;
        list1 = nullptr;
        status_btn = nullptr;
        wlan_switch = nullptr;
        scan_btn = nullptr;
        wifi_icon = nullptr;
        spinner = nullptr;
        password_title = nullptr;
        connected_text = nullptr;
        scan_wait_spinner = nullptr;
        conn_btn = nullptr;
        available_text = nullptr;
        ta = nullptr;
        kb = nullptr;
        wifi_btns.clear();
        settings_ui::reset_list_styles(
            style_list,
            style_list_btn,
            style_list_text,
            style_list_btn_pressed
        );
    }
    return true;
}

void WlanPage::CreateWifiUI()
{
    lv_obj_clean(lv_scr_act());

    page_root = settings_ui::create_page(lv_scr_act());
    status_btn = settings_ui::create_header(page_root, "WLAN", [](lv_event_t *event) {
        (void)event;
        lv_async_call([](void *param) {
            (void)param;
            Settings::requestInstance()->showRootPage();
        }, nullptr);
    });

    settings_ui::init_list_styles(
        style_list,
        style_list_btn,
        style_list_text,
        style_list_btn_pressed
    );
    lv_style_set_flex_cross_place(&style_list_btn, LV_FLEX_ALIGN_CENTER);

    list1 = settings_ui::create_content_list(page_root);
    lv_obj_add_style(list1, &style_list, LV_PART_MAIN);
    settings_ui::add_section(list1, "Network", style_list_text);

    lv_obj_t *wlan_btn = lv_list_add_button(list1, nullptr, "Wi-Fi");
    lv_obj_add_style(wlan_btn, &style_list_btn, LV_PART_MAIN);
    settings_ui::use_ellipsis_for_button_label(wlan_btn);

    wlan_switch = lv_switch_create(wlan_btn);
    lv_obj_set_size(wlan_switch, 64, 36);
    lv_obj_add_event_cb(wlan_switch, [](lv_event_t *event) {
        lv_obj_t *switch_obj = static_cast<lv_obj_t *>(lv_event_get_target(event));
        WlanPage *page = WlanPage::requestInstance();
        if (page != nullptr) {
            page->toggleWifiUI(lv_obj_has_state(switch_obj, LV_STATE_CHECKED));
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    scan_btn = lv_list_add_button(list1, nullptr, "Scan Wi-Fi");
    lv_obj_add_style(scan_btn, &style_list_btn, LV_PART_MAIN);
    lv_obj_add_style(scan_btn, &style_list_btn_pressed, LV_STATE_PRESSED);
    settings_ui::use_ellipsis_for_button_label(scan_btn);
    lv_obj_add_event_cb(scan_btn, scan_btn_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_state(scan_btn, LV_STATE_DISABLED);

    lv_obj_update_layout(page_root);
    lv_coord_t content_width = lv_obj_get_width(page_root) - settings_ui::PAGE_HORIZONTAL_MARGIN * 2;
    lv_coord_t keyboard_height = lv_obj_get_height(page_root) * 48 / 100;
    if (content_width < 1) {
        content_width = 1;
    }
    if (keyboard_height < 1) {
        keyboard_height = 1;
    }

    password_title = lv_label_create(page_root);
    lv_label_set_text(password_title, "Connect to network");
    lv_label_set_long_mode(password_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(password_title, content_width);
    lv_obj_set_style_text_font(password_title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        password_title,
        lv_color_hex(settings_ui::COLOR_PRIMARY_TEXT),
        LV_PART_MAIN
    );
    lv_obj_align(
        password_title,
        LV_ALIGN_TOP_MID,
        0,
        settings_ui::PAGE_HEADER_HEIGHT + 20
    );
    lv_obj_add_flag(password_title, LV_OBJ_FLAG_HIDDEN);

    ta = lv_textarea_create(page_root);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(ta, content_width, 64);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, settings_ui::PAGE_HEADER_HEIGHT + 64);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, sizeof(wifi_pwd) - 1);
    lv_textarea_set_password_show_time(ta, 1500);
    lv_textarea_set_placeholder_text(ta, "Enter password...");
    lv_obj_set_style_bg_color(ta, lv_color_hex(settings_ui::COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(settings_ui::COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(ta, 6, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(settings_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_color(
        ta,
        lv_color_hex(settings_ui::COLOR_SECONDARY_TEXT),
        LV_PART_TEXTAREA_PLACEHOLDER
    );
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, lv_color_white(), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, this);

    kb = lv_keyboard_create(page_root);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(kb, content_width, keyboard_height);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x181818), LV_PART_MAIN);
    lv_obj_set_style_border_color(kb, lv_color_hex(settings_ui::COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(kb, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x333333), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(settings_ui::COLOR_PRIMARY_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x555555), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, this);
}

void WlanPage::updateWifiEnabledUi(bool enabled)
{
    if (wlan_switch != nullptr) {
        if (enabled) {
            lv_obj_add_state(wlan_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(wlan_switch, LV_STATE_CHECKED);
        }
        lv_obj_remove_state(wlan_switch, LV_STATE_DISABLED);
    }

    if (enabled) {
        if (available_text != nullptr) {
            lv_obj_remove_flag(available_text, LV_OBJ_FLAG_HIDDEN);
        }
        for (lv_obj_t *button : wifi_btns) {
            if (button != nullptr) {
                lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        if (scan_btn != nullptr) {
            lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
        }
        if (spinner != nullptr) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        if (connected_text != nullptr) {
            lv_obj_add_flag(connected_text, LV_OBJ_FLAG_HIDDEN);
        }
        if (scan_wait_spinner != nullptr) {
            lv_obj_add_flag(scan_wait_spinner, LV_OBJ_FLAG_HIDDEN);
        }
        if (conn_btn != nullptr) {
            lv_obj_add_flag(conn_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (available_text != nullptr) {
            lv_obj_add_flag(available_text, LV_OBJ_FLAG_HIDDEN);
        }
        for (lv_obj_t *button : wifi_btns) {
            if (button != nullptr) {
                lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void WlanPage::toggleWifiUI(bool enabled)
{
    if (!bsp_extra_wlan_wifi_set_enabled(enabled)) {
        ESP_UTILS_LOGW("Change Wi-Fi enable state failed");
        return;
    }

    updateWifiEnabledUi(enabled);
    refreshWifiUi();
}

void WlanPage::refreshWifiUi()
{
    if (!page_active || page_root == nullptr || list1 == nullptr) {
        return;
    }

    const bool enabled = bsp_extra_wlan_wifi_is_enabled();
    updateWifiEnabledUi(enabled);
    if (!enabled) {
        return;
    }

    bsp_extra_wlan_wifi_ap_t cached_records[DISPLAY_AP_COUNT] = {};
    uint16_t cached_count = 0;
    uint32_t cache_generation = 0;
    if (!bsp_extra_wlan_wifi_copy_scan_cache(
            cached_records,
            DISPLAY_AP_COUNT,
            &cached_count,
            &cache_generation)) {
        return;
    }

    if (!wifi_cache_loaded || wifi_cache_generation != cache_generation) {
        refreshWifiList(cached_records, cached_count, cache_generation);
    }

    char connected_ssid[BSP_EXTRA_WLAN_WIFI_SSID_SIZE] = {};
    const bool has_station_ssid = bsp_extra_wlan_wifi_get_station_ssid(
        connected_ssid,
        sizeof(connected_ssid)
    );
    const bool connected = bsp_extra_wlan_wifi_is_connected() && has_station_ssid;
    const bool connecting = bsp_extra_wlan_wifi_is_connecting();
    const bool scanning = bsp_extra_wlan_wifi_is_scanning();
    const bool show_connected_row = has_station_ssid && (connected || connecting);

    if (show_connected_row) {
        if (connected_text != nullptr) {
            lv_obj_remove_flag(connected_text, LV_OBJ_FLAG_HIDDEN);
        }
        if (conn_btn != nullptr) {
            lv_list_set_button_text(list1, conn_btn, connected_ssid);
            lv_obj_remove_flag(conn_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(conn_btn, LV_OBJ_FLAG_CLICKABLE);
        }
        if (wifi_icon != nullptr) {
            if (connected) {
                lv_label_set_text(wifi_icon, LV_SYMBOL_OK);
                lv_obj_remove_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (spinner != nullptr) {
            if (connecting) {
                lv_obj_remove_flag(spinner, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        if (connected_text != nullptr) {
            lv_obj_add_flag(connected_text, LV_OBJ_FLAG_HIDDEN);
        }
        if (conn_btn != nullptr) {
            lv_obj_add_flag(conn_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (spinner != nullptr) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (scan_wait_spinner != nullptr) {
        if (scanning) {
            lv_obj_remove_flag(scan_wait_spinner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(scan_wait_spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (scan_btn != nullptr) {
        if (connecting || !bsp_extra_wlan_wifi_is_station_ready() ||
            bsp_extra_wlan_wifi_is_scanning()) {
            lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(scan_btn, LV_STATE_DISABLED);
        }
    }
}

void WlanPage::refreshWifiList(
    const bsp_extra_wlan_wifi_ap_t *records,
    uint16_t record_count,
    uint32_t generation
)
{
    if (records == nullptr || list1 == nullptr) {
        return;
    }

    memset(ap_info, 0, sizeof(ap_info));
    if (record_count > DISPLAY_AP_COUNT) {
        record_count = DISPLAY_AP_COUNT;
    }
    memcpy(ap_info, records, record_count * sizeof(bsp_extra_wlan_wifi_ap_t));
    sort_wifi_records_by_rssi(ap_info, record_count);
    scanned_ap_count = record_count;
    wifi_cache_generation = generation;
    wifi_cache_loaded = true;

    if (connected_text != nullptr) {
        lv_obj_del(connected_text);
        connected_text = nullptr;
    }
    if (conn_btn != nullptr) {
        lv_obj_del(conn_btn);
        conn_btn = nullptr;
        wifi_icon = nullptr;
        spinner = nullptr;
    }
    if (available_text != nullptr) {
        lv_obj_del(available_text);
        available_text = nullptr;
        scan_wait_spinner = nullptr;
    }
    for (lv_obj_t *button : wifi_btns) {
        if (button != nullptr) {
            lv_obj_del(button);
        }
    }
    wifi_btns.clear();

    connected_text = lv_list_add_text(list1, "Connected WLAN");
    lv_obj_add_style(connected_text, &style_list_text, LV_PART_MAIN);
    lv_obj_add_flag(connected_text, LV_OBJ_FLAG_HIDDEN);

    conn_btn = lv_list_add_button(list1, nullptr, "SSID");
    lv_obj_add_style(conn_btn, &style_list_btn, LV_PART_MAIN);
    settings_ui::use_ellipsis_for_button_label(conn_btn);
    lv_obj_add_flag(conn_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(conn_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(conn_btn, connected_btn_cb, LV_EVENT_SHORT_CLICKED, this);

    wifi_icon = lv_label_create(conn_btn);
    lv_label_set_text(wifi_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_26, LV_PART_MAIN);

    spinner = lv_spinner_create(conn_btn);
    lv_obj_set_size(spinner, 24, 24);
    lv_obj_set_style_arc_width(spinner, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(
        spinner,
        lv_color_hex(settings_ui::COLOR_ACCENT),
        LV_PART_INDICATOR
    );
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

    available_text = lv_list_add_text(list1, "Available WLAN");
    lv_obj_add_style(available_text, &style_list_text, LV_PART_MAIN);
    lv_obj_set_width(available_text, lv_pct(100));

    scan_wait_spinner = lv_spinner_create(available_text);
    lv_obj_set_size(scan_wait_spinner, 24, 24);
    lv_obj_align(scan_wait_spinner, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_arc_width(scan_wait_spinner, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(scan_wait_spinner, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(
        scan_wait_spinner,
        lv_color_hex(settings_ui::COLOR_ACCENT),
        LV_PART_INDICATOR
    );
    lv_obj_add_flag(scan_wait_spinner, LV_OBJ_FLAG_HIDDEN);

    for (uint16_t i = 0; i < scanned_ap_count; ++i) {
        if (ap_info[i].ssid[0] == '\0') {
            continue;
        }

        lv_obj_t *wifi_button = lv_list_add_button(list1, nullptr, ap_info[i].ssid);
        lv_obj_add_style(wifi_button, &style_list_btn, LV_PART_MAIN);
        lv_obj_add_style(wifi_button, &style_list_btn_pressed, LV_STATE_PRESSED);
        settings_ui::use_ellipsis_for_button_label(wifi_button);

        lv_obj_t *icon = lv_image_create(wifi_button);
        if (ap_info[i].rssi > -25) {
            lv_img_set_src(icon, &wifi_4);
        } else if (ap_info[i].rssi > -50) {
            lv_img_set_src(icon, &wifi_3);
        } else if (ap_info[i].rssi > -75) {
            lv_img_set_src(icon, &wifi_2);
        } else {
            lv_img_set_src(icon, &wifi_1);
        }

        ButtonUserData *user_data = static_cast<ButtonUserData *>(
            malloc(sizeof(ButtonUserData))
        );
        if (user_data == nullptr) {
            ESP_UTILS_LOGW("Allocate Wi-Fi button user data failed");
            lv_obj_del(wifi_button);
            continue;
        }
        user_data->index = i;
        user_data->page = this;
        lv_obj_add_event_cb(wifi_button, wifi_btn_cb, LV_EVENT_ALL, user_data);
        wifi_btns.push_back(wifi_button);
    }
}

void WlanPage::showPasswordInput(uint16_t index)
{
    if (index >= scanned_ap_count || page_root == nullptr || ta == nullptr || kb == nullptr) {
        return;
    }

    selected_ap = ap_info[index];
    ESP_UTILS_LOGI("Selected SSID: %s", selected_ap.ssid);
    if (selected_ap.authmode == BSP_EXTRA_WLAN_WIFI_AUTH_OPEN) {
        if (!bsp_extra_wlan_wifi_connect(&selected_ap, "")) {
            ESP_UTILS_LOGW("Open Wi-Fi connection request rejected");
        }
        refreshWifiUi();
        return;
    }

    if (password_title != nullptr) {
        lv_label_set_text_fmt(password_title, "Connect to %s", selected_ap.ssid);
        lv_obj_remove_flag(password_title, LV_OBJ_FLAG_HIDDEN);
    }
    lv_textarea_set_text(ta, "");
    lv_obj_add_flag(list1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(kb, ta);
}

void WlanPage::submitPassword()
{
    if (ta == nullptr || page_root == nullptr) {
        return;
    }

    strlcpy(wifi_pwd, lv_textarea_get_text(ta), sizeof(wifi_pwd));
    const size_t password_length = strlen(wifi_pwd);
    ESP_UTILS_LOGI("Wi-Fi password length: %u", static_cast<unsigned>(password_length));

    const bool password_valid =
        selected_ap.authmode == BSP_EXTRA_WLAN_WIFI_AUTH_OPEN ||
        password_length >= 8;
    const bool accepted = password_valid &&
                          bsp_extra_wlan_wifi_connect(&selected_ap, wifi_pwd);

    if (kb != nullptr) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (ta != nullptr) {
        lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
    }
    if (password_title != nullptr) {
        lv_obj_add_flag(password_title, LV_OBJ_FLAG_HIDDEN);
    }
    if (!accepted) {
        ESP_UTILS_LOGW("Wi-Fi connection request rejected");
    }
    lv_obj_remove_flag(list1, LV_OBJ_FLAG_HIDDEN);
    refreshWifiUi();
}

void WlanPage::wifi_event_cb(void *context, bsp_extra_wlan_wifi_event_t event)
{
    (void)event;
    WlanPage *page = static_cast<WlanPage *>(context);
    if (page == nullptr || !page->page_active) {
        return;
    }
    lv_async_call(refresh_wifi_ui_async_cb, page);
}

void WlanPage::refresh_wifi_ui_async_cb(void *context)
{
    WlanPage *page = static_cast<WlanPage *>(context);
    if (page != nullptr && page->page_active) {
        page->refreshWifiUi();
    }
}

void WlanPage::wifi_btn_cb(lv_event_t *event)
{
    ButtonUserData *user_data = static_cast<ButtonUserData *>(lv_event_get_user_data(event));
    if (user_data == nullptr) {
        return;
    }
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        free(user_data);
        return;
    }

    WlanPage *page = user_data->page;
    if (page != nullptr && page->page_active &&
        lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
        page->showPasswordInput(user_data->index);
    }
}

void WlanPage::scan_btn_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (!bsp_extra_wlan_wifi_request_scan()) {
        ESP_UTILS_LOGW("Manual Wi-Fi scan request rejected");
        return;
    }
    ESP_UTILS_LOGI("Manual Wi-Fi scan requested");
}

void WlanPage::connected_btn_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
        return;
    }

    WlanPage *page = static_cast<WlanPage *>(lv_event_get_user_data(event));
    if (!bsp_extra_wlan_wifi_clear_saved_config()) {
        ESP_UTILS_LOGW("Clear saved Wi-Fi config failed");
    }
    if (page != nullptr) {
        page->refreshWifiUi();
    }
}

void WlanPage::kb_event_cb(lv_event_t *event)
{
    WlanPage *page = static_cast<WlanPage *>(lv_event_get_user_data(event));
    if (page == nullptr) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CANCEL) {
        if (page->kb != nullptr) {
            lv_obj_add_flag(page->kb, LV_OBJ_FLAG_HIDDEN);
        }
        if (page->ta != nullptr) {
            lv_obj_add_flag(page->ta, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_text(page->ta, "");
        }
        if (page->password_title != nullptr) {
            lv_obj_add_flag(page->password_title, LV_OBJ_FLAG_HIDDEN);
        }
        if (page->list1 != nullptr) {
            lv_obj_remove_flag(page->list1, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (code == LV_EVENT_READY) {
        page->submitPassword();
    }
}

void WlanPage::ta_event_cb(lv_event_t *event)
{
    WlanPage *page = static_cast<WlanPage *>(lv_event_get_user_data(event));
    lv_obj_t *textarea = static_cast<lv_obj_t *>(lv_event_get_target(event));
    if (page == nullptr || textarea == nullptr) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_FOCUSED) {
        if (page->kb != nullptr) {
            lv_obj_remove_flag(page->kb, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(page->kb, textarea);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        if (page->list1 != nullptr) {
            lv_obj_remove_flag(page->list1, LV_OBJ_FLAG_HIDDEN);
        }
        if (page->kb != nullptr) {
            lv_obj_add_flag(page->kb, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(page->kb, nullptr);
        }
        lv_obj_add_flag(textarea, LV_OBJ_FLAG_HIDDEN);
        if (page->password_title != nullptr) {
            lv_obj_add_flag(page->password_title, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

} // namespace esp_brookesia::apps
