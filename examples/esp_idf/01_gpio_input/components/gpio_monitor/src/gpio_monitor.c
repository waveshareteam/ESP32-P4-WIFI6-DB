/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "gpio_monitor.h"

static const char *TAG = "gpio_monitor";

typedef struct {
    lv_obj_t *card;
    lv_obj_t *label;
    int last_level;
    bool level_initialized;
    bool passed;
    bool input_checked_latched;
} gpio_monitor_pin_view_t;

static struct {
    gpio_monitor_pin_config_t *pins;
    gpio_monitor_pin_view_t *views;
    size_t pin_count;
    uint32_t refresh_period_ms;
    lv_timer_t *refresh_timer;
    lv_obj_t *count_label;
    lv_obj_t *mode_label;
#ifdef DYNAMIC_SWITCH_GPIO_MODE
    lv_obj_t *mode_switch;
#endif
    lv_obj_t *passed_card;
    lv_obj_t *passed_label;
    size_t passed_count;
    bool output_enabled;
    bool initialized;
    bool ui_started;
} s_monitor;

static gpio_mode_t current_gpio_mode(void)
{
    return s_monitor.output_enabled ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
}

static const char *gpio_mode_to_string(gpio_mode_t mode)
{
    switch (mode) {
    case GPIO_MODE_INPUT:
        return "IN";
    case GPIO_MODE_OUTPUT:
        return "OUT";
    default:
        return "?";
    }
}

static lv_color_t gpio_level_color(int level)
{
    return level ? lv_color_hex(0x18794E) : lv_color_hex(0x263238);
}

static int32_t positive_or_one(int32_t value)
{
    return value > 0 ? value : 1;
}

static size_t choose_column_count(size_t pin_count, int32_t width)
{
    const size_t min_cell_width = 88;
    size_t max_columns = (size_t)(width / min_cell_width);
    if (max_columns == 0) {
        max_columns = 1;
    }
    if (max_columns > pin_count) {
        max_columns = pin_count;
    }

    size_t columns = 1;
    while (columns * columns < pin_count && columns < max_columns) {
        columns++;
    }
    return columns;
}

static int find_pin_index(gpio_num_t gpio_num)
{
    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        if (s_monitor.pins[i].gpio_num == gpio_num) {
            return (int)i;
        }
    }
    return -1;
}

static void update_passed_card(void)
{
    if (s_monitor.passed_label != NULL) {
        lv_label_set_text_fmt(s_monitor.passed_label, "Passed: %u", (unsigned)s_monitor.passed_count);
    }
    if (s_monitor.passed_card != NULL) {
        lv_obj_set_style_bg_color(s_monitor.passed_card,
                                  s_monitor.passed_count == s_monitor.pin_count ?
                                  lv_color_hex(0x18794E) : lv_color_hex(0x263238),
                                  0);
    }
}

static void reset_passed_state(void)
{
    s_monitor.passed_count = 0;
    if (s_monitor.views == NULL) {
        update_passed_card();
        return;
    }
    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        s_monitor.views[i].level_initialized = false;
        s_monitor.views[i].passed = false;
        s_monitor.views[i].input_checked_latched = false;
    }
    update_passed_card();
}

static void update_pin_view(size_t index, bool detect_transition)
{
    const gpio_monitor_pin_config_t *pin = &s_monitor.pins[index];
    gpio_monitor_pin_view_t *view = &s_monitor.views[index];
    const gpio_mode_t mode = current_gpio_mode();
    int level = gpio_get_level(pin->gpio_num);

    if (!view->level_initialized) {
        view->last_level = level;
        view->level_initialized = true;
    } else if (detect_transition && mode == GPIO_MODE_INPUT &&
               level != view->last_level && !view->passed) {
        view->passed = true;
        s_monitor.passed_count++;
        update_passed_card();
    }
    view->last_level = level;

#if GPIO_MONITOR_LATCH_INPUT_HIGH
    if (mode == GPIO_MODE_INPUT && level) {
        view->input_checked_latched = true;
    }
#endif

    if (pin->name != NULL) {
        lv_label_set_text_fmt(view->label, "%s %s:%d", pin->name,
                              gpio_mode_to_string(mode), level);
    } else {
        lv_label_set_text_fmt(view->label, "GPIO%d %s:%d", pin->gpio_num,
                              gpio_mode_to_string(mode), level);
    }
    lv_obj_set_style_bg_color(view->card, gpio_level_color(level), 0);
    lv_obj_invalidate(view->card);
    lv_obj_add_flag(view->card, LV_OBJ_FLAG_CHECKABLE);
    bool checked = level != 0;
#if GPIO_MONITOR_LATCH_INPUT_HIGH
    checked = checked || (mode == GPIO_MODE_INPUT && view->input_checked_latched);
#endif
    if (checked) {
        lv_obj_add_state(view->card, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(view->card, LV_STATE_CHECKED);
    }
    if (mode == GPIO_MODE_OUTPUT) {
        lv_obj_add_flag(view->card, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(view->card, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        update_pin_view(i, true);
    }
}

static void output_card_event_cb(lv_event_t *event)
{
    gpio_monitor_pin_view_t *view = lv_event_get_user_data(event);
    if (view == NULL) {
        return;
    }

    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        if (&s_monitor.views[i] == view) {
            const gpio_monitor_pin_config_t *pin = &s_monitor.pins[i];
            if (s_monitor.output_enabled) {
                int current_level = gpio_get_level(pin->gpio_num);
                esp_err_t ret = gpio_monitor_set_level(pin->gpio_num, !current_level);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to set GPIO %d level: %s",
                             pin->gpio_num, esp_err_to_name(ret));
                } else {
                    update_pin_view(i, false);
                }
            }
            return;
        }
    }
}

