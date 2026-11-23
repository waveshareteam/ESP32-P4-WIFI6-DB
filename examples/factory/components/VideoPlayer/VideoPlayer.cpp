/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VideoPlayer.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "bsp/touch.h"
#include "esp_lib_utils.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include <inttypes.h>
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:VideoPlayer"

LV_IMG_DECLARE(img_app_vedioplayer);

#define DISPLAY_WIDTH  BSP_LCD_H_RES
#define DISPLAY_HEIGHT BSP_LCD_V_RES
#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB565
#define VIDEO_JPEG_OUTPUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#define VIDEO_PPA_COLOR_MODE    PPA_SRM_COLOR_MODE_RGB565
#define VIDEO_BYTES_PER_PIXEL   2
#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define VIDEO_JPEG_OUTPUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#define VIDEO_PPA_COLOR_MODE    PPA_SRM_COLOR_MODE_RGB888
#define VIDEO_BYTES_PER_PIXEL   3
#else
#error "Unsupported BSP LCD color format"
#endif

namespace esp_brookesia::apps
{

    VideoPlayer *VideoPlayer::_instance = nullptr;

    VideoPlayer *VideoPlayer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new VideoPlayer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    VideoPlayer::VideoPlayer(bool use_status_bar, bool use_navigation_bar) : App("VideoPlayer", &img_app_vedioplayer, true, use_status_bar, use_navigation_bar), sd_mounted(false)
    {
        display = nullptr;
        ppa_srm_handle = nullptr;
        touch_handle = nullptr;
        jpeg_handle = nullptr;
        jpeg_output_buffer = nullptr;
        jpeg_output_buffer_size = 0;
        avi_player_handle = nullptr;
        play_task_handle = nullptr;
        close_wait_task = nullptr;
        is_playing = false;
        is_paused = false;
        loop_playback = false;
        dummy_enabled = false;
        lvgl_paused = false;
        touch_active = false;
        avi_file_list = nullptr;
        avi_file_count = 0;
        last_video_width = 0;
        last_video_height = 0;
    }

    VideoPlayer::~VideoPlayer()
    {
        close();
    }

    bool VideoPlayer::run(void)
    {
        ESP_UTILS_LOGD("Run");
        is_paused = false;
        bsp_display_lock(-1);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
        status_label = lv_label_create(lv_scr_act());
        lv_label_set_text(status_label, sd_mounted ? "loading files..." : "sd error");
        lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(status_label, DISPLAY_WIDTH);
        lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
        bsp_display_unlock();

        if (!sd_mounted)
        {
            return true;
        }

        if (getAviFileList("/sdcard/avi") != ESP_OK || avi_file_count == 0)
        {
            bsp_display_lock(-1);
            lv_label_set_text(status_label, "avi error");
            bsp_display_unlock();
            return true;
        }

        bsp_display_lock(-1);
        lv_obj_del(status_label);
        status_label = nullptr;
        bsp_display_unlock();

        return startPlaybackTask();
    }

    bool VideoPlayer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool VideoPlayer::close()
    {
        ESP_UTILS_LOGD("Close");
        if (!cancel_close_on_gui_task()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Cancel pending close request failed");
            return false;
        }

        if (!stopPlaybackTask(pdMS_TO_TICKS(2000))) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Playback task did not stop before close");
            return false;
        }

        deinitJpegDecoder();
        releaseJpegOutputBuffer();
        deinitDisplayBypass();

        bsp_display_lock(-1);
        if (status_label)
        {
            lv_obj_del(status_label);
            status_label = nullptr;
        }
        bsp_display_unlock();

        if (avi_file_list)
        {
            for (int i = 0; i < avi_file_count; i++)
            {
                free(avi_file_list[i]);
            }
            free(avi_file_list);
            avi_file_list = nullptr;
            avi_file_count = 0;
        }

