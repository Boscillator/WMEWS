#include "uploader_http.h"

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "uploader_http";
enum {
    REQUEST_URL_SIZE = CONFIG_LAMBDA_URL_MAX_LENGTH + sizeof("/upload-url"),
    RESPONSE_BUFFER_SIZE = UPLOADER_HTTP_UPLOAD_URL_MAX_LENGTH + 1U,
    HTTP_TIMEOUT_MS = 10000U,
    /* SigV4 query parameters require a request line larger than the HTTP default. */
    UPLOAD_REQUEST_BUFFER_SIZE = UPLOADER_HTTP_UPLOAD_URL_MAX_LENGTH + 512U,
};
static const char *const UPLOAD_CONTENT_TYPE = "application/octet-stream";

typedef struct {
    char data[RESPONSE_BUFFER_SIZE];
    size_t length;
    bool overflowed;
} response_buffer_t;

/* The single uploader task serializes requests, so this workspace needs no lock. */
static char s_request_url[REQUEST_URL_SIZE];
static response_buffer_t s_response;

static esp_err_t collect_response(esp_http_client_event_t *event)
{
    if (event == NULL || event->user_data == NULL) {
        ESP_LOGE(TAG, "Response callback received invalid context");
        return ESP_FAIL;
    }
    if (event->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }

    response_buffer_t *response = event->user_data;
    if (event->data_len == 0) {
        return ESP_OK;
    }
    if (event->data == NULL || event->data_len < 0 || (size_t)event->data_len >
        sizeof(response->data) - 1U - response->length) {
        response->overflowed = true;
        return ESP_FAIL;
    }

    memcpy(response->data + response->length, event->data, (size_t)event->data_len);
    response->length += (size_t)event->data_len;
    response->data[response->length] = '\0';
    return ESP_OK;
}

static bool is_valid_string(const char *value, size_t value_size)
{
    if (value == NULL) {
        return false;
    }

    const size_t length = strnlen(value, value_size);
    return length > 0U && length < value_size;
}

void uploader_http_upload_init(uploader_http_upload_session_t *session)
{
    if (session != NULL) {
        memset(session, 0, sizeof(*session));
    }
}

void uploader_http_upload_abort(uploader_http_upload_session_t *session)
{
    if (session == NULL || session->client == NULL) {
        return;
    }

    esp_http_client_close(session->client);
    esp_http_client_cleanup(session->client);
    session->client = NULL;
    session->content_length = 0U;
    session->bytes_written = 0U;
}

uploader_http_error_t uploader_http_upload_start(uploader_http_upload_session_t *session,
                                                 const char *upload_url, size_t content_length)
{
    if (session == NULL || upload_url == NULL || strncmp(upload_url, "https://", 8U) != 0 ||
        upload_url[8] == '\0' || content_length == 0U) {
        ESP_LOGE(TAG, "Upload start failed: invalid URL or content length");
        return UPLOADER_HTTP_ERR_INVALID_ARGUMENT;
    }
    if (session->client != NULL) {
        ESP_LOGE(TAG, "Upload start failed: session is already active");
        return UPLOADER_HTTP_ERR_INVALID_STATE;
    }
    if (content_length > INT_MAX) {
        ESP_LOGE(TAG, "Upload start failed: content length exceeds HTTP client limit");
        return UPLOADER_HTTP_ERR_CONTENT_LENGTH;
    }

    const esp_http_client_config_t config = {
        .url = upload_url,
        .method = HTTP_METHOD_PUT,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size_tx = UPLOAD_REQUEST_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    session->client = esp_http_client_init(&config);
    if (session->client == NULL) {
        ESP_LOGE(TAG, "Upload start failed: could not initialize HTTP client");
        return UPLOADER_HTTP_ERR_CLIENT_INITIALIZATION;
    }

    esp_err_t err = esp_http_client_set_header(session->client, "Content-Type", UPLOAD_CONTENT_TYPE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Upload start failed: could not set content type: %s", esp_err_to_name(err));
        uploader_http_upload_abort(session);
        return UPLOADER_HTTP_ERR_CLIENT_CONFIGURATION;
    }
    err = esp_http_client_open(session->client, (int)content_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Upload start failed: could not open request: %s", esp_err_to_name(err));
        uploader_http_upload_abort(session);
        return UPLOADER_HTTP_ERR_OPEN;
    }

    session->content_length = content_length;
    session->bytes_written = 0U;
    return UPLOADER_HTTP_OK;
}

uploader_http_error_t uploader_http_upload_write(uploader_http_upload_session_t *session,
                                                 const uint8_t *bytes, size_t length)
{
    if (session == NULL || session->client == NULL) {
        ESP_LOGE(TAG, "Upload write failed: inactive session");
        return UPLOADER_HTTP_ERR_INVALID_STATE;
    }
    if (bytes == NULL || length == 0U || length > session->content_length - session->bytes_written) {
        ESP_LOGE(TAG, "Upload write failed: invalid fragment");
        return UPLOADER_HTTP_ERR_INVALID_ARGUMENT;
    }

    size_t offset = 0U;
    while (offset < length) {
        const size_t remaining = length - offset;
        const int requested = remaining > INT_MAX ? INT_MAX : (int)remaining;
        const int written = esp_http_client_write(session->client, (const char *)bytes + offset, requested);
        if (written <= 0 || written > requested) {
            ESP_LOGE(TAG, "Upload write failed: HTTP client accepted %d bytes", written);
            return UPLOADER_HTTP_ERR_WRITE;
        }
        offset += (size_t)written;
        session->bytes_written += (size_t)written;
    }
    return UPLOADER_HTTP_OK;
}

uploader_http_error_t uploader_http_upload_finish(uploader_http_upload_session_t *session)
{
    if (session == NULL || session->client == NULL) {
        ESP_LOGE(TAG, "Upload finish failed: inactive session");
        return UPLOADER_HTTP_ERR_INVALID_STATE;
    }
    if (session->bytes_written != session->content_length) {
        ESP_LOGE(TAG, "Upload finish failed: wrote %u of %u bytes", (unsigned)session->bytes_written,
                 (unsigned)session->content_length);
        uploader_http_upload_abort(session);
        return UPLOADER_HTTP_ERR_CONTENT_LENGTH;
    }

    const int64_t response_length = esp_http_client_fetch_headers(session->client);
    if (response_length < 0) {
        ESP_LOGE(TAG, "Upload finish failed: could not fetch response headers");
        uploader_http_upload_abort(session);
        return UPLOADER_HTTP_ERR_RESPONSE_FETCH;
    }
    const int status_code = esp_http_client_get_status_code(session->client);
    uploader_http_upload_abort(session);
    if (status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "Upload finish failed: HTTP status %d", status_code);
        return UPLOADER_HTTP_ERR_HTTP_STATUS;
    }
    return UPLOADER_HTTP_OK;
}