#ifdef DYNAMIC_SWITCH_GPIO_MODE
static void mode_switch_event_cb(lv_event_t *event)
{
    lv_obj_t *mode_switch = lv_event_get_target(event);
    const bool output_enabled = lv_obj_has_state(mode_switch, LV_STATE_CHECKED);
    esp_err_t err = gpio_monitor_set_output_enabled(output_enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to switch GPIO mode: %s", esp_err_to_name(err));
        if (output_enabled) {
            lv_obj_clear_state(mode_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(mode_switch, LV_STATE_CHECKED);
        }
    }
}
#endif

static esp_err_t validate_config(const gpio_monitor_config_t *config)
{
    if (config == NULL || (config->pin_count > 0 && config->pins == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->initial_mode != GPIO_MODE_INPUT && config->initial_mode != GPIO_MODE_OUTPUT) {
        ESP_LOGE(TAG, "only GPIO_MODE_INPUT and GPIO_MODE_OUTPUT are supported");
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < config->pin_count; ++i) {
        const gpio_monitor_pin_config_t *pin = &config->pins[i];
        if (pin->gpio_num < GPIO_NUM_0 || pin->gpio_num >= GPIO_NUM_MAX) {
            ESP_LOGE(TAG, "invalid GPIO configuration at index %u", (unsigned)i);
            return ESP_ERR_INVALID_ARG;
        }
        for (size_t j = 0; j < i; ++j) {
            if (config->pins[j].gpio_num == pin->gpio_num) {
                ESP_LOGE(TAG, "GPIO%d is configured more than once", pin->gpio_num);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
}

static esp_err_t configure_gpio(size_t index)
{
    const gpio_monitor_pin_config_t *pin = &s_monitor.pins[index];
    const gpio_mode_t mode = current_gpio_mode();
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = 1ULL << pin->gpio_num,
        .mode = mode,
        .pull_up_en = pin->pull_up_en,
        .pull_down_en = pin->pull_down_en,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&gpio_cfg), TAG, "gpio_config GPIO%d failed", pin->gpio_num);
    if (mode == GPIO_MODE_OUTPUT) {
        ESP_RETURN_ON_ERROR(gpio_set_level(pin->gpio_num, pin->initial_level ? 1 : 0),
                            TAG, "gpio_set_level GPIO%d failed", pin->gpio_num);
    }
    return ESP_OK;
}

static esp_err_t configure_gpios(void)
{
    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        ESP_RETURN_ON_ERROR(configure_gpio(i), TAG, "GPIO%d initialization failed",
                            s_monitor.pins[i].gpio_num);
    }
    return ESP_OK;
}

static void reset_configured_gpios(void)
{
    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        esp_err_t err = gpio_reset_pin(s_monitor.pins[i].gpio_num);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "gpio_reset_pin GPIO%d failed: %s",
                     s_monitor.pins[i].gpio_num, esp_err_to_name(err));
        }
    }
}

