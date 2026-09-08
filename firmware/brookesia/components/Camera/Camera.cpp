/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Camera.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "bsp/esp-bsp.h"
#include "esp_lib_utils.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Camera"

LV_IMG_DECLARE(img_app_camera);

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB565
#define CAMERA_VIDEO_FMT V4L2_PIX_FMT_RGB565
#define CAMERA_PPA_COLOR_MODE PPA_SRM_COLOR_MODE_RGB565
#define CAMERA_BYTES_PER_PIXEL 2
#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define CAMERA_VIDEO_FMT V4L2_PIX_FMT_RGB24
#define CAMERA_PPA_COLOR_MODE PPA_SRM_COLOR_MODE_RGB888
#define CAMERA_BYTES_PER_PIXEL 3
#else
#error "Unsupported BSP LCD color format"
#endif

#ifdef CONFIG_CAMERA_OV5647_MIPI_RAW8_800x1280_50FPS
#define CAMERA_FALLBACK_CAPTURE_WIDTH  800
#define CAMERA_FALLBACK_CAPTURE_HEIGHT 1280
#else
#define CAMERA_FALLBACK_CAPTURE_WIDTH  BSP_LCD_H_RES
#define CAMERA_FALLBACK_CAPTURE_HEIGHT BSP_LCD_V_RES
#endif

namespace esp_brookesia::apps
{
    struct CoverCropConfig {
        uint32_t offset_x;
        uint32_t offset_y;
        uint32_t width;
        uint32_t height;
        float scale_x;
        float scale_y;
    };

    static CoverCropConfig computeCoverCrop(uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h)
    {
        CoverCropConfig crop = {
            .offset_x = 0,
            .offset_y = 0,
            .width = src_w,
            .height = src_h,
            .scale_x = 1.0f,
            .scale_y = 1.0f,
        };

        if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
            crop.width = crop.width ? crop.width : 1;
            crop.height = crop.height ? crop.height : 1;
            return crop;
        }

        if (static_cast<uint64_t>(src_w) * dst_h > static_cast<uint64_t>(src_h) * dst_w) {
            crop.width = static_cast<uint32_t>((static_cast<uint64_t>(src_h) * dst_w) / dst_h);
            crop.height = src_h;
        } else {
            crop.width = src_w;
            crop.height = static_cast<uint32_t>((static_cast<uint64_t>(src_w) * dst_h) / dst_w);
        }

        crop.width = crop.width ? crop.width : 1;
        crop.height = crop.height ? crop.height : 1;
        crop.offset_x = (src_w - crop.width) / 2;
        crop.offset_y = (src_h - crop.height) / 2;
        crop.scale_x = static_cast<float>(dst_w) / static_cast<float>(crop.width);
        crop.scale_y = static_cast<float>(dst_h) / static_cast<float>(crop.height);

        return crop;
    }

    Camera *Camera::_instance = nullptr;

    Camera *Camera::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new Camera(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    Camera::Camera(bool use_status_bar, bool use_navigation_bar) : App("Camera", &img_app_camera, true, use_status_bar, use_navigation_bar)
    {
    }

    Camera::~Camera()
    {
        close();
        deinit();
        if (_instance == this) {
            _instance = nullptr;
        }
    }

    bool Camera::run(void)
    {
        ESP_UTILS_LOGD("Run");

        lv_obj_clean(lv_scr_act());
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
        _status_label = lv_label_create(lv_scr_act());
        lv_label_set_text(_status_label, "Camera");
        lv_obj_set_width(_status_label, BSP_LCD_H_RES);
        lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(_status_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_align(_status_label, LV_ALIGN_CENTER, 0, 0);

        if (!startPreview()) {
            if (_status_label) {
                lv_label_set_text(_status_label, "camera error");
            }
        }

        return true;
    }

    bool Camera::back(void)
    {
        ESP_UTILS_LOGD("Back");
        ESP_UTILS_CHECK_FALSE_RETURN(requestStopPreview(), false, "Stop camera preview failed");
        esp_err_t ret = stopDummyPreview();
        ESP_UTILS_CHECK_FALSE_RETURN(
            ret == ESP_OK,
            false,
            "Restore LVGL display failed: %s",
            esp_err_to_name(ret)
        );
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool Camera::close()
    {
        ESP_UTILS_LOGD("Close");
        ESP_UTILS_CHECK_FALSE_RETURN(requestStopPreview(), false, "Stop camera preview failed");
        esp_err_t ret = stopDummyPreview();
        ESP_UTILS_CHECK_FALSE_RETURN(
            ret == ESP_OK,
            false,
            "Restore LVGL display failed: %s",
            esp_err_to_name(ret)
        );

        if (_status_label) {
            lv_obj_del(_status_label);
            _status_label = nullptr;
        }
        return true;
    }

    bool Camera::init()
    {
        ESP_UTILS_LOGD("Init");
        if (_preview_done_sem == nullptr) {
            _preview_done_sem = xSemaphoreCreateBinary();
        }
        ESP_UTILS_CHECK_NULL_RETURN(_preview_done_sem, false, "Create preview semaphore failed");

        return true;
    }

    bool Camera::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        ESP_UTILS_CHECK_FALSE_RETURN(requestStopPreview(), false, "Stop camera preview failed");
        esp_err_t ret = stopDummyPreview();
        ESP_UTILS_CHECK_FALSE_RETURN(
            ret == ESP_OK,
            false,
            "Restore LVGL display failed: %s",
            esp_err_to_name(ret)
        );

        lv_async_call_cancel(previewFinishedAsync, this);

        if (_ppa_srm_handle != nullptr) {
            ret = ppa_unregister_client(_ppa_srm_handle);
            ESP_UTILS_CHECK_FALSE_RETURN(
                ret == ESP_OK,
                false,
                "Unregister PPA SRM failed: %s",
                esp_err_to_name(ret)
            );
            _ppa_srm_handle = nullptr;
        }

        if (_preview_done_sem != nullptr) {
            vSemaphoreDelete(_preview_done_sem);
            _preview_done_sem = nullptr;
        }

        return true;
    }

    bool Camera::pause()
    {
        ESP_UTILS_LOGD("Pause");
        ESP_UTILS_CHECK_FALSE_RETURN(requestStopPreview(), false, "Stop camera preview failed");
        esp_err_t ret = stopDummyPreview();
        ESP_UTILS_CHECK_FALSE_RETURN(
            ret == ESP_OK,
            false,
            "Restore LVGL display failed: %s",
            esp_err_to_name(ret)
        );
        return true;
    }

    bool Camera::resume()
    {
        ESP_UTILS_LOGD("Resume");
        return startPreview();
    }

    bool Camera::startPreview()
    {
        bool expected = false;
        if (!_preview_active.compare_exchange_strong(expected, true)) {
            return true;
        }

        if (_preview_done_sem == nullptr) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Preview semaphore is not initialized");
            _preview_active = false;
            return false;
        }

        lv_async_call_cancel(previewFinishedAsync, this);
        xSemaphoreTake(_preview_done_sem, 0);

        esp_err_t err = startDummyPreview();
        if (err != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Enable camera preview display failed: %s", esp_err_to_name(err));
            _preview_active = false;
            return false;
        }

        _preview_running = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            previewTaskEntry,
            "camera_preview",
            PREVIEW_TASK_STACK_SIZE,
            this,
            PREVIEW_TASK_PRIORITY,
            nullptr,
            0);

        if (ret != pdPASS) {
            _preview_running = false;
            _preview_active = false;
            const esp_err_t stop_ret = stopDummyPreview();
            if (stop_ret != ESP_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Rollback preview display failed: %s",
                         esp_err_to_name(stop_ret));
            }
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Create camera preview task failed");
            return false;
        }

        return true;
    }

    bool Camera::requestStopPreview()
    {
        if (!_preview_active.load()) {
            return true;
        }

        _preview_running = false;
        ESP_UTILS_CHECK_NULL_RETURN(_preview_done_sem, false, "Preview semaphore is not initialized");

        if (xSemaphoreTake(_preview_done_sem, pdMS_TO_TICKS(PREVIEW_STOP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Timed out after %d ms waiting for camera preview to stop",
                PREVIEW_STOP_TIMEOUT_MS
            );
            return false;
        }

        _preview_active = false;
        return true;
    }

    esp_err_t Camera::initVideoDriver()
    {
        if (_video_driver_initialized) {
            return ESP_OK;
        }

        esp_err_t ret = bsp_camera_start(nullptr);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "BSP camera start failed: %s", esp_err_to_name(ret));
            return ret;
        }

        _video_driver_initialized = true;
        return ESP_OK;
    }

    esp_err_t Camera::openVideoDevice()
    {
        if (_video_fd >= 0) {
            return ESP_OK;
        }

        _video_fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
        if (_video_fd < 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Open %s failed", BSP_CAMERA_DEVICE);
            return ESP_FAIL;
        }

        struct timeval dqbuf_timeout = {};
        dqbuf_timeout.tv_sec = PREVIEW_DQBUF_TIMEOUT_MS / 1000;
        dqbuf_timeout.tv_usec = (PREVIEW_DQBUF_TIMEOUT_MS % 1000) * 1000;
        if (ioctl(_video_fd, VIDIOC_S_DQBUF_TIMEOUT, &dqbuf_timeout) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_S_DQBUF_TIMEOUT failed");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_FAIL;
        }

        struct v4l2_capability capability = {};
        if (ioctl(_video_fd, VIDIOC_QUERYCAP, &capability) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYCAP failed");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_FAIL;
        }
        const uint32_t device_caps = (capability.capabilities & V4L2_CAP_DEVICE_CAPS) ?
            capability.device_caps :
            capability.capabilities;
        constexpr uint32_t required_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        if ((device_caps & required_caps) != required_caps) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera device lacks capture/streaming capabilities: 0x%08" PRIx32,
                device_caps
            );
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_NOT_SUPPORTED;
        }

        struct v4l2_format format = {};
        auto set_capture_format = [&](uint32_t width, uint32_t height) -> bool {
            memset(&format, 0, sizeof(format));
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format.fmt.pix.width = width;
            format.fmt.pix.height = height;
            format.fmt.pix.pixelformat = CAMERA_VIDEO_FMT;

            return ioctl(_video_fd, VIDIOC_S_FMT, &format) == 0;
        };

        // Prefer the panel size when the sensor supports it. Some camera modes are
        // fixed by Kconfig, so fall back to the known native capture mode and adapt
        // the preview to the LCD with PPA.
        if (!set_capture_format(BSP_LCD_H_RES, BSP_LCD_V_RES)) {
            ESP_LOGW(
                ESP_UTILS_LOG_TAG,
                "VIDIOC_S_FMT %" PRIu32 "x%" PRIu32 " failed, trying fallback %" PRIu32 "x%" PRIu32,
                static_cast<uint32_t>(BSP_LCD_H_RES),
                static_cast<uint32_t>(BSP_LCD_V_RES),
                static_cast<uint32_t>(CAMERA_FALLBACK_CAPTURE_WIDTH),
                static_cast<uint32_t>(CAMERA_FALLBACK_CAPTURE_HEIGHT)
            );
            if (!set_capture_format(CAMERA_FALLBACK_CAPTURE_WIDTH, CAMERA_FALLBACK_CAPTURE_HEIGHT)) {
                ESP_LOGE(
                    ESP_UTILS_LOG_TAG,
                    "VIDIOC_S_FMT fallback %" PRIu32 "x%" PRIu32 " failed",
                    static_cast<uint32_t>(CAMERA_FALLBACK_CAPTURE_WIDTH),
                    static_cast<uint32_t>(CAMERA_FALLBACK_CAPTURE_HEIGHT)
                );
                ::close(_video_fd);
                _video_fd = -1;
                return ESP_FAIL;
            }
        }

        memset(&format, 0, sizeof(format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_video_fd, VIDIOC_G_FMT, &format) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_G_FMT failed");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_FAIL;
        }

        if (format.fmt.pix.width == 0 || format.fmt.pix.height == 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera returned an invalid zero-sized format");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (format.fmt.pix.pixelformat != CAMERA_VIDEO_FMT) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera returned unsupported format " V4L2_FMT_STR,
                V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat)
            );
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_NOT_SUPPORTED;
        }

        _camera_width = format.fmt.pix.width;
        _camera_height = format.fmt.pix.height;
        const size_t row_size = static_cast<size_t>(_camera_width) * CAMERA_BYTES_PER_PIXEL;
        if (format.fmt.pix.bytesperline != 0 && format.fmt.pix.bytesperline != row_size) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Unsupported camera stride: %" PRIu32 " bytes (expected %zu)",
                format.fmt.pix.bytesperline,
                row_size
            );
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_NOT_SUPPORTED;
        }

        const size_t minimum_frame_size = row_size * _camera_height;
        if (format.fmt.pix.sizeimage != 0 && format.fmt.pix.sizeimage < minimum_frame_size) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera frame size %" PRIu32 " is smaller than required %zu",
                format.fmt.pix.sizeimage,
                minimum_frame_size
            );
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_INVALID_RESPONSE;
        }
        _camera_buffer_size = format.fmt.pix.sizeimage ?
            format.fmt.pix.sizeimage :
            minimum_frame_size;
        ESP_LOGI(
            ESP_UTILS_LOG_TAG,
            "Camera format: %" PRIu32 "x%" PRIu32 " " V4L2_FMT_STR,
            _camera_width,
            _camera_height,
            V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat)
        );

        return ESP_OK;
    }

    esp_err_t Camera::setupCameraBuffers()
    {
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
        _data_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
        _data_cache_line_size = 64;
#endif

        struct v4l2_requestbuffers req = {};
        req.count = CAMERA_BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (ioctl(_video_fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_REQBUFS failed");
            return ESP_FAIL;
        }
        if (req.count < CAMERA_BUFFER_COUNT) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera returned only %" PRIu32 " buffers, need %d",
                req.count,
                CAMERA_BUFFER_COUNT
            );
            return ESP_ERR_NOT_SUPPORTED;
        }

        for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
            struct v4l2_buffer buf = {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;

            if (ioctl(_video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYBUF %d failed", i);
                return ESP_FAIL;
            }

            size_t alloc_size = buf.length ? buf.length : _camera_buffer_size;
            if (alloc_size < _camera_buffer_size) {
                ESP_LOGE(
                    ESP_UTILS_LOG_TAG,
                    "Camera buffer %d is too small: %zu bytes (need %zu)",
                    i,
                    alloc_size,
                    _camera_buffer_size
                );
                return ESP_ERR_INVALID_SIZE;
            }
            _camera_buffer_lengths[i] = alloc_size;
            _camera_buffers[i] = heap_caps_aligned_calloc(_data_cache_line_size, 1, alloc_size, MALLOC_CAP_SPIRAM);
            if (!_camera_buffers[i]) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera buffer %d allocation failed", i);
                return ESP_ERR_NO_MEM;
            }

            buf.m.userptr = (unsigned long)_camera_buffers[i];
            buf.length = alloc_size;

            if (ioctl(_video_fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF %d failed", i);
                return ESP_FAIL;
            }
        }

        return ESP_OK;
    }

    esp_err_t Camera::startDummyPreview()
    {
        if (_ppa_srm_handle == nullptr) {
            ppa_client_config_t ppa_srm_config = {
                .oper_type = PPA_OPERATION_SRM,
            };
            ESP_RETURN_ON_ERROR(ppa_register_client(&ppa_srm_config, &_ppa_srm_handle), ESP_UTILS_LOG_TAG, "Register PPA SRM failed");
        }

        if (!_dummy_enabled.load()) {
            ESP_RETURN_ON_ERROR(
                bsp_display_set_dummy_draw(true),
                ESP_UTILS_LOG_TAG,
                "Enable dummy draw failed"
            );
            _dummy_enabled = true;
        }

        return ESP_OK;
    }

    esp_err_t Camera::stopDummyPreview()
    {
        if (!_dummy_enabled.load()) {
            return ESP_OK;
        }

        esp_err_t ret = bsp_display_set_dummy_draw(false);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Disable dummy draw failed: %s", esp_err_to_name(ret));
            return ret;
        }

        _dummy_enabled = false;
        return ESP_OK;
    }

    void Camera::releaseCameraBuffers()
    {
        if (_video_fd >= 0) {
            ::close(_video_fd);
            _video_fd = -1;
        }

        for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
            if (_camera_buffers[i]) {
                heap_caps_free(_camera_buffers[i]);
                _camera_buffers[i] = nullptr;
            }
            _camera_buffer_lengths[i] = 0;
        }
    }

    esp_err_t Camera::handleFrame()
    {
        struct v4l2_buffer v4l2_buf = {};
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_USERPTR;

        if (ioctl(_video_fd, VIDIOC_DQBUF, &v4l2_buf) != 0) {
            const int saved_errno = errno;
            // esp_video returns ESP_FAIL when its configured DQBUF wait expires,
            // and its VFS layer maps that value to EPERM.
            if (saved_errno == EPERM) {
                return ESP_ERR_TIMEOUT;
            }
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "VIDIOC_DQBUF failed: errno=%d (%s)",
                saved_errno,
                strerror(saved_errno)
            );
            return ESP_FAIL;
        }

        if (v4l2_buf.index >= CAMERA_BUFFER_COUNT) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera returned invalid buffer index %" PRIu32,
                v4l2_buf.index
            );
            return ESP_ERR_INVALID_RESPONSE;
        }

        const uint32_t display_w = BSP_LCD_H_RES;
        const uint32_t display_h = BSP_LCD_V_RES;
        CoverCropConfig crop = computeCoverCrop(_camera_width, _camera_height, display_w, display_h);
        void *lcd_frame_buffer = nullptr;
        esp_err_t ret = ESP_OK;

        if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) ||
                v4l2_buf.bytesused < _camera_buffer_size) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Camera returned an invalid frame: flags=0x%" PRIx32 ", bytes=%" PRIu32 "/%zu",
                v4l2_buf.flags,
                v4l2_buf.bytesused,
                _camera_buffer_size
            );
            ret = ESP_FAIL;
        } else {
            lcd_frame_buffer = bsp_display_get_free_frame_buffer();
            if (lcd_frame_buffer == nullptr) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "No free LCD frame buffer");
                ret = ESP_ERR_NOT_FOUND;
            }
        }

        if (ret == ESP_OK) {
            ppa_srm_oper_config_t srm_config = {};
            srm_config.in.buffer = _camera_buffers[v4l2_buf.index];
            srm_config.in.pic_w = _camera_width;
            srm_config.in.pic_h = _camera_height;
            srm_config.in.block_w = crop.width;
            srm_config.in.block_h = crop.height;
            srm_config.in.block_offset_x = crop.offset_x;
            srm_config.in.block_offset_y = crop.offset_y;
            srm_config.in.srm_cm = CAMERA_PPA_COLOR_MODE;

            srm_config.out.buffer = lcd_frame_buffer;
            srm_config.out.buffer_size = ALIGN_UP(
                static_cast<size_t>(display_w) * display_h * CAMERA_BYTES_PER_PIXEL,
                _data_cache_line_size
            );
            srm_config.out.pic_w = display_w;
            srm_config.out.pic_h = display_h;
            srm_config.out.block_offset_x = 0;
            srm_config.out.block_offset_y = 0;
            srm_config.out.srm_cm = CAMERA_PPA_COLOR_MODE;

            srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
            srm_config.scale_x = crop.scale_x;
            srm_config.scale_y = crop.scale_y;
            srm_config.mirror_x = 0;
            srm_config.mirror_y = 0;
            srm_config.rgb_swap = 0;
            srm_config.byte_swap = 0;
            srm_config.mode = PPA_TRANS_MODE_BLOCKING;

            ret = ppa_do_scale_rotate_mirror(_ppa_srm_handle, &srm_config);
        }

        if (lcd_frame_buffer != nullptr) {
            esp_err_t flush_ret = bsp_display_flush_frame_buffer(lcd_frame_buffer);
            if (flush_ret != ESP_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Return LCD frame buffer failed: %s",
                         esp_err_to_name(flush_ret));
                if (ret == ESP_OK) {
                    ret = flush_ret;
                }
            }
        }

        v4l2_buf.m.userptr = (unsigned long)_camera_buffers[v4l2_buf.index];
        v4l2_buf.length = _camera_buffer_lengths[v4l2_buf.index];
        if (ioctl(_video_fd, VIDIOC_QBUF, &v4l2_buf) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF failed");
            return ESP_FAIL;
        }

        return ret;
    }

    void Camera::previewTask()
    {
        bool streaming = false;
        esp_err_t ret = initVideoDriver();
        if (ret == ESP_OK && _preview_running.load()) {
            ret = openVideoDevice();
        }
        if (ret == ESP_OK && _preview_running.load()) {
            ret = setupCameraBuffers();
        }

        if (ret == ESP_OK && _preview_running.load()) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(_video_fd, VIDIOC_STREAMON, &type) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_STREAMON failed");
                ret = ESP_FAIL;
            } else {
                streaming = true;
            }
        }

        while (_preview_running.load() && ret == ESP_OK) {
            ret = handleFrame();
            if (ret == ESP_ERR_TIMEOUT) {
                ret = ESP_OK;
            }
        }

        const bool preview_failed = (ret != ESP_OK) && _preview_running.load();

        if (streaming) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(_video_fd, VIDIOC_STREAMOFF, &type) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_STREAMOFF failed");
            }
        }

        releaseCameraBuffers();
        _preview_running = false;

        if (preview_failed) {
            if (lv_async_call(previewFinishedAsync, this) != LV_RESULT_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Queue camera error UI update failed");
            }
        }

        if (_preview_done_sem != nullptr) {
            xSemaphoreGive(_preview_done_sem);
        }

        vTaskDelete(nullptr);
    }

    void Camera::previewTaskEntry(void *arg)
    {
        static_cast<Camera *>(arg)->previewTask();
    }

    void Camera::previewFinishedAsync(void *arg)
    {
        Camera *camera = static_cast<Camera *>(arg);
        if (camera == nullptr) {
            return;
        }

        if (!camera->requestStopPreview()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Failed to join camera preview task");
            return;
        }
        esp_err_t ret = camera->stopDummyPreview();
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Restore display after camera failure failed: %s",
                     esp_err_to_name(ret));
        }
        if (camera->_status_label != nullptr) {
            lv_label_set_text(camera->_status_label, "camera error");
        }
    }

} // namespace esp_brookesia::apps
