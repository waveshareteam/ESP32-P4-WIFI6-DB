/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

/* Show the current input level instead of keeping a previous high level. */
#ifndef GPIO_MONITOR_LATCH_INPUT_HIGH
#define GPIO_MONITOR_LATCH_INPUT_HIGH 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One GPIO shown in the LVGL monitor.
 *
 * The caller owns the name string and the configuration array. Both must stay
 * valid for the lifetime of the monitor.
 */
typedef struct {
    const char *name;
    gpio_num_t gpio_num;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    uint8_t initial_level;
} gpio_monitor_pin_config_t;

typedef struct {
    const gpio_monitor_pin_config_t *pins;
    size_t pin_count;
    gpio_mode_t initial_mode;
    uint32_t refresh_period_ms;
} gpio_monitor_config_t;

/**
 * Configure and store the selected GPIOs.
 *
 * Call this before starting a BSP display so the BSP can reclaim any pins that
 * it owns. Call gpio_monitor_start_ui() after the display has been started.
 */
esp_err_t gpio_monitor_init(const gpio_monitor_config_t *config);

/** Create the LVGL grid monitor after the display has been initialized. */
esp_err_t gpio_monitor_start_ui(void);

/**
 * Switch all monitored GPIOs between GPIO_MODE_INPUT and GPIO_MODE_OUTPUT.
 */
esp_err_t gpio_monitor_set_output_enabled(bool output_enabled);

/** Set an output-capable monitored GPIO to 0 or 1. */
esp_err_t gpio_monitor_set_level(gpio_num_t gpio_num, uint32_t level);

#ifdef __cplusplus
}
#endif
