/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "audio_player.h"
#include "file_iterator.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE           (16000)
#define CODEC_DEFAULT_BIT_WIDTH             (16)
#define CODEC_DEFAULT_ADC_VOLUME            (24.0)
#define CODEC_DEFAULT_CHANNEL               I2S_SLOT_MODE_STEREO
#define CODEC_DEFAULT_VOLUME                (80)

typedef enum {
    BSP_EXTRA_AUDIO_CLIENT_NONE = 0,
    BSP_EXTRA_AUDIO_CLIENT_MUSIC,
    BSP_EXTRA_AUDIO_CLIENT_VIDEO,
    BSP_EXTRA_AUDIO_CLIENT_XIAOZHI,
    BSP_EXTRA_AUDIO_CLIENT_SPECTRUM,
    BSP_EXTRA_AUDIO_CLIENT_PROMPT,
} bsp_extra_audio_client_t;

typedef struct {
    bool enable_input;
    bool enable_output;
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    i2s_slot_mode_t channel_mode;
} bsp_extra_audio_session_config_t;

#define BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX    (95)
#define BSP_LCD_BACKLIGHT_BRIGHTNESS_MIN    (0)
#define LCD_LEDC_CH                         (CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH)

/**************************************************************************************************
 * BSP Extra interface
 * Mainly provided some I2S Codec interfaces.
 **************************************************************************************************/
/**
 * @brief Get display brightness.
 *
 * @return
 *   - brightness in percent
 */
int bsp_display_brightness_get(void);

/**
 * @brief Player set mute.
 *
 * @param enable: true or false
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_mute_set(bool enable);

/**
 * @brief Player set volume.
 *
 * @param volume: volume set
 * @param volume_set: volume set response
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);

/**
 * @brief Player get volume.
 *
 * @return
 *   - volume: volume get
 */
int bsp_extra_codec_volume_get(void);

/** Acquire exclusive ownership of the shared codec. */
esp_err_t bsp_extra_audio_acquire(bsp_extra_audio_client_t client,
                                  const bsp_extra_audio_session_config_t *config,
                                  TickType_t timeout);

/** Reconfigure a session owned by client. */
esp_err_t bsp_extra_audio_reconfigure(bsp_extra_audio_client_t client,
                                      const bsp_extra_audio_session_config_t *config);

/** Read microphone PCM for the owning client. */
esp_err_t bsp_extra_audio_read(bsp_extra_audio_client_t client, void *audio_buffer,
                               size_t len, size_t *bytes_read, uint32_t timeout_ms);

/** Write speaker PCM for the owning client. */
esp_err_t bsp_extra_audio_write(bsp_extra_audio_client_t client, const void *audio_buffer,
                                size_t len, size_t *bytes_written, uint32_t timeout_ms);

/** Release a session owned by client after its I/O tasks have stopped. */
esp_err_t bsp_extra_audio_release(bsp_extra_audio_client_t client, TickType_t timeout);

/**
 * @brief Initialize codec play and record handle.
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Fail
 */
esp_err_t bsp_extra_codec_init(void);

/**
 * @brief Initialize audio player task.
 *
 * @param path file path
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Fail
 */
esp_err_t bsp_extra_player_init(void);

/**
 * @brief Delete audio player task.
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Fail
 */
esp_err_t bsp_extra_player_del(void);

/** Pause the player and release its audio session while keeping decoder state. */
esp_err_t bsp_extra_player_suspend(TickType_t timeout);

/** Reacquire the music session and resume a suspended player. */
esp_err_t bsp_extra_player_resume_session(TickType_t timeout);

/**
 * @brief Initialize a file iterator instance
 *
 * @param path The file path for the iterator.
 * @param ret_instance A pointer to the file iterator instance to be returned.
 * @return
 *     - ESP_OK: Successfully initialized the file iterator instance.
 *     - ESP_FAIL: Failed to initialize the file iterator instance due to invalid parameters or memory allocation failure.
 */
esp_err_t bsp_extra_file_instance_init(const char *path, file_iterator_instance_t **ret_instance);

/**
 * @brief Play the audio file at the specified index in the file iterator
 *
 * @param instance The file iterator instance.
 * @param index The index of the file to play within the iterator.
 * @return
 *     - ESP_OK: Successfully started playing the audio file.
 *     - ESP_FAIL: Failed to play the audio file due to invalid parameters or file access issues.
 */
esp_err_t bsp_extra_player_play_index(file_iterator_instance_t *instance, int index);

/**
 * @brief Play the audio file specified by the file path
 *
 * @param file_path The path to the audio file to be played.
 * @return
 *     - ESP_OK: Successfully started playing the audio file.
 *     - ESP_FAIL: Failed to play the audio file due to file access issues.
 */
esp_err_t bsp_extra_player_play_file(const char *file_path);

/**
 * @brief Register a callback function for the audio player
 *
 * @param cb The callback function to be registered.
 * @param user_data User data to be passed to the callback function.
 */
void bsp_extra_player_register_callback(audio_player_cb_t cb, void *user_data);

/**
 * @brief Check if the specified audio file is currently playing
 *
 * @param file_path The path to the audio file to check.
 * @return
 *     - true: The specified audio file is currently playing.
 *     - false: The specified audio file is not currently playing.
 */
bool bsp_extra_player_is_playing_by_path(const char *file_path);

/**
 * @brief Check if the audio file at the specified index is currently playing
 *
 * @param instance The file iterator instance.
 * @param index The index of the file to check.
 * @return
 *     - true: The audio file at the specified index is currently playing.
 *     - false: The audio file at the specified index is not currently playing.
 */
bool bsp_extra_player_is_playing_by_index(file_iterator_instance_t *instance, int index);

#ifdef __cplusplus
}
#endif