uploader_http_error_t uploader_http_get_upload_url(const config_credentials_t *credentials,
                                                   char *upload_url, size_t upload_url_size)
{
    if (credentials == NULL || upload_url == NULL || upload_url_size == 0U ||
        !is_valid_string(credentials->lambda_url, sizeof(credentials->lambda_url)) ||
        !is_valid_string(credentials->device_id, sizeof(credentials->device_id)) ||
        !is_valid_string(credentials->secret_key, sizeof(credentials->secret_key)) ||
        strncmp(credentials->lambda_url, "https://", sizeof("https://") - 1U) != 0) {
        ESP_LOGE(TAG, "Get upload URL failed: invalid credentials or output buffer");
        return UPLOADER_HTTP_ERR_INVALID_ARGUMENT;
    }
    upload_url[0] = '\0';

    const size_t base_url_length = strlen(credentials->lambda_url);
    const char *separator = credentials->lambda_url[base_url_length - 1U] == '/' ? "" : "/";
    const int request_url_length = snprintf(s_request_url, sizeof(s_request_url), "%s%supload-url",
                                            credentials->lambda_url, separator);
    if (request_url_length < 0 || (size_t)request_url_length >= sizeof(s_request_url)) {
        ESP_LOGE(TAG, "Get upload URL failed: request URL exceeds buffer");
        return UPLOADER_HTTP_ERR_URL_CONSTRUCTION;
    }

    memset(&s_response, 0, sizeof(s_response));
    const esp_http_client_config_t config = {
        .url = s_request_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = collect_response,
        .user_data = &s_response,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Get upload URL failed: could not initialize HTTP client");
        return UPLOADER_HTTP_ERR_CLIENT_INITIALIZATION;
    }

    uploader_http_error_t result = UPLOADER_HTTP_OK;
    esp_err_t err = esp_http_client_set_header(client, "X-WMEWS-Secret", credentials->secret_key);
    if (err == ESP_OK) {
        err = esp_http_client_set_header(client, "X-WMEWS-Device-Id", credentials->device_id);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Get upload URL failed: could not set HTTP headers: %s", esp_err_to_name(err));
        result = UPLOADER_HTTP_ERR_CLIENT_CONFIGURATION;
        goto cleanup;
    }

    err = esp_http_client_perform(client);
    if (s_response.overflowed) {
        ESP_LOGE(TAG, "Get upload URL failed: response exceeds %u bytes",
                 (unsigned)UPLOADER_HTTP_UPLOAD_URL_MAX_LENGTH);
        result = UPLOADER_HTTP_ERR_RESPONSE_OVERFLOW;
        goto cleanup;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Get upload URL failed: HTTP transport error: %s", esp_err_to_name(err));
        result = UPLOADER_HTTP_ERR_TRANSPORT;
        goto cleanup;
    }

    const int status_code = esp_http_client_get_status_code(client);
    if (status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "Get upload URL failed: HTTP status %d", status_code);
        result = UPLOADER_HTTP_ERR_HTTP_STATUS;
        goto cleanup;
    }

    cJSON *document = cJSON_ParseWithLengthOpts(s_response.data, s_response.length + 1U, NULL, true);
    const cJSON *url = document == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(document, "upload_url");
    if (!cJSON_IsString(url) || url->valuestring == NULL || url->valuestring[0] == '\0') {
        ESP_LOGE(TAG, "Get upload URL failed: response has no valid upload_url");
        cJSON_Delete(document);
        result = UPLOADER_HTTP_ERR_MALFORMED_RESPONSE;
        goto cleanup;
    }

    const size_t url_length = strlen(url->valuestring);
    if (url_length >= upload_url_size) {
        ESP_LOGE(TAG, "Get upload URL failed: upload URL exceeds output buffer");
        cJSON_Delete(document);
        result = UPLOADER_HTTP_ERR_OUTPUT_OVERFLOW;
        goto cleanup;
    }
    memcpy(upload_url, url->valuestring, url_length + 1U);
    cJSON_Delete(document);

cleanup:
    esp_http_client_cleanup(client);
    return result;
}
