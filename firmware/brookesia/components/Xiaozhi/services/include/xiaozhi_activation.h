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

typedef struct xiaozhi_activation_client xiaozhi_activation_client_t;

typedef struct {
    char uuid[37];
    char mac_address[18];
    char serial_number[33];
    bool factory;
} xiaozhi_activation_identity_t;

typedef struct {
    void *context;
    esp_err_t (*load_identity)(
        xiaozhi_activation_identity_t *identity,
        void *context
    );
    esp_err_t (*calculate_hmac)(
        const void *data,
        size_t data_length,
        uint8_t *output,
        size_t output_size,
        void *context
    );
} xiaozhi_activation_port_t;

esp_err_t xiaozhi_activation_client_create(
    const xiaozhi_activation_port_t *port,
    xiaozhi_activation_client_t **out_client
);

esp_err_t xiaozhi_activation_client_destroy(
    xiaozhi_activation_client_t *client
);

esp_err_t xiaozhi_activation_client_activate(
    xiaozhi_activation_client_t *client,
    const char *challenge,
    int *http_status
);

void xiaozhi_activation_client_cancel(
    xiaozhi_activation_client_t *client
);

#ifdef __cplusplus
}
#endif
