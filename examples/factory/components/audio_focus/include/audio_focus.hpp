#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace audio_focus {

enum class client : uint8_t {
    spectrum = 0,
    music,
    video,
    prompt,
    xiaozhi,
    max,
};

using callback = bool (*)(void *user_data);

struct callbacks {
    callback activate = nullptr;
    callback suspend = nullptr;
    void *user_data = nullptr;
};

class manager {
public:
    static manager &instance();

    bool request(client requested_client, const callbacks &client_callbacks);
    bool abandon(client abandoned_client);
    bool is_active(client queried_client);

private:
    struct entry {
        bool requested = false;
        callbacks client_callbacks = {};
    };

    manager();
    client select_next() const;
    bool activate_next();

    SemaphoreHandle_t mutex_ = nullptr;
    std::array<entry, static_cast<std::size_t>(client::max)> entries_ = {};
    client active_client_ = client::max;
};

} // namespace audio_focus
