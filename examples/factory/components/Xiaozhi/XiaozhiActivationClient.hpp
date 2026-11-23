/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <string>

#include "esp_err.h"
#include "services/include/xiaozhi_activation.h"

namespace esp_brookesia::apps {

class XiaozhiActivationClient final {
public:
    enum class BindingState : uint8_t {
        Bound,
        ActivationRequired,
    };

    struct Info {
        BindingState state = BindingState::Bound;
        std::string challenge;
    };

    XiaozhiActivationClient();
    ~XiaozhiActivationClient();

    XiaozhiActivationClient(const XiaozhiActivationClient &) = delete;
    XiaozhiActivationClient &operator=(const XiaozhiActivationClient &) = delete;

    esp_err_t activate(const Info &info, int &http_status);
    void cancel();

private:
    xiaozhi_activation_client_t *_client = nullptr;
    esp_err_t _init_error = ESP_OK;
};

} // namespace esp_brookesia::apps