        return true;
    }

    bool VideoPlayer::schedule_close_on_gui_task()
    {
        if (close_request_pending.exchange(true)) {
            return true;
        }

        if (!bsp_display_lock(-1)) {
            close_request_pending.store(false);
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Acquire display lock to queue close request failed");
            return false;
        }

        if (!close_request_pending.load()) {
            bsp_display_unlock();
            return true;
        }

        const lv_result_t ret = lv_async_call(close_on_gui_task, this);
        bsp_display_unlock();
        if (ret != LV_RESULT_OK) {
            close_request_pending.store(false);
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Queue close request on GUI task failed");
            return false;
        }

        return true;
    }

    bool VideoPlayer::cancel_close_on_gui_task()
    {
        if (!close_request_pending.load()) {
            return true;
        }

        if (!bsp_display_lock(-1)) {
            return false;
        }

        if (!close_request_pending.load()) {
            bsp_display_unlock();
            return true;
        }

        (void)lv_async_call_cancel(close_on_gui_task, this);
        close_request_pending.store(false);
        bsp_display_unlock();
        return true;
    }

    bool VideoPlayer::init()
    {
        ESP_UTILS_LOGD("Init");
        if (bsp_sdcard_mount() == ESP_OK)
        {
            sd_mounted = true;
            ESP_UTILS_LOGD("SD card mounted successfully");
        }
        else
        {
            sd_mounted = false;
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Failed to mount SD card");
        }
        return true;
    }

    bool VideoPlayer::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool VideoPlayer::pause()
    {
        ESP_UTILS_LOGD("Pause");
        return stopPlaybackTask(pdMS_TO_TICKS(2000));
    }

    bool VideoPlayer::resume()
    {
        ESP_UTILS_LOGD("Resume");
        if (!sd_mounted || !avi_file_list || avi_file_count == 0) {
            return true;
        }
        return startPlaybackTask();
    }

    bool VideoPlayer::startPlaybackTask()
    {
        if (play_task_handle) {
            return true;
        }
        if (!avi_file_list || avi_file_count == 0) {
            return false;
        }

        loop_playback = true;
        is_paused = false;

        // AVI parsing, JPEG decoding, and PPA blitting share this task.
        BaseType_t ret = xTaskCreatePinnedToCore(
            playTask,
            "avi_play_task",
            32768,
            this,
            7,
            &play_task_handle,
            0
        );
        if (ret != pdPASS) {
            loop_playback = false;
            play_task_handle = nullptr;
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Create playback task failed");
            return false;
        }

        return true;
    }

    bool VideoPlayer::stopPlaybackTask(TickType_t timeout)
    {
        loop_playback = false;
        is_paused = false;

        if (!play_task_handle) {
            return true;
        }
        if (xTaskGetCurrentTaskHandle() == play_task_handle) {
            return false;
        }

        close_wait_task = xTaskGetCurrentTaskHandle();
        if (!play_task_handle) {
            close_wait_task = nullptr;
            return true;
        }

        if (avi_player_handle) {
            avi_player_play_stop(avi_player_handle);
        }

        bool stopped = ulTaskNotifyTake(pdTRUE, timeout) != 0;
        close_wait_task = nullptr;

        if (!stopped || play_task_handle) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Timed out waiting for playback task");
            return false;
        }

        return true;
    }

    esp_err_t VideoPlayer::getAviFileList(const char *dir_path)
    {
        DIR *dir = opendir(dir_path);
        if (!dir)
            return ESP_FAIL;

        struct dirent *entry;
        int count = 0;

        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type == DT_REG)
            {
                char *ext = strrchr(entry->d_name, '.');
                if (ext && strcasecmp(ext, ".avi") == 0)
                {
                    count++;
                }
            }
        }

        if (count == 0)
        {
            closedir(dir);
            return ESP_FAIL;
        }

        avi_file_list = (char **)malloc(sizeof(char *) * count);
        if (!avi_file_list)
        {
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }

        avi_file_count = count;
        int loaded_count = 0;
        rewinddir(dir);

        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type == DT_REG)
            {
                char *ext = strrchr(entry->d_name, '.');
                if (ext && strcasecmp(ext, ".avi") == 0)
                {
                    size_t len = strlen(dir_path) + strlen(entry->d_name) + 2;
                    char *full_path = (char *)malloc(len);
                    if (!full_path) {
                        for (int i = 0; i < loaded_count; i++) {
                            free(avi_file_list[i]);
                        }
                        free(avi_file_list);
                        avi_file_list = nullptr;
                        avi_file_count = 0;
                        closedir(dir);
                        return ESP_ERR_NO_MEM;
                    }
                    snprintf(full_path, len, "%s/%s", dir_path, entry->d_name);
                    avi_file_list[loaded_count++] = full_path;
                }
            }
        }

        closedir(dir);
        return ESP_OK;
    }

    esp_err_t VideoPlayer::initDisplayBypass()
    {
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
        data_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
        data_cache_line_size = 64;
#endif

        display = lv_display_get_default();
        ESP_RETURN_ON_FALSE(display != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LVGL display is not ready");

        if (ppa_srm_handle == nullptr) {
            ppa_client_config_t ppa_srm_config = {
                .oper_type = PPA_OPERATION_SRM,
            };
            ESP_RETURN_ON_ERROR(ppa_register_client(&ppa_srm_config, &ppa_srm_handle), ESP_UTILS_LOG_TAG, "Register PPA SRM failed");
        }

        ESP_RETURN_ON_ERROR(bsp_touch_new(NULL, &touch_handle), ESP_UTILS_LOG_TAG, "Create touch handle failed");

        ESP_RETURN_ON_ERROR(esp_lv_adapter_set_dummy_draw(display, true), ESP_UTILS_LOG_TAG, "Enable dummy draw failed");
        dummy_enabled = true;

        ESP_RETURN_ON_ERROR(esp_lv_adapter_pause(-1), ESP_UTILS_LOG_TAG, "Pause LVGL failed");
        lvgl_paused = true;
        resetVideoPerfStats();

        return ESP_OK;
    }

    void VideoPlayer::deinitDisplayBypass()
    {
        reportVideoPerfStats(esp_timer_get_time(), true);

        if (lvgl_paused) {
            esp_lv_adapter_resume();
            lvgl_paused = false;
        }

        if (dummy_enabled && display) {
            esp_lv_adapter_set_dummy_draw(display, false);
            dummy_enabled = false;
        }

        if (touch_handle) {
            // bsp_touch_new() returns the BSP-owned singleton that is also used
            // by the LVGL input device.  This component only borrows it; deleting
            // it here leaves LVGL with a dangling get_xy callback.
            touch_handle = nullptr;
        }

        touch_active = false;
        last_video_width = 0;
        last_video_height = 0;
    }

    esp_err_t VideoPlayer::initJpegDecoder()
    {
        if (jpeg_handle != nullptr)
            return ESP_OK;

        jpeg_decode_engine_cfg_t config = {
            .intr_priority = 0,
            .timeout_ms = 100,
        };

        return jpeg_new_decoder_engine(&config, &jpeg_handle);
    }

    void VideoPlayer::deinitJpegDecoder()
    {
        if (jpeg_handle)
        {
            jpeg_del_decoder_engine(jpeg_handle);
            jpeg_handle = nullptr;
        }
    }

    esp_err_t VideoPlayer::ensureJpegOutputBuffer(size_t out_len)
    {
        if (out_len == 0) {
            return ESP_ERR_INVALID_SIZE;
        }

        if (jpeg_output_buffer && jpeg_output_buffer_size >= out_len) {
            return ESP_OK;
        }

        releaseJpegOutputBuffer();
        jpeg_decode_memory_alloc_cfg_t mem_cfg = {
            .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
        };
        size_t allocated_size = 0;
        jpeg_output_buffer = static_cast<uint8_t *>(
            jpeg_alloc_decoder_mem(out_len, &mem_cfg, &allocated_size));
        if (!jpeg_output_buffer) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG output buffer allocation failed: %u bytes", static_cast<unsigned>(out_len));
            return ESP_ERR_NO_MEM;
        }

        jpeg_output_buffer_size = allocated_size;
        return ESP_OK;
    }

    void VideoPlayer::releaseJpegOutputBuffer()
    {
        if (jpeg_output_buffer) {
            heap_caps_free(jpeg_output_buffer);
            jpeg_output_buffer = nullptr;
            jpeg_output_buffer_size = 0;
        }
    }

    void VideoPlayer::resetVideoPerfStats()
    {
        perf_frame_count = 0;
        perf_window_start_us = esp_timer_get_time();
        perf_jpeg_total_us = 0;
        perf_ppa_total_us = 0;
        perf_lcd_total_us = 0;
        perf_jpeg_max_us = 0;
        perf_ppa_max_us = 0;
        perf_lcd_max_us = 0;
    }

    void VideoPlayer::reportVideoPerfStats(int64_t now_us, bool force)
    {
        if (perf_frame_count == 0 || perf_window_start_us == 0) {
            return;
        }

        const uint64_t elapsed_us = static_cast<uint64_t>(now_us - perf_window_start_us);
        if (!force && elapsed_us < 1000000ULL) {
            return;
        }

        const uint32_t fps_x100 = static_cast<uint32_t>(
            (static_cast<uint64_t>(perf_frame_count) * 100000000ULL) /
            (elapsed_us ? elapsed_us : 1ULL));
        const uint32_t avg_jpeg_us = static_cast<uint32_t>(perf_jpeg_total_us / perf_frame_count);
        const uint32_t avg_ppa_us = static_cast<uint32_t>(perf_ppa_total_us / perf_frame_count);
        const uint32_t avg_lcd_us = static_cast<uint32_t>(perf_lcd_total_us / perf_frame_count);

        ESP_LOGI(ESP_UTILS_LOG_TAG,
                 "Video perf: fps=%" PRIu32 ".%02" PRIu32 ", frames=%" PRIu32
                 ", JPEG avg/max=%" PRIu32 "/%" PRIu32 " us"
                 ", PPA avg/max=%" PRIu32 "/%" PRIu32 " us"
                 ", LCD avg/max=%" PRIu32 "/%" PRIu32 " us",
                 fps_x100 / 100, fps_x100 % 100, perf_frame_count,
                 avg_jpeg_us, perf_jpeg_max_us,
                 avg_ppa_us, perf_ppa_max_us,
                 avg_lcd_us, perf_lcd_max_us);

        resetVideoPerfStats();
    }

    void VideoPlayer::videoCallback(frame_data_t *data, void *arg)
    {
        if (!data || !data->data || data->data_bytes == 0)
            return;

        VideoPlayer *self = static_cast<VideoPlayer *>(arg);

        if (self->initJpegDecoder() != ESP_OK || !self->dummy_enabled || !self->ppa_srm_handle)
            return;

        // Decode each MJPEG frame with the ESP32-P4 JPEG hardware into PSRAM,
        // then use PPA SRM to scale it into the next LCD frame buffer.
        if (data->data_bytes > UINT32_MAX) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG frame is too large: %u bytes", static_cast<unsigned>(data->data_bytes));
            return;
        }

        jpeg_decode_picture_info_t info = {};
        esp_err_t ret = jpeg_decoder_get_info(
            data->data,
            static_cast<uint32_t>(data->data_bytes),
            &info);
        if (ret != ESP_OK || info.width == 0 || info.height == 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG header parse failed: %s", esp_err_to_name(ret));
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Invalid JPEG frame size: %ux%u", info.width, info.height);
            return;
        }

        uint32_t decode_mcu_width = 8;
        uint32_t decode_mcu_height = 8;
        switch (info.sample_method) {
        case JPEG_DOWN_SAMPLING_YUV422:
            decode_mcu_width = 16;
            break;
        case JPEG_DOWN_SAMPLING_YUV420:
            decode_mcu_width = 16;
            decode_mcu_height = 16;
            break;
        case JPEG_DOWN_SAMPLING_YUV444:
        case JPEG_DOWN_SAMPLING_GRAY:
            break;
        default:
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Unsupported JPEG sampling method: %d", info.sample_method);
            return;
        }

        const uint32_t decode_width = ALIGN_UP(info.width, decode_mcu_width);
        const uint32_t decode_height = ALIGN_UP(info.height, decode_mcu_height);
        const size_t decode_output_size =
            static_cast<size_t>(decode_width) * decode_height * VIDEO_BYTES_PER_PIXEL;
        if (self->ensureJpegOutputBuffer(decode_output_size) != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Prepare JPEG output buffer failed");
            return;
        }

        const jpeg_decode_cfg_t decode_config = {
            .output_format = VIDEO_JPEG_OUTPUT_FORMAT,
            .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
            .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
        };
        uint32_t decoded_output_size = 0;
        const int64_t jpeg_start_us = esp_timer_get_time();
        ret = jpeg_decoder_process(
            self->jpeg_handle,
            &decode_config,
            data->data,
            static_cast<uint32_t>(data->data_bytes),
            self->jpeg_output_buffer,
            static_cast<uint32_t>(self->jpeg_output_buffer_size),
            &decoded_output_size);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG hardware decode failed: %s", esp_err_to_name(ret));
            return;
        }
        if (decoded_output_size < decode_output_size) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG output is smaller than expected: %u < %u",
                     decoded_output_size, static_cast<unsigned>(decode_output_size));
            return;
        }
        const uint32_t jpeg_elapsed_us = static_cast<uint32_t>(esp_timer_get_time() - jpeg_start_us);

        uint32_t crop_x = 0;
        uint32_t crop_y = 0;
        uint32_t crop_w = info.width;
        uint32_t crop_h = info.height;

        if (static_cast<uint64_t>(info.width) * DISPLAY_HEIGHT >
                static_cast<uint64_t>(info.height) * DISPLAY_WIDTH) {
            crop_w = static_cast<uint32_t>(
                (static_cast<uint64_t>(info.height) * DISPLAY_WIDTH) / DISPLAY_HEIGHT
            );
            crop_x = (info.width - crop_w) / 2;
        } else {
            crop_h = static_cast<uint32_t>(
                (static_cast<uint64_t>(info.width) * DISPLAY_HEIGHT) / DISPLAY_WIDTH
            );
            crop_y = (info.height - crop_h) / 2;
        }

        crop_w = crop_w ? crop_w : 1;
        crop_h = crop_h ? crop_h : 1;
        float scale_x = static_cast<float>(DISPLAY_WIDTH) / static_cast<float>(crop_w);
        float scale_y = static_cast<float>(DISPLAY_HEIGHT) / static_cast<float>(crop_h);

        if (self->last_video_width != info.width || self->last_video_height != info.height) {
            ESP_LOGI(ESP_UTILS_LOG_TAG, "Cover video frame: %ux%u, crop %u,%u %ux%u",
                     info.width, info.height, crop_x, crop_y, crop_w, crop_h);
            self->last_video_width = info.width;
            self->last_video_height = info.height;
        }

        void *lcd_buffer = esp_lv_adapter_dummy_draw_get_free_buf(self->display);
        if (!lcd_buffer) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "No free LCD frame buffer");
            return;
        }

        ppa_srm_oper_config_t srm_config = {};
        srm_config.in.buffer = self->jpeg_output_buffer;
        srm_config.in.pic_w = decode_width;
        srm_config.in.pic_h = decode_height;
        srm_config.in.block_w = crop_w;
        srm_config.in.block_h = crop_h;
        srm_config.in.block_offset_x = crop_x;
        srm_config.in.block_offset_y = crop_y;
        srm_config.in.srm_cm = VIDEO_PPA_COLOR_MODE;

        srm_config.out.buffer = lcd_buffer;
        srm_config.out.buffer_size = ALIGN_UP(DISPLAY_WIDTH * DISPLAY_HEIGHT * VIDEO_BYTES_PER_PIXEL, self->data_cache_line_size);
        srm_config.out.pic_w = DISPLAY_WIDTH;
        srm_config.out.pic_h = DISPLAY_HEIGHT;
        srm_config.out.block_offset_x = 0;
        srm_config.out.block_offset_y = 0;
        srm_config.out.srm_cm = VIDEO_PPA_COLOR_MODE;

        srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
        srm_config.scale_x = scale_x;
        srm_config.scale_y = scale_y;
        srm_config.mirror_x = 0;
        srm_config.mirror_y = 0;
        srm_config.rgb_swap = 0;
        srm_config.byte_swap = 0;
        srm_config.mode = PPA_TRANS_MODE_BLOCKING;

        const int64_t ppa_start_us = esp_timer_get_time();
        ret = ppa_do_scale_rotate_mirror(self->ppa_srm_handle, &srm_config);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "PPA scale failed: %s", esp_err_to_name(ret));
            return;
        }
        const uint32_t ppa_elapsed_us = static_cast<uint32_t>(esp_timer_get_time() - ppa_start_us);

        const int64_t lcd_start_us = esp_timer_get_time();
        ret = esp_lv_adapter_dummy_draw_flush_buf(self->display, lcd_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Dummy draw flush failed: %s", esp_err_to_name(ret));
            return;
        }
        const uint32_t lcd_elapsed_us = static_cast<uint32_t>(esp_timer_get_time() - lcd_start_us);

        self->perf_frame_count++;
        self->perf_jpeg_total_us += jpeg_elapsed_us;
        self->perf_ppa_total_us += ppa_elapsed_us;
        self->perf_lcd_total_us += lcd_elapsed_us;
        self->perf_jpeg_max_us = self->perf_jpeg_max_us > jpeg_elapsed_us ?
                                 self->perf_jpeg_max_us : jpeg_elapsed_us;
        self->perf_ppa_max_us = self->perf_ppa_max_us > ppa_elapsed_us ?
                                self->perf_ppa_max_us : ppa_elapsed_us;
        self->perf_lcd_max_us = self->perf_lcd_max_us > lcd_elapsed_us ?
                                self->perf_lcd_max_us : lcd_elapsed_us;
        self->reportVideoPerfStats(esp_timer_get_time(), false);
    }

    void VideoPlayer::playEndCallback(void *arg)
    {
        VideoPlayer *self = static_cast<VideoPlayer *>(arg);
        self->is_playing = false;
    }

    void VideoPlayer::close_on_gui_task(void *arg)
    {
        VideoPlayer *self = static_cast<VideoPlayer *>(arg);
        if (self == nullptr || !self->close_request_pending.exchange(false)) {
            return;
        }

        if (!self->notifyCoreClosed()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Notify core closed from GUI task failed");
        }
    }

    bool VideoPlayer::shouldExitBySwipe()
    {
        if (!touch_handle) {
            return false;
        }

        esp_lcd_touch_read_data(touch_handle);

        esp_lcd_touch_point_data_t points[1] = {};
        uint8_t point_num = 0;
        esp_err_t ret = esp_lcd_touch_get_data(touch_handle, points, &point_num, 1);

        if (ret != ESP_OK || point_num == 0) {
            touch_active = false;
            return false;
        }

        if (!touch_active) {
            touch_active = true;
            touch_start_x = points[0].x;
            touch_start_y = points[0].y;
            return false;
        }

        int dx = abs((int)points[0].x - (int)touch_start_x);
        int dy = abs((int)points[0].y - (int)touch_start_y);
        return dx > SWIPE_EXIT_THRESHOLD || dy > SWIPE_EXIT_THRESHOLD;
    }

    void VideoPlayer::playTask(void *arg)
    {
        VideoPlayer *self = static_cast<VideoPlayer *>(arg);
        bool request_close = false;

        // The AVI reader keeps a larger buffer because high-resolution MJPEG frames
        // can otherwise underflow while the PPA path is preparing the next frame.
        avi_player_config_t cfg = {
            .buffer_size = 2 * 1024 * 1024,
            .video_cb = videoCallback,
            .audio_cb = nullptr,
            .audio_set_clock_cb = nullptr,
            .avi_play_end_cb = playEndCallback,
            .priority = 7,
            .coreID = 0,
            .user_data = self,
            .stack_size = 32768,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
            .stack_in_psram = true,
#endif
        };

        if (avi_player_init(cfg, &self->avi_player_handle) != ESP_OK)
        {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "avi_player_init failed");
            self->play_task_handle = nullptr;
            if (self->close_wait_task) {
                xTaskNotifyGive(self->close_wait_task);
            }
            vTaskDelete(NULL);
            return;
        }

        if (self->initDisplayBypass() != ESP_OK)
        {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "initDisplayBypass failed");
            avi_player_deinit(self->avi_player_handle);
            self->avi_player_handle = nullptr;
            self->deinitDisplayBypass();
            self->play_task_handle = nullptr;
            if (self->close_wait_task) {
                xTaskNotifyGive(self->close_wait_task);
            }
            vTaskDelete(NULL);
            return;
        }

        // Loop through every AVI file discovered on the SD card until the app closes.
        while (self->loop_playback)
        {
            for (int i = 0; i < self->avi_file_count && self->loop_playback; i++)
            {
                while (self->is_paused && self->loop_playback)
                {
                    if (self->shouldExitBySwipe()) {
                        request_close = true;
                        self->loop_playback = false;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                if (!self->loop_playback)
                    break;

                ESP_LOGI(ESP_UTILS_LOG_TAG, "Playing AVI file: %s", self->avi_file_list[i]);
                self->is_playing = true;
                if (avi_player_play_from_file(self->avi_player_handle, self->avi_file_list[i]) != ESP_OK) {
                    ESP_LOGE(ESP_UTILS_LOG_TAG, "Failed to play file: %s", self->avi_file_list[i]);
                    self->is_playing = false;
                    continue;
                }

                while (self->is_playing && self->loop_playback)
                {
                    if (self->shouldExitBySwipe()) {
                        request_close = true;
                        self->loop_playback = false;
                        avi_player_play_stop(self->avi_player_handle);
                        self->is_playing = false;
                        break;
                    }

                    if (self->is_paused)
                    {
                        avi_player_play_stop(self->avi_player_handle);
                        self->is_playing = false;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(30));
                }
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }

        avi_player_play_stop(self->avi_player_handle);
        avi_player_deinit(self->avi_player_handle);
        self->avi_player_handle = nullptr;
        self->deinitDisplayBypass();

        self->play_task_handle = nullptr;
        if (self->close_wait_task)
        {
            xTaskNotifyGive(self->close_wait_task);
        }

        if (request_close)
        {
            if (!self->schedule_close_on_gui_task()) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Schedule close request failed");
            }
        }

        vTaskDelete(NULL);
    }

} // namespace esp_brookesia::apps
