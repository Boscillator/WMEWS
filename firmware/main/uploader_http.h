#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "esp_http_client.h"

enum {
    UPLOADER_HTTP_UPLOAD_URL_MAX_LENGTH = 4096U,
};

typedef enum {
    UPLOADER_HTTP_OK = 0,
    UPLOADER_HTTP_ERR_INVALID_ARGUMENT,
    UPLOADER_HTTP_ERR_URL_CONSTRUCTION,
    UPLOADER_HTTP_ERR_CLIENT_INITIALIZATION,
    UPLOADER_HTTP_ERR_CLIENT_CONFIGURATION,
    UPLOADER_HTTP_ERR_TRANSPORT,
    UPLOADER_HTTP_ERR_HTTP_STATUS,
    UPLOADER_HTTP_ERR_RESPONSE_OVERFLOW,
    UPLOADER_HTTP_ERR_MALFORMED_RESPONSE,
    UPLOADER_HTTP_ERR_OUTPUT_OVERFLOW,
    UPLOADER_HTTP_ERR_INVALID_STATE,
    UPLOADER_HTTP_ERR_CONTENT_LENGTH,
    UPLOADER_HTTP_ERR_OPEN,
    UPLOADER_HTTP_ERR_WRITE,
    UPLOADER_HTTP_ERR_RESPONSE_FETCH,
} uploader_http_error_t;

/**
 * One active presigned PUT request. The uploader task must serialize all module
 * calls; a session is inactive when client is NULL.
 */
typedef struct {
    esp_http_client_handle_t client;
    size_t content_length;
    size_t bytes_written;
} uploader_http_upload_session_t;

/**
 * On success, upload_url contains the nonempty upload_url string returned by the API.
 * Calls must be serialized because the module uses static request and response workspace.
 */
uploader_http_error_t uploader_http_get_upload_url(const config_credentials_t *credentials,
                                                   char *upload_url, size_t upload_url_size);

void uploader_http_upload_init(uploader_http_upload_session_t *session);

/** Open an HTTPS PUT request with an exact nonzero content length. */
uploader_http_error_t uploader_http_upload_start(uploader_http_upload_session_t *session,
                                                 const char *upload_url, size_t content_length);

/** Accept a complete nonempty fragment, including internally handled partial writes. */
uploader_http_error_t uploader_http_upload_write(uploader_http_upload_session_t *session,
                                                 const uint8_t *bytes, size_t length);

/** Finish the request and return success only for a final 2xx response. */
uploader_http_error_t uploader_http_upload_finish(uploader_http_upload_session_t *session);

/** Idempotently close and clean up an active request after a prior failure. */
void uploader_http_upload_abort(uploader_http_upload_session_t *session);
