/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "xiaozhi_activation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *tag = "xiaozhi_activation";
static const char *language = "zh-CN";

struct xiaozhi_activation_client {
    xiaozhi_activation_port_t port;
    SemaphoreHandle_t mutex;
    esp_http_client_handle_t active_client;
};

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (event->event_id == HTTP_EVENT_REDIRECT) {
        return esp_http_client_set_redirection(event->client);
    }
    return ESP_OK;
}

static esp_err_t set_request_headers(
    esp_http_client_handle_t client,
    const xiaozhi_activation_identity_t *identity
)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char user_agent[96] = {0};
    char activation_version[2] = {
        identity->factory ? '2' : '1',
        '\0',
    };

    snprintf(
        user_agent,
        sizeof(user_agent),
        "%s/%s",
        CONFIG_IDF_TARGET,
        app != NULL ? app->version : "unknown"
    );

    esp_err_t ret = esp_http_client_set_header(
        client,
        "Activation-Version",
        activation_version
    );
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_http_client_set_header(
        client,
        "Device-Id",
        identity->mac_address
    );
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_http_client_set_header(client, "Client-Id", identity->uuid);
    if (ret != ESP_OK) {
        return ret;
    }
    if (identity->factory) {
        ret = esp_http_client_set_header(
            client,
            "Serial-Number",
            identity->serial_number
        );
        if (ret != ESP_OK) {
            return ret;
        }
    }
    ret = esp_http_client_set_header(client, "User-Agent", user_agent);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_http_client_set_header(client, "Accept-Language", language);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_http_client_set_header(
        client,
        "Content-Type",
        "application/json"
    );
}

