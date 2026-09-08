/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_heap_caps.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Drawpanel"
#include "esp_lib_utils.h"
#include "Drawpanel.hpp"
#include <algorithm>
#include <cstdlib>

#define APP_NAME "DrawPanel"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(img_app_drawpanel);

namespace {

constexpr uint32_t BACKGROUND_COLOR = 0xFFFFE0;
constexpr uint32_t BRUSH_COLOR = 0xFF0000;
constexpr int32_t BRUSH_RADIUS = 5;

void disableObjectScrolling(lv_obj_t *obj)
{
    if (obj == nullptr) {
        return;
    }

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

} // namespace

namespace esp_brookesia::apps {

Drawpanel *Drawpanel::_instance = nullptr;

Drawpanel *Drawpanel::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new Drawpanel(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

Drawpanel::Drawpanel(bool use_status_bar, bool use_navigation_bar) :
    App(APP_NAME, &img_app_drawpanel, true, use_status_bar, use_navigation_bar),
    _canvas_obj(nullptr),
    _canvas_buffer(nullptr),
    _canvas_buffer_size(0),
    _canvas_stride(0),
    _canvas_width(0),
    _canvas_height(0),
    _prev_point{},
    _stroke_active(false)
{
}

Drawpanel::~Drawpanel()
{
    releaseCanvas();
}

bool Drawpanel::run(void)
{
    ESP_UTILS_LOGD("Run");

    lv_obj_t *screen = lv_scr_act();
    ESP_UTILS_CHECK_NULL_RETURN(screen, false, "Active screen is invalid");

    lv_obj_set_style_bg_color(screen, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    disableObjectScrolling(screen);

    lv_area_t canvas_area = getVisualArea();
    const int32_t visual_width = lv_area_get_width(&canvas_area);
    const int32_t visual_height = lv_area_get_height(&canvas_area);
    ESP_UTILS_CHECK_FALSE_RETURN(
        (visual_width > 0) && (visual_height > 0),
        false,
        "Invalid visual area: %ld x %ld",
        static_cast<long>(visual_width),
        static_cast<long>(visual_height)
    );
    int32_t gesture_guard_height = 0;

    auto *phone = getSystem();
    if ((phone != nullptr) && getActiveConfig().flags.enable_navigation_gesture) {
        auto *gesture = phone->getManager().getGesture();
        if (gesture != nullptr) {
            gesture_guard_height = gesture->data.threshold.vertical_edge;
        }
    }
    gesture_guard_height = std::clamp(gesture_guard_height, int32_t{0}, visual_height - 1);
    canvas_area.y2 -= gesture_guard_height;

    ESP_UTILS_CHECK_FALSE_RETURN(createCanvas(canvas_area), false, "Create drawing canvas failed");

    return true;
}

bool Drawpanel::back(void)
{
    ESP_UTILS_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool Drawpanel::close(void)
{
    ESP_UTILS_LOGD("Close");

    releaseCanvas();

    return true;
}

bool Drawpanel::init()
{
    ESP_UTILS_LOGD("Init");

    return true;
}

bool Drawpanel::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    releaseCanvas();

    return true;
}

bool Drawpanel::pause()
{
    ESP_UTILS_LOGD("Pause");

    _stroke_active = false;

    return true;
}

bool Drawpanel::resume()
{
    ESP_UTILS_LOGD("Resume");

    _stroke_active = false;

    return true;
}

bool Drawpanel::createCanvas(const lv_area_t &area)
{
    releaseCanvas();

    const int32_t width = lv_area_get_width(&area);
    const int32_t height = lv_area_get_height(&area);
    ESP_UTILS_CHECK_FALSE_RETURN((width > 0) && (height > 0), false, "Invalid canvas size: %ld x %ld",
                                 static_cast<long>(width), static_cast<long>(height));

    _canvas_stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
    _canvas_buffer_size = static_cast<size_t>(_canvas_stride) * height;
    _canvas_buffer = static_cast<uint8_t *>(
        heap_caps_aligned_alloc(
            LV_DRAW_BUF_ALIGN,
            _canvas_buffer_size,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );
    if (_canvas_buffer == nullptr) {
        ESP_UTILS_LOGE("Allocate canvas buffer from PSRAM failed: %zu bytes", _canvas_buffer_size);
        releaseCanvas();
        return false;
    }

    _canvas_obj = lv_canvas_create(lv_scr_act());
    if (_canvas_obj == nullptr) {
        ESP_UTILS_LOGE("Create canvas object failed");
        releaseCanvas();
        return false;
    }

    lv_obj_remove_style_all(_canvas_obj);
    lv_canvas_set_buffer(_canvas_obj, _canvas_buffer, width, height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(_canvas_obj, width, height);
    lv_obj_set_pos(_canvas_obj, area.x1, area.y1);
    lv_obj_add_flag(_canvas_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    disableObjectScrolling(_canvas_obj);

    lv_obj_add_event_cb(_canvas_obj, touch_event_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(_canvas_obj, touch_event_cb, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(_canvas_obj, touch_event_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(_canvas_obj, touch_event_cb, LV_EVENT_PRESS_LOST, this);
    lv_obj_add_event_cb(_canvas_obj, touch_event_cb, LV_EVENT_INDEV_RESET, this);

    _canvas_width = width;
    _canvas_height = height;
    _stroke_active = false;
    clearCanvas();
    lv_obj_update_layout(_canvas_obj);

    ESP_UTILS_LOGI(
        "Canvas ready: %ld x %ld, stride=%lu, buffer=%zu bytes",
        static_cast<long>(_canvas_width),
        static_cast<long>(_canvas_height),
        static_cast<unsigned long>(_canvas_stride),
        _canvas_buffer_size
    );

    return true;
}

void Drawpanel::releaseCanvas()
{
    _stroke_active = false;

    if (_canvas_obj != nullptr) {
        if (lv_obj_is_valid(_canvas_obj)) {
            lv_obj_delete(_canvas_obj);
        }
        _canvas_obj = nullptr;
    }

    if (_canvas_buffer != nullptr) {
        heap_caps_free(_canvas_buffer);
        _canvas_buffer = nullptr;
    }

    _canvas_buffer_size = 0;
    _canvas_stride = 0;
    _canvas_width = 0;
    _canvas_height = 0;
    _prev_point = {};
}

void Drawpanel::clearCanvas()
{
    if ((_canvas_obj == nullptr) || (_canvas_buffer == nullptr)) {
        return;
    }

    lv_canvas_fill_bg(_canvas_obj, lv_color_hex(BACKGROUND_COLOR), LV_OPA_COVER);
}

bool Drawpanel::getCanvasPoint(lv_event_t *event, lv_point_t &point) const
{
    if ((_canvas_obj == nullptr) || !lv_obj_is_valid(_canvas_obj)) {
        return false;
    }

    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev == nullptr) {
        return false;
    }

    lv_point_t screen_point = {};
    lv_indev_get_point(indev, &screen_point);

    lv_area_t canvas_coords = {};
    lv_obj_get_coords(_canvas_obj, &canvas_coords);
    if ((screen_point.x < canvas_coords.x1) || (screen_point.x > canvas_coords.x2) ||
            (screen_point.y < canvas_coords.y1) || (screen_point.y > canvas_coords.y2)) {
        return false;
    }

    point.x = screen_point.x - canvas_coords.x1;
    point.y = screen_point.y - canvas_coords.y1;

    return true;
}

void Drawpanel::drawBrushAt(int32_t center_x, int32_t center_y, uint16_t color)
{
    for (int32_t offset_y = -BRUSH_RADIUS; offset_y <= BRUSH_RADIUS; offset_y++) {
        const int32_t y = center_y + offset_y;
        if ((y < 0) || (y >= _canvas_height)) {
            continue;
        }

        auto *row = reinterpret_cast<uint16_t *>(
            _canvas_buffer + static_cast<size_t>(y) * _canvas_stride
        );
        for (int32_t offset_x = -BRUSH_RADIUS; offset_x <= BRUSH_RADIUS; offset_x++) {
            if ((offset_x * offset_x + offset_y * offset_y) > (BRUSH_RADIUS * BRUSH_RADIUS)) {
                continue;
            }

            const int32_t x = center_x + offset_x;
            if ((x >= 0) && (x < _canvas_width)) {
                row[x] = color;
            }
        }
    }
}

void Drawpanel::drawSegment(const lv_point_t &from, const lv_point_t &to)
{
    if ((_canvas_obj == nullptr) || (_canvas_buffer == nullptr)) {
        return;
    }

    const int32_t from_x = static_cast<int32_t>(from.x);
    const int32_t from_y = static_cast<int32_t>(from.y);
    const int32_t to_x = static_cast<int32_t>(to.x);
    const int32_t to_y = static_cast<int32_t>(to.y);
    int32_t x = from_x;
    int32_t y = from_y;
    const int32_t delta_x = std::abs(to_x - from_x);
    const int32_t step_x = (from_x < to_x) ? 1 : -1;
    const int32_t delta_y = -std::abs(to_y - from_y);
    const int32_t step_y = (from_y < to_y) ? 1 : -1;
    int32_t error = delta_x + delta_y;
    const uint16_t brush_color = lv_color_to_u16(lv_color_hex(BRUSH_COLOR));

    while (true) {
        drawBrushAt(x, y, brush_color);
        if ((x == to_x) && (y == to_y)) {
            break;
        }

        const int32_t error_twice = 2 * error;
        if (error_twice >= delta_y) {
            error += delta_y;
            x += step_x;
        }
        if (error_twice <= delta_x) {
            error += delta_x;
            y += step_y;
        }
    }

    lv_area_t dirty_area = {
        .x1 = std::max(int32_t{0}, std::min(from_x, to_x) - BRUSH_RADIUS),
        .y1 = std::max(int32_t{0}, std::min(from_y, to_y) - BRUSH_RADIUS),
        .x2 = std::min(_canvas_width - int32_t{1}, std::max(from_x, to_x) + BRUSH_RADIUS),
        .y2 = std::min(_canvas_height - int32_t{1}, std::max(from_y, to_y) + BRUSH_RADIUS),
    };

    lv_draw_buf_t *draw_buffer = lv_canvas_get_draw_buf(_canvas_obj);
    if (draw_buffer != nullptr) {
        lv_draw_buf_flush_cache(draw_buffer, &dirty_area);
    }

    lv_area_t canvas_coords = {};
    lv_obj_get_coords(_canvas_obj, &canvas_coords);
    lv_area_t invalid_area = {
        .x1 = canvas_coords.x1 + dirty_area.x1,
        .y1 = canvas_coords.y1 + dirty_area.y1,
        .x2 = canvas_coords.x1 + dirty_area.x2,
        .y2 = canvas_coords.y1 + dirty_area.y2,
    };
    lv_obj_invalidate_area(_canvas_obj, &invalid_area);
}

void Drawpanel::touch_event_cb(lv_event_t *e)
{
    Drawpanel *app = static_cast<Drawpanel *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        ESP_UTILS_LOGE("Get drawpanel instance failed");
        return;
    }

    const lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST) || (code == LV_EVENT_INDEV_RESET)) {
        app->_stroke_active = false;
        return;
    }

    lv_point_t point = {};
    if (!app->getCanvasPoint(e, point)) {
        app->_stroke_active = false;
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        app->_prev_point = point;
        app->_stroke_active = true;
        app->drawSegment(point, point);
        return;
    }

    if ((code == LV_EVENT_PRESSING) && app->_stroke_active) {
        if ((point.x == app->_prev_point.x) && (point.y == app->_prev_point.y)) {
            return;
        }

        app->drawSegment(app->_prev_point, point);
        app->_prev_point = point;
    }
}

} // namespace esp_brookesia::apps
