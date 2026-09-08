/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiActivationClient.hpp"

#include <cstring>

#include "board/bsp_extra_xiaozhi_identity.h"
#include "services/include/xiaozhi_activation.h"

namespace esp_brookesia::apps {

namespace {

esp_err_t load_identity(
    xiaozhi_activation_identity_t *identity,
    void *context
)
{
    (void)context;
    if (identity == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    bsp_extra_xiaozhi_identity_t board_identity = {};
    esp_err_t ret = bsp_extra_xiaozhi_identity_load(&board_identity);
    if (ret != ESP_OK) {
        return ret;
    }

    memcpy(identity->uuid, board_identity.uuid, sizeof(identity->uuid));
    memcpy(
        identity->mac_address,
        board_identity.mac_address,
        sizeof(identity->mac_address)
    );
    memcpy(
        identity->serial_number,
        board_identity.serial_number,
        sizeof(identity->serial_number)
    );
    identity->factory = board_identity.factory;
    return ESP_OK;
}

esp_err_t calculate_hmac(
    const void *data,
    size_t data_length,
    uint8_t *output,
    size_t output_size,
    void *context
)
{
    (void)context;
    if (output == nullptr || output_size < 32) {
        return ESP_ERR_INVALID_SIZE;
    }
    return bsp_extra_xiaozhi_identity_calculate_hmac(
        data,
        data_length,
        output
    );
}

} // namespace

XiaozhiActivationClient::XiaozhiActivationClient()
{
    xiaozhi_activation_port_t port = {};
    port.load_identity = load_identity;
    port.calculate_hmac = calculate_hmac;
    _init_error = xiaozhi_activation_client_create(&port, &_client);
}

XiaozhiActivationClient::~XiaozhiActivationClient()
{
    if (_client != nullptr) {
        (void)xiaozhi_activation_client_destroy(_client);
        _client = nullptr;
    }
}

esp_err_t XiaozhiActivationClient::activate(
    const Info &info,
    int &http_status
)
{
    http_status = 0;
    if (_client == nullptr) {
        return _init_error;
    }
    if (info.state != BindingState::ActivationRequired ||
            info.challenge.empty()) {
        return ESP_ERR_INVALID_STATE;
    }
    return xiaozhi_activation_client_activate(
        _client,
        info.challenge.c_str(),
        &http_status
    );
}

void XiaozhiActivationClient::cancel()
{
    if (_client != nullptr) {
        xiaozhi_activation_client_cancel(_client);
    }
}

} // namespace esp_brookesia::apps
