/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_check.h"
#include "example_config.h"
#include "bsp/esp32_p4_wifi6_db.h"

#include "bsp_board_extra.h"

static const char *TAG = "i2s_es8311";

/* Import music file as buffer */
#if CONFIG_EXAMPLE_MODE_MUSIC
extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t music_pcm_end[]   asm("_binary_canon_pcm_end");
#endif

static void gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_POWER_AMP_IO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(BSP_POWER_AMP_IO, 1));
}

#if CONFIG_EXAMPLE_MODE_MUSIC
static void i2s_music(void *args)
{
    (void)args;
    esp_err_t ret = ESP_OK;
    size_t bytes_write = 0;
    const size_t music_size = music_pcm_end - music_pcm_start;

    while (1) {
        ret = bsp_extra_i2s_write(music_pcm_start, music_size, &bytes_write);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[music] i2s write failed, %s (%d)", esp_err_to_name(ret), (int)ret);
            abort();
        }
        if (bytes_write == music_size) {
            ESP_LOGI(TAG, "[music] i2s music played, %u bytes are written.", (unsigned int)bytes_write);
        } else {
            ESP_LOGE(TAG, "[music] i2s music play failed.");
            abort();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#else

static void i2s_echo(void *args)
{
    (void)args;
    int16_t *mic_data = malloc(EXAMPLE_RECV_BUF_SIZE);
    if (!mic_data) {
        ESP_LOGE(TAG, "[echo] No memory for read data buffer");
        abort();
    }
    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;
    size_t bytes_write = 0;
    ESP_LOGI(TAG, "[echo] Echo start");

    while (1) {
        ret = bsp_extra_i2s_read(mic_data, EXAMPLE_RECV_BUF_SIZE, &bytes_read);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[echo] i2s read failed, %s (%d)", esp_err_to_name(ret), (int)ret);
            abort();
        }

        ret = bsp_extra_i2s_write(mic_data, bytes_read, &bytes_write);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[echo] i2s write failed, %s (%d)", esp_err_to_name(ret), (int)ret);
            abort();
        }
        if (bytes_read != bytes_write) {
            ESP_LOGW(TAG, "[echo] %u bytes read but only %u bytes are written", (unsigned int)bytes_read, (unsigned int)bytes_write);
        }
    }
}
#endif

void app_main(void)
{
    gpio_init();
    printf("i2s codec example start\n-----------------------------\n");

    if (bsp_extra_codec_init() != ESP_OK) {
        ESP_LOGE(TAG, "codec init failed");
        abort();
    } else {
        ESP_LOGI(TAG, "codec init success");
    }
#if CONFIG_EXAMPLE_MODE_MUSIC
    BaseType_t task_created = xTaskCreate(i2s_music, "i2s_music", 4096, NULL, 5, NULL);
#else
    BaseType_t task_created = xTaskCreate(i2s_echo, "i2s_echo", 8192, NULL, 5, NULL);
#endif
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        abort();
    }
}
