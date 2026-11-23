/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "esp_err.h"
#include "lvgl.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"

#include "gpio_monitor.h"

#define GPIO_MONITOR_NAME_LEN 8

static gpio_monitor_pin_config_t s_gpio_pins[GPIO_NUM_MAX];
static char s_gpio_pin_names[GPIO_NUM_MAX][GPIO_MONITOR_NAME_LEN];

static esp_err_t gpio_monitor_init_from_bsp(void)
{
    size_t gpio_count = 0;
    const gpio_num_t *gpios = bsp_get_header_gpios(&gpio_count);
    if (gpios == NULL || gpio_count == 0 || gpio_count > GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < gpio_count; i++) {
        snprintf(s_gpio_pin_names[i], sizeof(s_gpio_pin_names[i]), "GPIO%d", gpios[i]);
        s_gpio_pins[i] = (gpio_monitor_pin_config_t) {
            .name = s_gpio_pin_names[i],
            .gpio_num = gpios[i],
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .initial_level = 0,
        };
    }

    const gpio_monitor_config_t config = {
        .pins = s_gpio_pins,
        .pin_count = gpio_count,
        .initial_mode = GPIO_MODE_INPUT,
        .refresh_period_ms = 100,
    };
    return gpio_monitor_init(&config);
}

void app_main(void)
{
    bsp_display_cfg_t display_cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    /* Configure GPIOs first; the BSP may reclaim the pins used by the display. */
    ESP_ERROR_CHECK(gpio_monitor_init_from_bsp());
    lv_display_t *display = bsp_display_start_with_config(&display_cfg);
    ESP_ERROR_CHECK(display != NULL ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    if (!bsp_display_lock(-1)) {
        bsp_display_stop(display);
        ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    }
    esp_err_t ret = gpio_monitor_start_ui();
    bsp_display_unlock();
    if (ret != ESP_OK) {
        bsp_display_stop(display);
        ESP_ERROR_CHECK(ret);
    }
}
