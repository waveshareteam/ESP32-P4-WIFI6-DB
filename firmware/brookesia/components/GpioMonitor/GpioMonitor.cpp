/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:GpioMonitor"
#include "esp_lib_utils.h"

#include <algorithm>
#include <cstdint>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "GpioMonitor.hpp"

#define APP_NAME                 "GPIO"
#define GPIO_REFRESH_PERIOD_MS   100

namespace esp_brookesia::apps {

namespace {

constexpr uint32_t COLOR_SCREEN = 0x0B0F14;
constexpr uint32_t COLOR_CARD = 0x151B23;
constexpr uint32_t COLOR_CARD_LOW = 0x1E293B;
constexpr uint32_t COLOR_CARD_HIGH = 0x166534;
constexpr uint32_t COLOR_BORDER = 0x334155;
constexpr uint32_t COLOR_PRIMARY = 0x38BDF8;
constexpr uint32_t COLOR_TEXT = 0xF8FAFC;
constexpr uint32_t COLOR_TEXT_MUTED = 0x94A3B8;
constexpr uint32_t COLOR_SUCCESS = 0x22C55E;
constexpr uint32_t COLOR_WARNING = 0xD97706;

static void style_transparent(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_card(lv_obj_t *obj, uint32_t color, int radius = 18)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_20, 0);
    lv_obj_set_style_shadow_width(obj, 10, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 3, 0);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color
)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

} // namespace

GpioMonitor *GpioMonitor::_instance = nullptr;

GpioMonitor *GpioMonitor::requestInstance(
    bool use_status_bar, bool use_navigation_bar
)
{
    if (_instance == nullptr) {
        _instance = new GpioMonitor(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

GpioMonitor::GpioMonitor(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, nullptr, true, use_status_bar, use_navigation_bar)
{
}

GpioMonitor::~GpioMonitor()
{
    deinitPins();
}

bool GpioMonitor::loadPinsFromBsp()
{
    size_t pin_count = 0;
    const gpio_num_t *gpios = bsp_get_header_gpios(&pin_count);
    ESP_UTILS_CHECK_FALSE_RETURN(
        (gpios != nullptr) && (pin_count > 0) && (pin_count <= GPIO_NUM_MAX),
        false,
        "Invalid BSP header GPIO list"
    );

    _pins.clear();
    _pins.reserve(pin_count);
    for (size_t i = 0; i < pin_count; ++i) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            (gpios[i] >= GPIO_NUM_0) && (gpios[i] < GPIO_NUM_MAX),
            false,
            "Invalid BSP header GPIO at index %u: %d",
            static_cast<unsigned>(i),
            static_cast<int>(gpios[i])
        );

        PinState pin;
        pin.gpio = gpios[i];
        _pins.push_back(pin);
    }
    return true;
}

bool GpioMonitor::acquirePins()
{
    if (_pins_owned) {
        return true;
    }

    return configurePins(_output_mode);
}

bool GpioMonitor::configurePins(bool output_mode)
{
    uint64_t pin_mask = 0;
    for (const auto &pin : _pins) {
        pin_mask |= (1ULL << static_cast<unsigned>(pin.gpio));
    }

    if (output_mode) {
        /*
         * Preload every output latch before enabling the output drivers. A
         * newly selected OUTPUT mode therefore always starts LOW without a
         * software-created HIGH pulse.
         */
        for (const auto &pin : _pins) {
            const esp_err_t err = gpio_set_level(pin.gpio, pin.output_level);
            ESP_UTILS_CHECK_ERROR_RETURN(
                err,
                false,
                "Preload GPIO%d output failed",
                static_cast<int>(pin.gpio)
            );
        }
    }

    gpio_config_t config = {};
    config.pin_bit_mask = pin_mask;
    config.mode = output_mode ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en =
        output_mode ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    const esp_err_t err = gpio_config(&config);
    ESP_UTILS_CHECK_ERROR_RETURN(err, false, "Configure BSP header GPIOs failed");

    _pins_owned = true;
    resetSamplingBaseline(output_mode);
    ESP_UTILS_LOGI(
        "Configured %u BSP header GPIOs as %s",
        static_cast<unsigned>(_pins.size()),
        output_mode ? "OUTPUT" : "INPUT with pull-down"
    );
    return true;
}

bool GpioMonitor::setOutputMode(bool output_mode)
{
    if (output_mode == _output_mode) {
        return true;
    }

    if (output_mode) {
        for (auto &pin : _pins) {
            pin.output_level = 0;
        }
    }

    if (!configurePins(output_mode)) {
        ESP_UTILS_LOGE(
            "Switch GPIO mode to %s failed",
            output_mode ? "OUTPUT" : "INPUT"
        );
        if (!configurePins(_output_mode)) {
            ESP_UTILS_LOGE("Restore previous GPIO mode failed");
        }
        return false;
    }

    _output_mode = output_mode;
    resetTransitions();
    updateModeUi();
    return true;
}

void GpioMonitor::deinitPins()
{
    if (!_pins_owned) {
        return;
    }

    for (auto &pin : _pins) {
        const esp_err_t err = gpio_reset_pin(pin.gpio);
        if (err != ESP_OK) {
            ESP_UTILS_LOGW(
                "Reset GPIO%d failed: %s",
                static_cast<int>(pin.gpio),
                esp_err_to_name(err)
            );
        }
        pin.level_initialized = false;
    }
    _pins_owned = false;
    ESP_UTILS_LOGI(
        "Deinitialized %u BSP header GPIOs",
        static_cast<unsigned>(_pins.size())
    );
}

void GpioMonitor::resetSamplingBaseline(bool output_mode)
{
    for (auto &pin : _pins) {
        pin.last_level =
            output_mode ? pin.output_level : gpio_get_level(pin.gpio);
        pin.level_initialized = true;
    }
}

bool GpioMonitor::run(void)
{
    ESP_UTILS_LOGD("Run(@0x%p)", this);

    _transitioned_count = 0;
    _paused = false;
    _output_mode = false;
    ESP_UTILS_CHECK_FALSE_RETURN(loadPinsFromBsp(), false, "Load BSP GPIOs failed");
    ESP_UTILS_CHECK_FALSE_RETURN(acquirePins(), false, "Acquire BSP GPIOs failed");

    const auto visual_area = getVisualArea();
    int width = lv_area_get_width(&visual_area);
    int height = lv_area_get_height(&visual_area);
    if ((width <= 0) || (height <= 0)) {
        width = 720;
        height = 1280;
    }

    _screen = lv_screen_active();
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COLOR_SCREEN), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    createUi(_screen, width, height);

    _refresh_timer = lv_timer_create(
        refresh_timer_cb, GPIO_REFRESH_PERIOD_MS, this
    );
    if (_refresh_timer == nullptr) {
        ESP_UTILS_LOGE("Create GPIO refresh timer failed");
        deinitPins();
        clearUiPointers();
        return false;
    }
    return true;
}

