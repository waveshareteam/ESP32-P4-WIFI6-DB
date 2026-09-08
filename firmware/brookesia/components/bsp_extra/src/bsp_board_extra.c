/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"

static const char *TAG = "bsp_extra_board";

static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;

static bool s_audio_initialized;
static bool s_player_initialized;
static bool s_player_initializing;
static bool s_player_session_active;
static bool s_player_resume_after_suspend;
static bsp_extra_audio_session_config_t s_player_session_config;
static bool s_play_device_opened;
static bool s_record_device_opened;
static int s_volume_percent = CODEC_DEFAULT_VOLUME;
static SemaphoreHandle_t s_audio_mutex;
static SemaphoreHandle_t s_input_mutex;
static SemaphoreHandle_t s_output_mutex;
static portMUX_TYPE s_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;
static bsp_extra_audio_client_t s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
static bsp_extra_audio_session_config_t s_session_config;
static bool s_session_active;
static bool s_session_stopping;
static uint32_t s_input_in_flight;
static uint32_t s_output_in_flight;
static esp_codec_dev_sample_info_t s_output_fs;
static bool s_output_fs_valid;

static audio_player_cb_t s_audio_idle_callback;
static void *s_audio_idle_cb_user_data;
static char s_audio_file_path[128];

#define BSP_EXTRA_BRIGHTNESS_DUTY_MAX    (1023U)

static void update_codec_result(esp_err_t *result, esp_err_t codec_result);

static esp_err_t ensure_audio_mutex(void)
{
    taskENTER_CRITICAL(&s_mutex_init_lock);
    bool mutexes_ready = s_audio_mutex != NULL && s_input_mutex != NULL &&
                         s_output_mutex != NULL;
    taskEXIT_CRITICAL(&s_mutex_init_lock);
    if (mutexes_ready) {
        return ESP_OK;
    }

    SemaphoreHandle_t audio_mutex = xSemaphoreCreateMutex();
    SemaphoreHandle_t input_mutex = xSemaphoreCreateMutex();
    SemaphoreHandle_t output_mutex = xSemaphoreCreateMutex();
    if (audio_mutex == NULL || input_mutex == NULL || output_mutex == NULL) {
        if (audio_mutex != NULL) {
            vSemaphoreDelete(audio_mutex);
        }
        if (input_mutex != NULL) {
            vSemaphoreDelete(input_mutex);
        }
        if (output_mutex != NULL) {
            vSemaphoreDelete(output_mutex);
        }
        return ESP_ERR_NO_MEM;
    }

    taskENTER_CRITICAL(&s_mutex_init_lock);
    if (s_audio_mutex == NULL) {
        s_audio_mutex = audio_mutex;
        audio_mutex = NULL;
    }
    if (s_input_mutex == NULL) {
        s_input_mutex = input_mutex;
        input_mutex = NULL;
    }
    if (s_output_mutex == NULL) {
        s_output_mutex = output_mutex;
        output_mutex = NULL;
    }
    taskEXIT_CRITICAL(&s_mutex_init_lock);

    if (audio_mutex != NULL) {
        vSemaphoreDelete(audio_mutex);
    }
    if (input_mutex != NULL) {
        vSemaphoreDelete(input_mutex);
    }
    if (output_mutex != NULL) {
        vSemaphoreDelete(output_mutex);
    }
    return ESP_OK;
}

static esp_err_t lock_audio(TickType_t timeout)
{
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    return xSemaphoreTake(s_audio_mutex, timeout) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void unlock_audio(void)
{
    if (s_audio_mutex != NULL) {
        xSemaphoreGive(s_audio_mutex);
    }
}

static esp_err_t codec_stop_locked(void)
{
    esp_err_t ret = ESP_OK;

    if (record_dev_handle && s_record_device_opened) {
        esp_err_t close_ret = esp_codec_dev_close(record_dev_handle);
        update_codec_result(&ret, close_ret);
        if (close_ret == ESP_OK) {
            s_record_device_opened = false;
        }
    }
    if (play_dev_handle && s_play_device_opened) {
        esp_err_t close_ret = esp_codec_dev_close(play_dev_handle);
        update_codec_result(&ret, close_ret);
        if (close_ret == ESP_OK) {
            s_play_device_opened = false;
        }
    }
    if (!s_play_device_opened) {
        s_output_fs_valid = false;
    }

    return ret;
}

static esp_err_t codec_set_fs_locked(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(play_dev_handle && record_dev_handle, ESP_ERR_INVALID_STATE, TAG,
                        "Codec is not initialized");
    ESP_RETURN_ON_FALSE(rate > 0 && bits_cfg > 0 &&
                        (ch == I2S_SLOT_MODE_MONO || ch == I2S_SLOT_MODE_STEREO),
                        ESP_ERR_INVALID_ARG, TAG, "Invalid output codec format");
    ESP_RETURN_ON_ERROR(codec_stop_locked(), TAG, "Close codec devices failed");

    esp_codec_dev_sample_info_t output_fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    esp_err_t ret = esp_codec_dev_open(play_dev_handle, &output_fs);
    if (ret == ESP_OK) {
        s_play_device_opened = true;
        ret = esp_codec_dev_set_out_mute(play_dev_handle, false);
    }
    if (ret == ESP_OK) {
        ret = esp_codec_dev_set_out_vol(play_dev_handle, s_volume_percent);
    }
    if (ret != ESP_OK) {
        esp_err_t close_ret = esp_codec_dev_close(play_dev_handle);
        if (close_ret == ESP_OK) {
            s_play_device_opened = false;
        } else {
            ESP_LOGW(TAG, "Failed to close output codec after setup error: %d", close_ret);
        }
        return ret;
    }

    s_output_fs = output_fs;
    s_output_fs_valid = true;
    return ESP_OK;
}

static esp_err_t codec_set_record_fs_locked(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(play_dev_handle && record_dev_handle, ESP_ERR_INVALID_STATE, TAG,
                        "Codec is not initialized");
    ESP_RETURN_ON_FALSE(rate > 0 && bits_cfg > 0 &&
                        (ch == I2S_SLOT_MODE_MONO || ch == I2S_SLOT_MODE_STEREO),
                        ESP_ERR_INVALID_ARG, TAG, "Invalid ES8311 record format");
    ESP_RETURN_ON_ERROR(codec_stop_locked(), TAG, "Close codec devices failed");

    esp_codec_dev_sample_info_t duplex_fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    esp_err_t ret = esp_codec_dev_open(play_dev_handle, &duplex_fs);
    if (ret == ESP_OK) {
        s_play_device_opened = true;
        ret = esp_codec_dev_set_out_mute(play_dev_handle, false);
    }
    if (ret == ESP_OK) {
        ret = esp_codec_dev_set_out_vol(play_dev_handle, s_volume_percent);
    }
    if (ret != ESP_OK) {
        esp_err_t close_ret = esp_codec_dev_close(play_dev_handle);
        if (close_ret == ESP_OK) {
            s_play_device_opened = false;
        } else {
            ESP_LOGW(TAG, "Failed to close output codec after setup error: %d", close_ret);
        }
        return ret;
    }

    ret = esp_codec_dev_open(record_dev_handle, &duplex_fs);
    if (ret == ESP_OK) {
        s_record_device_opened = true;
        ret = esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    }
    if (ret != ESP_OK) {
        esp_err_t record_close_ret = esp_codec_dev_close(record_dev_handle);
        if (record_close_ret == ESP_OK) {
            s_record_device_opened = false;
        } else {
            ESP_LOGW(TAG, "Failed to close input codec after setup error: %d", record_close_ret);
        }
        esp_err_t close_ret = esp_codec_dev_close(play_dev_handle);
        if (close_ret == ESP_OK) {
            s_play_device_opened = false;
        } else {
            ESP_LOGW(TAG, "Failed to close output codec after record setup failure: %d", close_ret);
        }
        return ret;
    }

    s_output_fs = duplex_fs;
    s_output_fs_valid = true;
    return ESP_OK;
}

/**************************************************************************************************
 *
 * Extra Board Function
 *
 **************************************************************************************************/

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    // Volume saved when muting and restored when unmuting. Restoring volume is necessary
    // as es8311_set_voice_mute(true) results in voice volume (REG32) being set to zero.

    ESP_RETURN_ON_FALSE(setting == AUDIO_PLAYER_MUTE || setting == AUDIO_PLAYER_UNMUTE,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio mute setting");
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_output_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Lock audio output failed");
    esp_err_t lock_ret = lock_audio(portMAX_DELAY);
    if (lock_ret != ESP_OK) {
        xSemaphoreGive(s_output_mutex);
        return lock_ret;
    }
    if (!play_dev_handle || !s_play_device_opened ||
            (s_session_active && s_audio_owner != BSP_EXTRA_AUDIO_CLIENT_MUSIC)) {
        unlock_audio();
        xSemaphoreGive(s_output_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_codec_dev_set_out_mute(
                        play_dev_handle, setting == AUDIO_PLAYER_MUTE
                    );
    if (ret == ESP_OK && setting == AUDIO_PLAYER_UNMUTE) {
        ret = esp_codec_dev_set_out_vol(play_dev_handle, s_volume_percent);
    }
    unlock_audio();
    xSemaphoreGive(s_output_mutex);
    return ret;
}

static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    if (ctx == NULL || lock_audio(portMAX_DELAY) != ESP_OK) {
        return;
    }
    audio_player_cb_t callback = s_audio_idle_callback;
    void *user_data = s_audio_idle_cb_user_data;
    unlock_audio();
    if (callback != NULL) {
        ctx->user_ctx = user_data;
        callback(ctx);
    }
}

static esp_err_t music_write_function(void *audio_buffer, size_t len,
                                      size_t *bytes_written, uint32_t timeout_ms)
{
    return bsp_extra_audio_write(BSP_EXTRA_AUDIO_CLIENT_MUSIC, audio_buffer,
                                 len, bytes_written, timeout_ms);
}

static esp_err_t music_clock_function(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    const bsp_extra_audio_session_config_t config = {
        .enable_input = false,
        .enable_output = true,
        .sample_rate = rate,
        .bits_per_sample = bits_cfg,
        .channel_mode = ch,
    };
    esp_err_t ret = bsp_extra_audio_reconfigure(BSP_EXTRA_AUDIO_CLIENT_MUSIC, &config);
    if (ret == ESP_OK && lock_audio(portMAX_DELAY) == ESP_OK) {
        s_player_session_config = config;
        unlock_audio();
    }
    return ret;
}

static void update_codec_result(esp_err_t *result, esp_err_t codec_result)
{
    if (*result == ESP_OK && codec_result != ESP_OK) {
        *result = codec_result;
    }
}

static esp_err_t validate_session_config(const bsp_extra_audio_session_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Audio session config is NULL");
    ESP_RETURN_ON_FALSE(config->enable_input || config->enable_output,
                        ESP_ERR_INVALID_ARG, TAG, "Audio session has no enabled direction");
    ESP_RETURN_ON_FALSE(config->sample_rate > 0 && config->bits_per_sample > 0,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio session format");
    ESP_RETURN_ON_FALSE(config->channel_mode == I2S_SLOT_MODE_MONO ||
                        config->channel_mode == I2S_SLOT_MODE_STEREO,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio channel mode");
    return ESP_OK;
}

static esp_err_t configure_session_locked(const bsp_extra_audio_session_config_t *config)
{
    esp_err_t ret = config->enable_input ?
                    codec_set_record_fs_locked(config->sample_rate,
                                               config->bits_per_sample,
                                               config->channel_mode) :
                    codec_set_fs_locked(config->sample_rate,
                                        config->bits_per_sample,
                                        config->channel_mode);
    if (ret == ESP_OK) {
        s_session_config = *config;
    }
    return ret;
}

esp_err_t bsp_extra_audio_acquire(bsp_extra_audio_client_t client,
                                  const bsp_extra_audio_session_config_t *config,
                                  TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(client != BSP_EXTRA_AUDIO_CLIENT_NONE,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio client");
    ESP_RETURN_ON_ERROR(validate_session_config(config), TAG, "Invalid audio session config");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "Initialize codec failed");
    TickType_t started = xTaskGetTickCount();
    while (true) {
        TickType_t elapsed = xTaskGetTickCount() - started;
        TickType_t remaining = timeout == portMAX_DELAY ? portMAX_DELAY :
                               (elapsed < timeout ? timeout - elapsed : 0);
        ESP_RETURN_ON_ERROR(lock_audio(remaining), TAG, "Lock audio device failed");
        if (!s_session_active) {
            break;
        }
        unlock_audio();
        if (timeout != portMAX_DELAY && elapsed >= timeout) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_session_stopping = true;
    esp_err_t ret = configure_session_locked(config);
    if (ret == ESP_OK) {
        s_audio_owner = client;
        s_session_active = true;
    } else {
        s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
        memset(&s_session_config, 0, sizeof(s_session_config));
    }
    s_session_stopping = false;
    unlock_audio();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Audio session acquired: client=%d input=%d output=%d",
                 client, config->enable_input, config->enable_output);
    }
    return ret;
}

esp_err_t bsp_extra_audio_reconfigure(bsp_extra_audio_client_t client,
                                      const bsp_extra_audio_session_config_t *config)
{
    ESP_RETURN_ON_ERROR(validate_session_config(config), TAG, "Invalid audio session config");
    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock audio device failed");
    if (!s_session_active || s_audio_owner != client || s_session_stopping) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }
    if (s_input_in_flight != 0 || s_output_in_flight != 0) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }

    s_session_stopping = true;
    esp_err_t ret = configure_session_locked(config);
    if (ret != ESP_OK) {
        s_session_active = false;
        s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
        memset(&s_session_config, 0, sizeof(s_session_config));
    }
    s_session_stopping = false;
    unlock_audio();
    return ret;
}

static esp_err_t begin_session_io(bsp_extra_audio_client_t client, bool input)
{
    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock audio device failed");
    bool direction_enabled = input ? s_session_config.enable_input : s_session_config.enable_output;
    bool device_opened = input ? s_record_device_opened : s_play_device_opened;
    if (!s_session_active || s_session_stopping || s_audio_owner != client ||
            !direction_enabled || !device_opened) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }
    if (input) {
        ++s_input_in_flight;
    } else {
        ++s_output_in_flight;
    }
    unlock_audio();
    return ESP_OK;
}

