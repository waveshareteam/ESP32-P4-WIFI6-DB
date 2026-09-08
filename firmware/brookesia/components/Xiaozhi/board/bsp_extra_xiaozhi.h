/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The ES8311 path is a mono duplex stream shared by capture and playback. */
#define BSP_EXTRA_XIAOZHI_SAMPLE_RATE (16000U)
#define BSP_EXTRA_XIAOZHI_CHANNELS    (1U)
#define BSP_EXTRA_XIAOZHI_BITS        (16U)

/** Open the board's Xiaozhi capture/playback stream. */
esp_err_t bsp_extra_xiaozhi_audio_start(void);

/** Close the board's Xiaozhi capture/playback stream. */
esp_err_t bsp_extra_xiaozhi_audio_stop(void);

/** Read mono PCM from the board microphone. */
esp_err_t bsp_extra_xiaozhi_audio_read(
    void *audio_buffer,
    size_t len,
    size_t *bytes_read,
    uint32_t timeout_ms
);

/** Write mono PCM to the board speaker. */
esp_err_t bsp_extra_xiaozhi_audio_write(
    void *audio_buffer,
    size_t len,
    size_t *bytes_written,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif
