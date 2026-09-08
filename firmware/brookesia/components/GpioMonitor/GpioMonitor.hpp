/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <vector>

#include "driver/gpio.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class GpioMonitor : public systems::phone::App {
public:
    static GpioMonitor *requestInstance(
        bool use_status_bar = false, bool use_navigation_bar = false
    );
    ~GpioMonitor() override;

protected:
    GpioMonitor(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool pause(void) override;
    bool resume(void) override;
    bool cleanResource(void) override;

private:
    struct PinState {
        gpio_num_t gpio = GPIO_NUM_NC;
        lv_obj_t *card = nullptr;
        lv_obj_t *level_label = nullptr;
        lv_obj_t *transition_label = nullptr;
        int last_level = 0;
        int output_level = 0;
        bool level_initialized = false;
        bool transitioned = false;
    };

    bool loadPinsFromBsp();
    bool acquirePins();
    bool configurePins(bool output_mode);
    bool setOutputMode(bool output_mode);
    void deinitPins();
    void resetSamplingBaseline(bool output_mode);
    void createUi(lv_obj_t *screen, int width, int height);
    void updatePinView(PinState &pin);
    void updateModeUi();
    void updateSummary();
    void resetTransitions();
    void setAllOutputsLow();
    void clearUiPointers();

    static void refresh_timer_cb(lv_timer_t *timer);
    static void mode_button_event_cb(lv_event_t *event);
    static void reset_button_event_cb(lv_event_t *event);
    static void pin_card_event_cb(lv_event_t *event);

    std::vector<PinState> _pins;
    std::size_t _transitioned_count = 0;
    bool _pins_owned = false;
    bool _paused = false;
    bool _output_mode = false;

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_root = nullptr;
    lv_obj_t *_subtitle_label = nullptr;
    lv_obj_t *_summary_card = nullptr;
    lv_obj_t *_summary_label = nullptr;
    lv_obj_t *_mode_button = nullptr;
    lv_obj_t *_mode_label = nullptr;
    lv_obj_t *_reset_label = nullptr;
    lv_timer_t *_refresh_timer = nullptr;

    static GpioMonitor *_instance;
};

} // namespace esp_brookesia::apps
