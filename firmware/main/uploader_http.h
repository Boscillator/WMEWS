#pragma once

#include <stddef.h>

#include "config.h"

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
} uploader_http_error_t;

/**
 * On success, upload_url contains the nonempty upload_url string returned by the API.
 * Calls must be serialized because the module uses static request and response workspace.
 */
uploader_http_error_t uploader_http_get_upload_url(const config_credentials_t *credentials,
                                                   char *upload_url, size_t upload_url_size);
