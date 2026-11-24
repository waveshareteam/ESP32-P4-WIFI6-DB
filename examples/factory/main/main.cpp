/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// #include "esp_3"
#include <boost/chrono/duration.hpp>
#include <boost/config.hpp>
#include <boost/thread.hpp>

#include "bsp/display.h"
#include "esp_brookesia.hpp"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

#include "esp_ota_ops.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include <atomic>

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "bsp_wlan_wifi.h"
#include "phone/stylesheets/esp_brookesia_phone_stylesheets.hpp"

#include "Drawpanel.hpp"
#include "SpecAnalyzer.hpp"
#include "MusicPlayer.hpp"
#include "Settings.hpp"
#include "Camera.hpp"
#include "VideoPlayer.hpp"
#include "GpioMonitor.hpp"
#include "XiaozhiApp.hpp"
#include "esp_brookesia_app_calculator.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

constexpr bool EXAMPLE_SHOW_MEM_INFO = true;

static std::atomic<int> s_wifi_icon_state{
    static_cast<int>(StatusBar::WifiState::DISCONNECTED)
};
static int s_applied_wifi_icon_state = -1;
static constexpr uint32_t wifi_icon_refresh_interval_ms = 200;

static void update_wifi_icon_timer_cb(lv_timer_t *timer)
{
    if (timer == nullptr) {
        return;
    }

    Phone *phone = static_cast<Phone *>(timer->user_data);
    if (phone == nullptr) {
        return;
    }

    StatusBar *status_bar = phone->getDisplay().getStatusBar();
    if (status_bar == nullptr) {
        return;
    }

    const int state = s_wifi_icon_state.load();
    if (state == s_applied_wifi_icon_state) {
        return;
    }
    if (!status_bar->setWifiIconState(static_cast<StatusBar::WifiState>(state))) {
        ESP_UTILS_LOGW("Set Wi-Fi icon state failed");
        return;
    }
    s_applied_wifi_icon_state = state;
}

static void wifi_status_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data
)
{
    (void)arg;
    (void)event_data;
    int state;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        state = static_cast<int>(StatusBar::WifiState::SIGNAL_3);
    } else if (event_base == WIFI_EVENT &&
               (event_id == WIFI_EVENT_STA_DISCONNECTED || event_id == WIFI_EVENT_STA_STOP)) {
        state = static_cast<int>(StatusBar::WifiState::DISCONNECTED);
    } else {
        return;
    }

    s_wifi_icon_state.store(state);
}

extern "C" void app_main(void)
{
    esp_log_level_set("rpc_rsp", ESP_LOG_ERROR);
    const esp_partition_t *update_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL
    );
    ESP_UTILS_CHECK_NULL_EXIT(update_partition, "Factory partition not found");
    ESP_LOGI(ESP_UTILS_LOG_TAG, "Switch to partition factory");

    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot == nullptr || boot->address != update_partition->address) {
      ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));
      esp_restart();
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if (!bsp_extra_wlan_wifi_init()) {
        ESP_UTILS_LOGW("Start Wi-Fi service failed");
    }

    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_LOGI(ESP_UTILS_LOG_TAG, "SPIFFS mount successfully");

    ESP_ERROR_CHECK(bsp_extra_codec_init());

    ESP_ERROR_CHECK(bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL));

    ESP_UTILS_LOGI("Display ESP-Brookesia phone demo");

    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0
        }
    };

    // Keep LVGL on core 1 with a larger PSRAM-backed stack; high-resolution UI and
    // app switching can otherwise starve the GUI task or overflow the default stack.
    cfg.lv_adapter_cfg.task_stack_size = 16 * 1024;
    cfg.lv_adapter_cfg.task_priority = 10;
    cfg.lv_adapter_cfg.task_core_id = 1;
    cfg.lv_adapter_cfg.stack_in_psram = true;

    /* Configure display */
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    ESP_UTILS_CHECK_NULL_EXIT(disp, "Start display failed");
    bsp_display_backlight_off();

    /* Configure GUI lock */
    LvLock::registerCallbacks([](int timeout_ms) {
        bool locked = bsp_display_lock(timeout_ms);
        ESP_UTILS_CHECK_FALSE_RETURN(locked, false, "Lock failed (timeout_ms: %d)", timeout_ms);
        return true;
    }, []() {
        bsp_display_unlock();
        return true;
    });

    /* Create a phone object */
    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");

    using namespace esp_brookesia::apps;
    {
        // When operating on non-GUI tasks, should acquire a lock before operating on LVGL
        LvLockGuard gui_guard;

        /* Begin the phone */
        const auto &phone_stylesheet =
#if CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A || CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A
            ESP_BROOKESIA_PHONE_800_1280_DARK_STYLESHEET();
#elif CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A || CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A
            ESP_BROOKESIA_PHONE_720_1280_DARK_STYLESHEET();
#else
            ESP_BROOKESIA_PHONE_DEFAULT_DARK_STYLESHEET();
#endif
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(phone_stylesheet), "Add phone stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(phone_stylesheet), "Activate phone stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
        StatusBar *status_bar = phone->getDisplay().getStatusBar();
        if (status_bar != nullptr) {
            status_bar->hideBatteryIcon();
            status_bar->hideBatteryPercent();
        }
        // assert(phone->getDisplay().showContainerBorder() && "Show container border failed");

        /* Init and install apps from registry */
        std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps), "Install app registry failed");
        Calculator *calculator = Calculator::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(calculator), "Start Calculator failed");
        Drawpanel *draw_panel = Drawpanel::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(draw_panel), "Start Drawpanel failed");
        SpecAnalyzer *spec_analyzer = SpecAnalyzer::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(spec_analyzer), "Start SpecAnalyzer failed");
        MusicPlayer *music_player = MusicPlayer::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(music_player), "Start MusicPlayer failed");
        Camera *camera_app = Camera::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(camera_app), "Start Camera failed");
        VideoPlayer *video_player_app = VideoPlayer::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(video_player_app), "Start VideoPlayer failed");
        Settings *settings = Settings::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(settings), "Start Settings failed");
        XiaozhiApp *xiaozhi_app = XiaozhiApp::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(xiaozhi_app), "Install Xiaozhi app failed");
        GpioMonitor *gpio_monitor_app = GpioMonitor::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(gpio_monitor_app), "Install GpioMonitor failed");

        bsp_display_backlight_on();

        ESP_ERROR_CHECK(esp_event_handler_register(
            WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_status_event_handler, nullptr
        ));
        ESP_ERROR_CHECK(esp_event_handler_register(
            WIFI_EVENT, WIFI_EVENT_STA_STOP, wifi_status_event_handler, nullptr
        ));
        ESP_ERROR_CHECK(esp_event_handler_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_status_event_handler, nullptr
        ));

        s_wifi_icon_state.store(
            bsp_extra_wlan_wifi_is_connected() ?
            static_cast<int>(StatusBar::WifiState::SIGNAL_3) :
            static_cast<int>(StatusBar::WifiState::DISCONNECTED)
        );

        lv_timer_create(update_wifi_icon_timer_cb, wifi_icon_refresh_interval_ms, phone);

        lv_timer_create([](lv_timer_t *t) {
            time_t now;
            struct tm timeinfo;
            Phone *phone = (Phone *)t->user_data;

            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

            time(&now);
            localtime_r(&now, &timeinfo);

            ESP_UTILS_CHECK_FALSE_EXIT(
                phone->getDisplay().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
                "Refresh status bar failed"
            );
        }, 1000, phone);
    }

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[128];    /* Make sure buffer is enough for `sprintf` */
            size_t internal_free = 0;
            size_t internal_total = 0;
            size_t external_free = 0;
            size_t external_total = 0;

            while (1) {
                internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
                external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
                snprintf(
                    buffer,
                    sizeof(buffer),
                    "\t           Biggest /     Free /    Total\n"
                    "\t  SRAM : [%8zu / %8zu / %8zu]\n"
                    "\t PSRAM : [%8zu / %8zu / %8zu]",
                    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total
                );
                ESP_UTILS_LOGI("\n%s", buffer);

                {
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getRecentsScreen()->setMemoryLabel(
                            internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024
                        ), "Set memory label failed"
                    );
                }

                boost::this_thread::sleep_for(boost::chrono::seconds(5));
            }
        }).detach();
    }
}
