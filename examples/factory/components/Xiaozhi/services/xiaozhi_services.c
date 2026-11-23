/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "xiaozhi_services.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mcp_engine.h"
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *tag = "xiaozhi_services";

struct xiaozhi_services {
    xiaozhi_services_config_t config;
    xiaozhi_services_callbacks_t callbacks;
    esp_xiaozhi_chat_handle_t chat;
    esp_mcp_t *mcp;
    esp_event_handler_instance_t event_instance;
    atomic_bool callbacks_enabled;
    atomic_bool connected;
    atomic_bool audio_channel_open;
    atomic_uint_least32_t transport_started_tick;
    atomic_uint_least32_t channel_open_started_tick;
    atomic_uint_least32_t audio_packets_sent;
    atomic_uint_least32_t audio_packets_received;
};

static const char *transport_name(const xiaozhi_services_t *services)
{
    return services != NULL && services->config.has_mqtt_config ?
           "MQTT+UDP" : "WebSocket";
}

static bool is_config_valid(const xiaozhi_services_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (!config->has_mqtt_config && !config->has_websocket_config) {
        return false;
    }
    if (config->sample_rate < 8000 || config->sample_rate > 48000) {
        return false;
    }
    if (config->channels < 1 || config->channels > 2) {
        return false;
    }
    if (config->frame_duration_ms < 10 || config->frame_duration_ms > 120) {
        return false;
    }
    return config->playback_sample_rate > 0;
}

static char *copy_string(const char *source)
{
    if (source == NULL) {
        return NULL;
    }

    size_t length = strlen(source) + 1;
    char *copy = malloc(length);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, length);
    return copy;
}

static void report_error(
    xiaozhi_services_t *services,
    esp_err_t error,
    const char *source
)
{
    if (services == NULL || !atomic_load(&services->callbacks_enabled)) {
        return;
    }
    if (services->callbacks.on_error != NULL) {
        services->callbacks.on_error(
            error,
            source != NULL ? source : "xiaozhi_services",
            services->callbacks.context
        );
    }
}

