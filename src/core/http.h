#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    WAIOT_HTTP_OK = 0,
    WAIOT_HTTP_BAD_REQUEST = 400,
    WAIOT_HTTP_CONTENT_TOO_LARGE = 413,
    WAIOT_HTTP_INTERNAL_ERROR = 500,
} waiot_http_status_t;

typedef struct
{
    waiot_http_status_t status;
    const char *content_type;
    const char *body;
} waiot_http_text_response_t;

const char *waiot_http_root_body(void);
waiot_http_text_response_t waiot_http_create(const uint8_t *wasm_data,
                                             size_t wasm_size);
waiot_http_text_response_t waiot_http_stop(void);
waiot_http_text_response_t waiot_http_apply_pinmap_json(const char *body,
                                                        size_t len);
waiot_http_status_t waiot_http_get_logs(int tail, uint8_t **out,
                                        size_t *out_len,
                                        const char **error_message);