static esp_err_t create_grid_ui(uint32_t refresh_period_ms)
{
    lv_obj_t *screen = lv_screen_active();
    if (screen == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const int32_t screen_width = positive_or_one(lv_display_get_horizontal_resolution(NULL));
    const int32_t screen_height = positive_or_one(lv_display_get_vertical_resolution(NULL));
    const int32_t header_height = screen_height < 240 ? 52 : 68;
    const int32_t passed_height = screen_height < 240 ? 44 : 56;
    const int32_t grid_padding = 6;
    const int32_t grid_gap = 6;
    const int32_t grid_y = header_height + passed_height + grid_gap;
    const int32_t grid_height = positive_or_one(screen_height - grid_y);
    const int32_t inner_width = positive_or_one(screen_width - 2 * grid_padding);
    const int32_t inner_height = positive_or_one(grid_height - 2 * grid_padding);

    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *header = lv_obj_create(screen);
    if (header == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(header, screen_width, header_height);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x172A3A), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(header, 4, 0);

    lv_obj_t *title = lv_label_create(header);
    if (title == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(title, "GPIO monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F7FA), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    s_monitor.count_label = lv_label_create(header);
    if (s_monitor.count_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text_fmt(s_monitor.count_label, "Total Nums : %u GPIO", (unsigned)s_monitor.pin_count);
    lv_obj_set_style_text_color(s_monitor.count_label, lv_color_hex(0xB0BEC5), 0);
    lv_obj_align(s_monitor.count_label, LV_ALIGN_RIGHT_MID, -116, 0);

#ifdef DYNAMIC_SWITCH_GPIO_MODE
    s_monitor.mode_label = lv_label_create(header);
    if (s_monitor.mode_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(s_monitor.mode_label, "IN");
    lv_obj_set_style_text_color(s_monitor.mode_label, lv_color_hex(0xF5F7FA), 0);
    lv_obj_align(s_monitor.mode_label, LV_ALIGN_RIGHT_MID, -68, 0);

    s_monitor.mode_switch = lv_switch_create(header);
    if (s_monitor.mode_switch == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(s_monitor.mode_switch, 52, 28);
    lv_obj_align(s_monitor.mode_switch, LV_ALIGN_RIGHT_MID, -8, 0);
    if (s_monitor.output_enabled) {
        lv_obj_add_state(s_monitor.mode_switch, LV_STATE_CHECKED);
        lv_label_set_text(s_monitor.mode_label, "OUT");
    }
    lv_obj_set_style_bg_color(s_monitor.mode_switch, lv_color_hex(0x607D8B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_monitor.mode_switch, lv_color_hex(0x18794E),
                              LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_monitor.mode_switch, mode_switch_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
#endif

    s_monitor.passed_card = lv_obj_create(screen);
    if (s_monitor.passed_card == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_pos(s_monitor.passed_card, grid_gap, header_height + grid_gap);
    lv_obj_set_size(s_monitor.passed_card, positive_or_one(screen_width - 2 * grid_gap), passed_height);
    lv_obj_clear_flag(s_monitor.passed_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_monitor.passed_card, 8, 0);
    lv_obj_set_style_border_width(s_monitor.passed_card, 1, 0);
    lv_obj_set_style_border_color(s_monitor.passed_card, lv_color_hex(0x607D8B), 0);
    lv_obj_set_style_bg_color(s_monitor.passed_card, lv_color_hex(0x263238), 0);
    lv_obj_set_style_bg_opa(s_monitor.passed_card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_monitor.passed_card, 4, 0);

    s_monitor.passed_label = lv_label_create(s_monitor.passed_card);
    if (s_monitor.passed_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_color(s_monitor.passed_label, lv_color_hex(0xF5F7FA), 0);
    lv_obj_center(s_monitor.passed_label);
    update_passed_card();

    lv_obj_t *grid = lv_obj_create(screen);
    if (grid == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_pos(grid, 0, grid_y);
    lv_obj_set_size(grid, screen_width, grid_height);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);

    if (s_monitor.pin_count == 0) {
        lv_obj_t *empty_label = lv_label_create(grid);
        if (empty_label == NULL) {
            return ESP_ERR_NO_MEM;
        }
        lv_label_set_text(empty_label, "Add GPIOs in main.c");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0xB0BEC5), 0);
        lv_obj_center(empty_label);
        return ESP_OK;
    }

    const size_t columns = choose_column_count(s_monitor.pin_count, inner_width);
    const size_t rows = (s_monitor.pin_count + columns - 1) / columns;
    const int32_t calculated_cell_height = positive_or_one(
        (inner_height - grid_gap * (int32_t)(rows - 1)) / (int32_t)rows);
    const int32_t cell_height = calculated_cell_height < 42 ? 42 : calculated_cell_height;
    const int32_t cell_width = positive_or_one(
        (inner_width - grid_gap * (int32_t)(columns - 1)) / (int32_t)columns);
    const int32_t content_height = positive_or_one(
        grid_padding * 2 + (int32_t)rows * cell_height +
        grid_gap * (int32_t)(rows - 1));

    s_monitor.views = calloc(s_monitor.pin_count, sizeof(*s_monitor.views));
    if (s_monitor.views == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /*
     * Use an explicit content object and positions instead of relying on the
     * grid layout pass. This keeps the cards visible on all LVGL 9.x builds,
     * while the resulting layout is still a responsive, scrollable grid.
     */
    lv_obj_t *content = lv_obj_create(grid);
    if (content == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, screen_width, content_height > grid_height ? content_height : grid_height);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, 0, 0);

    ESP_LOGI(TAG, "UI: %ldx%ld, %u GPIO, %u columns x %u rows, card %ldx%ld",
             (long)screen_width, (long)screen_height, (unsigned)s_monitor.pin_count,
             (unsigned)columns, (unsigned)rows, (long)cell_width, (long)cell_height);

    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        gpio_monitor_pin_view_t *view = &s_monitor.views[i];

        const int32_t column = (int32_t)(i % columns);
        const int32_t row = (int32_t)(i / columns);
        view->card = lv_obj_create(content);
        if (view->card == NULL) {
            return ESP_ERR_NO_MEM;
        }
        lv_obj_set_pos(view->card,
                       grid_padding + column * (cell_width + grid_gap),
                       grid_padding + row * (cell_height + grid_gap));
        lv_obj_set_size(view->card, cell_width, cell_height);
        lv_obj_clear_flag(view->card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(view->card, 8, 0);
        lv_obj_set_style_border_width(view->card, 1, 0);
        lv_obj_set_style_border_color(view->card, lv_color_hex(0x607D8B), 0);
        lv_obj_set_style_pad_all(view->card, 4, 0);
        lv_obj_set_style_bg_opa(view->card, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(view->card, lv_color_hex(0x18794E),
                                  LV_PART_MAIN | LV_STATE_CHECKED);

        view->label = lv_label_create(view->card);
        if (view->label == NULL) {
            return ESP_ERR_NO_MEM;
        }
        lv_obj_set_width(view->label, LV_PCT(100));
        lv_obj_set_style_text_color(view->label, lv_color_hex(0xF5F7FA), 0);
        lv_obj_set_style_text_align(view->label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(view->label);
        update_pin_view(i, false);

        lv_obj_add_event_cb(view->card, output_card_event_cb, LV_EVENT_CLICKED, view);
    }

    s_monitor.refresh_timer = lv_timer_create(refresh_timer_cb,
                                               refresh_period_ms > 0 ? refresh_period_ms : 100,
                                               NULL);
    return s_monitor.refresh_timer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t gpio_monitor_init(const gpio_monitor_config_t *config)
{
    if (s_monitor.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(validate_config(config), TAG, "invalid monitor configuration");

    s_monitor.pin_count = config->pin_count;
    s_monitor.refresh_period_ms = config->refresh_period_ms;
    s_monitor.pins = calloc(s_monitor.pin_count, sizeof(*s_monitor.pins));
    if (s_monitor.pin_count > 0 && s_monitor.pins == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (s_monitor.pin_count > 0) {
        memcpy(s_monitor.pins, config->pins, s_monitor.pin_count * sizeof(*s_monitor.pins));
    }

    s_monitor.output_enabled = config->initial_mode == GPIO_MODE_OUTPUT;
    esp_err_t err = configure_gpios();
    if (err != ESP_OK) {
        reset_configured_gpios();
        free(s_monitor.pins);
        memset(&s_monitor, 0, sizeof(s_monitor));
        ESP_LOGE(TAG, "GPIO initialization failed: %s", esp_err_to_name(err));
        return err;
    }
    s_monitor.initialized = true;
    return ESP_OK;
}

esp_err_t gpio_monitor_start_ui(void)
{
    if (!s_monitor.initialized || s_monitor.ui_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (lv_display_get_default() == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(create_grid_ui(s_monitor.refresh_period_ms), TAG, "UI initialization failed");
    s_monitor.ui_started = true;
    return ESP_OK;
}

esp_err_t gpio_monitor_set_output_enabled(bool output_enabled)
{
    if (!s_monitor.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const bool previous_output_enabled = s_monitor.output_enabled;
    s_monitor.output_enabled = output_enabled;

    for (size_t i = 0; i < s_monitor.pin_count; ++i) {
        esp_err_t err = configure_gpio(i);
        if (err != ESP_OK) {
            s_monitor.output_enabled = previous_output_enabled;
            for (size_t j = 0; j < i; ++j) {
                esp_err_t restore_err = configure_gpio(j);
                if (restore_err != ESP_OK) {
                    ESP_LOGE(TAG, "Restore GPIO%d mode failed: %s",
                             s_monitor.pins[j].gpio_num, esp_err_to_name(restore_err));
                }
            }
            return err;
        }
    }

    reset_passed_state();
    if (s_monitor.ui_started) {
        if (s_monitor.mode_label != NULL) {
            lv_label_set_text(s_monitor.mode_label, output_enabled ? "OUT" : "IN");
        }
        for (size_t i = 0; i < s_monitor.pin_count; ++i) {
            update_pin_view(i, false);
        }
    }
    return ESP_OK;
}

esp_err_t gpio_monitor_set_level(gpio_num_t gpio_num, uint32_t level)
{
    const int index = find_pin_index(gpio_num);
    if (index < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!s_monitor.output_enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    return gpio_set_level(gpio_num, level ? 1 : 0);
}
