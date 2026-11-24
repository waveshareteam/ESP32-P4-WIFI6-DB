/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xiaozhi_services xiaozhi_services_t;

typedef enum {
    XIAOZHI_SERVICES_LISTENING_AUTO_STOP = 0,
    XIAOZHI_SERVICES_LISTENING_MANUAL_STOP,
    XIAOZHI_SERVICES_LISTENING_REALTIME,
} xiaozhi_services_listening_mode_t;

typedef enum {
    XIAOZHI_SERVICES_ABORT_NONE = 0,
    XIAOZHI_SERVICES_ABORT_WAKE_WORD_DETECTED,
} xiaozhi_services_abort_reason_t;

typedef enum {
    XIAOZHI_SERVICES_TEXT_ROLE_USER = 0,
    XIAOZHI_SERVICES_TEXT_ROLE_ASSISTANT,
} xiaozhi_services_text_role_t;

typedef enum {
    XIAOZHI_SERVICES_TTS_START = 0,
    XIAOZHI_SERVICES_TTS_STOP,
    XIAOZHI_SERVICES_TTS_SENTENCE_START,
} xiaozhi_services_tts_state_t;

typedef struct {
    bool has_mqtt_config;
    bool has_websocket_config;
    int sample_rate;
    int channels;
    int frame_duration_ms;
    int playback_sample_rate;
} xiaozhi_services_config_t;

typedef struct {
    bool has_activation_code;
    bool has_activation_challenge;
    bool has_mqtt_config;
    bool has_websocket_config;
    int activation_timeout_ms;
    char *activation_code;
    char *activation_message;
    char *activation_challenge;
} xiaozhi_services_info_t;

typedef struct {
    void *context;

    void (*on_connected)(void *context);
    void (*on_disconnected)(void *context);
    void (*on_audio_channel_opened)(void *context);
    void (*on_audio_channel_closed)(void *context);
    void (*on_server_goodbye)(void *context);
    void (*on_text)(
        xiaozhi_services_text_role_t role,
        const char *text,
        void *context
    );
    void (*on_emotion)(const char *emotion, void *context);
    void (*on_tts)(
        xiaozhi_services_tts_state_t state,
        const char *text,
        void *context
    );
    void (*on_audio)(
        const uint8_t *data,
        size_t length,
        int sample_rate,
        int frame_duration_ms,
        uint32_t timestamp,
        void *context
    );
    void (*on_error)(esp_err_t error, const char *source, void *context);
} xiaozhi_services_callbacks_t;

esp_err_t xiaozhi_services_create(
    const xiaozhi_services_config_t *config,
    const xiaozhi_services_callbacks_t *callbacks,
    xiaozhi_services_t **out_services
);

esp_err_t xiaozhi_services_destroy(xiaozhi_services_t *services);
esp_err_t xiaozhi_services_start(xiaozhi_services_t *services);
esp_err_t xiaozhi_services_stop(xiaozhi_services_t *services);

esp_err_t xiaozhi_services_open_audio_channel(xiaozhi_services_t *services);
esp_err_t xiaozhi_services_close_audio_channel(xiaozhi_services_t *services);
bool xiaozhi_services_is_connected(const xiaozhi_services_t *services);
bool xiaozhi_services_is_audio_channel_open(
    const xiaozhi_services_t *services
);

esp_err_t xiaozhi_services_send_audio(
    xiaozhi_services_t *services,
    const uint8_t *data,
    size_t length,
    uint32_t timestamp
);

esp_err_t xiaozhi_services_send_wake_word(
    xiaozhi_services_t *services,
    const char *wake_word
);

esp_err_t xiaozhi_services_send_start_listening(
    xiaozhi_services_t *services,
    xiaozhi_services_listening_mode_t mode
);

esp_err_t xiaozhi_services_send_stop_listening(
    xiaozhi_services_t *services
);

esp_err_t xiaozhi_services_send_abort_speaking(
    xiaozhi_services_t *services,
    xiaozhi_services_abort_reason_t reason
);

/*
 * The caller must pass a zero-initialized structure on first use, or a
 * structure previously returned by this function and released with
 * xiaozhi_services_free_info().
 */
esp_err_t xiaozhi_services_get_info(xiaozhi_services_info_t *info);
void xiaozhi_services_free_info(xiaozhi_services_info_t *info);

#ifdef __cplusplus
}
#endif
