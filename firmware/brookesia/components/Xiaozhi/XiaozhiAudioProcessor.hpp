/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

#include "services/include/xiaozhi_audio_processor.h"

namespace esp_brookesia::apps {

class XiaozhiAudioProcessor final {
public:
    static constexpr uint32_t SAMPLE_RATE = XIAOZHI_AUDIO_SAMPLE_RATE;
    static constexpr size_t INPUT_CHANNELS = XIAOZHI_AUDIO_INPUT_CHANNELS;
    static constexpr size_t PCM_FRAME_SAMPLES =
        XIAOZHI_AUDIO_PCM_FRAME_SAMPLES;

    using WakeWordCallback = std::function<void(const std::string &wake_word)>;
    using PcmFrameCallback = std::function<void(
        const int16_t *pcm,
        size_t sample_count
    )>;
    using VadCallback = std::function<void(bool speaking)>;

    XiaozhiAudioProcessor();
    ~XiaozhiAudioProcessor();

    XiaozhiAudioProcessor(const XiaozhiAudioProcessor &) = delete;
    XiaozhiAudioProcessor &operator=(const XiaozhiAudioProcessor &) = delete;

    bool initialize();
    bool shutdown();
    bool feed(const int16_t *interleaved_pcm, size_t frame_count);
    bool enableWakeWord(bool enable);
    bool enableVoiceProcessing(bool enable);
    bool isReady() const;
    size_t getFeedFrames() const;
    bool takeWakeWordPcm(std::vector<int16_t> &pcm);

    void setWakeWordCallback(WakeWordCallback callback);
    void setPcmFrameCallback(PcmFrameCallback callback);
    void setVadCallback(VadCallback callback);

private:
    xiaozhi_audio_processor_t *_processor = nullptr;
    esp_err_t _init_error = ESP_OK;
    mutable std::mutex _callback_mutex;
    WakeWordCallback _wake_word_callback;
    PcmFrameCallback _pcm_frame_callback;
    VadCallback _vad_callback;

    WakeWordCallback copyWakeWordCallback();
    PcmFrameCallback copyPcmFrameCallback();
    VadCallback copyVadCallback();

    static void wake_word_callback(const char *wake_word, void *context);
    static void pcm_frame_callback(
        const int16_t *pcm,
        size_t sample_count,
        void *context
    );
    static void vad_callback(bool speaking, void *context);
};

} // namespace esp_brookesia::apps
