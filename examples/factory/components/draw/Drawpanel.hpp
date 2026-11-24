/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief Drawpanel application for touch-based drawing on the device screen
 *
 */
class Drawpanel: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of Drawpanel
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return Drawpanel* Singleton instance pointer
     */
    static Drawpanel *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the Drawpanel object
     *
     */
    ~Drawpanel() override;

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    /**
     * @brief Construct a new Drawpanel object (private to enforce singleton)
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    Drawpanel(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;

    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;
    // bool cleanResource(void) override;

private:
    static Drawpanel *_instance;

    bool createCanvas(const lv_area_t &area);
    void releaseCanvas();
    void clearCanvas();
    bool getCanvasPoint(lv_event_t *event, lv_point_t &point) const;
    void drawSegment(const lv_point_t &from, const lv_point_t &to);
    void drawBrushAt(int32_t center_x, int32_t center_y, uint16_t color);
    static void touch_event_cb(lv_event_t *e);

    lv_obj_t *_canvas_obj;
    uint8_t *_canvas_buffer;
    size_t _canvas_buffer_size;
    uint32_t _canvas_stride;
    int32_t _canvas_width;
    int32_t _canvas_height;
    lv_point_t _prev_point;
    bool _stroke_active;
};

} // namespace esp_brookesia::apps
