/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "driver/i2s_std.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE 16000
#define CODEC_DEFAULT_BIT_WIDTH   16
#define CODEC_DEFAULT_CHANNEL     I2S_SLOT_MODE_STEREO
#define CODEC_DEFAULT_VOLUME      CONFIG_EXAMPLE_VOICE_VOLUME
#define CODEC_DEFAULT_MIC_GAIN_DB CONFIG_EXAMPLE_MIC_GAIN_DB

/**
 * @brief Initialize the board speaker and microphone codec devices.
 *
 * @return ESP_OK on success, or an error returned by the BSP/codec driver.
 */
esp_err_t bsp_extra_codec_init(void);

/**
 * @brief Configure the sample format used by both codec devices.
 *
 * @param rate Sample rate in Hz.
 * @param bits_cfg Bits per sample.
 * @param ch I2S slot mode.
 * @return ESP_OK on success, or an error returned by the codec driver.
 */
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief Set speaker output volume.
 *
 * @param volume Volume percentage in the range 0 to 100.
 * @param[out] volume_set Optional pointer that receives the applied volume.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for an invalid volume, or a
 *         codec driver error.
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);

/**
 * @brief Read one audio block from the microphone codec.
 *
 * The underlying esp_codec_dev API is synchronous and does not expose a
 * timeout parameter.
 *
 * @param[out] audio_buffer Destination audio buffer.
 * @param len Number of bytes to read.
 * @param[out] bytes_read Number of bytes read; set to zero on failure.
 * @return ESP_OK on success, or an error returned by the codec driver.
 */
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read);

/**
 * @brief Write one audio block to the speaker codec.
 *
 * The underlying esp_codec_dev API is synchronous and does not expose a
 * timeout parameter.
 *
 * @param audio_buffer Source audio buffer.
 * @param len Number of bytes to write.
 * @param[out] bytes_written Number of bytes written; set to zero on failure.
 * @return ESP_OK on success, or an error returned by the codec driver.
 */
esp_err_t bsp_extra_i2s_write(const void *audio_buffer, size_t len, size_t *bytes_written);

#ifdef __cplusplus
}
#endif
