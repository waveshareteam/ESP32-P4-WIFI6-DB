#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "driver/ppa.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

namespace esp_brookesia::apps
{

    class Camera : public systems::phone::App
    {
    public:
        static Camera *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
        ~Camera();

    protected:
        Camera(bool use_status_bar, bool use_navigation_bar);

        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static Camera *_instance;

        static constexpr int CAMERA_BUFFER_COUNT = 2;
        static constexpr int PREVIEW_TASK_STACK_SIZE = 8 * 1024;
        static constexpr int PREVIEW_TASK_PRIORITY = 8;
        static constexpr int PREVIEW_DQBUF_TIMEOUT_MS = 100;
        static constexpr int PREVIEW_STOP_TIMEOUT_MS = 2000;

        lv_obj_t *_status_label = nullptr;
        ppa_client_handle_t _ppa_srm_handle = nullptr;

        SemaphoreHandle_t _preview_done_sem = nullptr;
        int _video_fd = -1;
        bool _video_driver_initialized = false;
        std::atomic<bool> _preview_active{false};
        std::atomic<bool> _preview_running{false};
        std::atomic<bool> _dummy_enabled{false};
        uint32_t _camera_width = 0;
        uint32_t _camera_height = 0;
        size_t _camera_buffer_size = 0;
        size_t _camera_buffer_lengths[CAMERA_BUFFER_COUNT] = {};
        size_t _data_cache_line_size = 64;
        void *_camera_buffers[CAMERA_BUFFER_COUNT] = {};

        bool startPreview();
        bool requestStopPreview();
        esp_err_t initVideoDriver();
        esp_err_t openVideoDevice();
        esp_err_t setupCameraBuffers();
        esp_err_t startDummyPreview();
        esp_err_t stopDummyPreview();
        void releaseCameraBuffers();
        esp_err_t handleFrame();
        void previewTask();

        static void previewTaskEntry(void *arg);
        static void previewFinishedAsync(void *arg);
    };

} // namespace esp_brookesia::apps