void GpioMonitor::createUi(lv_obj_t *screen, int width, int height)
{
    const int pad_x = std::max(18, width * 4 / 100);
    const int pad_y = std::max(14, height * 2 / 100);
    const int content_width = width - (2 * pad_x);
    const bool narrow = content_width < 560;

    _root = lv_obj_create(screen);
    lv_obj_set_size(_root, content_width, height - (2 * pad_y));
    lv_obj_center(_root);
    style_transparent(_root);
    lv_obj_set_style_pad_row(_root, narrow ? 10 : 14, 0);
    lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(_root);
    lv_obj_set_size(header, LV_PCT(100), narrow ? 126 : 138);
    style_card(header, COLOR_CARD);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(header, narrow ? 14 : 18, 0);

    lv_obj_t *title = make_label(
        header,
        "GPIO Monitor",
        narrow ? &lv_font_montserrat_30 : &lv_font_montserrat_36,
        COLOR_TEXT
    );
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    _subtitle_label = make_label(
        header,
        "BSP header pins  |  INPUT + pull-down  |  100 ms",
        narrow ? &lv_font_montserrat_18 : &lv_font_montserrat_20,
        COLOR_TEXT_MUTED
    );
    lv_obj_set_width(_subtitle_label, LV_PCT(100));
    lv_label_set_long_mode(_subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(_subtitle_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *reset_button = lv_button_create(header);
    lv_obj_set_size(reset_button, narrow ? 100 : 118, narrow ? 44 : 50);
    lv_obj_align(reset_button, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(reset_button, 12, 0);
    lv_obj_set_style_bg_color(reset_button, lv_color_hex(COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_color(reset_button, lv_color_hex(0x0284C7), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(reset_button, 0, 0);
    lv_obj_add_event_cb(
        reset_button, reset_button_event_cb, LV_EVENT_CLICKED, this
    );
    _reset_label = make_label(
        reset_button,
        "Reset",
        narrow ? &lv_font_montserrat_18 : &lv_font_montserrat_20,
        COLOR_TEXT
    );
    lv_obj_center(_reset_label);

    _summary_card = lv_obj_create(_root);
    lv_obj_set_size(_summary_card, LV_PCT(100), narrow ? 76 : 84);
    style_card(_summary_card, COLOR_CARD);
    lv_obj_remove_flag(_summary_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_hor(_summary_card, narrow ? 14 : 20, 0);
    lv_obj_set_style_pad_ver(_summary_card, 10, 0);

    _summary_label = make_label(
        _summary_card,
        "",
        narrow ? &lv_font_montserrat_22 : &lv_font_montserrat_24,
        COLOR_TEXT
    );
    lv_obj_align(_summary_label, LV_ALIGN_LEFT_MID, 0, 0);

    _mode_button = lv_button_create(_summary_card);
    lv_obj_set_size(_mode_button, narrow ? 108 : 136, narrow ? 46 : 54);
    lv_obj_align(_mode_button, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(_mode_button, 12, 0);
    lv_obj_set_style_shadow_width(_mode_button, 0, 0);
    lv_obj_add_event_cb(
        _mode_button, mode_button_event_cb, LV_EVENT_CLICKED, this
    );
    _mode_label = make_label(
        _mode_button,
        "",
        narrow ? &lv_font_montserrat_18 : &lv_font_montserrat_20,
        COLOR_TEXT
    );
    lv_obj_center(_mode_label);

    lv_obj_t *grid = lv_obj_create(_root);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_radius(grid, 18, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_bg_color(grid, lv_color_hex(COLOR_SCREEN), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);

    const size_t columns = content_width >= 900 ? 4 : (content_width >= 560 ? 3 : 2);
    const int gap = narrow ? 10 : 12;
    const int grid_inner_width = content_width;
    const int card_width = std::max(
        120,
        (grid_inner_width - gap * static_cast<int>(columns - 1)) /
        static_cast<int>(columns)
    );
    const int card_height = narrow ? 118 : 132;
    const size_t rows = (_pins.size() + columns - 1) / columns;
    const int content_height =
        static_cast<int>(rows) * card_height +
        static_cast<int>(rows > 0 ? rows - 1 : 0) * gap;

    lv_obj_t *content = lv_obj_create(grid);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, grid_inner_width, std::max(content_height, 1));
    style_transparent(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < _pins.size(); ++i) {
        PinState &pin = _pins[i];
        const int column = static_cast<int>(i % columns);
        const int row = static_cast<int>(i / columns);

        pin.card = lv_obj_create(content);
        lv_obj_set_pos(
            pin.card,
            column * (card_width + gap),
            row * (card_height + gap)
        );
        lv_obj_set_size(pin.card, card_width, card_height);
        style_card(pin.card, COLOR_CARD_LOW, 14);
        lv_obj_remove_flag(pin.card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(pin.card, narrow ? 10 : 12, 0);
        lv_obj_add_event_cb(
            pin.card, pin_card_event_cb, LV_EVENT_CLICKED, this
        );

        lv_obj_t *gpio_label = make_label(
            pin.card, "", &lv_font_montserrat_20, COLOR_TEXT_MUTED
        );
        lv_label_set_text_fmt(
            gpio_label, "GPIO%d", static_cast<int>(pin.gpio)
        );
        lv_obj_align(gpio_label, LV_ALIGN_TOP_LEFT, 0, 0);

        pin.level_label = make_label(
            pin.card,
            "",
            narrow ? &lv_font_montserrat_24 : &lv_font_montserrat_30,
            COLOR_TEXT
        );
        lv_obj_align(pin.level_label, LV_ALIGN_CENTER, 0, 2);

        pin.transition_label = make_label(
            pin.card,
            "",
            &lv_font_montserrat_16,
            COLOR_TEXT_MUTED
        );
        lv_obj_align(pin.transition_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        updatePinView(pin);
    }

    updateModeUi();
}

void GpioMonitor::updatePinView(PinState &pin)
{
    if ((pin.card == nullptr) || (pin.level_label == nullptr) ||
        (pin.transition_label == nullptr)) {
        return;
    }

    const bool high = pin.last_level != 0;
    lv_label_set_text(pin.level_label, high ? "HIGH  1" : "LOW  0");
    if (_output_mode) {
        lv_label_set_text(pin.transition_label, "tap to toggle");
        lv_obj_add_flag(pin.card, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_label_set_text(
            pin.transition_label,
            pin.transitioned ? "transition seen" : "waiting"
        );
        lv_obj_remove_flag(pin.card, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_bg_color(
        pin.card,
        lv_color_hex(high ? COLOR_CARD_HIGH : COLOR_CARD_LOW),
        0
    );
    lv_obj_set_style_border_color(
        pin.card,
        lv_color_hex(
            (_output_mode || pin.transitioned) ? COLOR_PRIMARY : COLOR_BORDER
        ),
        0
    );
    lv_obj_set_style_border_width(
        pin.card, (_output_mode || pin.transitioned) ? 2 : 1, 0
    );
}

void GpioMonitor::updateModeUi()
{
    if (_subtitle_label != nullptr) {
        lv_label_set_text(
            _subtitle_label,
            _output_mode ?
            "BSP header pins  |  OUTPUT  |  tap cards to toggle" :
            "BSP header pins  |  INPUT + pull-down  |  100 ms"
        );
    }
    if (_mode_label != nullptr) {
        lv_label_set_text(_mode_label, _output_mode ? "OUTPUT" : "INPUT");
    }
    if (_mode_button != nullptr) {
        lv_obj_set_style_bg_color(
            _mode_button,
            lv_color_hex(_output_mode ? COLOR_WARNING : COLOR_PRIMARY),
            0
        );
        lv_obj_set_style_bg_color(
            _mode_button,
            lv_color_hex(_output_mode ? 0xB45309 : 0x0284C7),
            LV_STATE_PRESSED
        );
    }
    if (_reset_label != nullptr) {
        lv_label_set_text(_reset_label, _output_mode ? "All LOW" : "Reset");
    }
    updateSummary();
}

void GpioMonitor::updateSummary()
{
    if (_summary_label != nullptr) {
        if (_output_mode) {
            lv_label_set_text(_summary_label, "Tap a GPIO card");
        } else {
            lv_label_set_text_fmt(
                _summary_label,
                "Transitions  %u / %u",
                static_cast<unsigned>(_transitioned_count),
                static_cast<unsigned>(_pins.size())
            );
        }
    }
    if (_summary_card != nullptr) {
        lv_obj_set_style_border_color(
            _summary_card,
            lv_color_hex(
                _output_mode ? COLOR_PRIMARY :
                ((!_pins.empty() && (_transitioned_count == _pins.size())) ?
                 COLOR_SUCCESS : COLOR_BORDER)
            ),
            0
        );
    }
}

void GpioMonitor::resetTransitions()
{
    _transitioned_count = 0;
    for (auto &pin : _pins) {
        pin.transitioned = false;
        pin.last_level =
            _output_mode ? pin.output_level : gpio_get_level(pin.gpio);
        pin.level_initialized = true;
        updatePinView(pin);
    }
    updateSummary();
}

void GpioMonitor::setAllOutputsLow()
{
    if (!_output_mode || !_pins_owned) {
        return;
    }

    for (auto &pin : _pins) {
        const esp_err_t err = gpio_set_level(pin.gpio, 0);
        if (err != ESP_OK) {
            ESP_UTILS_LOGE(
                "Set GPIO%d LOW failed: %s",
                static_cast<int>(pin.gpio),
                esp_err_to_name(err)
            );
            continue;
        }
        pin.output_level = 0;
        pin.last_level = 0;
        pin.level_initialized = true;
        updatePinView(pin);
    }
}

void GpioMonitor::refresh_timer_cb(lv_timer_t *timer)
{
    auto *self = static_cast<GpioMonitor *>(lv_timer_get_user_data(timer));
    if ((self == nullptr) || self->_paused || !self->_pins_owned) {
        return;
    }
    if (self->_output_mode) {
        return;
    }

    bool summary_changed = false;
    for (auto &pin : self->_pins) {
        const int level = gpio_get_level(pin.gpio);
        if (!pin.level_initialized) {
            pin.last_level = level;
            pin.level_initialized = true;
            self->updatePinView(pin);
            continue;
        }
        if (level == pin.last_level) {
            continue;
        }

        pin.last_level = level;
        if (!pin.transitioned) {
            pin.transitioned = true;
            ++self->_transitioned_count;
            summary_changed = true;
        }
        self->updatePinView(pin);
    }
    if (summary_changed) {
        self->updateSummary();
    }
}

void GpioMonitor::mode_button_event_cb(lv_event_t *event)
{
    auto *self = static_cast<GpioMonitor *>(lv_event_get_user_data(event));
    if ((self != nullptr) && self->_pins_owned) {
        self->setOutputMode(!self->_output_mode);
    }
}

void GpioMonitor::reset_button_event_cb(lv_event_t *event)
{
    auto *self = static_cast<GpioMonitor *>(lv_event_get_user_data(event));
    if ((self != nullptr) && self->_pins_owned) {
        if (self->_output_mode) {
            self->setAllOutputsLow();
        } else {
            self->resetTransitions();
        }
    }
}

void GpioMonitor::pin_card_event_cb(lv_event_t *event)
{
    auto *self = static_cast<GpioMonitor *>(lv_event_get_user_data(event));
    if ((self == nullptr) || !self->_pins_owned || !self->_output_mode) {
        return;
    }

    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
    for (auto &pin : self->_pins) {
        if (pin.card != target) {
            continue;
        }

        const int next_level = pin.output_level ? 0 : 1;
        const esp_err_t err = gpio_set_level(pin.gpio, next_level);
        if (err != ESP_OK) {
            ESP_UTILS_LOGE(
                "Set GPIO%d to %d failed: %s",
                static_cast<int>(pin.gpio),
                next_level,
                esp_err_to_name(err)
            );
            return;
        }

        pin.output_level = next_level;
        pin.last_level = next_level;
        pin.level_initialized = true;
        self->updatePinView(pin);
        return;
    }
}

bool GpioMonitor::back(void)
{
    ESP_UTILS_CHECK_FALSE_RETURN(
        notifyCoreClosed(), false, "Notify core closed failed"
    );
    return true;
}

bool GpioMonitor::close(void)
{
    ESP_UTILS_LOGD("Close(@0x%p)", this);
    _paused = true;
    if (_refresh_timer != nullptr) {
        lv_timer_pause(_refresh_timer);
    }
    deinitPins();
    return true;
}

bool GpioMonitor::pause(void)
{
    ESP_UTILS_LOGD("Pause(@0x%p)", this);
    _paused = true;
    if (_refresh_timer != nullptr) {
        lv_timer_pause(_refresh_timer);
    }
    deinitPins();
    return true;
}

bool GpioMonitor::resume(void)
{
    ESP_UTILS_LOGD("Resume(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(
        acquirePins(), false, "Reacquire BSP GPIOs failed"
    );
    _paused = false;
    if (_refresh_timer != nullptr) {
        lv_timer_resume(_refresh_timer);
        lv_timer_ready(_refresh_timer);
    }
    for (auto &pin : _pins) {
        updatePinView(pin);
    }
    updateModeUi();
    return true;
}

bool GpioMonitor::cleanResource(void)
{
    ESP_UTILS_LOGD("CleanResource(@0x%p)", this);
    _paused = true;
    if (_refresh_timer != nullptr) {
        lv_timer_delete(_refresh_timer);
        _refresh_timer = nullptr;
    }
    deinitPins();
    clearUiPointers();
    _screen = nullptr;
    _pins.clear();
    _pins.shrink_to_fit();
    _transitioned_count = 0;
    _output_mode = false;
    return true;
}

void GpioMonitor::clearUiPointers()
{
    for (auto &pin : _pins) {
        pin.card = nullptr;
        pin.level_label = nullptr;
        pin.transition_label = nullptr;
    }
    _root = nullptr;
    _subtitle_label = nullptr;
    _summary_card = nullptr;
    _summary_label = nullptr;
    _mode_button = nullptr;
    _mode_label = nullptr;
    _reset_label = nullptr;
}

} // namespace esp_brookesia::apps
