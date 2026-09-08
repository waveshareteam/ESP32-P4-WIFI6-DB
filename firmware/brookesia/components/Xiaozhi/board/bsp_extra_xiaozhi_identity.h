/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char uuid[37];
    char mac_address[18];
    char serial_number[33];
    bool factory;
} bsp_extra_xiaozhi_identity_t;

/** Load the persistent and silicon-backed identity used by activation. */
esp_err_t bsp_extra_xiaozhi_identity_load(
    bsp_extra_xiaozhi_identity_t *identity
);

/** Calculate the factory activation HMAC with the board-provisioned key. */
esp_err_t bsp_extra_xiaozhi_identity_calculate_hmac(
    const void *data,
    size_t data_length,
    unsigned char output[32]
);

#ifdef __cplusplus
}
#endif
