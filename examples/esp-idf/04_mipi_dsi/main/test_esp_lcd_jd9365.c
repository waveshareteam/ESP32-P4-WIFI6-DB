/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "soc/gpio_num.h"

static char *TAG = "jd9365_test";
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t lcd_io;

static esp_err_t show_white_screen(esp_lcd_panel_handle_t panel)
{
    void *frame_buffer = NULL;
    const size_t frame_buffer_size =
        BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8;

    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid LCD panel handle");
    ESP_RETURN_ON_ERROR(
        esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &frame_buffer),
        TAG, "Failed to get LCD frame buffer"
    );

    memset(frame_buffer, 0xFF, frame_buffer_size);
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_draw_bitmap(
            panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, frame_buffer
        ),
        TAG, "Failed to draw white screen"
    );
    return esp_lcd_dpi_panel_set_pattern(panel, MIPI_DSI_PATTERN_NONE);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_err_t ret = bsp_display_new(NULL, &panel_handle, &lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }
#if 1 // 白屏测试
    ESP_LOGI(TAG, "Show full white screen");
    ret = show_white_screen(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show white screen: %s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_display_backlight_on();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display backlight: %s", esp_err_to_name(ret));
    }
#else
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
    esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL);
#endif
}
