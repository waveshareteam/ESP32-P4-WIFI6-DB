/*
 * Derived from 78/xiaozhi-esp32 (MIT).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOZHI_AUDIO_SAMPLE_RATE          (16000U)
#define XIAOZHI_AUDIO_INPUT_CHANNELS      (1U)
#define XIAOZHI_AUDIO_PCM_FRAME_SAMPLES   (960U)
#define XIAOZHI_AUDIO_WAKE_WORD_CACHE_SAMPLES \
    (XIAOZHI_AUDIO_SAMPLE_RATE * 2U)

typedef struct xiaozhi_audio_processor xiaozhi_audio_processor_t;

typedef struct {
    void *context;
    void (*on_wake_word)(const char *wake_word, void *context);
    void (*on_pcm_frame)(
        const int16_t *pcm,
        size_t sample_count,
        void *context
    );
    void (*on_vad)(bool speaking, void *context);
} xiaozhi_audio_processor_callbacks_t;

esp_err_t xiaozhi_audio_processor_create(
    const xiaozhi_audio_processor_callbacks_t *callbacks,
    xiaozhi_audio_processor_t **out_processor
);

esp_err_t xiaozhi_audio_processor_destroy(
    xiaozhi_audio_processor_t *processor
);

esp_err_t xiaozhi_audio_processor_initialize(
    xiaozhi_audio_processor_t *processor
);

esp_err_t xiaozhi_audio_processor_shutdown(
    xiaozhi_audio_processor_t *processor
);

esp_err_t xiaozhi_audio_processor_feed(
    xiaozhi_audio_processor_t *processor,
    const int16_t *interleaved_pcm,
    size_t frame_count
);

esp_err_t xiaozhi_audio_processor_enable_wake_word(
    xiaozhi_audio_processor_t *processor,
    bool enable
);

esp_err_t xiaozhi_audio_processor_enable_voice_processing(
    xiaozhi_audio_processor_t *processor,
    bool enable
);

bool xiaozhi_audio_processor_is_ready(
    const xiaozhi_audio_processor_t *processor
);

size_t xiaozhi_audio_processor_get_feed_frames(
    const xiaozhi_audio_processor_t *processor
);

esp_err_t xiaozhi_audio_processor_take_wake_word_pcm(
    xiaozhi_audio_processor_t *processor,
    int16_t *pcm,
    size_t capacity,
    size_t *sample_count
);

esp_err_t xiaozhi_audio_processor_set_callbacks(
    xiaozhi_audio_processor_t *processor,
    const xiaozhi_audio_processor_callbacks_t *callbacks
);

#ifdef __cplusplus
}
#endif