static void end_session_io(bool input)
{
    if (lock_audio(portMAX_DELAY) != ESP_OK) {
        return;
    }
    uint32_t *in_flight = input ? &s_input_in_flight : &s_output_in_flight;
    if (*in_flight > 0) {
        --(*in_flight);
    }
    unlock_audio();
}

esp_err_t bsp_extra_audio_read(bsp_extra_audio_client_t client, void *audio_buffer,
                               size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (bytes_read != NULL) {
        *bytes_read = 0;
    }
    ESP_RETURN_ON_FALSE(audio_buffer != NULL && len > 0,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio read buffer");
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    TickType_t timeout = timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_input_mutex, timeout) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Lock audio input failed");
    esp_err_t ret = begin_session_io(client, true);
    if (ret == ESP_OK) {
        ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
        if (ret == ESP_OK && bytes_read != NULL) {
            *bytes_read = len;
        }
        end_session_io(true);
    }
    xSemaphoreGive(s_input_mutex);
    return ret;
}

esp_err_t bsp_extra_audio_write(bsp_extra_audio_client_t client, const void *audio_buffer,
                                size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    if (bytes_written != NULL) {
        *bytes_written = 0;
    }
    ESP_RETURN_ON_FALSE(audio_buffer != NULL && len > 0,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid audio write buffer");
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    TickType_t timeout = timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_output_mutex, timeout) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Lock audio output failed");
    esp_err_t ret = begin_session_io(client, false);
    if (ret == ESP_OK) {
        ret = esp_codec_dev_write(play_dev_handle, (void *)audio_buffer, len);
        if (ret == ESP_OK && bytes_written != NULL) {
            *bytes_written = len;
        }
        end_session_io(false);
    }
    xSemaphoreGive(s_output_mutex);
    return ret;
}

