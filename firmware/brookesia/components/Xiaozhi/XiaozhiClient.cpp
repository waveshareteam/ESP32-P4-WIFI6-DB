/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiClient.hpp"

#include <new>
#include <utility>

#include "services/include/xiaozhi_services.h"

namespace esp_brookesia::apps {

class XiaozhiClient::Impl {
public:
    Impl(Config config, Callbacks callbacks):
        _config(std::move(config)),
        _callbacks(std::move(callbacks))
    {
    }

    ~Impl()
    {
        (void)stop();
    }

    esp_err_t start()
    {
        if (_services != nullptr) {
            return ESP_OK;
        }

        xiaozhi_services_config_t service_config = {};
        service_config.has_mqtt_config = _config.has_mqtt_config;
        service_config.has_websocket_config = _config.has_websocket_config;
        service_config.sample_rate = _config.sample_rate;
        service_config.channels = _config.channels;
        service_config.frame_duration_ms = _config.frame_duration_ms;
        service_config.playback_sample_rate = _config.playback_sample_rate;

        xiaozhi_services_callbacks_t service_callbacks = {};
        service_callbacks.context = this;
        service_callbacks.on_connected = on_connected;
        service_callbacks.on_disconnected = on_disconnected;
        service_callbacks.on_audio_channel_opened = on_audio_channel_opened;
        service_callbacks.on_audio_channel_closed = on_audio_channel_closed;
        service_callbacks.on_server_goodbye = on_server_goodbye;
        service_callbacks.on_text = on_text;
        service_callbacks.on_emotion = on_emotion;
        service_callbacks.on_tts = on_tts;
        service_callbacks.on_audio = on_audio;
        service_callbacks.on_error = on_error;

        esp_err_t ret = xiaozhi_services_create(
            &service_config,
            &service_callbacks,
            &_services
        );
        if (ret != ESP_OK) {
            return ret;
        }

        ret = xiaozhi_services_start(_services);
        if (ret != ESP_OK) {
            (void)xiaozhi_services_destroy(_services);
            _services = nullptr;
        }
        return ret;
    }

    esp_err_t stop()
    {
        if (_services == nullptr) {
            return ESP_OK;
        }
        xiaozhi_services_t *services = _services;
        _services = nullptr;
        return xiaozhi_services_destroy(services);
    }

    esp_err_t open_audio_channel()
    {
        return _services != nullptr ?
               xiaozhi_services_open_audio_channel(_services) :
               ESP_ERR_INVALID_STATE;
    }

    esp_err_t close_audio_channel()
    {
        return _services != nullptr ?
               xiaozhi_services_close_audio_channel(_services) :
               ESP_ERR_INVALID_STATE;
    }

    bool is_connected() const
    {
        return xiaozhi_services_is_connected(_services);
    }

    bool is_audio_channel_open() const
    {
        return xiaozhi_services_is_audio_channel_open(_services);
    }

    esp_err_t send_audio(
        const uint8_t *data,
        size_t length,
        uint32_t timestamp
    )
    {
        return _services != nullptr ?
               xiaozhi_services_send_audio(
                   _services,
                   data,
                   length,
                   timestamp
               ) : ESP_ERR_INVALID_STATE;
    }

    esp_err_t send_wake_word_detected(const std::string &wake_word)
    {
        return _services != nullptr ?
               xiaozhi_services_send_wake_word(
                   _services,
                   wake_word.c_str()
               ) : ESP_ERR_INVALID_STATE;
    }

    esp_err_t send_start_listening(ListeningMode mode)
    {
        if (_services == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        xiaozhi_services_listening_mode_t service_mode =
            XIAOZHI_SERVICES_LISTENING_AUTO_STOP;
        switch (mode) {
        case ListeningMode::AutoStop:
            service_mode = XIAOZHI_SERVICES_LISTENING_AUTO_STOP;
            break;
        case ListeningMode::ManualStop:
            service_mode = XIAOZHI_SERVICES_LISTENING_MANUAL_STOP;
            break;
        case ListeningMode::Realtime:
            service_mode = XIAOZHI_SERVICES_LISTENING_REALTIME;
            break;
        }
        return xiaozhi_services_send_start_listening(_services, service_mode);
    }

    esp_err_t send_stop_listening()
    {
        return _services != nullptr ?
               xiaozhi_services_send_stop_listening(_services) :
               ESP_ERR_INVALID_STATE;
    }

    esp_err_t send_abort_speaking(AbortReason reason)
    {
        if (_services == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        xiaozhi_services_abort_reason_t service_reason =
            XIAOZHI_SERVICES_ABORT_NONE;
        if (reason == AbortReason::WakeWordDetected) {
            service_reason = XIAOZHI_SERVICES_ABORT_WAKE_WORD_DETECTED;
        }
        return xiaozhi_services_send_abort_speaking(
            _services,
            service_reason
        );
    }

private:
    Config _config;
    Callbacks _callbacks;
    xiaozhi_services_t *_services = nullptr;

    static Impl *from_context(void *context)
    {
        return static_cast<Impl *>(context);
    }

    static void on_connected(void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onConnected) {
            self->_callbacks.onConnected();
        }
    }

    static void on_disconnected(void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onDisconnected) {
            self->_callbacks.onDisconnected();
        }
    }