static esp_err_t perform_post(
    xiaozhi_activation_client_t *client,
    const char *url,
    const xiaozhi_activation_identity_t *identity,
    const char *body,
    int *http_status
)
{
    if (client == NULL || url == NULL || url[0] == '\0' ||
            identity == NULL || body == NULL || client->mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (http_status != NULL) {
        *http_status = 0;
    }

    esp_http_client_config_t config = {0};
    config.url = url;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = CONFIG_XIAOZHI_INFO_TIMEOUT_MS;

    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (http_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(client->mutex, portMAX_DELAY) != pdTRUE) {
        esp_http_client_cleanup(http_client);
        return ESP_ERR_INVALID_STATE;
    }
    if (client->active_client != NULL) {
        xSemaphoreGive(client->mutex);
        esp_http_client_cleanup(http_client);
        return ESP_ERR_INVALID_STATE;
    }
    client->active_client = http_client;
    xSemaphoreGive(client->mutex);

    esp_err_t ret = set_request_headers(http_client, identity);
    if (ret == ESP_OK) {
        ret = esp_http_client_set_post_field(
            http_client,
            body,
            (int)strlen(body)
        );
    }
    if (ret == ESP_OK) {
        do {
            ret = esp_http_client_perform(http_client);
        } while (ret == ESP_ERR_HTTP_EAGAIN);
    }
    if (ret == ESP_OK && http_status != NULL) {
        *http_status = esp_http_client_get_status_code(http_client);
    }

    if (xSemaphoreTake(client->mutex, portMAX_DELAY) == pdTRUE) {
        if (client->active_client == http_client) {
            client->active_client = NULL;
        }
        xSemaphoreGive(client->mutex);
    }
    esp_http_client_cleanup(http_client);
    return ret;
}

static esp_err_t build_activation_body(
    xiaozhi_activation_client_t *client,
    const xiaozhi_activation_identity_t *identity,
    const char *challenge,
    char **out_body
)
{
    if (client == NULL || identity == NULL || challenge == NULL ||
            out_body == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_body = NULL;
    if (!identity->factory) {
        cJSON *payload = cJSON_CreateObject();
        if (payload == NULL) {
            return ESP_ERR_NO_MEM;
        }
        *out_body = cJSON_PrintUnformatted(payload);
        cJSON_Delete(payload);
        return *out_body != NULL ? ESP_OK : ESP_ERR_NO_MEM;
    }
    if (client->port.calculate_hmac == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint8_t hmac[32] = {0};
    esp_err_t ret = client->port.calculate_hmac(
        challenge,
        strlen(challenge),
        hmac,
        sizeof(hmac),
        client->port.context
    );
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to calculate activation HMAC");
        return ret;
    }

    char hmac_hex[65] = {0};
    for (size_t index = 0; index < sizeof(hmac); ++index) {
        snprintf(hmac_hex + index * 2, 3, "%02x", hmac[index]);
    }

    cJSON *payload = cJSON_CreateObject();
    if (payload == NULL ||
            cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256") ==
            NULL ||
            cJSON_AddStringToObject(
                payload,
                "serial_number",
                identity->serial_number
            ) == NULL ||
            cJSON_AddStringToObject(payload, "challenge", challenge) == NULL ||
            cJSON_AddStringToObject(payload, "hmac", hmac_hex) == NULL) {
        cJSON_Delete(payload);
        return ESP_ERR_NO_MEM;
    }

    *out_body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    return *out_body != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t xiaozhi_activation_client_create(
    const xiaozhi_activation_port_t *port,
    xiaozhi_activation_client_t **out_client
)
{
    if (port == NULL || port->load_identity == NULL || out_client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_client = NULL;
    xiaozhi_activation_client_t *client = calloc(1, sizeof(*client));
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    client->port = *port;
    client->mutex = xSemaphoreCreateMutex();
    if (client->mutex == NULL) {
        free(client);
        return ESP_ERR_NO_MEM;
    }
    *out_client = client;
    return ESP_OK;
}

esp_err_t xiaozhi_activation_client_destroy(
    xiaozhi_activation_client_t *client
)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xiaozhi_activation_client_cancel(client);
    if (client->mutex != NULL) {
        vSemaphoreDelete(client->mutex);
    }
    free(client);
    return ESP_OK;
}

esp_err_t xiaozhi_activation_client_activate(
    xiaozhi_activation_client_t *client,
    const char *challenge,
    int *http_status
)
{
    if (client == NULL || challenge == NULL || challenge[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    int local_http_status = 0;
    if (http_status == NULL) {
        http_status = &local_http_status;
    }

    xiaozhi_activation_identity_t identity = {0};
    esp_err_t ret = client->port.load_identity(
        &identity,
        client->port.context
    );
    if (ret != ESP_OK) {
        return ret;
    }

    char *body = NULL;
    ret = build_activation_body(client, &identity, challenge, &body);
    if (ret != ESP_OK) {
        return ret;
    }

    const char *base_url = CONFIG_XIAOZHI_OTA_URL;
    size_t base_length = strlen(base_url);
    bool has_separator = base_length > 0 && base_url[base_length - 1] == '/';
    size_t url_length = base_length + (has_separator ? 0 : 1) +
                        strlen("activate") + 1;
    char *url = malloc(url_length);
    if (url == NULL) {
        cJSON_free(body);
        return ESP_ERR_NO_MEM;
    }
    snprintf(
        url,
        url_length,
        "%s%sactivate",
        base_url,
        has_separator ? "" : "/"
    );

    ret = perform_post(client, url, &identity, body, http_status);
    if (ret == ESP_OK) {
        if (*http_status == 200) {
            ret = ESP_OK;
        } else if (*http_status == 202) {
            ret = ESP_ERR_NOT_FINISHED;
        } else {
            ESP_LOGE(
                tag,
                "Activation request failed with HTTP status %d",
                *http_status
            );
            ret = ESP_FAIL;
        }
    }

    free(url);
    cJSON_free(body);
    return ret;
}

void xiaozhi_activation_client_cancel(
    xiaozhi_activation_client_t *client
)
{
    if (client == NULL || client->mutex == NULL ||
            xSemaphoreTake(client->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (client->active_client != NULL) {
        (void)esp_http_client_cancel_request(client->active_client);
    }
    xSemaphoreGive(client->mutex);
}