esp_err_t bsp_extra_audio_release(bsp_extra_audio_client_t client, TickType_t timeout)
{
    TickType_t started = xTaskGetTickCount();
    ESP_RETURN_ON_ERROR(lock_audio(timeout), TAG, "Lock audio device failed");
    if (!s_session_active || s_session_stopping || s_audio_owner != client) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }
    s_session_stopping = true;
    unlock_audio();

    while (true) {
        ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock audio device failed");
        bool idle = s_input_in_flight == 0 && s_output_in_flight == 0;
        if (idle) {
            esp_err_t ret = codec_stop_locked();
            s_session_active = false;
            s_session_stopping = false;
            s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
            memset(&s_session_config, 0, sizeof(s_session_config));
            unlock_audio();
            ESP_LOGI(TAG, "Audio session released: client=%d", client);
            return ret;
        }
        if (timeout != portMAX_DELAY && (xTaskGetTickCount() - started) >= timeout) {
            s_session_stopping = false;
            unlock_audio();
            return ESP_ERR_TIMEOUT;
        }
        unlock_audio();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_output_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Lock audio output failed");
    esp_err_t lock_ret = lock_audio(portMAX_DELAY);
    if (lock_ret != ESP_OK) {
        xSemaphoreGive(s_output_mutex);
        return lock_ret;
    }
    if (!play_dev_handle) {
        unlock_audio();
        xSemaphoreGive(s_output_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (volume < 0 || volume > 100) {
        unlock_audio();
        xSemaphoreGive(s_output_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    s_volume_percent = volume;
    esp_err_t ret = s_play_device_opened ?
                    esp_codec_dev_set_out_vol(play_dev_handle, volume) : ESP_OK;
    if (volume_set != NULL) {
        *volume_set = s_volume_percent;
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Setting volume: %d", volume);
    }
    unlock_audio();
    xSemaphoreGive(s_output_mutex);
    return ret;
}

int bsp_extra_codec_volume_get(void)
{
    if (lock_audio(portMAX_DELAY) != ESP_OK) {
        return CODEC_DEFAULT_VOLUME;
    }
    int volume = s_volume_percent;
    unlock_audio();
    return volume;
}

esp_err_t bsp_extra_codec_mute_set(bool enable)
{
    ESP_RETURN_ON_ERROR(ensure_audio_mutex(), TAG, "Create audio mutex failed");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_output_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Lock audio output failed");
    esp_err_t lock_ret = lock_audio(portMAX_DELAY);
    if (lock_ret != ESP_OK) {
        xSemaphoreGive(s_output_mutex);
        return lock_ret;
    }
    if (s_session_active) {
        unlock_audio();
        xSemaphoreGive(s_output_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (!play_dev_handle) {
        unlock_audio();
        xSemaphoreGive(s_output_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
    unlock_audio();
    xSemaphoreGive(s_output_mutex);
    return ret;
}

esp_err_t bsp_extra_codec_init(void)
{
    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock audio device failed");
    if (s_audio_initialized) {
        unlock_audio();
        return ESP_OK;
    }

    esp_err_t ret;

    play_dev_handle = bsp_audio_codec_speaker_init();
    if (!play_dev_handle) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    record_dev_handle = bsp_audio_codec_microphone_init();
    if (!record_dev_handle) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    ret = codec_set_fs_locked(
              CODEC_DEFAULT_SAMPLE_RATE,
              CODEC_DEFAULT_BIT_WIDTH,
              CODEC_DEFAULT_CHANNEL
          );
    if (ret != ESP_OK) {
        goto cleanup;
    }
    s_audio_initialized = true;
    s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
    s_session_active = false;
    s_session_stopping = false;
    unlock_audio();
    return ESP_OK;

cleanup:
    if (record_dev_handle != NULL) {
        esp_codec_dev_delete(record_dev_handle);
    }
    if (play_dev_handle != NULL) {
        esp_codec_dev_delete(play_dev_handle);
    }
    play_dev_handle = NULL;
    record_dev_handle = NULL;
    s_play_device_opened = false;
    s_record_device_opened = false;
    s_output_fs_valid = false;
    s_audio_owner = BSP_EXTRA_AUDIO_CLIENT_NONE;
    s_session_active = false;
    s_session_stopping = false;
    s_input_in_flight = 0;
    s_output_in_flight = 0;
    memset(&s_session_config, 0, sizeof(s_session_config));
    unlock_audio();
    return ret;
}

esp_err_t bsp_extra_player_init(void)
{
    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player state failed");
    if (s_player_initialized) {
        unlock_audio();
        return ESP_OK;
    }
    if (s_player_initializing) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }
    s_player_initializing = true;
    unlock_audio();

    const bsp_extra_audio_session_config_t session_config = {
        .enable_input = false,
        .enable_output = true,
        .sample_rate = CODEC_DEFAULT_SAMPLE_RATE,
        .bits_per_sample = CODEC_DEFAULT_BIT_WIDTH,
        .channel_mode = CODEC_DEFAULT_CHANNEL,
    };
    esp_err_t ret = bsp_extra_audio_acquire(
                        BSP_EXTRA_AUDIO_CLIENT_MUSIC, &session_config,
                        pdMS_TO_TICKS(1000)
                    );
    if (ret != ESP_OK) {
        if (lock_audio(portMAX_DELAY) == ESP_OK) {
            s_player_initializing = false;
            unlock_audio();
        }
        ESP_LOGE(TAG, "Acquire music audio session failed: %s", esp_err_to_name(ret));
        return ret;
    }
    audio_player_config_t config = { .mute_fn = audio_mute_function,
                                     .write_fn = music_write_function,
                                     .clk_set_fn = music_clock_function,
                                     .priority = 5
                                   };
    ret = audio_player_new(config);
    if (ret != ESP_OK) {
        esp_err_t stop_ret = bsp_extra_audio_release(
                                 BSP_EXTRA_AUDIO_CLIENT_MUSIC, pdMS_TO_TICKS(1000)
                             );
        if (stop_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop codec after player initialization error: %s",
                     esp_err_to_name(stop_ret));
        }
        ESP_LOGE(TAG, "audio_player_init failed: %s", esp_err_to_name(ret));
        if (lock_audio(portMAX_DELAY) == ESP_OK) {
            s_player_initializing = false;
            unlock_audio();
        }
        return ret;
    }
    audio_player_callback_register(audio_callback, NULL);

    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player state failed");
    s_player_initialized = true;
    s_player_initializing = false;
    s_player_session_active = true;
    s_player_session_config = session_config;
    unlock_audio();

    return ESP_OK;
}

esp_err_t bsp_extra_player_del(void)
{
    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player state failed");
    bool player_initialized = s_player_initialized;
    unlock_audio();
    if (!player_initialized) {
        return ESP_OK;
    }
    esp_err_t ret = audio_player_delete();
    bool release_session = false;
    if (ret == ESP_OK) {
        ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player state failed");
        release_session = s_player_session_active;
        s_player_initialized = false;
        s_player_session_active = false;
        s_player_resume_after_suspend = false;
        s_audio_file_path[0] = '\0';
        unlock_audio();
    }
    if (release_session) {
        update_codec_result(
            &ret,
            bsp_extra_audio_release(BSP_EXTRA_AUDIO_CLIENT_MUSIC, pdMS_TO_TICKS(1000))
        );
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Delete audio player or stop codec failed");

    return ESP_OK;
}

esp_err_t bsp_extra_player_suspend(TickType_t timeout)
{
    ESP_RETURN_ON_ERROR(lock_audio(timeout), TAG, "Lock player state failed");
    if (!s_player_initialized || !s_player_session_active) {
        unlock_audio();
        return ESP_OK;
    }
    unlock_audio();

    bool was_playing = audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING;
    esp_err_t ret = ESP_OK;
    if (was_playing) {
        ret = audio_player_pause();
        if (ret != ESP_OK) {
            return ret;
        }
    }
    TickType_t started = xTaskGetTickCount();
    while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
        if (timeout != portMAX_DELAY && (xTaskGetTickCount() - started) >= timeout) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    ret = bsp_extra_audio_release(BSP_EXTRA_AUDIO_CLIENT_MUSIC, timeout);
    if (ret == ESP_OK && lock_audio(portMAX_DELAY) == ESP_OK) {
        s_player_session_active = false;
        s_player_resume_after_suspend = was_playing;
        unlock_audio();
    } else if (ret != ESP_OK && was_playing) {
        esp_err_t resume_ret = audio_player_resume();
        if (resume_ret != ESP_OK) {
            ESP_LOGE(TAG, "Restore music after suspend failure failed: %s",
                     esp_err_to_name(resume_ret));
        }
    }
    return ret;
}

esp_err_t bsp_extra_player_resume_session(TickType_t timeout)
{
    ESP_RETURN_ON_ERROR(lock_audio(timeout), TAG, "Lock player state failed");
    if (!s_player_initialized) {
        unlock_audio();
        return ESP_ERR_INVALID_STATE;
    }
    if (s_player_session_active) {
        unlock_audio();
        return ESP_OK;
    }
    unlock_audio();

    bsp_extra_audio_session_config_t config;
    ESP_RETURN_ON_ERROR(lock_audio(timeout), TAG, "Lock music format failed");
    config = s_player_session_config;
    unlock_audio();
    esp_err_t ret = bsp_extra_audio_acquire(
                        BSP_EXTRA_AUDIO_CLIENT_MUSIC, &config, timeout
                    );
    if (ret != ESP_OK) {
        return ret;
    }
    bool resume_playback = false;
    if (lock_audio(portMAX_DELAY) == ESP_OK) {
        s_player_session_active = true;
        resume_playback = s_player_resume_after_suspend;
        s_player_resume_after_suspend = false;
        unlock_audio();
    }
    ret = resume_playback ? audio_player_resume() : ESP_OK;
    if (ret != ESP_OK) {
        (void)bsp_extra_audio_release(BSP_EXTRA_AUDIO_CLIENT_MUSIC, timeout);
        if (lock_audio(portMAX_DELAY) == ESP_OK) {
            s_player_session_active = false;
            unlock_audio();
        }
    }
    return ret;
}

esp_err_t bsp_extra_file_instance_init(const char *path, file_iterator_instance_t **ret_instance)
{
    ESP_RETURN_ON_FALSE(path, ESP_FAIL, TAG, "path is NULL");
    ESP_RETURN_ON_FALSE(ret_instance, ESP_FAIL, TAG, "ret_instance is NULL");

    file_iterator_instance_t *file_iterator = file_iterator_new(path);
    ESP_RETURN_ON_FALSE(file_iterator, ESP_FAIL, TAG, "file_iterator_new failed, %s", path);

    *ret_instance = file_iterator;

    return ESP_OK;
}

esp_err_t bsp_extra_player_play_index(file_iterator_instance_t *instance, int index)
{
    ESP_RETURN_ON_FALSE(instance, ESP_ERR_INVALID_ARG, TAG, "instance is NULL");

    ESP_LOGI(TAG, "play_index(%d)", index);
    char filename[sizeof(s_audio_file_path)] = {0};
    int retval = file_iterator_get_full_path_from_index(instance, index, filename, sizeof(filename));
    ESP_RETURN_ON_FALSE(retval != 0, ESP_FAIL, TAG, "file_iterator_get_full_path_from_index failed");
    ESP_RETURN_ON_FALSE(strnlen(filename, sizeof(filename)) < sizeof(filename),
                        ESP_ERR_INVALID_SIZE, TAG, "Audio file path is too long");

    ESP_LOGI(TAG, "opening file '%s'", filename);
    FILE *fp = fopen(filename, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", filename);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player path failed");
    strlcpy(s_audio_file_path, filename, sizeof(s_audio_file_path));
    unlock_audio();

    return ESP_OK;
}

esp_err_t bsp_extra_player_play_file(const char *file_path)
{
    ESP_RETURN_ON_FALSE(file_path, ESP_ERR_INVALID_ARG, TAG, "file_path is NULL");
    ESP_RETURN_ON_FALSE(strnlen(file_path, sizeof(s_audio_file_path)) < sizeof(s_audio_file_path),
                        ESP_ERR_INVALID_SIZE, TAG, "Audio file path is too long");
    ESP_LOGI(TAG, "opening file '%s'", file_path);
    FILE *fp = fopen(file_path, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", file_path);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_RETURN_ON_ERROR(lock_audio(portMAX_DELAY), TAG, "Lock player path failed");
    strlcpy(s_audio_file_path, file_path, sizeof(s_audio_file_path));
    unlock_audio();

    return ESP_OK;
}

void bsp_extra_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    if (lock_audio(portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Lock audio callback state failed");
        return;
    }
    s_audio_idle_callback = cb;
    s_audio_idle_cb_user_data = user_data;
    unlock_audio();
}

bool bsp_extra_player_is_playing_by_path(const char *file_path)
{
    if (file_path == NULL || lock_audio(portMAX_DELAY) != ESP_OK) {
        return false;
    }
    bool is_playing = s_audio_file_path[0] != '\0' &&
                      strcmp(s_audio_file_path, file_path) == 0;
    unlock_audio();
    return is_playing;
}

bool bsp_extra_player_is_playing_by_index(file_iterator_instance_t *instance, int index)
{
    return instance != NULL && index == file_iterator_get_index(instance);
}