    static void on_audio_channel_opened(void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onAudioChannelOpened) {
            self->_callbacks.onAudioChannelOpened();
        }
    }

    static void on_audio_channel_closed(void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onAudioChannelClosed) {
            self->_callbacks.onAudioChannelClosed();
        }
    }

    static void on_server_goodbye(void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onServerGoodbye) {
            self->_callbacks.onServerGoodbye();
        }
    }

    static void on_text(
        xiaozhi_services_text_role_t role,
        const char *text,
        void *context
    )
    {
        Impl *self = from_context(context);
        if (self == nullptr || !self->_callbacks.onText) {
            return;
        }
        TextRole app_role = role == XIAOZHI_SERVICES_TEXT_ROLE_ASSISTANT ?
                            TextRole::Assistant : TextRole::User;
        self->_callbacks.onText(app_role, text != nullptr ? text : "");
    }

    static void on_emotion(const char *emotion, void *context)
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onEmotion) {
            self->_callbacks.onEmotion(emotion != nullptr ? emotion : "");
        }
    }

    static void on_tts(
        xiaozhi_services_tts_state_t state,
        const char *text,
        void *context
    )
    {
        Impl *self = from_context(context);
        if (self == nullptr || !self->_callbacks.onTts) {
            return;
        }
        TtsState app_state = TtsState::SentenceStart;
        if (state == XIAOZHI_SERVICES_TTS_START) {
            app_state = TtsState::Start;
        } else if (state == XIAOZHI_SERVICES_TTS_STOP) {
            app_state = TtsState::Stop;
        }
        self->_callbacks.onTts(
            app_state,
            text != nullptr ? text : ""
        );
    }

    static void on_audio(
        const uint8_t *data,
        size_t length,
        int sample_rate,
        int frame_duration_ms,
        uint32_t timestamp,
        void *context
    )
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onAudio) {
            self->_callbacks.onAudio(
                data,
                length,
                sample_rate,
                frame_duration_ms,
                timestamp
            );
        }
    }

    static void on_error(
        esp_err_t error,
        const char *source,
        void *context
    )
    {
        Impl *self = from_context(context);
        if (self != nullptr && self->_callbacks.onError) {
            self->_callbacks.onError(
                error,
                source != nullptr ? source : "xiaozhi_services"
            );
        }
    }
};

XiaozhiClient::Config::Config(
    bool mqtt_config,
    bool websocket_config,
    int uplink_sample_rate,
    int uplink_channels,
    int frame_duration,
    int output_sample_rate
):
    has_mqtt_config(mqtt_config),
    has_websocket_config(websocket_config),
    sample_rate(uplink_sample_rate),
    channels(uplink_channels),
    frame_duration_ms(frame_duration),
    playback_sample_rate(output_sample_rate)
{
}

XiaozhiClient::XiaozhiClient(Config config, Callbacks callbacks):
    _impl(new (std::nothrow) Impl(std::move(config), std::move(callbacks)))
{
}

XiaozhiClient::~XiaozhiClient()
{
    delete _impl;
}

esp_err_t XiaozhiClient::start()
{
    return _impl != nullptr ? _impl->start() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::stop()
{
    return _impl != nullptr ? _impl->stop() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::openAudioChannel()
{
    return _impl != nullptr ? _impl->open_audio_channel() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::closeAudioChannel()
{
    return _impl != nullptr ? _impl->close_audio_channel() : ESP_ERR_NO_MEM;
}

bool XiaozhiClient::isConnected() const
{
    return _impl != nullptr && _impl->is_connected();
}

bool XiaozhiClient::isAudioChannelOpen() const
{
    return _impl != nullptr && _impl->is_audio_channel_open();
}

esp_err_t XiaozhiClient::sendAudio(
    const uint8_t *data,
    size_t length,
    uint32_t timestamp
)
{
    return _impl != nullptr ?
           _impl->send_audio(data, length, timestamp) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::sendWakeWordDetected(
    const std::string &wake_word
)
{
    return _impl != nullptr ?
           _impl->send_wake_word_detected(wake_word) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::sendStartListening(ListeningMode mode)
{
    return _impl != nullptr ?
           _impl->send_start_listening(mode) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::sendStopListening()
{
    return _impl != nullptr ?
           _impl->send_stop_listening() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiClient::sendAbortSpeaking(AbortReason reason)
{
    return _impl != nullptr ?
           _impl->send_abort_speaking(reason) : ESP_ERR_NO_MEM;
}

} // namespace esp_brookesia::apps
