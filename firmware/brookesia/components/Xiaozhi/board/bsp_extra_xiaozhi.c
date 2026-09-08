/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp_extra_xiaozhi.h"

#include "bsp_board_extra.h"

esp_err_t bsp_extra_xiaozhi_audio_start(void)
{
    const bsp_extra_audio_session_config_t config = {
        .enable_input = true,
        .enable_output = true,
        .sample_rate = BSP_EXTRA_XIAOZHI_SAMPLE_RATE,
        .bits_per_sample = BSP_EXTRA_XIAOZHI_BITS,
        .channel_mode = I2S_SLOT_MODE_MONO,
    };
    return bsp_extra_audio_acquire(
               BSP_EXTRA_AUDIO_CLIENT_XIAOZHI, &config, pdMS_TO_TICKS(1000)
           );
}

esp_err_t bsp_extra_xiaozhi_audio_stop(void)
{
    return bsp_extra_audio_release(
               BSP_EXTRA_AUDIO_CLIENT_XIAOZHI, pdMS_TO_TICKS(1000)
           );
}

esp_err_t bsp_extra_xiaozhi_audio_read(
    void *audio_buffer,
    size_t len,
    size_t *bytes_read,
    uint32_t timeout_ms
)
{
    return bsp_extra_audio_read(
               BSP_EXTRA_AUDIO_CLIENT_XIAOZHI,
               audio_buffer, len, bytes_read, timeout_ms
           );
}

esp_err_t bsp_extra_xiaozhi_audio_write(
    void *audio_buffer,
    size_t len,
    size_t *bytes_written,
    uint32_t timeout_ms
)
{
    return bsp_extra_audio_write(
               BSP_EXTRA_AUDIO_CLIENT_XIAOZHI,
               audio_buffer, len, bytes_written, timeout_ms
           );
}
