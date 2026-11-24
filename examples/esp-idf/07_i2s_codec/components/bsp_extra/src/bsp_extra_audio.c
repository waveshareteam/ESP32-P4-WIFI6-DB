/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

static const char *TAG = "bsp_extra_audio";

static esp_codec_dev_handle_t s_play_dev;
static esp_codec_dev_handle_t s_record_dev;
static bool s_audio_initialized;
static bool s_play_opened;
static bool s_record_opened;
static int s_volume = CODEC_DEFAULT_VOLUME;
static esp_codec_dev_sample_info_t s_sample_info = {
    .sample_rate = CODEC_DEFAULT_SAMPLE_RATE,
    .channel = CODEC_DEFAULT_CHANNEL,
    .bits_per_sample = CODEC_DEFAULT_BIT_WIDTH,
};

static void restore_previous_format(bool restore_play, bool restore_record)
{
    if (s_play_opened) {
        esp_err_t ret = esp_codec_dev_close(s_play_dev);
        if (ret == ESP_OK) {
            s_play_opened = false;
        } else {
            ESP_LOGE(TAG, "Failed to close speaker codec during rollback: %s",
                     esp_err_to_name(ret));
        }
    }
    if (s_record_opened) {
        esp_err_t ret = esp_codec_dev_close(s_record_dev);
        if (ret == ESP_OK) {
            s_record_opened = false;
        } else {
            ESP_LOGE(TAG, "Failed to close microphone codec during rollback: %s",
                     esp_err_to_name(ret));
        }
    }

    if (restore_play && s_play_dev != NULL && !s_play_opened) {
        esp_err_t ret = esp_codec_dev_open(s_play_dev, &s_sample_info);
        s_play_opened = (ret == ESP_OK);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore speaker format: %s", esp_err_to_name(ret));
        }
    }
    if (restore_record && s_record_dev != NULL && !s_record_opened) {
        esp_err_t ret = esp_codec_dev_set_in_gain(s_record_dev, CODEC_DEFAULT_MIC_GAIN_DB);
        if (ret == ESP_OK) {
            ret = esp_codec_dev_open(s_record_dev, &s_sample_info);
        }
        s_record_opened = (ret == ESP_OK);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore microphone format: %s", esp_err_to_name(ret));
        }
    }
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(s_play_dev && s_record_dev, ESP_ERR_INVALID_STATE, TAG,
                        "codec devices are not initialized");
    ESP_RETURN_ON_FALSE(rate > 0 && bits_cfg > 0 &&
                        (ch == I2S_SLOT_MODE_MONO || ch == I2S_SLOT_MODE_STEREO),
                        ESP_ERR_INVALID_ARG, TAG, "invalid codec sample format");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };
    const bool restore_play = s_play_opened;
    const bool restore_record = s_record_opened;
    esp_err_t ret;

    if (s_play_opened) {
        ret = esp_codec_dev_close(s_play_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close speaker codec: %s", esp_err_to_name(ret));
            return ret;
        }
        s_play_opened = false;
    }
    if (s_record_opened) {
        ret = esp_codec_dev_close(s_record_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close microphone codec: %s", esp_err_to_name(ret));
            if (restore_play) {
                esp_err_t restore_ret = esp_codec_dev_open(s_play_dev, &s_sample_info);
                s_play_opened = (restore_ret == ESP_OK);
                if (restore_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to restore speaker format: %s",
                             esp_err_to_name(restore_ret));
                }
            }
            return ret;
        }
        s_record_opened = false;
    }

    if (s_play_dev) {
        ret = esp_codec_dev_open(s_play_dev, &fs);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open speaker codec: %s", esp_err_to_name(ret));
            restore_previous_format(restore_play, restore_record);
            return ret;
        }
        s_play_opened = true;
    }
    if (s_record_dev) {
        ret = esp_codec_dev_set_in_gain(s_record_dev, CODEC_DEFAULT_MIC_GAIN_DB);
        if (ret == ESP_OK) {
            ret = esp_codec_dev_open(s_record_dev, &fs);
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open microphone codec: %s", esp_err_to_name(ret));
            restore_previous_format(restore_play, restore_record);
            return ret;
        }
        s_record_opened = true;
    }

    s_sample_info = fs;
    return ESP_OK;
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_FALSE(s_play_dev && s_play_opened, ESP_ERR_INVALID_STATE, TAG,
                        "speaker codec is not open");
    ESP_RETURN_ON_FALSE(volume >= 0 && volume <= 100, ESP_ERR_INVALID_ARG, TAG,
                        "volume must be in the range 0 to 100");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_play_dev, volume), TAG, "set codec volume failed");

    s_volume = volume;
    if (volume_set) {
        *volume_set = s_volume;
    }
    return ESP_OK;
}

esp_err_t bsp_extra_codec_init(void)
{
    if (s_audio_initialized) {
        return ESP_OK;
    }

    s_play_dev = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_FAIL, TAG, "speaker codec init failed");

    s_record_dev = bsp_audio_codec_microphone_init();
    if (s_record_dev == NULL) {
        esp_codec_dev_delete(s_play_dev);
        s_play_dev = NULL;
        ESP_LOGE(TAG, "Microphone codec init failed");
        return ESP_FAIL;
    }

    esp_err_t ret = bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE,
                                           CODEC_DEFAULT_BIT_WIDTH,
                                           CODEC_DEFAULT_CHANNEL);
    if (ret == ESP_OK) {
        ret = bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL);
    }
    if (ret != ESP_OK) {
        restore_previous_format(false, false);
        esp_codec_dev_delete(s_record_dev);
        esp_codec_dev_delete(s_play_dev);
        s_record_dev = NULL;
        s_play_dev = NULL;
        ESP_LOGE(TAG, "Codec initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_audio_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read)
{
    if (bytes_read) {
        *bytes_read = 0;
    }
    ESP_RETURN_ON_FALSE(s_record_dev && s_record_opened, ESP_ERR_INVALID_STATE, TAG,
                        "microphone codec is not open");
    ESP_RETURN_ON_FALSE(audio_buffer && len > 0 && bytes_read, ESP_ERR_INVALID_ARG, TAG,
                        "invalid microphone read arguments");

    esp_err_t ret = esp_codec_dev_read(s_record_dev, audio_buffer, len);
    if (ret == ESP_OK) {
        *bytes_read = len;
    }
    return ret;
}

esp_err_t bsp_extra_i2s_write(const void *audio_buffer, size_t len, size_t *bytes_written)
{
    if (bytes_written) {
        *bytes_written = 0;
    }
    ESP_RETURN_ON_FALSE(s_play_dev && s_play_opened, ESP_ERR_INVALID_STATE, TAG,
                        "speaker codec is not open");
    ESP_RETURN_ON_FALSE(audio_buffer && len > 0 && bytes_written, ESP_ERR_INVALID_ARG, TAG,
                        "invalid speaker write arguments");

    esp_err_t ret = esp_codec_dev_write(s_play_dev, (void *)audio_buffer, len);
    if (ret == ESP_OK) {
        *bytes_written = len;
    }
    return ret;
}
