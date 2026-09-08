#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/ppa.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"
#include "sdkconfig.h"
#include <dirent.h>
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include "avi_player.h"
#include "driver/jpeg_decode.h"
#include <stdint.h>

namespace esp_brookesia::apps
{

    class VideoPlayer : public systems::phone::App
    {
    public:
        static VideoPlayer *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
        ~VideoPlayer();

    protected:
        VideoPlayer(bool use_status_bar, bool use_navigation_bar);
        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static VideoPlayer *_instance;

        static constexpr int SWIPE_EXIT_THRESHOLD = 180;

        lv_display_t *display = nullptr;
        ppa_client_handle_t ppa_srm_handle = nullptr;
        esp_lcd_touch_handle_t touch_handle = nullptr;
        uint8_t *jpeg_output_buffer = nullptr;
        size_t jpeg_output_buffer_size = 0;
        size_t data_cache_line_size = 64;
        uint32_t last_video_width = 0;
        uint32_t last_video_height = 0;
        std::atomic_bool loop_playback{true};
        std::atomic_bool is_playing{false};
        std::atomic_bool is_paused{false};
        bool sd_mounted = false;
        bool dummy_enabled = false;
        bool lvgl_paused = false;
        bool touch_active = false;
        uint16_t touch_start_x = 0;
        uint16_t touch_start_y = 0;
        uint32_t perf_frame_count = 0;
        int64_t perf_window_start_us = 0;
        uint64_t perf_jpeg_total_us = 0;
        uint64_t perf_ppa_total_us = 0;
        uint64_t perf_lcd_total_us = 0;
        uint32_t perf_jpeg_max_us = 0;
        uint32_t perf_ppa_max_us = 0;
        uint32_t perf_lcd_max_us = 0;

        TaskHandle_t play_task_handle = nullptr;
        TaskHandle_t close_wait_task = nullptr;
        std::atomic_bool close_request_pending = false;

        avi_player_handle_t avi_player_handle;
        jpeg_decoder_handle_t jpeg_handle = nullptr;

        char **avi_file_list = nullptr;
        int avi_file_count = 0;

        lv_obj_t *status_label = nullptr;

        bool startPlaybackTask();
        bool stopPlaybackTask(TickType_t timeout);
        bool schedule_close_on_gui_task();
        bool cancel_close_on_gui_task();
        esp_err_t getAviFileList(const char *dir_path);
        esp_err_t initDisplayBypass();
        void deinitDisplayBypass();
        esp_err_t initJpegDecoder();
        void deinitJpegDecoder();
        esp_err_t ensureJpegOutputBuffer(size_t out_len);
        void releaseJpegOutputBuffer();
        void resetVideoPerfStats();
        void reportVideoPerfStats(int64_t now_us, bool force);
        bool shouldExitBySwipe();

        static void playTask(void *arg);
        static void close_on_gui_task(void *arg);
        static void videoCallback(frame_data_t *data, void *arg);
        static void playEndCallback(void *arg);
    };

} // namespace esp_brookesia::apps