static esp_err_t register_developer_tools(esp_mcp_t *mcp)
{
    if (mcp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Add board-specific MCP tools here without exposing MCP to the UI. */
    return ESP_OK;
}

static void audio_callback(const uint8_t *data, int length, void *context)
{
    xiaozhi_services_t *services = context;
    if (services == NULL || !atomic_load(&services->callbacks_enabled) ||
            data == NULL || length <= 0 || services->callbacks.on_audio == NULL) {
        return;
    }

    atomic_fetch_add(&services->audio_packets_received, 1);
    services->callbacks.on_audio(
        data,
        (size_t)length,
        services->config.playback_sample_rate,
        services->config.frame_duration_ms,
        0,
        services->callbacks.context
    );
}

static void chat_event_callback(
    esp_xiaozhi_chat_event_t event,
    void *event_data,
    void *context
)
{
    xiaozhi_services_t *services = context;
    if (services == NULL || !atomic_load(&services->callbacks_enabled)) {
        return;
    }

    switch (event) {
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT: {
        esp_xiaozhi_chat_text_data_t *text = event_data;
        if (text != NULL && text->text != NULL &&
                services->callbacks.on_text != NULL) {
            xiaozhi_services_text_role_t role =
                text->role == ESP_XIAOZHI_CHAT_TEXT_ROLE_ASSISTANT ?
                XIAOZHI_SERVICES_TEXT_ROLE_ASSISTANT :
                XIAOZHI_SERVICES_TEXT_ROLE_USER;
            services->callbacks.on_text(
                role,
                text->text,
                services->callbacks.context
            );
        }
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI:
        if (event_data != NULL && services->callbacks.on_emotion != NULL) {
            services->callbacks.on_emotion(
                event_data,
                services->callbacks.context
            );
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE: {
        esp_xiaozhi_chat_tts_state_t *tts = event_data;
        if (tts == NULL || services->callbacks.on_tts == NULL) {
            break;
        }

        xiaozhi_services_tts_state_t state =
            XIAOZHI_SERVICES_TTS_SENTENCE_START;
        if (tts->state == ESP_XIAOZHI_CHAT_TTS_STATE_START) {
            state = XIAOZHI_SERVICES_TTS_START;
        } else if (tts->state == ESP_XIAOZHI_CHAT_TTS_STATE_STOP) {
            state = XIAOZHI_SERVICES_TTS_STOP;
        }
        services->callbacks.on_tts(
            state,
            tts->text != NULL ? tts->text : "",
            services->callbacks.context
        );
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR: {
        esp_xiaozhi_chat_error_info_t *error = event_data;
        report_error(
            services,
            error != NULL ? error->code : ESP_FAIL,
            error != NULL && error->source != NULL ?
            error->source : "esp_xiaozhi"
        );
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED:
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED:
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD:
        break;
    }
}

static void event_handler(
    void *context,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)event_base;
    (void)event_data;

    xiaozhi_services_t *services = context;
    if (services == NULL || !atomic_load(&services->callbacks_enabled)) {
        return;
    }

    switch (event_id) {
    case ESP_XIAOZHI_CHAT_EVENT_CONNECTED:
        atomic_store(&services->connected, true);
        ESP_LOGI(
            tag, "Connected via %s in %u ms", transport_name(services),
            (unsigned)pdTICKS_TO_MS(
                xTaskGetTickCount() -
                atomic_load(&services->transport_started_tick)
            )
        );
        if (services->callbacks.on_connected != NULL) {
            services->callbacks.on_connected(services->callbacks.context);
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED:
        atomic_store(&services->connected, false);
        atomic_store(&services->audio_channel_open, false);
        ESP_LOGI(
            tag, "Disconnected from %s (tx=%u, rx=%u)",
            transport_name(services),
            (unsigned)atomic_load(&services->audio_packets_sent),
            (unsigned)atomic_load(&services->audio_packets_received)
        );
        if (services->callbacks.on_disconnected != NULL) {
            services->callbacks.on_disconnected(services->callbacks.context);
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED:
        atomic_store(&services->audio_channel_open, true);
        ESP_LOGI(
            tag, "Audio channel opened in %u ms",
            (unsigned)pdTICKS_TO_MS(
                xTaskGetTickCount() -
                atomic_load(&services->channel_open_started_tick)
            )
        );
        if (services->callbacks.on_audio_channel_opened != NULL) {
            services->callbacks.on_audio_channel_opened(
                services->callbacks.context
            );
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        atomic_store(&services->audio_channel_open, false);
        if (services->callbacks.on_audio_channel_closed != NULL) {
            services->callbacks.on_audio_channel_closed(
                services->callbacks.context
            );
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE:
        if (atomic_exchange(&services->audio_channel_open, false) &&
                services->chat != 0) {
            (void)esp_xiaozhi_chat_close_audio_channel(services->chat);
        }
        ESP_LOGI(
            tag,
            "Server goodbye (%s, tx=%u, rx=%u)",
            transport_name(services),
            (unsigned)atomic_load(&services->audio_packets_sent),
            (unsigned)atomic_load(&services->audio_packets_received)
        );
        if (services->callbacks.on_server_goodbye != NULL) {
            services->callbacks.on_server_goodbye(services->callbacks.context);
        }
        break;
    default:
        break;
    }
}

esp_err_t xiaozhi_services_create(
    const xiaozhi_services_config_t *config,
    const xiaozhi_services_callbacks_t *callbacks,
    xiaozhi_services_t **out_services
)
{
    if (!is_config_valid(config) || out_services == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_services = NULL;
    xiaozhi_services_t *services = calloc(1, sizeof(*services));
    if (services == NULL) {
        return ESP_ERR_NO_MEM;
    }

    services->config = *config;
    if (callbacks != NULL) {
        services->callbacks = *callbacks;
    }
    atomic_init(&services->callbacks_enabled, false);
    atomic_init(&services->connected, false);
    atomic_init(&services->audio_channel_open, false);
    atomic_init(&services->transport_started_tick, 0);
    atomic_init(&services->channel_open_started_tick, 0);
    atomic_init(&services->audio_packets_sent, 0);
    atomic_init(&services->audio_packets_received, 0);
    *out_services = services;
    return ESP_OK;
}

esp_err_t xiaozhi_services_destroy(xiaozhi_services_t *services)
{
    if (services == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = xiaozhi_services_stop(services);
    free(services);
    return ret;
}

esp_err_t xiaozhi_services_start(xiaozhi_services_t *services)
{
    if (services == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (services->chat != 0) {
        return ESP_OK;
    }

    esp_err_t ret = esp_mcp_create(&services->mcp);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = register_developer_tools(services->mcp);
    if (ret != ESP_OK) {
        esp_mcp_destroy(services->mcp);
        services->mcp = NULL;
        return ret;
    }

    esp_xiaozhi_chat_config_t chat_config = {0};
    chat_config.audio_type = ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS;
    chat_config.audio_callback = audio_callback;
    chat_config.event_callback = chat_event_callback;
    chat_config.audio_callback_ctx = services;
    chat_config.event_callback_ctx = services;
    chat_config.mcp_engine = services->mcp;
    chat_config.owns_mcp_engine = false;
    chat_config.has_mqtt_config = services->config.has_mqtt_config;
    chat_config.has_websocket_config = services->config.has_websocket_config;

    ret = esp_xiaozhi_chat_init(&chat_config, &services->chat);
    if (ret != ESP_OK) {
        esp_mcp_destroy(services->mcp);
        services->mcp = NULL;
        return ret;
    }

    ret = esp_event_handler_instance_register(
        ESP_XIAOZHI_CHAT_EVENTS,
        ESP_EVENT_ANY_ID,
        event_handler,
        services,
        &services->event_instance
    );
    if (ret != ESP_OK) {
        (void)esp_xiaozhi_chat_deinit(services->chat);
        services->chat = 0;
        esp_mcp_destroy(services->mcp);
        services->mcp = NULL;
        return ret;
    }

    atomic_store(&services->transport_started_tick, xTaskGetTickCount());
    atomic_store(&services->callbacks_enabled, true);
    ret = esp_xiaozhi_chat_start(services->chat);
    if (ret != ESP_OK) {
        atomic_store(&services->callbacks_enabled, false);
        (void)esp_event_handler_instance_unregister(
            ESP_XIAOZHI_CHAT_EVENTS,
            ESP_EVENT_ANY_ID,
            services->event_instance
        );
        services->event_instance = NULL;
        (void)esp_xiaozhi_chat_deinit(services->chat);
        services->chat = 0;
        esp_mcp_destroy(services->mcp);
        services->mcp = NULL;
        return ret;
    }

    ESP_LOGI(
        tag,
        "Started managed Xiaozhi transport (%s)",
        transport_name(services)
    );
    return ESP_OK;
}

esp_err_t xiaozhi_services_stop(xiaozhi_services_t *services)
{
    if (services == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    atomic_store(&services->callbacks_enabled, false);
    atomic_store(&services->connected, false);
    atomic_store(&services->audio_channel_open, false);

    if (services->event_instance != NULL) {
        (void)esp_event_handler_instance_unregister(
            ESP_XIAOZHI_CHAT_EVENTS,
            ESP_EVENT_ANY_ID,
            services->event_instance
        );
        services->event_instance = NULL;
    }

    esp_err_t ret = ESP_OK;
    if (services->chat != 0) {
        esp_xiaozhi_chat_handle_t chat = services->chat;
        services->chat = 0;
        ret = esp_xiaozhi_chat_deinit(chat);
    }
    if (services->mcp != NULL) {
        esp_mcp_destroy(services->mcp);
        services->mcp = NULL;
    }
    ESP_LOGI(
        tag, "Stopped %s transport (tx=%u, rx=%u)",
        transport_name(services),
        (unsigned)atomic_load(&services->audio_packets_sent),
        (unsigned)atomic_load(&services->audio_packets_received)
    );
    return ret;
}

esp_err_t xiaozhi_services_open_audio_channel(xiaozhi_services_t *services)
{
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (atomic_load(&services->audio_channel_open)) {
        return ESP_OK;
    }

    esp_xiaozhi_chat_audio_t audio = {0};
    audio.format = "opus";
    audio.sample_rate = services->config.sample_rate;
    audio.channels = services->config.channels;
    audio.frame_duration = services->config.frame_duration_ms;

    atomic_store(&services->channel_open_started_tick, xTaskGetTickCount());
    atomic_store(&services->audio_packets_sent, 0);
    atomic_store(&services->audio_packets_received, 0);
    esp_err_t ret = esp_xiaozhi_chat_open_audio_channel(
        services->chat,
        &audio,
        NULL,
        0
    );
    if (ret == ESP_OK) {
        atomic_store(&services->audio_channel_open, true);
    }
    return ret;
}

esp_err_t xiaozhi_services_close_audio_channel(xiaozhi_services_t *services)
{
    if (services == NULL || services->chat == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&services->audio_channel_open)) {
        return ESP_OK;
    }
    if (!atomic_load(&services->connected)) {
        atomic_store(&services->audio_channel_open, false);
        return ESP_OK;
    }

    esp_err_t ret = esp_xiaozhi_chat_close_audio_channel(services->chat);
    if (ret == ESP_OK) {
        atomic_store(&services->audio_channel_open, false);
    }
    return ret;
}

bool xiaozhi_services_is_connected(const xiaozhi_services_t *services)
{
    return services != NULL && atomic_load(&services->connected);
}

bool xiaozhi_services_is_audio_channel_open(
    const xiaozhi_services_t *services
)
{
    return services != NULL && atomic_load(&services->audio_channel_open);
}

esp_err_t xiaozhi_services_send_audio(
    xiaozhi_services_t *services,
    const uint8_t *data,
    size_t length,
    uint32_t timestamp
)
{
    (void)timestamp;
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected) ||
            !atomic_load(&services->audio_channel_open)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_xiaozhi_chat_send_audio_data(
        services->chat,
        (const char *)data,
        length
    );
    if (ret == ESP_OK) {
        atomic_fetch_add(&services->audio_packets_sent, 1);
    }
    return ret;
}

esp_err_t xiaozhi_services_send_wake_word(
    xiaozhi_services_t *services,
    const char *wake_word
)
{
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected) ||
            !atomic_load(&services->audio_channel_open)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (wake_word == NULL || wake_word[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_xiaozhi_chat_send_wake_word(services->chat, wake_word);
}

esp_err_t xiaozhi_services_send_start_listening(
    xiaozhi_services_t *services,
    xiaozhi_services_listening_mode_t mode
)
{
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected) ||
            !atomic_load(&services->audio_channel_open)) {
        return ESP_ERR_INVALID_STATE;
    }

    int component_mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO;
    switch (mode) {
    case XIAOZHI_SERVICES_LISTENING_AUTO_STOP:
        component_mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO;
        break;
    case XIAOZHI_SERVICES_LISTENING_MANUAL_STOP:
        component_mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL;
        break;
    case XIAOZHI_SERVICES_LISTENING_REALTIME:
        component_mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return esp_xiaozhi_chat_send_start_listening(
        services->chat,
        component_mode
    );
}

esp_err_t xiaozhi_services_send_stop_listening(
    xiaozhi_services_t *services
)
{
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected) ||
            !atomic_load(&services->audio_channel_open)) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_xiaozhi_chat_send_stop_listening(services->chat);
}

esp_err_t xiaozhi_services_send_abort_speaking(
    xiaozhi_services_t *services,
    xiaozhi_services_abort_reason_t reason
)
{
    if (services == NULL || services->chat == 0 ||
            !atomic_load(&services->connected) ||
            !atomic_load(&services->audio_channel_open)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_xiaozhi_chat_abort_speaking_reason_t component_reason =
        ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING;
    if (reason == XIAOZHI_SERVICES_ABORT_WAKE_WORD_DETECTED) {
        component_reason =
            ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED;
    } else if (reason != XIAOZHI_SERVICES_ABORT_NONE) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_xiaozhi_chat_send_abort_speaking(
        services->chat,
        component_reason
    );
}

esp_err_t xiaozhi_services_get_info(xiaozhi_services_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xiaozhi_services_free_info(info);
    esp_xiaozhi_chat_info_t managed_info = {0};
    esp_err_t ret = esp_xiaozhi_chat_get_info(&managed_info);
    if (ret != ESP_OK) {
        (void)esp_xiaozhi_chat_free_info(&managed_info);
        return ret;
    }

    info->has_activation_code = managed_info.has_activation_code;
    info->has_activation_challenge = managed_info.has_activation_challenge;
    info->has_mqtt_config = managed_info.has_mqtt_config;
    info->has_websocket_config = managed_info.has_websocket_config;
    info->activation_timeout_ms = managed_info.activation_timeout_ms;
    info->activation_code = copy_string(managed_info.activation_code);
    info->activation_message = copy_string(managed_info.activation_message);
    info->activation_challenge = copy_string(managed_info.activation_challenge);

    if ((managed_info.activation_code != NULL &&
            info->activation_code == NULL) ||
            (managed_info.activation_message != NULL &&
            info->activation_message == NULL) ||
            (managed_info.activation_challenge != NULL &&
            info->activation_challenge == NULL)) {
        xiaozhi_services_free_info(info);
        ret = ESP_ERR_NO_MEM;
    }

    (void)esp_xiaozhi_chat_free_info(&managed_info);
    return ret;
}

void xiaozhi_services_free_info(xiaozhi_services_info_t *info)
{
    if (info == NULL) {
        return;
    }
    free(info->activation_code);
    free(info->activation_message);
    free(info->activation_challenge);
    memset(info, 0, sizeof(*info));
}
