/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "MusicPlayer.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:MusicPlayer"
#include "esp_lib_utils.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "gui_music/lv_demo_music.h"
#include "gui_music/lv_demo_music_main.h"
#include "file_iterator.h"
#include "audio_focus.hpp"

#define MUSIC_DIR BSP_SPIFFS_MOUNT_POINT "/music"

LV_IMG_DECLARE(img_app_musicplayer);

static const char *TAG = "MusicPlayer";

static bool activate_music_audio(void *)
{
    esp_err_t ret = bsp_extra_player_resume_session(pdMS_TO_TICKS(1000));
    return ret == ESP_OK || bsp_extra_player_init() == ESP_OK;
}

static bool suspend_music_audio(void *)
{
    return bsp_extra_player_suspend(pdMS_TO_TICKS(1000)) == ESP_OK;
}

namespace esp_brookesia::apps
{

    MusicPlayer *MusicPlayer::_instance = nullptr;

    MusicPlayer *MusicPlayer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new MusicPlayer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    MusicPlayer::MusicPlayer(bool use_status_bar, bool use_navigation_bar) :
                App("MusicPlayer", &img_app_musicplayer, true, use_status_bar, use_navigation_bar),
                _file_iterator(NULL)
    {
    }

    MusicPlayer::~MusicPlayer()
    {
        if (_file_iterator != nullptr) {
            // file_iterator_delete(_file_iterator);
            _file_iterator = nullptr;
        }
    }

    bool MusicPlayer::run(void)
    {
        ESP_UTILS_LOGD("Run");

        const audio_focus::callbacks focus_callbacks = {
            .activate = activate_music_audio,
            .suspend = suspend_music_audio,
        };
        if (!audio_focus::manager::instance().request(
                audio_focus::client::music, focus_callbacks))
        {
            ESP_LOGE(TAG, "Play init with SPIFFS failed");
            return false;
        }

        if (bsp_extra_file_instance_init(MUSIC_DIR, &_file_iterator) != ESP_OK)
        {
            ESP_LOGE(TAG, "bsp_extra_file_instance_init failed");
            (void)audio_focus::manager::instance().abandon(
                audio_focus::client::music
            );
            esp_err_t cleanup_ret = bsp_extra_player_del();
            if (cleanup_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to roll back audio player initialization: %s",
                         esp_err_to_name(cleanup_ret));
            }
            return false;
        }

        lv_demo_music(lv_scr_act(), _file_iterator);
        return true;
    }

    bool MusicPlayer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool MusicPlayer::close(void)
    {
        ESP_UTILS_LOGD("Close");
        bool success = audio_focus::manager::instance().abandon(
                           audio_focus::client::music
                       );
        esp_err_t ret = bsp_extra_player_del();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Delete audio player failed: %s", esp_err_to_name(ret));
            success = false;
        }

        if (_file_iterator != nullptr) {
            // file_iterator_delete(_file_iterator);
            _file_iterator = nullptr;
        }
        return success;
    }

    bool MusicPlayer::init()
    {
        ESP_UTILS_LOGD("Init");
        return true;
    }

    bool MusicPlayer::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool MusicPlayer::pause()
    {
        ESP_UTILS_LOGD("Pause");
        lv_demo_music_exit_pause();
        return true;
    }

    bool MusicPlayer::resume()
    {
        ESP_UTILS_LOGD("Resume");
        const audio_focus::callbacks focus_callbacks = {
            .activate = activate_music_audio,
            .suspend = suspend_music_audio,
        };
        return audio_focus::manager::instance().request(
                   audio_focus::client::music, focus_callbacks
               );
    }

} // namespace esp_brookesia::apps
