/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiAudioProcessor.hpp"

#include <mutex>
#include <utility>

namespace esp_brookesia::apps {

XiaozhiAudioProcessor::XiaozhiAudioProcessor()
{
    xiaozhi_audio_processor_callbacks_t callbacks = {};
    callbacks.context = this;
    callbacks.on_wake_word = wake_word_callback;
    callbacks.on_pcm_frame = pcm_frame_callback;
    callbacks.on_vad = vad_callback;
    _init_error = xiaozhi_audio_processor_create(&callbacks, &_processor);
}

XiaozhiAudioProcessor::~XiaozhiAudioProcessor()
{
    if (_processor != nullptr) {
        (void)xiaozhi_audio_processor_destroy(_processor);
        _processor = nullptr;
    }
}

bool XiaozhiAudioProcessor::initialize()
{
    return _processor != nullptr &&
           xiaozhi_audio_processor_initialize(_processor) == ESP_OK;
}

bool XiaozhiAudioProcessor::shutdown()
{
    if (_processor == nullptr) {
        return _init_error == ESP_OK;
    }
    return xiaozhi_audio_processor_shutdown(_processor) == ESP_OK;
}

bool XiaozhiAudioProcessor::feed(
    const int16_t *interleaved_pcm,
    size_t frame_count
)
{
    return _processor != nullptr &&
           xiaozhi_audio_processor_feed(
               _processor,
               interleaved_pcm,
               frame_count
           ) == ESP_OK;
}

bool XiaozhiAudioProcessor::enableWakeWord(bool enable)
{
    return _processor != nullptr &&
           xiaozhi_audio_processor_enable_wake_word(_processor, enable) ==
           ESP_OK;
}

bool XiaozhiAudioProcessor::enableVoiceProcessing(bool enable)
{
    return _processor != nullptr &&
           xiaozhi_audio_processor_enable_voice_processing(
               _processor,
               enable
           ) == ESP_OK;
}

bool XiaozhiAudioProcessor::isReady() const
{
    return xiaozhi_audio_processor_is_ready(_processor);
}

size_t XiaozhiAudioProcessor::getFeedFrames() const
{
    return xiaozhi_audio_processor_get_feed_frames(_processor);
}

bool XiaozhiAudioProcessor::takeWakeWordPcm(std::vector<int16_t> &pcm)
{
    pcm.resize(XIAOZHI_AUDIO_WAKE_WORD_CACHE_SAMPLES);
    size_t sample_count = 0;
    esp_err_t ret = xiaozhi_audio_processor_take_wake_word_pcm(
        _processor,
        pcm.data(),
        pcm.size(),
        &sample_count
    );
    if (ret != ESP_OK) {
        pcm.clear();
        return false;
    }
    pcm.resize(sample_count);
    return true;
}

void XiaozhiAudioProcessor::setWakeWordCallback(WakeWordCallback callback)
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    _wake_word_callback = std::move(callback);
}

void XiaozhiAudioProcessor::setPcmFrameCallback(PcmFrameCallback callback)
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    _pcm_frame_callback = std::move(callback);
}

void XiaozhiAudioProcessor::setVadCallback(VadCallback callback)
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    _vad_callback = std::move(callback);
}

XiaozhiAudioProcessor::WakeWordCallback
XiaozhiAudioProcessor::copyWakeWordCallback()
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    return _wake_word_callback;
}

XiaozhiAudioProcessor::PcmFrameCallback
XiaozhiAudioProcessor::copyPcmFrameCallback()
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    return _pcm_frame_callback;
}

XiaozhiAudioProcessor::VadCallback
XiaozhiAudioProcessor::copyVadCallback()
{
    std::lock_guard<std::mutex> lock(_callback_mutex);
    return _vad_callback;
}

void XiaozhiAudioProcessor::wake_word_callback(
    const char *wake_word,
    void *context
)
{
    auto *processor = static_cast<XiaozhiAudioProcessor *>(context);
    if (processor == nullptr) {
        return;
    }
    WakeWordCallback callback = processor->copyWakeWordCallback();
    if (callback) {
        std::string value = wake_word != nullptr ? wake_word : "";
        callback(value);
    }
}

void XiaozhiAudioProcessor::pcm_frame_callback(
    const int16_t *pcm,
    size_t sample_count,
    void *context
)
{
    auto *processor = static_cast<XiaozhiAudioProcessor *>(context);
    if (processor == nullptr) {
        return;
    }
    PcmFrameCallback callback = processor->copyPcmFrameCallback();
    if (callback) {
        callback(pcm, sample_count);
    }
}

void XiaozhiAudioProcessor::vad_callback(bool speaking, void *context)
{
    auto *processor = static_cast<XiaozhiAudioProcessor *>(context);
    if (processor == nullptr) {
        return;
    }
    VadCallback callback = processor->copyVadCallback();
    if (callback) {
        callback(speaking);
    }
}

} // namespace esp_brookesia::apps
