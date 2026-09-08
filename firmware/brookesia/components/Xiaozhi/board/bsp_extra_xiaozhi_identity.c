/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board/bsp_extra_xiaozhi_identity.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_hmac.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs.h"
#include "soc/soc_caps.h"

esp_err_t bsp_extra_xiaozhi_identity_load(
    bsp_extra_xiaozhi_identity_t *identity
)
{
    if (identity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(identity, 0, sizeof(*identity));
    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open("board", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t uuid_length = sizeof(identity->uuid);
    ret = nvs_get_str(handle, "uuid", identity->uuid, &uuid_length);
    if (ret == ESP_ERR_NVS_NOT_FOUND ||
            (ret == ESP_OK && identity->uuid[0] == '\0')) {
        uint8_t uuid[16] = {};
        esp_fill_random(uuid, sizeof(uuid));
        uuid[6] = (uuid[6] & 0x0f) | 0x40;
        uuid[8] = (uuid[8] & 0x3f) | 0x80;
        snprintf(
            identity->uuid,
            sizeof(identity->uuid),
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
            "%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3],
            uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11],
            uuid[12], uuid[13], uuid[14], uuid[15]
        );
        ret = nvs_set_str(handle, "uuid", identity->uuid);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
    }
    nvs_close(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t mac[6] = {};
    ret = esp_read_mac(mac, ESP_MAC_BASE);
    if (ret != ESP_OK) {
        return ret;
    }
    snprintf(
        identity->mac_address,
        sizeof(identity->mac_address),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );

#ifdef ESP_EFUSE_BLOCK_USR_DATA
    uint8_t serial_number[33] = {};
    ret = esp_efuse_read_field_blob(
        ESP_EFUSE_USER_DATA, serial_number, 32 * 8
    );
    if (ret != ESP_OK) {
        return ret;
    }
    if (serial_number[0] != 0) {
        memcpy(identity->serial_number, serial_number, 32);
        identity->serial_number[32] = '\0';
        identity->factory = true;
    }
#endif

    return ESP_OK;
}

esp_err_t bsp_extra_xiaozhi_identity_calculate_hmac(
    const void *data,
    size_t data_length,
    unsigned char output[32]
)
{
    if (data == NULL || data_length == 0 || output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if SOC_HMAC_SUPPORTED
    if (esp_efuse_get_key_purpose(EFUSE_BLK_KEY0) !=
            ESP_EFUSE_KEY_PURPOSE_HMAC_UP) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_hmac_calculate(
        HMAC_KEY0,
        data,
        data_length,
        output
    );
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
