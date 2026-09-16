/*
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set LCD backlight brightness through the custom Board Manager device handle.
 *
 * @param[in] device_handle Handle returned for "lcd_brightness".
 * @param[in] brightness_percent Brightness percentage from 0 to 100.
 *
 * @return ESP_OK on success, otherwise an error code.
 */
#if CONFIG_BSP_LCD_BRIGHTNESS_USE_I2C
esp_err_t custom_lcd_brightness_set(void *device_handle, int brightness_percent);
#endif  /* CONFIG_BSP_LCD_BRIGHTNESS_USE_I2C */

#ifdef __cplusplus
}
#endif
